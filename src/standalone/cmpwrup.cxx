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

#define DEFAULT_CONNECTION_FILE "/opt/address_table/connections.xml"
#define DEFAULT_CM_ID 1

  //#For cmpwrup.cxx
#define DEFAULT_CM_POWER_GOOD "CM.CM_1.CTRL.PWR_GOOD"
#define DEFAULT_CM_POWER_UP true






namespace po = boost::program_options;
// ================================================================================
int main(int argc, char** argv) { 
  
  int const noArgs         = 1;
  int const cmFound        = 2;
  int const cmAndPowerGood = 3;
  

  if((noArgs != argc) && (cmFound != argc) && (cmAndPowerGood != argc)) {
    // wrong number args
    printf("Program takes 0, 1, or 2 arguments\n");
    printf("ex: for 1 argument to power up CM_2: ./cmpwrup 2\n");
    printf("ex: for 2 arguments to power up CM_2: ./cmpwrup 2 CM.CM_2.CTRL.PWR_GOOD\n");
    //printf("Terminating program\n");
    return -1;
  }

    //Set up program options
  po::options_description options("cmpwrup options");
  options.add_options()
    ("DEFAULT_CONNECTION_FILE,c", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file")
    ("DEFAULT_CM_ID,C",           po::value<int>()->default_value(1),                                            "Default CM to power down")
    ("DEFAULT_CM_POWER_GOOD,g",   po::value<std::string>()->default_value("CM.CM_1.CTRL.PWR_GOOD"),              "Default register for PWR_GOOD")
    ("DEFAULT_CM_POWER_UP,p",     po::value<bool>()->default_value(true),                                        "Default power up variable"); 
  //note DEFAULT_CM_POWER_UP option does nothing currenty

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



  
  // Make an ApolloSM and CM
  ApolloSM * SM      = NULL;
  CM * commandModule = NULL;
  try{
    SM = new ApolloSM();
    if(NULL == SM){
      fprintf(stderr, "Failed to create new ApolloSM. Terminating program\n");
      exit(EXIT_FAILURE);
    }//else{
    //  fprintf(stdout,"Created new ApolloSM\n");      
    //}
    // load connection file
    std::vector<std::string> arg;
    std::string connectionFile = DEFAULT_CONNECTION_FILE;
    printf("Using %s\n", connectionFile.c_str());
    arg.push_back(connectionFile);
    SM->Connect(arg);
    
    // ==============================
    // Make a command module
    //CM * commandModule = NULL;
    commandModule = new CM();
    if(NULL == commandModule){
      fprintf(stderr, "Failed to create new CM. Terminating program\n");
      exit(EXIT_FAILURE);
    }//else{
    //  fprintf(stdout,"Created new CM\n");      
    //}
    
    // ==============================
    // parse command line
    
    switch(argc) {
    case noArgs: 
      {
	commandModule->ID        = DEFAULT_CM_ID;
	commandModule->powerGood = DEFAULT_CM_POWER_GOOD;
	commandModule->powerUp   = DEFAULT_CM_POWER_UP;
	//printf("No arguments specified. Default: Powering up CM %d and checking %s\n", commandModule->ID, commandModule->powerGood.c_str());
	break;
      }
    case cmFound:
      {
	int ID                = std::stoi(argv[1]);
	std::string powerGood = "CM.CM_" + std::to_string(ID) + ".CTRL.PWR_GOOD";
	commandModule->ID        = ID;
	commandModule->powerGood = powerGood;
	commandModule->powerUp   = true;
	//printf("One argument specified. Powering up CM %d and checking %s\n", ID, powerGood.c_str());
	break;
      }    
    case cmAndPowerGood:
      {      
	int ID                = std::stoi(argv[1]);
	std::string powerGood = argv[2];
	commandModule->ID        = ID;
	commandModule->powerGood = powerGood;
	commandModule->powerUp   = true;
	//printf("Two arguments specified. Powering up CM %d with %s\n", ID, powerGood.c_str());
	break;
      }   
//    default:
//      {   
//	printf("Program takes 0, 1, or 2 arguments\n");
//	printf("ex for 1 argument to power up CM_2: ./cmpwrup 2\n");
//	printf("ex for 2 arguments to power up CM_2: ./cmpwrup 2 CM.CM_2.CTRL.PWR_GOOD\n");
//	printf("Terminating program\n");
//	return 0;
//      }    
    }
    
    // ==============================
    // power up CM
    int wait_time = 5; // 1 second
    //printf("Using wait_time = 1 second\n");    
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
  //printf("Deleting CM\n");
  if(NULL != commandModule) {
    delete commandModule;
  }
  
  //printf("Deleting ApolloSM\n");
  if(NULL != SM) {
    delete SM;
  }

  return 0;
}
