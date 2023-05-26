#include "interpolator.hpp"

#include <iostream>

int main() {
  cadical_itp::Interpolator interpolator;
  interpolator.append_formula({ {-2, -3, 1}, {-2, -3, -1}}, true);
  std::vector<int> assumptions = {2, 3};
  auto result = interpolator.solve(assumptions);
  std::cout << result << std::endl;
  interpolator.get_interpolant({}, 0);
  return 0;
}
