#include <standalone/CM.hh>
#include <standalone/FPGA.hh>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <sstream>

CM::CM(std::string nameArg, boost::program_options::parsed_options PO) {

  // initialize variables
  this->name      = nameArg;
  this->ID        = std::stoi(nameArg.substr(2,1)); // Ex: if nameArg = "CM12" then ID = 12
  this->powerGood = "";
  this->powerUp   = false;
  
  // Total number of options received from config file
  int numOptions = PO.options.size();

  // Look for all options related to this CM (ie. PWRGOOD, PWRUP, FPGAs, etc.)
  for(int i = 0; i < numOptions; i++) {
    std::string nextOption = PO.options[i].string_key;

    if(0 == nextOption.compare(nameArg + ".PWRGOOD"))    // ex: CM1.PWRGOOD
      {
	// found value for power good
	this->powerGood = PO.options[i].value[0].c_str();
      }
    else if(0 == nextOption.compare(nameArg + ".PWRUP")) // ex: CM1.PWRUP
      {
	// found value for power up
	// is c_str() necessary?
	std::istringstream(PO.options[i].value[0]) >> (this->powerUp);
      }
    else if(0 == nextOption.compare(nameArg + ".FPGA"))  // ex: CM1.FPGA
      {
	// found another FPGA
	FPGA newFPGA(PO.options[i].value[0].c_str(), this->name, PO);
	this->FPGAs.push_back(newFPGA);
      }
  }
}

CM::~CM() {
}

// std::vector<FPGA> FPGAs;
//   
// std::string name;
// int ID;
// std::string powerGood;
// bool powerUp;


