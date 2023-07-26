#ifndef DEFINABILITYCHECKER_H_
#define DEFINABILITYCHECKER_H_

#include "interpolator.hpp"

#include <vector>
#include <utility>

// Define exception thrown when get_definition is called in undefined state.
class UndefinedException : public std::exception {
 public:
  const char* what() const noexcept override {
    return "can only call get_definition in DEFINED state";
  }
};

class Definabilitychecker {
 public:
  Definabilitychecker();
  void add_clause(const std::vector<int>& clause);
  void append_formula(const std::vector<std::vector<int>>& formula);
  bool has_definition(int variable, const std::vector<int>& shared_variables, const std::vector<int>& assumptions);
  std::pair<std::vector<std::vector<int>>, int> get_definition(bool rewrite);

 protected:
  enum class State {
    UNDEFINED,
    DEFINED
  };

  State state;
  void add_variable(int variable);
  int translate_literal(int literal, bool first_part);
  int original_literal(int translated_literal);
  std::vector<int> translate_clause(const std::vector<int>& clause, bool first_part);
  void original_clause(std::vector<int>& translated_clause);

  cadical_itp::Interpolator interpolator;
  std::vector<int> equality_selector;
  std::vector<int> last_shared_variables;
  int last_variable;
};

#endif /* DEFINABILITYCHECKER_H_ */