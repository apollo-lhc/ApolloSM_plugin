#ifndef __PROGOPT_HH__
#define __PROGOPT_HH__

#include <boost/program_options.hpp>
#include <string>

namespace po = boost::program_options;

void setOptionValue(int &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map);
void setOptionValue(std::string &Arg, std::string option, po::variables_map cli_map, po::variables_map cfg_map);

#endif
