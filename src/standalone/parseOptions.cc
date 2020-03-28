#include <standalone/parseOptions.hh>
#include <boost/program_options.hpp>
#include <fstream>

// non-template function implementations

boost::program_options::variables_map loadConfig(std::string const & configFileName,
						 boost::program_options::options_description const & fileOptions) {
  // This is a container for the information that fileOptions will get from the config file
  boost::program_options::variables_map vm;  

  // Check if config file exists
  std::ifstream ifs{configFileName};
  syslog(LOG_INFO, "Config file \"%s\" %s\n",configFileName.c_str(), (!ifs.fail()) ? "exists" : "does not exist");

  if(ifs) {
    // If config file exists, parse ifs into fileOptions and store information from fileOptions into vm
    boost::program_options::store(parse_config_file(ifs, fileOptions), vm);
  }

  return vm;
}
