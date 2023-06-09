#ifndef ITP_CADICAL_H_
#define ITP_CADICAL_H_

#include <vector>
#include <cstdio>

#include "cadical.hpp"

namespace cadical_itp {

class Cadical {
 public:
  Cadical();
  ~Cadical();
  void append_formula(const std::vector<std::vector<int>>& formula);
  void add_clause(const std::vector<int>& clause);
  void assume(const std::vector<int>& assumptions);
  int solve(const std::vector<int>& assumptions);
  int solve();
  int solve(int conflict_limit);
  std::vector<int> get_failed(const std::vector<int>& assumptions);
  std::vector<int> get_values(const std::vector<int>& variables);
  std::vector<int> get_model();
  int val(int variable);
  uint64_t get_current_clause_id() const;
  uint64_t get_latest_id() const;
  bool is_initial_clause(uint64_t id) const;
  const std::vector<uint64_t>& get_premises(uint64_t id) const;
  const std::vector<int>& get_clause(uint64_t id) const;

 private:
  void set_assumptions(const std::vector<int>& assumptions);

  CaDiCaL::Solver solver;

  class CadicalTerminator: public CaDiCaL::Terminator {
   public:
    virtual bool terminate();
  };

  static CadicalTerminator terminator;
};

inline uint64_t Cadical::get_current_clause_id() const {
  return solver.get_current_clause_id();
}

inline uint64_t Cadical::get_latest_id() const {
  return solver.get_latest_id();
}

inline bool Cadical::is_initial_clause(uint64_t id) const {
  return solver.is_initial_clause(id);
}

inline const std::vector<uint64_t>& Cadical::get_premises(uint64_t id) const {
  return solver.get_premises(id);
}

inline const std::vector<int>& Cadical::get_clause(uint64_t id) const {
  return solver.get_clause(id);
}

}

#endif // ITP_CADICAL_H_