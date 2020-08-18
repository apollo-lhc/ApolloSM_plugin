#include <standalone/progOpt.hh>
#include <boost/program_options.hpp>
#include <fstream>
#include <string>

namespace po = boost::program_options;

//Store Command Line Arguments into variables map
po::variables_map storeCliArguments(po::options_description cli_options, int argc, char ** argv) {
  po::variables_map cli_map;
  try {
    po::store(po::parse_command_line(argc, argv, cli_options), cli_map);
  } catch (std::exception &e) {
    fprintf(stderr, "ERROR in BOOST parse_command_line: %s\n", e.what());
    throw;
  }
  return cli_map;
}

//Store Config File Arguments into variables map
po::variables_map storeCfgArguments(po::options_description cfg_options, std::string configFile) {
  std::ifstream File(configFile);
  po::variables_map cfg_map;
  try {
    po::store(po::parse_config_file(File, cfg_options, true), cfg_map);
  } catch (std::exception &e) {
    fprintf(stderr, "ERROR in BOOST parse_config_file: %s\n", e.what());
    throw;
  }
  return cfg_map;
}

//Overloaded Function for returning an argument from an option 
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
void setOptionValue(bool &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map) {
  if (cli_map.count(option)) { //if option used on command line
    bool cliArg = cli_map[option].as<bool>();
    if (cliArg == true) { //cli argument is empty
      if (cfg_map.count(option)) { //if option used in config file
	Arg = cfg_map[option].as<bool>(); //assign argument from config file
      }
    } else {
      Arg = cliArg;
    }
  }
  return;
}
