#include "interpolator.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <string>

#include "opt/dar/dar.h"

using namespace abc; // Needed for macro expansion.

namespace cadical_itp {

Interpolator::Interpolator() {
  abc::Dar_LibStart();
}

Interpolator::~Interpolator() {
  abc::Dar_LibStop();
}

void Interpolator::add_clause(const std::vector<int>& clause, bool first_part) {
  auto id = solver.get_current_clause_id() + 1;
  solver.add_clause(clause);
  id_in_first_part[id] = first_part;
  for (auto l: clause) {
    auto v = abs(l);
    if (v >= is_assigned.size()) {
      is_assigned.reserve(v + 1);
      reason.reserve(v + 1);
      variable_seen.reserve(v + 1);
    }
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

std::vector<uint64_t> Interpolator::get_core() const {
  std::vector<uint64_t> core;
  std::vector<uint64_t> id_queue = {solver.get_latest_id()};
  std::unordered_set<uint64_t> seen;
  while (!id_queue.empty()) {
    auto id = id_queue.back();
    id_queue.pop_back();
    if (seen.contains(id)) {
      continue;
    }
    seen.insert(id);
    if (!solver.is_initial_clause(id)) {
      core.push_back(id);
      for (auto premise_id: solver.get_premises(id)) {
        assert(premise_id < id);
        id_queue.push_back(premise_id);
      }
    }
  }
  return core;
}

std::pair<std::vector<int>, abc::Aig_Obj_t*>  Interpolator::analyze_and_interpolate(uint64_t id) {
  std::vector<int> derived_clause;
  std::vector<int> variables_seen_vector;
  abc::Aig_Obj_t * interpolant_aig_node = get_aig_node(id);
  std::string interpolant_string = std::to_string(id);
  int abs_pivot = 0;
  while (id) {
    auto& premise = solver.get_clause(id);
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
          auto reason_aig_node = get_aig_node(r);
          if (first_part_variables_set.contains(abs_pivot) && !shared_variables_set.contains(abs_pivot)) {
            // This is a variable that is only in the first part.
            interpolant_aig_node = abc::Aig_Or(aig_man, interpolant_aig_node, reason_aig_node);
          } else {
            interpolant_aig_node = abc::Aig_And(aig_man, interpolant_aig_node, reason_aig_node);
          }
          id = r;
          break;
        }
      }
    }
  }
  assert(trail.empty());
  for (auto v: variables_seen_vector) {
    variable_seen[v] = false;
  }
  return std::make_pair(derived_clause, interpolant_aig_node);
}

void Interpolator::replay_proof(std::vector<uint64_t>& core) {
  std::sort(core.begin(), core.end());
  for (auto id: core) {
    auto conflict_id = propagate(id);
    assert(conflict_id > 0);
    auto [derived_clause, id_interpolant] = analyze_and_interpolate(conflict_id);
    //std::cerr << ":" << id << std::endl;
    assert(contains(derived_clause, solver.get_clause(id)));
    id_to_aig_node[id] = id_interpolant;
  }
}

uint64_t Interpolator::propagate(uint64_t id) {
  auto& clause = solver.get_clause(id);

  assert(trail.empty());
  for (auto l: clause) {
    trail.push_back(-l);
    is_assigned[abs(l)] = true;
    reason[abs(l)] = 0;
  }

  assert(!solver.is_initial_clause(id));
  auto& premise_ids = solver.get_premises(id);
  
  for (auto premise_id: premise_ids) {
    auto& premise = solver.get_clause(premise_id);

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
      trail.push_back(unassigned_literal);
      is_assigned[abs(unassigned_literal)] = true;
      reason[abs(unassigned_literal)] = premise_id;
    } else {
      return premise_id;
    }
  }
  return 0;
}


void Interpolator::delete_clauses() {
  to_delete.clear();
}

abc::Aig_Obj_t* Interpolator::get_aig_node(uint64_t id) {
  if (id_to_aig_node.contains(id)) {
    return id_to_aig_node.at(id);
  }
  // If there is no AIG node for this id, it has to be an original clause.
  assert(solver.is_initial_clause(id));
  assert(id_in_first_part.contains(id));
  if (id_in_first_part.at(id)) {
    auto& clause = solver.get_clause(id);
    // Create an AIG node for the shared clause.
    auto aig_output = abc::Aig_ManConst0(aig_man);
    
    for (auto l: clause) {
      auto v = abs(l);
      if (shared_variable_to_ci.contains(v)) {
        //std::cerr << l << " ";
        aig_output = abc::Aig_Or(aig_man, aig_output, abc::Aig_NotCond(shared_variable_to_ci.at(v), l < 0));
      }
    }
    id_to_aig_node[id] = aig_output;
  } else {
    // If it is in the second part, return a constant 1 node.
    id_to_aig_node[id] = abc::Aig_ManConst1(aig_man);
    //std::cerr << "constant 1";
  }
  //std::cerr << std::endl;
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
}

