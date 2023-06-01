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
  void flush_proof();
  void reset_proof();
  int val(int variable);
  uint64_t get_current_clause_id() const;

 private:
  void set_assumptions(const std::vector<int>& assumptions);

  CaDiCaL::Solver solver;
  FILE* trace_file;

  class CadicalTerminator: public CaDiCaL::Terminator {
   public:
    virtual bool terminate();
  };

  static CadicalTerminator terminator;
};

inline void Cadical::flush_proof() {
  solver.flush_proof_trace();
}

inline void Cadical::reset_proof() {
  trace_file = freopen("proof.lrat", "w", trace_file);
}

inline uint64_t Cadical::get_current_clause_id() const {
  return solver.get_current_clause_id();
}

}

#endif // ITP_CADICAL_H_