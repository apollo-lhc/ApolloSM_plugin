#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
//#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <standalone/CM.hh>
//#include <uhal/uhal.hpp>
#include <vector>
#include <string>
//#include <sys/stat.h> //for umask
//#include <sys/types.h> //for umask
//#include <BUException/ExceptionBase.hh>

// ================================================================================
// Setup for boost program_options
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/BUTool"
namespace po = boost::program_options;

po::variables_map getVariableMap(int argc, char** argv, po::options_descriptiom options, std::ifstream configFile) {
  //container for prog options grabbed from commandline and config file
  po::variables_map progOptions;

  //Get options from command line
  try { 
    po::store(parse_command_line(argc, argv, options), progOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    std::cout << options << std::endl;
    return 0;
  }

  //If configFile opens, get options from config file
  if(configFile) { 
    try{ 
      po::store(parse_config_file(configFile,options,true), progOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
      std::cout << options << std::endl;
      return 0; 
    }
  }

  //help option, this assumes help is a member of options_description
  if(progOptions.count("help")){
    std::cout << options << '\n';
    return 0;
  }
 
  return progOptons;
}

// ================================================================================
int main(int argc, char** argv) { 

  //Set up program options
  po::options_description options("cmpwrdown options");
  options.add_options()
    ("help,h",    "Help screen")
    ("DEFAULT_CONNECTION_FILE,C", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file")
    ("DEFAULT_CM_ID,c",           po::value<int>()->default_value(1),                                             "Default CM to power down");
  
  //setup for loading program options
  std::ifstream configFile(DEFAULT_CONFIG_FILE);
  po::variables_map progOptions = getVariableMap(argc, argv, options, configFile);

  //Set connection file
  std::string connectionFile = "";
  if (progOptions.count("DEFAULT_CONNECTION_FILE")) {
    connectionFile = progOptions["DEFAULT_CONNECTION_FILE"].as<std::string>();
  }
  //Set CM_ID
  int CM_ID = 0;
  if (progOptions.count("DEFAULT_CM_ID")) {
    CM_ID = progOptions["DEFAULT_CM_ID"].as<int>();
  }
  
  // Make an ApolloSM and command module
  CM * commandModule = NULL;  
  ApolloSM * SM      = NULL;
  try{
    SM = new ApolloSM();
    if(NULL == SM){
      fprintf(stderr, "Failed to create new ApolloSM. Terminating program\n");
      exit(EXIT_FAILURE);
    }else{
      fprintf(stdout,"Created new ApolloSM\n");      
    }

    // load connection file
    std::vector<std::string> arg;
    printf("Using %s\n", connectionFile.c_str());
    arg.push_back(connectionFile);
    SM->Connect(arg);
    
    // ==============================
    // Make a command module
    commandModule = new CM();
    if(NULL == commandModule){
      fprintf(stderr, "Failed to create new CM. Terminating program\n");
      exit(EXIT_FAILURE);
    }else{
      fprintf(stdout,"Created new CM\n");      
    }

    // ==============================
    // power down CM
    commandModule->ID = CM_ID;
    int wait_time = 5; // 1 second
    printf("Using wait_time = 1 second\n");    
    bool success = SM->PowerDownCM(commandModule->ID, wait_time);
    if(success) {
      printf("CM %d is powered down\n", commandModule->ID);
    } else {
      printf("CM %d did not power down in time\n", commandModule->ID);
    }
  }catch(BUException::exBase const & e){
    fprintf(stdout,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());
  }catch(std::exception const & e){
    fprintf(stdout,"Caught std::exception: %s\n",e.what());
  }
  
  // Clean up 
  printf("Deleting CM\n");
  if(NULL != commandModule) {
    delete commandModule;
  }
  
  printf("Deleting ApolloSM\n");
  if(NULL != SM) {
    delete SM;
  }
  
  return 0;
}
