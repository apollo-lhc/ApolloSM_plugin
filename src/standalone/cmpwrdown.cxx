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
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/BUTool"
namespace po = boost::program_options;

// ================================================================================
int main(int argc, char** argv) { 

  int const noArgs         = 1;
  int const cmFound        = 2;
  
  if((noArgs != argc) && (cmFound != argc)) {
    // wrong number args
    printf("Program takes 0 or 1 arguments\n");
    printf("ex: for 1 argument to power down CM_2: ./cmpwrdown 2\n");
    printf("Terminating program\n");
    return -1;
  }

  //Set up program options
  po::options_description options("cmpwrdown options");
  options.add_options()
    ("DEFAULT_CONNECTION_FILE,C", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file")
    ("DEFAULT_CM_ID,c",           po::value<int>()->default_value(1),                                             "Default CM to power down");
  
  //setup for loading program options
  std::ifstream configFile(DEFAULT_CONFIG_FILE);
  po::variables_map progOptions;

  try { //Get options from command line
    po::store(parse_command_line(argc, argv, options), progOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    std::cout << options << std::endl;
    return 0;
  }

  if(configFile) { //If configFile opens, get options from config file
    try{ 
      po::store(parse_config_file(configFile,options,true), progOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
      std::cout << options << std::endl;
      return 0; 
    }
  }

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
    //
    commandModule = new CM();
    if(NULL == commandModule){
      fprintf(stderr, "Failed to create new CM. Terminating program\n");
      exit(EXIT_FAILURE);
    }else{
      fprintf(stdout,"Created new CM\n");      
    }
    
    // ==============================
    // parse command line
    
    /*switch(argc) {
    case noArgs: 
      {
	commandModule->ID  = CM_ID;
	printf("No arguments specified. Default: Powering down CM %d\n", commandModule->ID);
	break;
      }
    case cmFound:
      {
	int ID = std::stoi(argv[1]); */
    commandModule->ID = CM_ID;/*ID;
	printf("One argument specified. Powering down CM %d\n", ID);
	break;
      }     
//    default:
//      {   
//	printf("Program takes 0 or 1 arguments\n");
//	printf("ex for 1 argument to power down CM_2: ./cmpwrdown 2\n");
//	printf("Terminating program\n");
//	return 0;
//      }    
} */
    
    // ==============================
    // power down CM
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
