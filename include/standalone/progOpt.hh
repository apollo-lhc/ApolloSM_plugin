#ifndef __PROGOPT_HH__
#define __PROGOPT_HH__

#include <boost/program_options.hpp>
#include <string>

namespace po = boost::program_options;

//Store Command Line Arguments into variables map
po::variables_map storeCliArguments(po::options_description cli_options, int argc, char ** argv);

//Store Config File Arguments into variables map
po::variables_map storeCfgArguments(po::options_description cfg_options, std::string configFile);

//Overloaded Function for returning an argument from an option 
void setOptionValue(int &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map);
void setOptionValue(std::string &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map);
void setOptionValue(bool &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map);

#endif
