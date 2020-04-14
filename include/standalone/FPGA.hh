#ifndef __FPGA_HH__
#define __FPGA_HH__

#include <string>

class FPGA {
public:
  FPGA(); 
  ~FPGA();

  std::string name;
  std::string cm;
  std::string svfFile;
  std::string xvc;
  std::string c2c;
  std::string done;
  std::string init;
  std::string axi;
  std::string axilite;
};

#endif
