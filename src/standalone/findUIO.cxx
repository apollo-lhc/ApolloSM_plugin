#include <ApolloSM/uioLabelFinder.hh>

#include <iostream>

int main(int argc, char ** argv){
  if(argc < 2){
    printf("Usage: %s uio_label\n",argv[0]);
    return 1;
  }
  int uio = label2uio(argv[1]);
  if(uio >= 0){
    printf("%s is at /dev/uio%d\n",argv[1],uio);
  }else{
    printf("%s not found\n",argv[1]);
  }
  return 0;
}

//   //Set up program options
// po::options_description options("cmpwrdown options");
// options.add_options()
// ("UIO_label,c", po::value<std::string>(), "Path to the default config file");
  
//   //setup for loading program options
//   std::ifstream configFile(DEFAULT_CONFIG_FILE);
//   po::variablesmap progOptions;

//   try { //Get options from command line
//     po::store(parse_command_line(argc, argv, options), progOptions);
//   } catch (std::exception &e) {
//     fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
//     std::cout << options << std::endl;
//     return 0;
//   }

//   if(configFile) { //If configFile opens, get options from config file
//     try{ 
//       po::store(parse_config_file(configFile,options,true), progOptions);
//     } catch (std::exception &e) {
//       fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
//       std::cout << options << std::endl;
//       return 0; 
//     }
//   }
