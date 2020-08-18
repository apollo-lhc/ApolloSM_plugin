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
#define DEFAULT_CONFIG_FILE "/etc/cmpwrup"
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
  
  //Set up program options
  po::options_description options("cmpwrup options");
  options.add_options()
    ("help,h",    "Help screen")
    ("CONNECTION_FILE,C", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file")
    ("CM_ID,c",           po::value<int>()->default_value(1),                                            "Default CM to power down")
    ("CM_POWER_GOOD,g",   po::value<std::string>()->default_value("CM.CM_1.CTRL.PWR_GOOD"),              "Default register for PWR_GOOD")
    ("CM_POWER_UP,p",     po::value<bool>()->default_value(true),                                        "Default power up variable"); 
  //note DEFAULT_CM_POWER_UP option does nothing currenty

  //setup for loading program options
  po::variables_map progOptions = getVariableMap(argc, argv, options, DEFAULT_CONFIG_FILE);

  //help option
  if(progOptions.count("help")){
    std::cout << options << '\n';
    return 0;
  }
  
  //Set connection file
  std::string connectionFile = "";
  if (progOptions.count("CONNECTION_FILE")) {connectionFile = progOptions["CONNECTION_FILE"].as<std::string>();}
  //Set CM_ID
  int CM_ID = 0;
  if (progOptions.count("CM_ID")) {CM_ID = progOptions["CM_ID"].as<int>();}
  //Set powerGood
  std::string powerGood = "";
  if (progOptions.count("CM_POWER_GOOD")) {powerGood = progOptions["CM_POWER_GOOD"].as<std::string>();}
  //Set powerUp
  bool powerUp = "";
  if (progOptions.count("CM_POWER_UP")) {powerUp = progOptions["CM_POWER_UP"].as<bool>();}

  // Make an ApolloSM and CM
  ApolloSM * SM      = NULL;
  CM * commandModule = NULL;
  try{
    SM = new ApolloSM();
    if(NULL == SM){
      fprintf(stderr, "Failed to create new ApolloSM. Terminating program\n");
      exit(EXIT_FAILURE);
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
    }
    
    // ==============================
    // power up CM
    commandModule->ID        = CM_ID;
    commandModule->powerGood = powerGood;
    commandModule->powerUp   = powerUp;

    int wait_time = 5; // 1 second
    bool success = SM->PowerUpCM(commandModule->ID, wait_time);
    if(success) {
      printf("CM %d is powered up\n", commandModule->ID);
    } else {
      printf("CM %d did not power up in %d second(s)\n", commandModule->ID, (wait_time / 5));
    }

    // read power good and print
    printf("%s is %d\n", commandModule->powerGood.c_str(), (int)(SM->RegReadRegister(commandModule->powerGood)));

  }catch(BUException::exBase const & e){
    fprintf(stdout,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());
  }catch(std::exception const & e){
    fprintf(stdout,"Caught std::exception: %s\n",e.what());
  }
  
  // Clean up
  if(NULL != commandModule) {
    delete commandModule;
  }
  
  if(NULL != SM) {
    delete SM;
  }

  return 0;
}
