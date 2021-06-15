#ifndef __CM_HH__
#define __CM_HH__

//#include "FPGA.hh"
#include <standalone/FPGA.hh>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <ApolloSM/ApolloSM.hh>

class CM {
public:
  CM(std::string nameArg, boost::program_options::parsed_options PO); 
  ~CM();
  
  void printInfo();
  //  void SetUp(ApolloSM const * const SM); 
  void SetUp(ApolloSM * SM); 

  //  void PowerUp(boost::program_options::variables_map vm);
  
  std::vector<FPGA> FPGAs;

  std::string name;
  int         ID;
  std::string powerGood;
  bool        powerUp;
private:
  bool checkNode(ApolloSM * SM, std::string const node, uint32_t const carrectVal);
};

#endif
