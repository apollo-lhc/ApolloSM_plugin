//#ifndef __OPTION_PARSING_BOOL__
//#define __OPTION_PARSING_BOOL__

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

//#endif
