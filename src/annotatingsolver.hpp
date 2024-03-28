#ifndef ANNOTATINGSOLVER_H_
#define ANNOTATINGSOLVER_H_

#include <vector>
#include <tuple>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <memory>

#include <boost/dynamic_bitset.hpp>

typedef std::pair<boost::dynamic_bitset<>, boost::dynamic_bitset<>> Annotation;

struct VariableAnnotation {
  const boost::dynamic_bitset<> dependencies; // TODO: make this a reference
  Annotation annotation_;
  
  VariableAnnotation(const boost::dynamic_bitset<>& dependencies): dependencies(dependencies), 
    annotation_{ boost::dynamic_bitset<>(dependencies.size()), boost::dynamic_bitset<>(dependencies.size()) } {};

  void instantiate(const Annotation& annotation) {
    // Instantiate only if the complementary literal is not already in the annotation
    annotation_.first  |= (~annotation_.second & annotation.first  & dependencies);
    annotation_.second |= (~annotation_.first  & annotation.second & dependencies);
  }

  void merge(const Annotation& annotation) {
    annotation_.first |= (annotation.first & dependencies);
    annotation_.second |= (annotation.second & dependencies);
  }

  boost::dynamic_bitset<> missing() const {
    return ~annotation_.first & ~annotation_.second & dependencies;
  }

  bool operator==(const VariableAnnotation& other) const {
    return annotation_ == other.annotation_ && dependencies == other.dependencies;
  }
};

typedef std::unordered_map<int, VariableAnnotation> ClauseAnnotation;

typedef std::pair<int, std::vector<int>> ConflictPair;

inline Annotation operator-(const Annotation& annotation1, const Annotation& annotation2) {
    return { annotation1.first - annotation2.first, annotation1.second - annotation2.second };
}

inline Annotation& operator|=(Annotation& annotation1, const Annotation& annotation2) {
    annotation1.first |= annotation2.first;
    annotation1.second |= annotation2.second;
    return annotation1;
}

inline Annotation operator-(const VariableAnnotation& annotation1, const VariableAnnotation& annotation2) {
    return annotation1.annotation_ - annotation2.annotation_;
}

inline Annotation operator&(const Annotation& annotation, const boost::dynamic_bitset<> bitset) {
    return Annotation {annotation.first & bitset, annotation.second & bitset};
}

inline void instantiateAll(ClauseAnnotation& clause_annotation, const Annotation& annotation) {
  for (auto& [v, annotation_v]: clause_annotation)
    annotation_v.instantiate(annotation);
}

namespace cadical_annotations {

class Cadical;

class Annotatingsolver {
 public:
  Annotatingsolver();
  ~Annotatingsolver();
  void add_universal(int variable);
  void create_arbiter_var(int variable, const std::vector<int>& annotation, const std::vector<int>& dependencies);
  void add_clause(const std::vector<int>& clause, const std::vector<int>& annotation={});
  void append_formula(const std::vector<std::vector<int>>& formula);
  bool solve(const std::vector<int>& assumptions);
  std::vector<int> get_model();
  std::vector<int> get_values(const std::vector<int>& variables);
  std::pair<std::vector<int>, std::vector<ConflictPair>> get_annotation_conflicts();

  // Exception class to throw when solver is not in the correct state.
  class SolverStateException : public std::exception {
   public:
    explicit SolverStateException(const std::string& message) : message(message) {}
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
  std::tuple<std::vector<int>, ClauseAnnotation, std::vector<ConflictPair>> analyze_and_annotate(uint64_t id);
  void delete_clauses();
  Annotation vector_to_annotation(const std::vector<int>& literal_vector);
  std::vector<int> annotation_to_vector(const Annotation& annotation);
  ClauseAnnotation get_initial_annotation(const std::vector<int>& clause);
  boost::dynamic_bitset<> get_missing_annotations(const ClauseAnnotation& clause_annotation);
  std::pair<Annotation, Annotation> compute_partial_unifiers(const ClauseAnnotation& clause_annotation1, const ClauseAnnotation& clause_annotation2);
  std::pair<Annotation, Annotation> compute_unifiers(const ClauseAnnotation& clause_annotation1, const ClauseAnnotation& clause_annotation2);
  std::vector<ConflictPair> get_conflicting_annotations(const ClauseAnnotation& clause_annotation1, const ClauseAnnotation& clause_annotation2, const Annotation& unifier_second);
  std::vector<ConflictPair> unify_annotations(ClauseAnnotation& running_clause_annotation, const ClauseAnnotation& reason_clause_annotation);

  // Debugging and printing
  std::vector<int> variable_annotation_to_vector(const VariableAnnotation& annotation);
  void print_annotation_map(const ClauseAnnotation& clause_annotation);

  std::vector<uint64_t> reason;
  std::vector<bool> is_assigned;
  std::vector<bool> variable_seen;
  std::vector<int> last_assumptions;
  std::vector<int> trail;
  std::vector<uint64_t> to_delete;
  std::unique_ptr<Cadical> solver;

  std::unordered_map<int, VariableAnnotation> variable_to_annotation;
  std::unordered_map<uint64_t, ClauseAnnotation> clause_id_to_annotation;
  std::unordered_map<int, int> annotation_index_to_variable;
  std::unordered_map<int, int> variable_to_annotation_index;
  std::unordered_map<uint64_t, std::vector<ConflictPair>> clause_id_to_annotation_conflicts;
  std::vector<int> annotation_core;

  bool empty_added;
};

// For debugging.

inline bool contains(std::vector<int> v1, std::vector<int> v2) {
  if(v1.size() < v2.size()) 
      return false; 

  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());

  return std::includes(v1.begin(), v1.end(), v2.begin(), v2.end());
}

}

#endif // ANNOTATINGSOLVER_H_