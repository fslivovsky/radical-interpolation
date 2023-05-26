#ifndef CADICAL_INTERRUPT_H_
#define CADICAL_INTERRUPT_H_

#include <iostream>
#include <exception>

namespace cadical_itp {

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

#endif // CADICAL_INTERRUPT_H_