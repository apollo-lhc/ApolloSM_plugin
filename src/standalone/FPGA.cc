#include <standalone/FPGA.hh>
#include <string>
#include <boost/program_options.hpp>
#include <sstream>

FPGA::FPGA(std::string nameArg, std::string cmArg, boost::program_options::parsed_options PO) {
  // initialize variables
  this->name    = nameArg;
  this->cm      = cmArg;
  this->program = false;
  this->svfFile = "";
  this->xvc     = "";
  this->c2c     = "";
  this->done    = "";
  this->init    = "";
  this->axi     = "";
  this->axilite = "";
  
  // Total number of options received from config file
  int numOptions = PO.options.size();

  // Look for all options related to this FPGA (ie. svf file, xvc label, c2c, etc.)
  for(int i = 0; i < numOptions; i++) {
    std::string nextOption = PO.options[i].string_key;
    std::string value      = PO.options[i].value[0].c_str();
    
    if(0 == nextOption.compare(cmArg + "." + nameArg + ".PROGRAM"))       // ex: CM1.KINTEX.PROGRAM
      {
	// found value for program
	std::istringstream(PO.options[i].value[0]) >> std::boolalpha >> (this->program);
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".SVFFILE"))  // ex: CM1.KINTEX.SVFFILE
      {
	// found value for svf file
	this->svfFile = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".XVCLABEL")) // ex: CM1.KINTEX.XVCLABEL
      {
	// found value for xvc label
	this->xvc = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".C2C"))      // ex: CM1.KINTEX.C2C
      {
	// found value for c2c base node
	this->c2c = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".DONEBIT"))  // ex: CM1.KINTEX.DONEBIT
      {
	// found value for done bit
	this->done = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".INITBIT"))  // ex: CM1.KINTEX.INITBIT
      {
	// found value for initialization bit
	this->init = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".AXI"))      // ex: CM1.KINTEX.AXI
      {
	// found value for axi unblock bit
	this->axi = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".AXILITE"))  // ex: CM1.KINTEX.AXILITE
      {
	// found value for axilite unblock bit
	this->axilite = value;
      }
  }
}

FPGA::~FPGA() {
}

//std::string name    ; 
//std::string cm      ;
//std::string svfFile ;
//std::string xvc     ;
//std::string c2c     ;
//std::string done    ;
//std::string init    ;
//std::string axi     ;
//std::string axilite ;
