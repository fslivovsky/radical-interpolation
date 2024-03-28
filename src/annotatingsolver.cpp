#include "annotatingsolver.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>

#include "cadical_solver.hpp"

namespace cadical_annotations {

Annotatingsolver::Annotatingsolver(): state(State::UNDEFINED), solver(std::make_unique<Cadical>()), empty_added(false) {
}

Annotatingsolver::~Annotatingsolver() {
}

void Annotatingsolver::add_universal(int variable) {
  auto index = variable_to_annotation_index.size();
  variable_to_annotation_index[variable] = index;
  annotation_index_to_variable[index] = variable;
}

void Annotatingsolver::create_arbiter_var(int variable, const std::vector<int>& annotation, const std::vector<int>& dependencies) {
  auto bitset_annotation = vector_to_annotation(annotation);
  auto [bitset_dependencies, _] = vector_to_annotation(dependencies);
  VariableAnnotation variable_annotation(bitset_dependencies);
  //variable_annotation.instantiate(bitset_annotation);
  variable_to_annotation.insert({ variable, variable_annotation });
}

void Annotatingsolver::add_clause(const std::vector<int>& clause, const std::vector<int>& annotation) {
  if (clause.empty()) {
    empty_added = true;
  }
  state = State::UNDEFINED;
  auto id = solver->get_current_clause_id() + 1;
  solver->add_clause(clause);
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
  }
  auto clause_annotation = get_initial_annotation(clause);
  //print_annotation_map(clause_annotation);
  instantiateAll(clause_annotation, vector_to_annotation(annotation));
  clause_id_to_annotation.insert({ id, clause_annotation });
  assert(clause_id_to_annotation.contains(id));
}

void Annotatingsolver::append_formula(const std::vector<std::vector<int>>& formula) {
  for (const auto& clause: formula) {
    add_clause(clause, {});
  }
}

bool Annotatingsolver::solve(const std::vector<int>& assumptions) {
  last_assumptions = assumptions;
  auto result = solver->solve(assumptions);
  if (result == 10) {
    state = State::SAT;
  } else if (result == 20) {
    state = State::UNSAT;
  } else {
    throw SolverStateException("unexpected result from solver");
  }
  return result != 20;
}

std::vector<int> Annotatingsolver::get_model() {
  if (state != State::SAT) {
    throw SolverStateException("can only call get_model in SAT state");
  }
  return solver->get_model();
}

std::vector<int> Annotatingsolver::get_values(const std::vector<int>& variables) {
  if (state != State::SAT) {
    throw SolverStateException("can only call get_model in SAT state");
  }
  return solver->get_values(variables);
}

std::pair<std::vector<int>, std::vector<ConflictPair>> Annotatingsolver::get_annotation_conflicts() {
  if (state != State::UNSAT) {
    throw SolverStateException("can only call get_annotation_conflicts in UNSAT state");
  }
  state = State::UNDEFINED;
  solver->get_failed(last_assumptions); // Needed to generate final part of LRAT proof.
  auto core = get_core();
  if (core.empty()) {
    // This should only happen if the empty clause is added.
    return {{}, {}};
  }
  replay_proof(core);
  auto conflicts = clause_id_to_annotation_conflicts.at(core.back());
  delete_clauses();
  return std::make_pair(annotation_core, conflicts);
}

std::vector<uint64_t> Annotatingsolver::get_core() const {
  if (empty_added) {
    return {};
  }
  std::vector<uint64_t> core;
  std::vector<uint64_t> id_queue = {solver->get_latest_id()};
  std::unordered_set<uint64_t> seen;
  while (!id_queue.empty()) {
    auto id = id_queue.back();
    id_queue.pop_back();
    if (seen.contains(id)) {
      continue;
    }
    seen.insert(id);
    if (!solver->is_initial_clause(id) && !clause_id_to_annotation.contains(id)) {
      core.push_back(id);
      for (auto premise_id: solver->get_premises(id)) {
        assert(premise_id < id);
        id_queue.push_back(premise_id);
      }
    }
  }
  return core;
}

