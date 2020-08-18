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
#define DEFAULT_CONFIG_FILE "/etc/cmpwrdown"
#define DEFAULT_CONN_FILE "/opt/address_table/connections.xml"
#define DEFAULT_CM_ID 1
namespace po = boost::program_options;

po::variables_map getVariableMap(int argc, char** argv, po::options_description options, std::string configFile) {
  //container for prog options grabbed from commandline and config file
  po::variables_map progOptions;
  //open config file
  std::ifstream File(configFile);

  //Get options from command line
  try { 
    po::store(po::parse_command_line(argc, argv, options), progOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    std::cout << options << std::endl;
    return 0;
  }

  //If configFile opens, get options from config file
  if(File) { 
    try{ 
      po::store(po::parse_config_file(File,options,true), progOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
      std::cout << options << std::endl;
      return 0; 
    }
  }
 
  return progOptions;
}

// ================================================================================
int main(int argc, char** argv) { 

  //======================
  //Set up program options
  po::options_description cli_options("cmpwrdown options"); //options read from command line
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("CONN_FILE,C", po::value<std::string>()->implicit_value(""), "Path to the default config file")
    ("CM_ID,c",     po::value<int>()->implicit_value(0),          "Default CM to power down");
  
  po::options_description cfg_options("cmpwrdown options"); //options read from config file
  cfg_options.add_options()
    ("CONN_FILE,C", po::value<std::string>(), "Path to the default config file")
    ("CM_ID,c",     po::value<int>(),         "Default CM to power down");
  
  //variable_maps for holding program options
  po::variables_map cli_map;
  po::variables_map cfg_map;
  
  //store command line arguments
  try {
    po::store(po::parse_command_line(argc, argv, cli_options), cli_map);
  } catch (std::exception &e) {
    fprintf(stderr, "ERROR in BOOST parse_command_line: %s\n", e.what());
    std::cout << cli_options << std::endl;
    return 0;
  }
  //store config file arguments
  std::ifstream configFile(DEFAULT_CONFIG_FILE);
  if(configFile){
    try {
      po::store(po::parse_config_file(configFile,cfg_options,true), cfg_map);
    } catch(std::exception &e) {
      fprintf(stderr, "ERROR in BOOST parse_config_file: %s\n", e.what());
      std::cout << cfg_options << std::endl;
      return 0;
    }
  }
  
  //Help option
  if(cli_map.count("help")){
    std::cout << cli_options << '\n';
    return 0;
  }

  std::string connectionFile = DEFAULT_CONN_FILE; //Assign default connection file
  if(cli_map.count("CONN_FILE")) { //if connection file argument used on command line
    std::string cliArg = cli_map["CONN_FILE"].as<std::string>(); //get argument
    if (cliArg == "") { //cli argument is empty 
      if (cfg_map.count("CONN_FILE")) { //if option is in config file
	connectionFile = cfg_map["CONN_FILE"].as<std::string>();
      }    
    } else { //use command line arg
      connectionFile = cliArg;
    }
  }
  
  int CM_ID = DEFAULT_CM_ID; //Assign defaul cm_id
  if(cli_map.count("CM_ID")) { //if connection file argument used on command line
    int cliArg = cli_map["CM_ID"].as<int>(); //get argument
    if (cliArg == 0) { //cli argument is empty
      if (cfg_map.count("CM_ID")) { //if option is in config file
	CM_ID = cfg_map["CM_ID"].as<int>();
      }
    } else { //use command line arg
      CM_ID = cliArg;
    }
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
