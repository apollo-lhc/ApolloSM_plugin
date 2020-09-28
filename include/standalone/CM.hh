#ifndef __CM_HH__
#define __CM_HH__

#include <standalone/FPGA.hh>
#include <string>
#include <vector>

class CM {
public:
  CM(); 
  ~CM();

  std::vector<FPGA> FPGAs;
  
  int ID;
  std::string powerGood;
  bool powerUp;
};

#endif
