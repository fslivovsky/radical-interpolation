#ifndef ITP_INTERPOLATOR_H_
#define ITP_INTERPOLATOR_H_

#include <vector>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <fstream>

#include "cadical_solver.hpp"

namespace cadical_itp {

class Interpolator {
 public:
  void add_clause(const std::vector<int>& clause, bool first_part);
  void append_formula(const std::vector<std::vector<int>>& formula, bool first_part);
  bool solve(const std::vector<int>& assumptions);
  std::vector<int> get_model();
  std::vector<std::vector<int>> get_interpolant(const std::vector<int>& shared_variables, int auxiliary_variable_start);

 protected:
  std::vector<int> get_clause(unsigned int id) const;
  void parse_proof();
  std::vector<unsigned> get_core() const;
  void replay_proof(std::vector<unsigned int>& core);
  unsigned int propagate(unsigned int id);
  void analyze_and_interpolate(unsigned int id);

  std::vector<std::vector<int>> current_formula;
  std::vector<bool> is_first_part;
  std::vector<unsigned int> reason;
  std::vector<bool> is_assigned;
  std::vector<bool> variable_seen;
  std::unordered_map<unsigned int, std::vector<int>> id_to_clause;
  std::unordered_map<unsigned int, std::vector<unsigned int>> id_to_premises;
  std::unordered_set<int> first_part_variables;
  std::vector<std::vector<int>> interpolant_clauses;
  std::vector<int> last_assumptions;
  unsigned int final_id;
  std::vector<int> trail;
  Cadical solver;

};

std::tuple<unsigned int, std::vector<int>, std::vector<unsigned int>> read_lrat_line(std::ifstream& input);

inline int get_literal(unsigned int literal_int) {
  int v = literal_int >> 1;
  bool negated = literal_int % 2;
  return negated ? -v: v;
}

inline unsigned read_number(std::ifstream& input) {
  // Decodes an integer from a binary LRAT file.
  unsigned int number = 0;
  unsigned char ch;
  int shift = 0;
  while (input.read(reinterpret_cast<char*>(&ch), sizeof(char))) {
    if (ch == (unsigned char) 0)
      return 0;
    number |= ((unsigned int)(ch & 0x7f) << shift);
    if ((ch & ~0x7f) == 0)
      break;
    shift += 7;
  }
  return number;
}

inline void Interpolator::append_formula(const std::vector<std::vector<int>>& formula, bool first_part) {
  current_formula.reserve(current_formula.size() + formula.size());
  for (const auto& clause: formula) {
    add_clause(clause, first_part);
  }
}

inline bool Interpolator::solve(const std::vector<int>& assumptions) {
  last_assumptions = assumptions;
  return solver.solve(assumptions) != 20;
}

inline std::vector<int> Interpolator::get_clause(unsigned int id) const {
  if (id > current_formula.size()) {
    return id_to_clause.at(id);
  } else {
    return current_formula[id - 1];
  }
}

inline std::vector<int> Interpolator::get_model() {
  return solver.get_model();
}

}

#endif // ITP_INTERPOLATOR_H_