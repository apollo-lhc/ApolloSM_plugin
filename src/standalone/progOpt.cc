#include <standalone/progOpt.hh>
#include <boost/program_options.hpp>
#include <string>

namespace po = boost::program_options;

void setOptionValue(int &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map) {
  if (cli_map.count(option)) { //if option used on command line
    int cliArg = cli_map[option].as<int>();
    if (cliArg == 0) { //cli argument is empty
      if (cfg_map.count(option)) { //if option used in config file
	Arg = cfg_map[option].as<int>(); //assign argument from config file
      }
    } else {
      Arg = cliArg;
    }
  }
  return;
}

void setOptionValue(std::string &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map) {
  if (cli_map.count(option)) { //if option used on command line
    std::string cliArg = cli_map[option].as<std::string>();
    if (cliArg == "") { //cli argument is empty
      if (cfg_map.count(option)) { //if option used in config file
	Arg = cfg_map[option].as<std::string>(); //assign argument from config file
      }
    } else {
      Arg = cliArg;
    }
  }
  return;
}
