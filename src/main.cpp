#include "interpolator.hpp"

#include "aig/aig/aig.h"
#include "base/abc/abc.h"
#include "opt/dar/dar.h"

#include <iostream>
#include <vector>

int main() {
  // Start an ABC AIG manager:
  auto aig_man = abc::Aig_ManStart(0);

  std::vector<abc::Aig_Obj_t*> inputs;

  for (int i = 0; i < 4; i++) {
    inputs.push_back(abc::Aig_ObjCreateCi(aig_man));
  }

  auto and_12 = abc::Aig_And(aig_man, inputs[0], inputs[1]);
  auto and_34 = abc::Aig_And(aig_man, inputs[2], inputs[3]);

  // Must set ID's first.
  abc::Aig_ManSetCioIds(aig_man);

  if (Aig_ObjIsCi(inputs[2])) { // Check if it is a combinational input
    int index = abc::Aig_ObjCioId(inputs[2]);
    std::cout << "The index of the input is: " << index << std::endl;
  }

  auto or_1234 = abc::Aig_Or(aig_man, and_12, and_34);

  auto neg_or_1234_ = abc::Aig_Not(or_1234);

  auto output = abc::Aig_ObjCreateCo(aig_man, neg_or_1234_);

  std::cout << "Number of PIs: " << Aig_ManCiNum(aig_man) << std::endl;
  std::cout << "Number of nodes: " << Aig_ManNodeNum(aig_man) << std::endl;
  std::cout << "Number of POs: " << Aig_ManCoNum(aig_man) << std::endl;

  // Cleanup AIG.
  auto removed = abc::Aig_ManCleanup(aig_man);
  std::cerr << "Removed " << removed << " nodes." << std::endl;

  // Check AIG consistency.
  if (!abc::Aig_ManCheck(aig_man)) {
    std::cerr << "Error: AIG failed consistency check." << std::endl;
    return 1;  // or handle error as appropriate for your program.
  }

  // // Compression:
  abc::Dar_LibStart();
  abc::Dar_ManRewriteDefault(aig_man);
  abc::Dar_LibStop();

  std::cout << "Number of nodes: " << Aig_ManNodeNum(aig_man) << std::endl;

  abc::Aig_ManStop(aig_man);


  return 0;
}
