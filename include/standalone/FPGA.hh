#ifndef __FPGA_HH__
#define __FPGA_HH__

#include <string>
#include <boost/program_options.hpp>

class FPGA {
public:
  FPGA(std::string nameArg, std::string cmArg, boost::program_options::parsed_options PO); 
  ~FPGA();

  std::string name;
  std::string cm;
  bool program;
  std::string svfFile;
  std::string xvc;
  std::string c2c;
  std::string done;
  std::string init;
  std::string axi;
  std::string axilite;
};

#endif
