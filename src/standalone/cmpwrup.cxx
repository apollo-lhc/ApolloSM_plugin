#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <standalone/CM.hh>
#include <vector>
#include <string>

// ================================================================================
// Setup for boost program_options
#include <boost/program_options.hpp>
#include <standalone/progOpt.hh>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/cmpwrup"
#define DEFAULT_CONN_FILE "/opt/address_table/connections.xml"
#define DEFAULT_CM_ID 1
#define DEFAULT_CM_POWER_UP true
namespace po = boost::program_options;

// ================================================================================
int main(int argc, char** argv) { 

  //=======================================================================
  // Set up program options
  //=======================================================================
  //Command Line options
  po::options_description cli_options("cmpwrup options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("CONN_FILE,C",     po::value<std::string>()->implicit_value(""), "Path to the default connection file")
    ("CM_ID,c",         po::value<int>()->implicit_value(0),          "Default CM to power down")
    ("CM_POWER_UP,p",   po::value<bool>()->implicit_value(true),      "Default power up variable");  //note DEFAULT_CM_POWER_UP option does nothing currenty
 
  //Config File options
  po::options_description cfg_options("cmpwrup options");
  cfg_options.add_options()
    ("CONN_FILE",     po::value<std::string>(), "Path to the default connection file")
    ("CM_ID",         po::value<int>(),         "Default CM to power down")
    ("CM_POWER_UP",   po::value<bool>(),        "Default power up variable"); 

  //variable_maps for holding program options
  po::variables_map cli_map;
  po::variables_map cfg_map;

  //Store command line and config file arguments into cli_map and cfg_map
  try {
    cli_map = storeCliArguments(cli_options, argc, argv);
    cfg_map = storeCfgArguments(cfg_options, DEFAULT_CONFIG_FILE);
  } catch (std::exception &e) {
    std::cout << cli_options << std::endl;
    return 0;
  }
  
  //Help option - ends program
  if(cli_map.count("help")){
    std::cout << cli_options << '\n';
    return 0;
  }
  
  //Set connection file
  std::string connectionFile = DEFAULT_CONN_FILE;
  setOptionValue(connectionFile, "CONN_FILE", cli_map, cfg_map);
  //Set CM_ID
  int CM_ID = DEFAULT_CM_ID;
  setOptionValue(CM_ID, "CM_ID", cli_map, cfg_map);
  //Set powerGood
  std::string powerGood;
  if (CM_ID != 0) {
    std::string num = boost::lexical_cast<std::string>(CM_ID);
    powerGood = "CM.CM_" + num + ".CTRL.PWR_GOOD";
  } else {
    powerGood = " ";
  }
  //Set powerUp
  bool powerUp = DEFAULT_CM_POWER_UP;
  setOptionValue(powerUp, "CM_POWER_UP", cli_map, cfg_map);

  //=======================================================================
  // Power up Command Module
  //=======================================================================
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
