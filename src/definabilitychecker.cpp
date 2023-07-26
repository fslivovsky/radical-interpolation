#include "definabilitychecker.hpp"

#include <cassert>

Definabilitychecker::Definabilitychecker() : state(State::UNDEFINED) {}

void Definabilitychecker::add_variable(int variable) {
  assert(variable > 0);
  while (variable >= equality_selector.size()) {
    equality_selector.push_back(0);
  }
  auto equal_selector = 5 * variable + 2;
  auto true_selector = 5 * variable + 3;
  auto false_selector = 5 * variable + 4;
  equality_selector[variable] = equal_selector;
  auto first_part_variable = translate_literal(variable, true);
  auto second_part_variable = translate_literal(variable, false);
  interpolator.append_formula({{-equal_selector, first_part_variable, -second_part_variable}, {-equal_selector, -first_part_variable, second_part_variable}}, false);
  // Workaround to avoid failed assumptions at decision level 0: we use -selector and 1 as an assumption. 1 is a variable that is unused.
  interpolator.add_clause({-true_selector, -1, first_part_variable}, true);
  interpolator.add_clause({-false_selector, -1, -second_part_variable}, false);
}

int Definabilitychecker::translate_literal(int literal, bool first_part) {
  auto v = abs(literal);
  auto v_translated = 5 * v + first_part;
  return literal < 0 ? -v_translated : v_translated;
}

int Definabilitychecker::original_literal(int translated_literal) {
  auto v = abs(translated_literal);
  auto v_original = v / 5;
  if (v_original >= equality_selector.size()) {
    // This is an auxiliary variable introduced during interpolation.
    return translated_literal;
  }
  return translated_literal < 0 ? -v_original : v_original;
}

std::vector<int> Definabilitychecker::translate_clause(const std::vector<int>& clause, bool first_part) {
  std::vector<int> translated_clause;
  for (auto l: clause) {
    translated_clause.push_back(translate_literal(l, first_part));
  }
  return translated_clause;
}

void Definabilitychecker::original_clause(std::vector<int>& translated_clause) {
  for (auto& l: translated_clause) {
    l = original_literal(l);
  }
}

void Definabilitychecker::add_clause(const std::vector<int>& clause) {
  state = State::UNDEFINED;
  for (auto l: clause) {
    auto v = abs(l);
    if (v >= equality_selector.size() or equality_selector[v] == 0) {
      add_variable(v);
    }
  }
  interpolator.add_clause(translate_clause(clause, true), true);
  interpolator.add_clause(translate_clause(clause, false), false);
}

void Definabilitychecker::append_formula(const std::vector<std::vector<int>>& formula) {
  for (const auto& clause: formula) {
    add_clause(clause);
  }
}

bool Definabilitychecker::has_definition(int variable, const std::vector<int>& shared_variables, const std::vector<int>& assumptions) {
  assert(variable > 0);
  state = State::UNDEFINED;
  std::vector<int> assumptions_internal;
  for (auto v: shared_variables) {
    if (v >= equality_selector.size() or equality_selector[v] == 0) {
      add_variable(v);
    }
    assumptions_internal.push_back(equality_selector[v]);
  }
  auto true_selector = 5 * variable + 3;
  auto false_selector = 5 * variable + 4;
  // Translate external assumptions.
  auto assumptions_first_part = translate_clause(assumptions, true);
  assumptions_internal.insert(assumptions_internal.end(), assumptions_first_part.begin(), assumptions_first_part.end());
  std::vector<int> assumptions_second_part = translate_clause(assumptions, false);
  assumptions_internal.insert(assumptions_internal.end(), assumptions_second_part.begin(), assumptions_second_part.end());
  assumptions_internal.push_back(true_selector);
  assumptions_internal.push_back(false_selector);
  assumptions_internal.push_back(1);
  bool has_definition = !interpolator.solve(assumptions_internal);
  if (has_definition) {
    state = State::DEFINED;
    last_shared_variables = shared_variables;
    last_variable = variable;
  }
  return has_definition;
}

std::pair<std::vector<std::vector<int>>, int> Definabilitychecker::get_definition(bool rewrite) {
  if (state != State::DEFINED) {
    throw UndefinedException();
  }
  auto [output_variable, definition] = interpolator.get_interpolant(translate_clause(last_shared_variables, true), 5 * equality_selector.size(), false);
  for (auto& clause: definition) {
    original_clause(clause);
  }
  definition.push_back({ output_variable, -last_variable});
  definition.push_back({-output_variable,  last_variable});
  return std::make_pair(definition, 5 * equality_selector.size());
}

