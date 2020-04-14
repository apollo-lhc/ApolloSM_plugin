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
//#define DEFAULT_CONFIG_FILE "/etc/cmpwrup"
//#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_CONNECTION_FILE "/opt/address_tables/connections.xml"
#define DEFAULT_CM_ID           1

// ================================================================================
int main(int argc, char** argv) { 

  int const noArgs         = 1;
  int const cmFound        = 2;
  
  if((noArgs != argc) && (cmFound != argc)) {
    // wrong number args
    printf("Program takes 0 or 1 arguments\n");
    printf("ex: for 1 argument to power down CM2: ./cmpwrdown 2\n");
    printf("Terminating program\n");
    return -1;
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
    std::string connectionFile = DEFAULT_CONNECTION_FILE;
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
    
    switch(argc) {
    case noArgs: 
      {
	commandModule->ID  = DEFAULT_CM_ID;
	printf("No arguments specified. Default: Powering down CM %d\n", commandModule->ID);
	break;
      }
    case cmFound:
      {
	int ID = std::stoi(argv[1]);
	commandModule->ID = ID;
	printf("One argument specified. Powering down CM %d\n", ID);
	break;
      }    
//    default:
//      {   
//	printf("Program takes 0 or 1 arguments\n");
//	printf("ex for 1 argument to power down CM2: ./cmpwrdown 2\n");
//	printf("Terminating program\n");
//	return 0;
//      }    
    }
    
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
