#include "ApolloSM_device/ApolloSM_device.hh"
#include <BUException/ExceptionBase.hh>
#include <boost/regex.hpp>

//For networking constants and structs                                                                          
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> //for inet_ntoa        

#include <ctype.h> //for isdigit

using namespace BUTool;

ApolloSMDevice::ApolloSMDevice(std::vector<std::string> arg)
  : CommandList<ApolloSMDevice>("ApolloSM"),
    IPBusRegHelper(),
    stream(NULL){
  
  SM = new ApolloSM();
  SM->Connect(arg);
  SetHWInterface(SM->GetHWInterface()); //Pass the inherited version of IPBusIO inside of IPBusREgHelper a pointer to the real hw interface
  
  //setup commands
  LoadCommandList();
}

ApolloSMDevice::~ApolloSMDevice(){
  if(NULL != SM){
    delete SM;
  }
}

  
void ApolloSMDevice::LoadCommandList(){
    // general commands (Launcher_commands)
    AddCommand("read",&ApolloSMDevice::Read,
	       "Read from ApolloSM\n"          \
	       "Usage: \n"                     \
	       "  read addr <count> <FLAGS>\n" \
	       "Flags: \n"                     \
	       "  D:  64bit words\n"           \
	       "  N:  suppress zero words\n",
	       &ApolloSMDevice::RegisterAutoComplete);
    AddCommandAlias("r","read");

    AddCommand("readFIFO",&ApolloSMDevice::ReadFIFO,
	       "Read from a FIFO\n"      \
	       "Usage: \n"               \
	       "  readFIFO addr count\n",
	       &ApolloSMDevice::RegisterAutoComplete);
    AddCommandAlias("rf","readFIFO");

    AddCommand("readoffset",&ApolloSMDevice::ReadOffset,
	       "Read from an offset to an address\n" \
	       "Usage: \n"                           \
	       "  readoffset addr offset <count>\n",
	       &ApolloSMDevice::RegisterAutoComplete);
    AddCommandAlias("ro","readoffset");



    AddCommand("write",&ApolloSMDevice::Write,
	       "Write to ApolloSM\n"           \
	       "Usage: \n"                     \
	       "  write addr <data> <count> \n",
	       &ApolloSMDevice::RegisterAutoComplete);
    AddCommandAlias("w","write");

    AddCommand("writeFIFO",&ApolloSMDevice::WriteFIFO,
	       "Write to ApolloSM FIFO\n"      \
	       "Usage: \n"                     \
	       "  writeFIFO addr data count\n",
	       &ApolloSMDevice::RegisterAutoComplete);
    AddCommandAlias("wf","writeFIFO");

    AddCommand("writeoffset",&ApolloSMDevice::WriteOffset,
	       "Write from an offset to an address\n"   \
	       "Usage: \n"                              \
	       "  writeoffset addr offset data count\n",
	       &ApolloSMDevice::RegisterAutoComplete);
    AddCommandAlias("wo","writeoffset");


    AddCommand("nodes", &ApolloSMDevice::ListRegs, 
	       "List matching address table items\n",
	       &ApolloSMDevice::RegisterAutoComplete);

    AddCommand("status",&ApolloSMDevice::StatusDisplay,
	       "Display tables of Apollo Status\n"  \
	       "Usage: \n"                          \
	       "  status level <table name>\n");
    
    AddCommand("cmpwrup",&ApolloSMDevice::CMPowerUP,
	       "Power up a command module\n"\
	       "Usage: \n" \
	       "  cmpwrup <iCM> <wait(s)>\n");

    AddCommand("uart_term",&ApolloSMDevice::UART_Term,
	       "The function used for communicating with the command module uart\n"\
	       "Usage: \n"\
	       "  uart_term \n");

    AddCommand("uart_cmd",&ApolloSMDevice::UART_CMD,
	       "Manages the IO for the command module Uart\n"\
	       "Usage: \n"\
	       "  uart_cmd CMD_STRING\n");
}

//If there is a file currently open, it closes it                                                             
void ApolloSMDevice::setStream(const char* file) {
  if (stream != NULL) {
    stream->close();
    delete stream;
    stream = NULL;
  }
  stream = new std::ofstream;
  stream->open(file);
  fileName = file;
}

//Closes the stream                                                                                           
void ApolloSMDevice::closeStream() {
  if (stream != NULL) {
    stream->close();
    delete stream;
    stream = NULL;
    fileName = "";
  }
}


CommandReturn::status ApolloSMDevice::StatusDisplay(std::vector<std::string> strArg,std::vector<uint64_t> intArg){

  std::string table("");
  int statusLevel = 1;
  switch (strArg.size()) {
    case 0:
      break;
    default: //fallthrough
    case 2:
      table = strArg[1];
      //fallthrough
    case 1:
      if(!isdigit(strArg[0][0])){
	return CommandReturn::BAD_ARGS;
      }else if ((intArg[0] < 1) || (intArg[0] > 9)) {
	return CommandReturn::BAD_ARGS;
      }      
      statusLevel = intArg[0];
      break;
    }
  SM->GenerateStatusDisplay(statusLevel,std::cout,table);
  return CommandReturn::OK;
}


CommandReturn::status ApolloSMDevice::CMPowerUP(std::vector<std::string> /*strArg*/,std::vector<uint64_t> intArg){

  int wait_time = 5; //1 second
  int CM_ID = 1;
  switch (intArg.size()){
  case 2:
    wait_time = intArg[1];
    //fallthrough
  case 1:
    CM_ID = intArg[0];
    break;
  case 0:
    break;
  default:
    return CommandReturn::BAD_ARGS;
    break;
  }
  bool success = SM->PowerUpCM(CM_ID,wait_time);
  if(success){
    printf("CM %d is powered up\n",CM_ID);
  }else{
    printf("CM %d failed to powered up in time\n",CM_ID);
  }
  return CommandReturn::OK;
}


CommandReturn::status ApolloSMDevice::UART_Term(std::vector<std::string>,std::vector<uint64_t>){
  SM->UART_Terminal();
  return CommandReturn::OK;
}

CommandReturn::status ApolloSMDevice::UART_CMD(std::vector<std::string> strArg,std::vector<uint64_t>){
  if(0 == strArg.size()) {
    return CommandReturn::BAD_ARGS;
  }

  //make one string to send
  std::string sendline;
  for(int i = 0; i < (int)strArg.size(); i++) {
    sendline.append(strArg[i]);
    sendline.push_back(' ');
  }
  //get rid of last space
  sendline.pop_back();
  sendline.push_back('\n');

  printf("Recieved:\n\n%s\n\n", (SM->UART_CMD(sendline)).c_str());

  return CommandReturn::OK;;
} 
