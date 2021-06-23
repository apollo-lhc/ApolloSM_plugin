
#ifndef __EYE_SCAN_LINK_HH__
#define __EYE_SCAN_LINK_HH__

#include <string>
//#include <vector>
#include <boost/program_options.hpp>
#include <ApolloSM/ApolloSM.hh> 

class EyeScanLink {
public:
  EyeScanLink(std::string linkName, std::string lpmName, boost::program_options::parsed_options PO);
  ~EyeScanLink();
  
  void printInfo();
  void enableEyeScan(ApolloSM * SM);
  //  void setOffsets();
  void scan(ApolloSM * SM);
  void plot();
    
private:
  std::string name;
  std::string lpmnode;
  double      phase;
  int         voltage;
  std::string outfile;
  std::string png;
  uint32_t    maxPrescale;

  std::vector<eyescanCoords> esCoords;
};

#endif