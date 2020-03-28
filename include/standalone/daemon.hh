#ifndef __DAEMON_HH__
#define __DAEMON_HH__

#include <string>
#include <signal.h>

// This class performs all functions that a daemon is suppose to perform (ie. fork, change sigactions, etc.). Later on, any functions that are deemed necessary for all daemons to perform, should be put in here. Each daemon will have ane Daemon object.

class Daemon {
public:  
  
  Daemon();
  ~Daemon();
  
  void daemonizeThisProgram(std::string pidFileName, std::string runPath);
  //void static signal_handler(int const signum);
  void changeSignal(struct sigaction * newAction, struct sigaction * oldAction, int const signum);

  //  bool static volatile loop;
  bool volatile loop;

  //  private:
  //  void signal_handler(int const signum);
};

#endif
