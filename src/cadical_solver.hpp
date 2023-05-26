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

}

#endif // ITP_CADICAL_H_