std::tuple<std::vector<int>, ClauseAnnotation, std::vector<ConflictPair>> Annotatingsolver::analyze_and_annotate(uint64_t id) {
  //std::cout << "Analyzing and annotating clause " << id << std::endl;
  std::vector<int> derived_clause;
  std::vector<int> variables_seen_vector;
  auto running_annotation_map = clause_id_to_annotation.at(id);
  auto running_annotation_conflicts = clause_id_to_annotation_conflicts[id];
  int abs_pivot = 0;
  while (id) {
    auto& premise = solver->get_clause(id);
    for (auto l: premise) {
      if (!variable_seen[abs(l)]) {
        variable_seen[abs(l)] = true;
        variables_seen_vector.push_back(abs(l));
        if (reason[abs(l)] == 0) {
          derived_clause.push_back(l);
        }
      }
    }
    //std::cout << "Running map: ";
    //print_annotation_map(running_annotation_map);
    if (abs_pivot && running_annotation_conflicts.empty()) {
      //std::cout << "Reason map: ";
      auto& reason_annotation_conflicts = clause_id_to_annotation_conflicts[id];
      if (!reason_annotation_conflicts.empty()) {
        running_annotation_conflicts = reason_annotation_conflicts;
      } else {
        //print_annotation_map(clause_id_to_annotation.at(id));
        running_annotation_conflicts = unify_annotations(running_annotation_map, clause_id_to_annotation.at(id));
        running_annotation_map.erase(abs_pivot);
        // If there is a conflict w.r.t. annotations, identify assumptions in the derived clause.
        if (!running_annotation_conflicts.empty()) {
          annotation_core.clear();
          std::unordered_set<int> assumptions_set(last_assumptions.begin(), last_assumptions.end());
          for (auto l: derived_clause) {
            if (assumptions_set.contains(-l)) {
              annotation_core.push_back(-l);
            }
          }
        }
      }
    }
    // Search for next pivot.
    id = 0;
    while(!trail.empty()) {
      int pivot = trail.back();
      trail.pop_back();
      abs_pivot = abs(pivot);
      is_assigned[abs_pivot] = false;

      if (variable_seen[abs_pivot]) {
        const auto& r = reason[abs_pivot];
        if (r) {
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
  // if (running_annotation_conflicts.empty()) {
  //   std::cout << "No conflicts found" << std::endl;
  // }
  return std::make_tuple(derived_clause, running_annotation_map, running_annotation_conflicts);
}

void Annotatingsolver::replay_proof(std::vector<uint64_t>& core) {
  std::sort(core.begin(), core.end());
  for (auto id: core) {
    auto conflict_id = propagate(id);
    assert(conflict_id > 0);
    auto [derived_clause, annotation, annotation_conflicts] = analyze_and_annotate(conflict_id);
    assert(contains(derived_clause, solver->get_clause(id)));
    clause_id_to_annotation[id] = annotation;
    clause_id_to_annotation_conflicts[id] = annotation_conflicts;
  }
}

uint64_t Annotatingsolver::propagate(uint64_t id) {
  auto& clause = solver->get_clause(id);

  assert(trail.empty());
  for (auto l: clause) {
    trail.push_back(-l);
    is_assigned[abs(l)] = true;
    reason[abs(l)] = 0;
  }

  assert(!solver->is_initial_clause(id));
  auto& premise_ids = solver->get_premises(id);
  
  for (auto premise_id: premise_ids) {
    auto& premise = solver->get_clause(premise_id);

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

void Annotatingsolver::delete_clauses() {
  for (auto id: solver->get_delete_ids()) {
    clause_id_to_annotation.erase(id);
    clause_id_to_annotation_conflicts.erase(id);
  }
  solver->clear_delete_ids();
}

Annotation Annotatingsolver::vector_to_annotation(const std::vector<int>& literal_vector) {
  Annotation annotation {boost::dynamic_bitset<>(variable_to_annotation_index.size()), boost::dynamic_bitset<>(variable_to_annotation_index.size())};
  for (auto l: literal_vector) {
    auto variable = abs(l);
    auto index = variable_to_annotation_index[variable];
    if (l > 0) {
      annotation.first[index] = true;
    } else {
      annotation.second[index] = true;
    }
  }
  return annotation;
}

std::vector<int> Annotatingsolver::annotation_to_vector(const Annotation& annotation) {
  std::vector<int> literal_vector;
  for (int i = 0; i < annotation.first.size(); i++) {
    if (annotation.first[i]) {
      literal_vector.push_back(annotation_index_to_variable[i]);
    }
    if (annotation.second[i]) {
      literal_vector.push_back(-annotation_index_to_variable[i]);
    }
  }
  return literal_vector;
}

ClauseAnnotation Annotatingsolver::get_initial_annotation(const std::vector<int>& clause) {
  ClauseAnnotation clause_annotation;
  for (auto l: clause) {
    auto variable = abs(l);
    if (variable_to_annotation.contains(variable)) {
      auto& variable_annotation = variable_to_annotation.at(variable);
      clause_annotation.insert({ variable, variable_annotation });
    }
  }
  return clause_annotation;
}

boost::dynamic_bitset<> Annotatingsolver::get_missing_annotations(const ClauseAnnotation& clause_annotation) {
  boost::dynamic_bitset<> missing_annotations(variable_to_annotation_index.size());
  for (auto [_, annotation]: clause_annotation) {
    missing_annotations |= annotation.missing();
  }
  return missing_annotations;
}

std::pair<Annotation, Annotation> Annotatingsolver::compute_partial_unifiers(const ClauseAnnotation& clause_annotation1, const ClauseAnnotation& clause_annotation2) {
  Annotation unifier_first = { boost::dynamic_bitset<>(variable_to_annotation_index.size()), boost::dynamic_bitset<>(variable_to_annotation_index.size()) };
  Annotation unifier_second = { boost::dynamic_bitset<>(variable_to_annotation_index.size()), boost::dynamic_bitset<>(variable_to_annotation_index.size()) };
  for (const auto& [v, v_annotation_1]: clause_annotation1) {
      if (clause_annotation2.find(v) == clause_annotation2.end())
          continue;
      auto& v_annotation_2 = clause_annotation2.at(v);
      unifier_first  |=  v_annotation_2 - v_annotation_1;
      unifier_second |=  v_annotation_1 - v_annotation_2;
  }
  return {unifier_first, unifier_second};
}

std::pair<Annotation, Annotation> Annotatingsolver::compute_unifiers(const ClauseAnnotation& clause_annotation1, const ClauseAnnotation& clause_annotation2) {
  auto [unifier_first, unifier_second] = compute_partial_unifiers(clause_annotation1, clause_annotation2);
  auto missing_first = get_missing_annotations(clause_annotation1);
  auto missing_second = get_missing_annotations(clause_annotation2);
  unifier_first |= unifier_second & missing_first;
  unifier_second |= unifier_first & missing_second;
  return { unifier_first, unifier_second };
}

std::vector<ConflictPair> Annotatingsolver::get_conflicting_annotations(const ClauseAnnotation& clause_annotation1, const ClauseAnnotation& clause_annotation2, const Annotation& unifier_second) {
  std::vector<ConflictPair> conflicting;
  for (const auto& [v, annotation1_v]: clause_annotation1) {
    if (clause_annotation2.find(v) == clause_annotation2.end())
      continue;
    auto& annotation2_v = clause_annotation2.at(v);
    auto bitset_conflicting = ((annotation1_v.annotation_.first & annotation2_v.annotation_.second) |  (annotation1_v.annotation_.second & annotation2_v.annotation_.first) | (unifier_second.first & unifier_second.second)) & annotation1_v.dependencies;
    if (bitset_conflicting.any()) {
      Annotation annotation_conflicting = {bitset_conflicting, boost::dynamic_bitset<>(bitset_conflicting.size()) };
      conflicting.emplace_back(std::make_pair(v, annotation_to_vector(annotation_conflicting)));
    }
  }
  return conflicting;
}

std::vector<ConflictPair> Annotatingsolver::unify_annotations(ClauseAnnotation& running_clause_annotation, const ClauseAnnotation& reason_clause_annotation) {
  auto [unifier_running, unifier_reason] = compute_unifiers(running_clause_annotation, reason_clause_annotation);
  instantiateAll(running_clause_annotation, unifier_running);
  for (auto& [v, annotation_v]: reason_clause_annotation) {
    // Find literals missing from the running clause, insert, and instantiate with the unifier.
    if (running_clause_annotation.find(v) == running_clause_annotation.end()) {
      running_clause_annotation.insert({v, annotation_v});
      auto& annotation_v_running = running_clause_annotation.at(v);
      annotation_v_running.instantiate(unifier_reason);
    }
  }
  return get_conflicting_annotations(running_clause_annotation, reason_clause_annotation, unifier_reason);
}

// Functions for printing clauses and annotations
std::vector<int> Annotatingsolver::variable_annotation_to_vector(const VariableAnnotation& annotation) {
  Annotation annotation_ = annotation.annotation_;
  // Apply dependency mask
  annotation_.first &= annotation.dependencies;
  annotation_.second &= annotation.dependencies;
  return annotation_to_vector(annotation_);
}

void Annotatingsolver::print_annotation_map(const ClauseAnnotation& clause_annotation) {
  for (auto [variable, annotation]: clause_annotation) {
    std::cout << variable;
    std::cout << "^[";
    auto annotation_vector = variable_annotation_to_vector(clause_annotation.at(variable));
    for (auto l: annotation_vector) {
      std::cout << l << " ";
    }
    std::cout << "], ";
  }
  std::cout << std::endl;
  }
}