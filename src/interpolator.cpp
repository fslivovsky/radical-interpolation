#include "interpolator.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <string>

#include "opt/dar/dar.h"

using namespace abc; // Needed for macro expansion.

namespace cadical_itp {

Interpolator::Interpolator(): state(State::UNDEFINED), aig_man(nullptr) {
  abc::Dar_LibStart();
}

Interpolator::~Interpolator() {
  abc::Dar_LibStop();
}

void Interpolator::add_clause(const std::vector<int>& clause, bool first_part) {
  state = State::UNDEFINED;
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
    if (!solver.is_initial_clause(id) && !clause_id_to_proofnode.contains(id)) {
      core.push_back(id);
      for (auto premise_id: solver.get_premises(id)) {
        assert(premise_id < id);
        id_queue.push_back(premise_id);
      }
    }
  }
  return core;
}

std::pair<std::vector<int>, std::shared_ptr<Proofnode>> Interpolator::analyze_and_interpolate(uint64_t id) {
  std::vector<int> derived_clause;
  std::vector<int> variables_seen_vector;
  std::shared_ptr<Proofnode> interpolant_proofnode = get_proofnode(id);
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
          auto reason_proofnode = get_proofnode(r);
          interpolant_proofnode = std::make_shared<Proofnode>(abs_pivot, interpolant_proofnode, reason_proofnode);
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
  return std::make_pair(derived_clause, interpolant_proofnode);
}

void Interpolator::replay_proof(std::vector<uint64_t>& core) {
  std::sort(core.begin(), core.end());
  for (auto id: core) {
    auto conflict_id = propagate(id);
    assert(conflict_id > 0);
    auto [derived_clause, proofnode_interpolant] = analyze_and_interpolate(conflict_id);
    assert(contains(derived_clause, solver.get_clause(id)));
    clause_id_to_proofnode[id] = proofnode_interpolant;
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
  for (auto id: solver.get_delete_ids()) {
    clause_id_to_proofnode.erase(id);
  }
  solver.clear_delete_ids();
}

std::shared_ptr<Proofnode> Interpolator::get_proofnode(uint64_t id) {
  if (clause_id_to_proofnode.contains(id)) {
    return clause_id_to_proofnode.at(id);
  }
  // If there is no Proofnode for this id, it has to be an original clause.
  assert(solver.is_initial_clause(id));
  assert(id_in_first_part.contains(id));
  if (id_in_first_part.at(id)) {
    auto& clause = solver.get_clause(id);
    // Create a Proofnode representing the clause.
    std::shared_ptr<Proofnode> clause_output = nullptr;
    for (auto l: clause) {
      auto literal_node = std::make_shared<Proofnode>(l, nullptr, nullptr);
      clause_output = std::make_shared<Proofnode>(0, clause_output, literal_node);
    }
    clause_id_to_proofnode[id] = clause_output;
  } else {
    // If it is in the second part, return a constant 1 node.
    clause_id_to_proofnode[id] = std::make_shared<Proofnode>(0, nullptr, nullptr);
  }
  return clause_id_to_proofnode.at(id);
}

std::vector<std::vector<int>> Interpolator::get_interpolant_clauses(std::shared_ptr<Proofnode>& rootnode, const std::vector<int>& shared_variables, int auxiliary_variable_start, bool rewrite_aig) {
  // Create an AIG manager.
  aig_man = abc::Aig_ManStart(shared_variables.size());
  construct_aig(rootnode, shared_variables);
  Aig_ManCleanup(aig_man);
  if (abc::Aig_ManNodeNum(aig_man) > 0 && rewrite_aig) {
    std::cout << "Number of nodes before: " << Aig_ManNodeNum(aig_man) << std::endl;
    aig_man = Dar_ManRewriteDefault(aig_man);
    std::cout << "Number of nodes after: " << Aig_ManNodeNum(aig_man) << std::endl;
  }
  std::vector<std::vector<int>> interpolant_clauses;
  interpolant_clauses.reserve(proofnode_to_aig_node.size());
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
    pObj->iData = aig_input_variables[i];
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
    interpolant_clauses.push_back( { literal_input0, -variable_output } );
    interpolant_clauses.push_back( { literal_input1, -variable_output } );
    interpolant_clauses.push_back( { -literal_input0, -literal_input1, variable_output } );
  }
  // Write clauses for PO.
  Aig_ManForEachCo( aig_man, pObj, i ) {
    auto variable_output = pObj->iData;
    auto variable_input0 = abc::Aig_ObjFanin0(pObj)->iData;
    auto literal_input0 = Aig_ObjFaninC0(pObj) ? -variable_input0 : variable_input0;
    interpolant_clauses.push_back( { literal_input0, -variable_output } );
    interpolant_clauses.push_back( { -literal_input0, variable_output } );
  }
  abc::Vec_PtrFree( vNodes );
  abc::Aig_ManStop(aig_man);
  return interpolant_clauses;
}

