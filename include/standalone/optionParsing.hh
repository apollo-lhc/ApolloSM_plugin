#ifndef __OPTION_PARSING__
#define __OPTION_PARSING__

#include <map>
#include <string>
#include <vector>
#include <boost/program_options.hpp> //for configfile parsing
#include <boost/lexical_cast.hpp>
namespace po = boost::program_options; //making life easier for boost                                 


//Remember to fill parsedOptions in order from highest to lowest priority
void FillOptions(po::parsed_options Options,
     std::map<std::string,std::vector<std::string> > & parsedOptions);

//#include <standalone/optionParsing_bool.hh>

template<typename T> 
T GetFinalParameterValue(std::string const & option,
       std::map<std::string,std::vector<std::string> > const &map,
       T const & defaultValue){
  T reg = defaultValue;
  auto optionVal = map.find(option);
  if(optionVal != map.end()){
    //Get the first value, highest priority
    reg = boost::lexical_cast<T>(optionVal->second.front());
  }
  return reg;
}

template<typename T> 
T *  GetFinalParameterValue(std::string const & option,std::map<std::string,std::vector<std::string> > const &map,T *const & defaultValue){
  return NULL;
}

#endif