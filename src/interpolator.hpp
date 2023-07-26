#ifndef ITP_INTERPOLATOR_H_
#define ITP_INTERPOLATOR_H_

#include <vector>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <string>

#include "aig/aig/aig.h"

#include "cadical_solver.hpp"

namespace cadical_itp {

struct Proofnode {
  int label;
  bool flag;
  std::shared_ptr<Proofnode> left;
  std::shared_ptr<Proofnode> right;
  // Constructors
  Proofnode(int label, const std::shared_ptr<Proofnode>& left, const std::shared_ptr<Proofnode>& right) : label(label), flag(false), left(left), right(right) {}
  Proofnode(int label) : label(label), flag(false), left(nullptr), right(nullptr) {}
};

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

  // Exception class to throw when interpolator is not in the correct state.
  class InterpolatorStateException : public std::exception {
   public:
    explicit InterpolatorStateException(const std::string& message) : message(message) {}
    const char* what() const noexcept override {
      return message.c_str();
    }
   private:
    std::string message;
  };

 protected:
  
  enum class State {
    UNDEFINED,
    SAT,
    UNSAT
  };

  State state;

  std::vector<int> get_clause(uint64_t id) const;
  std::vector<uint64_t> get_core() const;
  void replay_proof(std::vector<uint64_t>& core);
  uint64_t propagate(uint64_t id);
  std::pair<std::vector<int>, std::shared_ptr<Proofnode>> analyze_and_interpolate(uint64_t id);
  void delete_clauses();
  std::vector<std::vector<int>> get_interpolant_clauses(std::shared_ptr<Proofnode>& rootnode, const std::vector<int>& shared_variables, int auxiliary_variable_start, bool rewrite_aig);
  std::shared_ptr<Proofnode> get_proofnode(uint64_t id);
  void construct_aig(std::shared_ptr<Proofnode>& rootnode, const std::vector<int>& shared_variables);
  void process_node(const std::shared_ptr<Proofnode>& proofnode);

  std::vector<uint64_t> reason;
  std::vector<bool> is_assigned;
  std::vector<bool> variable_seen;
  std::unordered_map<uint64_t, bool> id_in_first_part;
  std::unordered_set<int> first_part_variables_set;
  std::vector<int> last_assumptions;
  std::vector<int> trail;
  std::vector<uint64_t> to_delete;
  Cadical solver;

  std::unordered_map<uint64_t, std::shared_ptr<Proofnode>> clause_id_to_proofnode;

  std::unordered_map<std::shared_ptr<Proofnode>,abc::Aig_Obj_t*> proofnode_to_aig_node;
  std::unordered_map<int, abc::Aig_Obj_t*> variable_to_ci;
  std::unordered_set<int> shared_variables_set;
  std::vector<int> aig_input_variables;
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
  auto result = solver.solve(assumptions);
  if (result == 10) {
    state = State::SAT;
  } else if (result == 20) {
    state = State::UNSAT;
  } else {
    throw InterpolatorStateException("unexpected result from solver");
  }
  return result != 20;
}

inline std::vector<int> Interpolator::get_model() {
  if (state != State::SAT) {
    throw InterpolatorStateException("can only call get_model in SAT state");
  }
  return solver.get_model();
}

inline std::vector<int> Interpolator::get_values(const std::vector<int>& variables) {
  if (state != State::SAT) {
    throw InterpolatorStateException("can only call get_model in SAT state");
  }
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