#ifndef __PARSE_OPTIONS_HH__
#define __PARSE_OPTIONS_HH__

#include <boost/program_options.hpp>
#include <stdio.h>
#include <string>
#include <sstream>
#include <syslog.h> 

// ====================================================================================================
// Function declarations

boost::program_options::variables_map loadConfig(std::string const & configFileName, boost::program_options::options_description const & fileOptions);
					   
template <typename T>
void setOption(boost::program_options::options_description * fileOptions, boost::program_options::options_description * commandLineOptions, std::string paramName, std::string paramDesc, T param);

template <typename T>
void paramLog(std::string paramBame, T paramValue, std::string where, bool logToSyslog);

template <typename T>
void setParamValue(T * param, std::string paramName, boost::program_options::variables_map configFileVM, boost::program_options::variables_map commandLineVM, bool logToSyslog);

// ====================================================================================================
// Function implementations 

template <typename T>
// Function to add parameters/options to both fileOptions and commandLineOptions Saves a few lines of code. Not necessary.
void setOption(boost::program_options::options_description * fileOptions, boost::program_options::options_description * commandLineOptions, std::string paramName, std::string paramDesc, T param) {
  // It's hacky to pass the parameter to this function solely to use its type. May be a better way
  (void)param;// suppress "unused variable" error

  (*fileOptions).add_options()
    (paramName.c_str(),
     boost::program_options::value<T>(),
     paramDesc.c_str());
  (*commandLineOptions).add_options()
    (paramName.c_str(),
     boost::program_options::value<T>(),
     paramDesc.c_str());
}

template <typename T>
// Log what the parameter value was set to and where it came from
void paramLog(std::string paramName, T paramValue, std::string where, bool logToSyslog) {
  // To print almost any type (int, etc) as strings
  std::stringstream ss;
  std::string str;
  ss << paramValue;
  ss >> str;
  
  // for printing bools
  std::string bStr("b");
  if(!bStr.compare(typeid(paramValue).name())) {
    // T is bool
    if(logToSyslog) {
      syslog(LOG_INFO, "%s was set to: %s %s\n", paramName.c_str(), (!str.compare("1")) ? "true" : "false", where.c_str()); 
    } else {
      fprintf(stdout, "%s was set to: %s %s\n", paramName.c_str(), (!str.compare("1")) ? "true" : "false", where.c_str()); 
    }
    return;
  }
  
  // for printing everything else
  if(logToSyslog) {
    syslog(LOG_INFO, "%s was set to: %s %s\n", paramName.c_str(), str.c_str(), where.c_str()); 
  } else {
    fprintf(stdout, "%s was set to: %s %s\n", paramName.c_str(), str.c_str(), where.c_str());
  }
}

template <typename T>
// Set the value of a parameter by looking at the command line, config file, and default value
void setParamValue(T * param, std::string paramName, boost::program_options::variables_map configFileVM, boost::program_options::variables_map commandLineVM, bool logToSyslog) {
  // The order of precedence is: command line specified, config file specified, default
  
  if(commandLineVM.count(paramName)) {
    // parameter value specified at command line
    paramLog(paramName, commandLineVM[paramName].as<T>(), "(COMMAND LINE)", logToSyslog);
    *param = commandLineVM[paramName].as<T>();
    return;
  }
      
  if(configFileVM.count(paramName)) {
    // parameter value specified in config file
    paramLog(paramName, configFileVM[paramName].as<T>(), "(CONFIG FILE)", logToSyslog);
    *param = configFileVM[paramName].as<T>();
    return;
  }

  // Parameter not specified anywhere, keep default
  paramLog(paramName, *param, "(DEFAULT)", logToSyslog);
  return;

}

  /*

// ====================================================================================================
// Read from config files and set up all parameters
// For further information see https://theboostcpplibraries.com/boost.program_options

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
  */
// // ====================================================================================================
/*
// Function to add parameters/options. Saves a few lines of code. Not necessary.
template <class T>
void setOption(boost::program_options::options_description * fileOptions, boost::program_options::options_description * commandLineOptions, std::string paramName, std::string paramDesc, T param) {
  // It's hacky to pass the parameter to this function solely to use its type. May be a better way
  (*fileOptions).add_options()
    (paramName.c_str(),
     boost::program_options::value<T>(),
     paramDesc.c_str());
  (*commandLineOptions).add_options()
    (paramName.c_str(),
     boost::program_options::value<T>(),
     paramDesc.c_str());
}

template <class T>
// Log what the parameter value was set to and where it came fro
void paramLog(std::string paramName, T paramValue, std::string where, bool logToSyslog) {
  std::stringstream ss;
  std::string str;
  ss << paramValue;
  ss >> str;
  if(logToSyslog) {
    syslog(LOG_INFO, "%s was set to: %s %s\n", paramName.c_str(), str.c_str(), where.c_str()); 
  } else {
    fprintf(stdout, "%s was set to: %s %s\n", paramName.c_str(), str.c_str(), where.c_str());
  }
}

template <class T>
void setParamValue(T * param, std::string paramName, boost::program_options::variables_map configFileVM, boost::program_options::variables_map commandLineVM, bool logToSyslog) {
  // The order of precedence is: command line specified, config file specified, default
  
  if(commandLineVM.count(paramName)) {
    // parameter value specified at command line
    paramLog(paramName, commandLineVM[paramName].as<T>(), "(COMMAND LINE)", logToSyslog);
    *param = commandLineVM[paramName].as<T>();
    return;
  }
      
  if(configFileVM.count(paramName)) {
    // parameter value specified in config file
    paramLog(paramName, configFileVM[paramName].as<T>(), "(CONFIG FILE)", logToSyslog);
    *param = configFileVM[paramName].as<T>();
    return;
  }

  // Parameter not specified anywhere, keep default
  paramLog(paramName, *param, "(DEFAULT)", logToSyslog);
  return;

}
*/ 
#endif
