#ifndef CADICAL_ANN_INTERRUPT_H_
#define CADICAL_ANN_INTERRUPT_H_

#include <iostream>
#include <exception>

namespace cadical_annotations {

class InterruptedException: std::exception {
  virtual const char* what();
};

class InterruptHandler {
 public:
  static void interrupt(int signal);
  static int interrupted(void*);

 private:
  static int signal_received;
};

}

#endif // CADICAL_ANN_INTERRUPT_H_