std::vector<std::vector<int>> Interpolator::get_interpolant_clauses(const std::vector<int>& shared_variables, int auxiliary_variable_start) {
  std::vector<std::vector<int>> interpolant_clauses;
  interpolant_clauses.reserve(id_to_aig_node.size());
  abc::Vec_Ptr_t * vNodes;
  abc::Aig_Obj_t * pObj, * pConst1 = NULL;
  int i;
  assert(abc::Aig_ManCoNum(aig_man) == 1);
  // check if constant is used
  Aig_ManForEachCo( aig_man, pObj, i) {
    if (abc::Aig_ObjIsConst1(abc::Aig_ObjFanin0(pObj)))
      pConst1 = abc::Aig_ManConst1(aig_man);
  }
  // Assign shared variables to CIs.
  Aig_ManForEachCi( aig_man, pObj, i) {
    pObj->iData = shared_variables[i];
  }
  // collect nodes in the DFS order
  vNodes = abc::Aig_ManDfs(aig_man, 1);
  // assign IDs to objects
  Aig_ManForEachCo( aig_man, pObj, i ) {
    pObj->iData = auxiliary_variable_start++;
  }
  abc::Aig_ManConst1(aig_man)->iData = auxiliary_variable_start++;
  Vec_PtrForEachEntry( abc::Aig_Obj_t *, vNodes, pObj, i ) {
    pObj->iData = auxiliary_variable_start++;
  }
  // Add clauses from Tseitin conversion.
  if (pConst1) { // Constant 1 if necessary.
    interpolant_clauses.push_back( { pConst1->iData } );
  }
  Vec_PtrForEachEntry( abc::Aig_Obj_t *, vNodes, pObj, i ) {
    auto variable_output = pObj->iData;
    auto variable_input0 = abc::Aig_ObjFanin0(pObj)->iData;
    auto variable_input1 = abc::Aig_ObjFanin1(pObj)->iData;
    auto literal_input0 = Aig_ObjFaninC0(pObj) ? -variable_input0 : variable_input0;
    auto literal_input1 = Aig_ObjFaninC1(pObj) ? -variable_input1 : variable_input1;
    interpolant_clauses.push_back( { -variable_output, literal_input0 } );
    interpolant_clauses.push_back( { -variable_output, literal_input1 } );
    interpolant_clauses.push_back( { variable_output, -literal_input0, -literal_input1 } );
  }
  // Write clauses for PO.
  Aig_ManForEachCo( aig_man, pObj, i ) {
    auto variable_output = pObj->iData;
    auto variable_input0 = abc::Aig_ObjFanin0(pObj)->iData;
    auto literal_input0 = Aig_ObjFaninC0(pObj) ? -variable_input0 : variable_input0;
    interpolant_clauses.push_back( { -variable_output, literal_input0 } );
    interpolant_clauses.push_back( { variable_output, -literal_input0 } );
  }
  abc::Vec_PtrFree( vNodes );
  return interpolant_clauses;
}

std::pair<int, std::vector<std::vector<int>>> Interpolator::get_interpolant(const std::vector<int>& shared_variables, int auxiliary_variable_start, bool rewrite_aig) {
  //delete_clauses();
  solver.get_failed(last_assumptions); // Needed to generate final part of LRAT proof.
  auto core = get_core();
  //std::cerr << "Core size: " << core.size() << std::endl;
  aig_man = abc::Aig_ManStart(core.size());
  set_shared_variables(shared_variables);
  replay_proof(core);
  abc::Aig_ObjCreateCo(aig_man, id_to_aig_node.at(core.back()));
  //std::cout << "Last in core:" << core.back() << " last clause solver " << solver.get_current_clause_id() << std::endl;
  //std::cout << "Number of nodes before: " << Aig_ManNodeNum(aig_man) << std::endl;
  if (rewrite_aig)
    aig_man = Dar_ManRewriteDefault(aig_man);
  //std::cout << "Number of nodes after: " << Aig_ManNodeNum(aig_man) << std::endl;
  auto interpolant_clauses = get_interpolant_clauses(shared_variables, auxiliary_variable_start);
  abc::Aig_ManStop(aig_man);
  return std::make_pair(auxiliary_variable_start, interpolant_clauses);
}

}