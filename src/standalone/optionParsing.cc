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


