#ifndef ITP_INTERPOLATOR_H_
#define ITP_INTERPOLATOR_H_

#include <vector>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <fstream>

#include "aig/aig/aig.h"

#include "cadical_solver.hpp"

namespace cadical_itp {

class Interpolator {
 public:
  Interpolator();
  ~Interpolator();
  void add_clause(const std::vector<int>& clause, bool first_part);
  void append_formula(const std::vector<std::vector<int>>& formula, bool first_part);
  bool solve(const std::vector<int>& assumptions);
  std::vector<int> get_model();
  std::vector<int> get_values(const std::vector<int>& variables);
  std::pair<int, std::vector<std::vector<int>>> get_interpolant(const std::vector<int>& shared_variables, int auxiliary_variable_start, bool rewrite_aig);

 protected:
  std::vector<int> get_clause(uint64_t id) const;
  std::vector<uint64_t> get_core() const;
  void replay_proof(std::vector<uint64_t>& core);
  uint64_t propagate(uint64_t id);
  std::pair<std::vector<int>, abc::Aig_Obj_t*> analyze_and_interpolate(uint64_t id);
  void delete_clauses();
  void set_shared_variables(const std::vector<int>& shared_variables);
  std::vector<std::vector<int>> get_interpolant_clauses(const std::vector<int>& shared_variables, int auxiliary_variable_start);

  // AIG procedures.
  abc::Aig_Obj_t* get_aig_node(uint64_t id);

  std::vector<uint64_t> reason;
  std::vector<bool> is_assigned;
  std::vector<bool> variable_seen;
  std::unordered_map<uint64_t, bool> id_in_first_part;
  std::unordered_set<int> first_part_variables_set;
  std::unordered_set<int> shared_variables_set;
  std::vector<int> last_assumptions;
  std::vector<int> trail;
  std::vector<uint64_t> to_delete;
  Cadical solver;

  // For managing AIGs.
  std::unordered_map<uint64_t, abc::Aig_Obj_t*> id_to_aig_node;
  std::unordered_map<int, abc::Aig_Obj_t*> shared_variable_to_ci;
  abc::Aig_Man_t * aig_man;
};

inline void Interpolator::append_formula(const std::vector<std::vector<int>>& formula, bool first_part) {
  id_in_first_part.reserve(id_in_first_part.size() + formula.size());
  for (const auto& clause: formula) {
    add_clause(clause, first_part);
  }
}

inline bool Interpolator::solve(const std::vector<int>& assumptions) {
  last_assumptions = assumptions;
  return solver.solve(assumptions) != 20;
}

inline std::vector<int> Interpolator::get_model() {
  return solver.get_model();
}

inline std::vector<int> Interpolator::get_values(const std::vector<int>& variables) {
  return solver.get_values(variables);
}

// For debugging.

inline bool contains(std::vector<int> v1, std::vector<int> v2) {
  if(v1.size() < v2.size()) 
      return false; 

  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());

  return std::includes(v1.begin(), v1.end(), v2.begin(), v2.end());
}

}

#endif // ITP_INTERPOLATOR_H_