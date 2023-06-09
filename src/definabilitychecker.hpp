#ifndef DEFINABILITYCHECKER_H_
#define DEFINABILITYCHECKER_H_

#include "interpolator.hpp"

#include <vector>

class Definabilitychecker {
 public:
  void add_clause(const std::vector<int>& clause);
  void append_formula(const std::vector<std::vector<int>>& formula);
  std::vector<std::vector<int>> get_definition(int variable, const std::vector<int>& shared_variables);

 protected:
  void add_variable(int variable);
  int translate_literal(int literal, bool first_part);
  int original_literal(int translated_literal);
  std::vector<int> translate_clause(const std::vector<int>& clause, bool first_part);
  std::vector<int> original_clause(const std::vector<int>& translated_clause);

  cadical_itp::Interpolator interpolator;
  std::vector<int> equality_selector;
};

#endif /* DEFINABILITYCHECKER_H_ */