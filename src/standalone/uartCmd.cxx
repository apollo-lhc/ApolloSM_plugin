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
#define DEFAULT_CM_POWER_GOOD   "CM.CM1.CTRL.PWR_GOOD"
#define DEFAULT_CM_POWER_UP     true // note: command modules power up variable actually does nothing in this program currently

// ================================================================================
int main(int argc, char** argv) { 

  int const numberGoodArgs = 3;  

  if(argc != numberGoodArgs) {
    printf("Wrong number of arguments. Please include a device and a command\n");
    return -1;
  }
  
  // Make an ApolloSM
  ApolloSM * SM = NULL;
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
    // parse command line
    
    // load argv into this vector
    std::vector<std::string> strArg;
    for(int i = 1; i < numberGoodArgs; i++) {
      strArg.push_back(argv[i]);
    }

    std::string ttyDev;
    char promptChar;
    if(boost::algorithm::iequals(strArg[0],"CM1")) {
      ttyDev.append("/dev/ttyUL1");    
      promptChar = '%';
    } else if(boost::algorithm::iequals(strArg[0],"CM2")) {
      ttyDev.append("/dev/ttyUL2");
      promptChar = '%';
    } else if(boost::algorithm::iequals(strArg[0],"ESM")) {
      ttyDev.append("/dev/ttyUL3");
      promptChar = '>';
    } else {
      printf("Invalid device\n");
      return -1;
    }

  //make one string to send
  std::string sendline;
  for(int i = 1; i < (int)strArg.size(); i++) {
    sendline.append(strArg[i]);
    sendline.push_back(' ');
  }
  //get rid of last space
  sendline.pop_back();

  printf("Recieved:\n\n%s\n\n", (SM->UART_CMD(ttyDev, sendline,promptChar)).c_str());



    switch(argc) {
    case noArgs: 
      {
	commandModule->ID        = DEFAULT_CM_ID;
	commandModule->powerGood = DEFAULT_CM_POWER_GOOD;
	commandModule->powerUp   = DEFAULT_CM_POWER_UP;
	printf("No arguments specified. Default: Powering up CM %d and checking %s\n", commandModule->ID, commandModule->powerGood.c_str());
	break;
      }
    case cmFound:
      {
	int ID                = std::stoi(argv[1]);
	std::string powerGood = "CM.CM" + std::to_string(ID) + ".CTRL.PWR_GOOD";
	commandModule->ID        = ID;
	commandModule->powerGood = powerGood;
	commandModule->powerUp   = true;
	printf("One argument specified. Powering up CM %d and checking %s\n", ID, powerGood.c_str());
	break;
      }    
    case cmAndPowerGood:
      {      
	int ID                = std::stoi(argv[1]);
	std::string powerGood = argv[2];
	commandModule->ID        = ID;
	commandModule->powerGood = powerGood;
	commandModule->powerUp   = true;
	printf("Two arguments specified. Powering up CM %d with %s\n", ID, powerGood.c_str());
	break;
      }   
//    default:
//      {   
//	printf("Program takes 0, 1, or 2 arguments\n");
//	printf("ex for 1 argument to power up CM2: ./cmpwrup 2\n");
//	printf("ex for 2 arguments to power up CM2: ./cmpwrup 2 CM.CM2.CTRL.PWR_GOOD\n");
//	printf("Terminating program\n");
//	return 0;
//      }    
    }
    
    // ==============================
    // power up CM
    int wait_time = 5; // 1 second
    printf("Using wait_time = 1 second\n");    
    bool success = SM->PowerUpCM(commandModule->ID, wait_time);
    if(success) {
      printf("CM %d is powered up\n", commandModule->ID);
    } else {
      printf("CM %d did not power up in time\n", commandModule->ID);
    }
  
    // read power good and print
    printf("%s is %d\n", commandModule->powerGood.c_str(), (int)(SM->RegReadRegister(commandModule->powerGood)));

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
