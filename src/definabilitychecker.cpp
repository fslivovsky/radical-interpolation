#include "definabilitychecker.hpp"

#include <cassert>

void Definabilitychecker::add_variable(int variable) {
  assert(variable > 0);
  while (variable >= equality_selector.size()) {
    equality_selector.push_back(0);
  }
  auto selector = 3 * variable + 2;
  equality_selector[variable] = selector;
  auto first_part_variable = translate_literal(variable, true);
  auto second_part_variable = translate_literal(variable, false);
  interpolator.append_formula({{-selector, first_part_variable, -second_part_variable}, {-selector, -first_part_variable, second_part_variable}}, false);
}

int Definabilitychecker::translate_literal(int literal, bool first_part) {
  auto v = abs(literal);
  auto v_translated = 3 * v + first_part;
  return literal < 0 ? -v_translated : v_translated;
}

int Definabilitychecker::original_literal(int translated_literal) {
  auto v = abs(translated_literal);
  auto v_original = v / 3;
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

std::vector<int> Definabilitychecker::original_clause(const std::vector<int>& translated_clause) {
  std::vector<int> clause;
  for (auto l: translated_clause) {
    clause.push_back(original_literal(l));
  }
  return clause;
}

void Definabilitychecker::add_clause(const std::vector<int>& clause) {
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
  for (auto clause: formula) {
    add_clause(clause);
  }
}

std::vector<std::vector<int>> Definabilitychecker::get_definition(int variable, const std::vector<int>& shared_variables) {
  assert(variable > 0);
  std::vector<int> assumptions;
  for (auto v: shared_variables) {
    if (v >= equality_selector.size() or equality_selector[v] == 0) {
      add_variable(v);
    }
    assumptions.push_back(equality_selector[v]);
  }
  assumptions.push_back(translate_literal(variable, true));
  assumptions.push_back(translate_literal(-variable, false));
  bool has_definition = !interpolator.solve(assumptions);
  if (!has_definition) {
    return {};
  }
  auto [output_variable, definition] = interpolator.get_interpolant(translate_clause(shared_variables, true), 3 * equality_selector.size());
  std::vector<std::vector<int>> definition_original;
  for (auto clause: definition) {
    definition_original.push_back(original_clause(clause));
  }
  definition_original.push_back({output_variable, -variable});
  definition_original.push_back({-output_variable, variable});
  return definition_original;
}

