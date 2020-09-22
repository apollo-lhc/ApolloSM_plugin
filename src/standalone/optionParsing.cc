#include <standalone/optionParsing.hh>

//Remember to fill parsedOptions in order from highest to lowest priority
void FillOptions(po::parsed_options Options,
		 std::map<std::string,std::vector<std::string> > & parsedOptions){
  for(size_t iOpt = 0; iOpt < Options.options.size();iOpt++){
    std::string optionName = Options.options[iOpt].string_key;      
    std::string optionValue = "";
    if(Options.options[iOpt].value.size()){
      for(size_t i=0;i<Options.options[iOpt].value.size();i++){
	if(i){
	  optionValue+=" ";
	}
	optionValue += Options.options[iOpt].value[i];
      }
    }else{
      optionValue = "";
    }
    parsedOptions[optionName].push_back(optionValue);   
  }  
}


//Stolen from https://stackoverflow.com/questions/4452136/how-do-i-use-boostlexical-cast-and-stdboolalpha-i-e-boostlexical-cast-b
//provides conversion between true/false to bool and back
namespace boost {
  template<> 
  bool lexical_cast<bool, std::string>(const std::string& arg) {
    std::istringstream ss(arg);
    bool b;
    ss >> std::boolalpha >> b;
    return b;
  }

  template<>
  std::string lexical_cast<std::string, bool>(const bool& b) {
    std::ostringstream ss;
    ss << std::boolalpha << b;
    return ss.str();
  }
}
