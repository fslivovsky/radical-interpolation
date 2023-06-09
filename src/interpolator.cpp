#include "interpolator.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>

namespace cadical_itp {

Interpolator::Interpolator(): clause_id(0) {}

void Interpolator::add_clause(const std::vector<int>& clause, bool first_part) {
  auto id = solver.get_current_clause_id() + 1;
  std::cerr << "Adding clause with id " << id << std::endl;
  solver.add_clause(clause);
  //auto id = ++clause_id;
  id_in_first_part[id] = first_part;
  id_to_clause[id] = clause;
  for (auto l: clause) {
    auto v = abs(l);
    while(v >= is_assigned.size()) {
      is_assigned.push_back(false);
      reason.push_back(0);
      variable_seen.push_back(false);
    }
    if (first_part) {
      first_part_variables_set.insert(v);
    }
  }
}

void Interpolator::parse_proof() {
  solver.flush_proof();
  std::ifstream input("proof.lrat", std::ios::binary);
  assert(input); // TODO: Replace with exception.

  unsigned char byte;
  while (input.read(reinterpret_cast<char*>(&byte), sizeof(char))) {
    if (byte == 'a') {
      clause_id++;
      auto [id, clause, premise_ids] = read_lrat_line(input);
      assert(id);
      std::cerr << "id: " << id << std::endl;
      id_to_clause[id] = clause;
      id_to_premises[id] = premise_ids;
      final_id = id;
    } else if (byte == 'd') {
      auto ids_to_delete = read_deletion_line(input);
      //to_delete.insert(to_delete.end(), ids_to_delete.begin(), ids_to_delete.end());
    }
  }
  input.close();
  solver.reset_proof();
}

std::vector<unsigned> Interpolator::get_core() const {
  std::vector<unsigned> core;
  std::vector<unsigned int> id_queue = {final_id};
  std::unordered_set<unsigned int> seen;
  while (!id_queue.empty()) {
    auto id = id_queue.back();
    id_queue.pop_back();
    if (seen.contains(id)) {
      continue;
    }
    seen.insert(id);
    if (id_to_premises.contains(id)) { // If id has no premises, it is an original clause.
      core.push_back(id);
      for (auto premise_id: id_to_premises.at(id)) {
        id_queue.push_back(premise_id);
      }
    }
  }
  return core;
}

abc::Aig_Obj_t* Interpolator::analyze_and_interpolate(unsigned int id) {
  std::vector<int> derived_clause;
  std::vector<int> variables_seen_vector;
  abc::Aig_Obj_t * interpolant_aig_node = abc::Aig_ManConst0(aig_man); //get_aig_node(id); // TODO
  int abs_pivot = 0;
  while (id) {
    assert(id_to_clause.contains(id));
    auto premise = id_to_clause.at(id);
    for (auto l: premise) {
      if (!variable_seen[abs(l)]) {
        variable_seen[abs(l)] = true;
        variables_seen_vector.push_back(abs(l));
        if (reason[abs(l)] == 0) {
          derived_clause.push_back(l);
        }
      }
    }
    id = 0;
    while(!trail.empty()) {
      int pivot = trail.back();
      trail.pop_back();
      abs_pivot = abs(pivot);
      is_assigned[abs_pivot] = false;

      if (variable_seen[abs_pivot]) {
        const auto& r = reason[abs_pivot];
        if (r) {
          /*auto reason_aig_node = get_aig_node(r);
          interpolant_aig_node = (!first_part_variables_set.contains(abs_pivot) || shared_variables_set.contains(abs_pivot)) ? abc::Aig_And(aig_man, interpolant_aig_node, reason_aig_node) : abc::Aig_Or(aig_man, interpolant_aig_node, reason_aig_node);*/
          id = r;
          break;
        }
      }
    }
  }
  for (auto v: variables_seen_vector) {
    variable_seen[v] = false;
  }
  return interpolant_aig_node;
}


void Interpolator::replay_proof(std::vector<unsigned int>& core) {
  std::sort(core.begin(), core.end());
  for (auto id: core) {
    auto conflict_id = propagate(id);
    assert(conflict_id);
    auto id_interpolant = analyze_and_interpolate(conflict_id);
    id_to_aig_node[id] = id_interpolant;
  }
}

unsigned int Interpolator::propagate(unsigned int id) {
  assert(id_to_clause.contains(id));
  auto clause = id_to_clause.at(id);

  assert(trail.empty());
  for (auto l: clause) {
    trail.push_back(-l);
    is_assigned[abs(l)] = true;
  }

  assert(id_to_premises.contains(id));
  auto premise_ids = id_to_premises.at(id);
  
  for (auto premise_id: premise_ids) {
    if (!id_to_clause.contains(premise_id)) {
      std::cerr << "Error: Clause id " << premise_id << " not found." << std::endl;
    }
    assert(id_to_clause.contains(premise_id));
    auto premise = id_to_clause.at(premise_id);

    int nr_unassigned = 0;
    int unassigned_literal = 0;
    for (auto l: premise) {
      if (!is_assigned[abs(l)]) {
        // We could also just break here and trust the proof.
        nr_unassigned++;
        unassigned_literal = l;
      }
    }
    assert(nr_unassigned <= 1);
    if (nr_unassigned == 1) {
      trail.push_back(-unassigned_literal);
      is_assigned[abs(unassigned_literal)] = true;
      reason[abs(unassigned_literal)] = premise_id;
    } else {
      return premise_id;
    }
  }
  return 0;
}


void Interpolator::delete_clauses() {
  for (auto id: to_delete) {
    id_to_clause.erase(id);
    id_to_premises.erase(id);
  }
  to_delete.clear();
}

abc::Aig_Obj_t* Interpolator::get_aig_node(unsigned int id) {
  if (id_to_aig_node.contains(id)) {
    return id_to_aig_node.at(id);
  }
  // If there is no AIG node for this id, it has to be an original clause.
  assert(!id_to_premises.contains(id));
  if (id_in_first_part.at(id)) {
    auto clause = id_to_clause.at(id);
    // Create an AIG node for the shared clause.
    auto aig_output = abc::Aig_ManConst0(aig_man);
    for (auto l: clause) {
      auto v = abs(l);
      if (shared_variable_to_ci.contains(v)) {
        aig_output = abc::Aig_Or(aig_man, aig_output, abc::Aig_NotCond(shared_variable_to_ci.at(v), l < 0));
      }
    }
    id_to_aig_node[id] = aig_output;
  } else {
    // If it is in the second part, return a constant 1 node.
    id_to_aig_node[id] = abc::Aig_ManConst1(aig_man);
  }
  return id_to_aig_node.at(id);
}

void Interpolator::set_shared_variables(const std::vector<int>& shared_variables) {
  shared_variables_set.clear();
  shared_variables_set.insert(shared_variables.begin(), shared_variables.end());
  shared_variable_to_ci.clear();
  id_to_aig_node.clear();
  // Create inputs for shared variables and mapping of variables to AIG input nodes.
  for (auto v: shared_variables) {
    shared_variable_to_ci[v] = abc::Aig_ObjCreateCi(aig_man);
  }
  abc::Aig_ManSetCioIds(aig_man);
}

std::pair<int, std::vector<std::vector<int>>> Interpolator::get_interpolant(const std::vector<int>& shared_variables, int auxiliary_variable_start) {
  //delete_clauses();
  parse_proof();
  auto core = get_core();
  //std::cerr << "Core size: " << core.size() << std::endl;
  aig_man = abc::Aig_ManStart(core.size());
  set_shared_variables(shared_variables);
  replay_proof(core);
  std::cout << "Number of nodes: " << Aig_ManNodeNum(aig_man) << std::endl;
  // Cleanup AIG.
  auto removed = abc::Aig_ManCleanup(aig_man);
  std::cerr << "Removed " << removed << " nodes." << std::endl;
  abc::Aig_ManStop(aig_man);
  return std::make_pair(0, std::vector<std::vector<int>>());
}

std::tuple<unsigned int, std::vector<int>, std::vector<unsigned int>> read_lrat_line(std::ifstream& input) {
  unsigned int id = read_number(input);
  unsigned int u;
  std::vector<int> clause;
  while ((u = read_number(input))) {
    clause.push_back(get_literal(u));
  }
  std::vector<unsigned int> premise_ids;
  while ((u = read_number(input))) {
    premise_ids.push_back(u / 2);
  }
  return std::make_tuple(id, clause, premise_ids);
}

std::vector<unsigned int> read_deletion_line(std::ifstream& input) {
  unsigned int u;
  std::vector<unsigned int> deleted_ids;
  while ((u = read_number(input))) {
    deleted_ids.push_back(u / 2);
  }
  return deleted_ids;
}

}