void Interpolator::process_node(const std::shared_ptr<Proofnode>& proofnode) {
  // The node must not have been processed.
  assert(!proofnode_to_aig_node.contains(proofnode));
  if (proofnode->left == nullptr && proofnode->right == nullptr) {
    // Leaf node: constant or CI.
    if (proofnode->label) {
      auto variable = abs(proofnode->label);
      if (shared_variables_set.contains(variable)) {
        // If the variable is shared and no CI has been created, create a CI node.
        if (!variable_to_ci.contains(variable)) {
          aig_input_variables.push_back(variable);
          variable_to_ci[variable] = abc::Aig_ObjCreateCi(aig_man);
        }
        auto variable_node = variable_to_ci.at(variable);
        // Negate variable if necessary.
        auto aig_node = abc::Aig_NotCond(variable_node, proofnode->label < 0);
        proofnode_to_aig_node[proofnode] = aig_node;
      } else {
        proofnode_to_aig_node[proofnode] = abc::Aig_ManConst0(aig_man);
      }
    } else { // Label 0 means constant 1.
      proofnode_to_aig_node[proofnode] = abc::Aig_ManConst1(aig_man);
    }
  } else if (proofnode->left == nullptr) {
    // Copy the object obtained from the right node.
    assert(proofnode_to_aig_node.contains(proofnode->right));
    proofnode_to_aig_node[proofnode] = proofnode_to_aig_node.at(proofnode->right);
  } else if (proofnode->right == nullptr) {
    // This ought not to occur.
    assert(false);
  } else {
    // Both left and right nodes are present.
    auto left_node = proofnode_to_aig_node.at(proofnode->left);
    auto right_node = proofnode_to_aig_node.at(proofnode->right);
    if (proofnode->label && (shared_variables_set.contains(proofnode->label) || (!first_part_variables_set.contains(proofnode->label)))) {
      // If the variables NOT local to the first part, create an AND node.
      proofnode_to_aig_node[proofnode] = abc::Aig_And(aig_man, left_node, right_node);
    } else {
      // Otherwise, create an OR node.
      proofnode_to_aig_node[proofnode] = abc::Aig_Or(aig_man, left_node, right_node);
    }
  }
  // std::cout << "Label: " << proofnode->label << std::endl;
  // abc::Aig_ObjPrint(aig_man, proofnode_to_aig_node.at(proofnode));
  // std::cout << std::endl;
}

void Interpolator::construct_aig(std::shared_ptr<Proofnode>& rootnode, const std::vector<int>& shared_variables) {
  // Reset AIG-related data structures.
  proofnode_to_aig_node.clear();
  variable_to_ci.clear();
  aig_input_variables.clear();
  shared_variables_set.clear();
  shared_variables_set.insert(shared_variables.begin(), shared_variables.end());

  assert(rootnode != nullptr);

  std::vector<std::shared_ptr<Proofnode>> stack;
  std::vector<std::shared_ptr<Proofnode>> processed_nodes;
  stack.push_back(rootnode);

  while (!stack.empty()) {
    auto node = stack.back();
    stack.pop_back();

    if (node->flag) {
      // If the node has already been processed, skip it.
      continue;
    }

    if ((node->left && !node->left->flag) || (node->right && !node->right->flag)) {
      // If any of the child nodes are not processed, push this node back into the stack.
      stack.push_back(node);
      // Push unprocessed child nodes into the stack.
      if (node->right && !node->right->flag) {
        stack.push_back(node->right);
      }
      if (node->left && !node->left->flag) {
        stack.push_back(node->left);
      }
    } else {
      // If both child nodes are processed (or don't exist), we can process this node.
      process_node(node);
      processed_nodes.push_back(node);
      node->flag = true;
    }
  }
  // Reset flags.
  for (auto node : processed_nodes) {
    node->flag = false;
  }
  // Create PO.
  abc::Aig_ObjCreateCo(aig_man, proofnode_to_aig_node.at(rootnode) );
}

std::pair<int, std::vector<std::vector<int>>> Interpolator::get_interpolant(const std::vector<int>& shared_variables, int auxiliary_variable_start, bool rewrite_aig) {
  if (state != State::UNSAT) {
    throw InterpolatorStateException("can only call get_interpolant in UNSAT state");
  }
  delete_clauses();
  state = State::UNDEFINED;
  solver.get_failed(last_assumptions); // Needed to generate final part of LRAT proof.
  auto core = get_core();
  if (core.empty()) {
    // If the core is empty, the formula is unsatisfiable. In this case, we return a trivial interpolant.
    return std::make_pair(auxiliary_variable_start, std::vector<std::vector<int>>{{-auxiliary_variable_start}});
  }
  replay_proof(core);
  auto interpolant_clauses = get_interpolant_clauses(clause_id_to_proofnode.at(core.back()), shared_variables, auxiliary_variable_start, rewrite_aig);
  return std::make_pair(auxiliary_variable_start, interpolant_clauses);
}

}