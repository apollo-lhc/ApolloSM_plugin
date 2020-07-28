#ifndef __FPGA_HH__
#define __FPGA_HH__

#include <string>
#include <boost/program_options.hpp>
#include <ApolloSM/ApolloSM.hh>

class FPGA {
public:
  FPGA(std::string nameArg, std::string cmArg, boost::program_options::parsed_options PO); 
  ~FPGA();

  void printInfo();
  //  void bringUp(ApolloSM const * const SM) const;
  void bringUp(ApolloSM * SM);

  std::string name;
  std::string cm;
  bool program;
private:
  bool checkNode(ApolloSM * SM, std::string const node, uint32_t const correctVal);
  int bringupCMFPGAs(ApolloSM * SM);
};

#endif
