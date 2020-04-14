#include <standalone/CM.hh>

#include <string>
#include <vector>
#include <boost/program_options.hh>

int howMany(boost::program_options::parsed_options po, std::string option) {
  int count = 0;
  int totalNumOptions = po.options.size();

  for(int i = 0; i < totalNumOptions; i++) {
    std::string nextOption = po.options[i].string_key;
    if(0 == nextOption.compare(option)) {
      count++;
    }
  }

  return count;
} 

CM::CM() {

}

CM::~CM() {

}

CM::PowerUp(boost::program_ioptions::variables_map vm) {
  powerGood    = "";
  bool powerUp = false;
  
}

// std::vector<FPGA> FPGAs;
//   
// int ID;
// std::string powerGood;
// bool powerUp;


