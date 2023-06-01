#include "interpolator.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>

namespace cadical_itp {

Interpolator::Interpolator(): clause_id(0) {}

void Interpolator::add_clause(const std::vector<int>& clause, bool first_part) {
  solver.add_clause(clause);
  //auto id = ++clause_id;
  auto id = solver.get_current_clause_id() + 1;
  id_in_first_part[id] = first_part;
  id_to_clause[id] = clause;
  for (auto l: clause) {
    auto v = abs(l);
    while(v <= is_assigned.size()) {
      is_assigned.push_back(false);
      reason.push_back(0);
      variable_seen.push_back(false);
    }
    if (first_part) {
      first_part_variables.insert(v);
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
      /*if (id != clause_id) {
        std::cout << "Error: Clause id " << id << " does not match expected id " << clause_id << std::endl;
        exit(1);
      }*/
      //assert(id == clause_id);
      id_to_clause[id] = clause;
      id_to_premises[id] = premise_ids;
      final_id = id;
    } else if (byte == 'd') {
      //auto ids_to_delete = read_deletion_line(input);
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
    core.push_back(id);
    if (id_to_premises.contains(id)) { // If id has no premises, it is an original clause.
      for (auto premise_id: id_to_premises.at(id)) {
        id_queue.push_back(premise_id);
      }
    }
  }
  return core;
}

void Interpolator::replay_proof(std::vector<unsigned int>& core) {
  while(!core.empty()) {
    auto id = core.back();
    core.pop_back();
    auto conflict_id = propagate(id);
    assert(conflict_id);
    analyze_and_interpolate(conflict_id);
  }
}

unsigned int Interpolator::propagate(unsigned int id) {
  auto clause = id_to_clause.at(id);
  trail.clear();
  for (auto l: clause) {
    trail.push_back(-l);
    is_assigned[abs(l)] = true;
  }
  auto premise_ids = id_to_premises.at(id);
  for (auto premise_id: premise_ids) {
    auto premise = id_to_clause.at(premise_id);
    int nr_unassigned = 0;
    int unassigned_literal = 0;
    for (auto l: premise) {
      if (!is_assigned[abs(l)]) {
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

void Interpolator::analyze_and_interpolate(unsigned int id) {
  std::vector<int> derived_clause;
  std::vector<int> variables_seen_vector;
  while (id) {
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
      if (variable_seen[abs(pivot)]) {
        id = reason[abs(pivot)];
        break;
      }
    }
  }
  for (auto v: variables_seen_vector) {
    variable_seen[v] = false;
  }
  auto clause = id_to_clause.at(id);
  // Sort both clause and derived clause and then check if derived clause is contained in clause.
  std::sort(clause.begin(), clause.end());
  std::sort(derived_clause.begin(), derived_clause.end());
  assert(std::includes(clause.begin(), clause.end(), derived_clause.begin(), derived_clause.end()));
}

void Interpolator::delete_clauses() {
  for (auto id: to_delete) {
    id_to_clause.erase(id);
    id_to_premises.erase(id);
  }
  to_delete.clear();
}

std::pair<int, std::vector<std::vector<int>>> Interpolator::get_interpolant(const std::vector<int>& shared_variables, int auxiliary_variable_start) {
  interpolant_clauses.clear();
  //delete_clauses();
  parse_proof();
  auto core = get_core();
  //std::cerr << "Core size: " << core.size() << std::endl;
  return std::make_pair(0, std::vector<std::vector<int>>());
}

std::tuple<unsigned int, std::vector<int>, std::vector<unsigned int>> read_lrat_line(std::ifstream& input) {
  unsigned int id = read_number(input);
  unsigned int u;
  std::vector<int> clause;
  while (u = read_number(input)) {
    clause.push_back(get_literal(u));
  }
  std::vector<unsigned int> premise_ids;
  while (u = read_number(input)) {
    premise_ids.push_back(u / 2);
  }
  return std::make_tuple(id, clause, premise_ids);
}

std::vector<unsigned int> read_deletion_line(std::ifstream& input) {
  unsigned int u;
  std::vector<unsigned int> deleted_ids;
  while (u = read_number(input)) {
    deleted_ids.push_back(u / 2);
  }
  return deleted_ids;
}

}