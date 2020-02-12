#include "ApolloSM_device/ApolloSM_device.hh"
#include <BUException/ExceptionBase.hh>
#include <boost/regex.hpp>

//For networking constants and structs                                                                          
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> //for inet_ntoa        

#include <ctype.h> //for isdigit

#include <iostream>
#include <iomanip>
#include <ctime>


#include <boost/algorithm/string/predicate.hpp> //for iequals

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

    AddCommand("cmpwrdown",&ApolloSMDevice::CMPowerDown,
	       "Power up a command module\n"\
	       "Usage: \n" \
	       "  cmpwrdown <iCM> <wait(s)>\n");
    
    AddCommand("svfplayer",&ApolloSMDevice::svfplayer,
	       "Converts an SVF file to jtag commands in AXI format\n" \
	       "Usage: \n" \
	       "  svfplayer svf-file XVC-device\n");

    AddCommand("GenerateHTMLStatus",&ApolloSMDevice::GenerateHTMLStatus,
	       "Creates a status table as an html file\n" \
	       "Usage: \n" \
	       "  GenerateHTMLStatus filename <level> <type>\n");
    
    AddCommand("uart_term",&ApolloSMDevice::UART_Term,
	       "The function used for communicating with the command module uart\n"\
	       "Usage: \n"\
	       "  uart_term \n");

    AddCommand("uart_cmd",&ApolloSMDevice::UART_CMD,
	       "Manages the IO for the command module Uart\n"\
	       "Usage: \n"\
	       "  uart_cmd CMD_STRING\n");

    AddCommand("dump_debug",&ApolloSMDevice::DumpDebug,
	       "Dumps all registers to a file for debugging\n"\
	       "Send to D. Gastler\n"\
	       "Usage: \n"\
	       "  dump_debug\n");

    AddCommand("unblockAXI",&ApolloSMDevice::unblockAXI,
	       "Unblocks all four C2CX AXI and AXILITE bits\n"\
	       "Usage: \n"\
	       "  unblockAXI\n");

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

CommandReturn::status ApolloSMDevice::CMPowerDown(std::vector<std::string> /*strArg*/,std::vector<uint64_t> intArg){

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
  bool success = SM->PowerDownCM(CM_ID,wait_time);
  if(success){
    printf("CM %d is powered down\n",CM_ID);
  }else{
    printf("CM %d failed to powered down in time (forced off)\n",CM_ID);
  }
  return CommandReturn::OK;
}


CommandReturn::status ApolloSMDevice::UART_Term(std::vector<std::string> strArg,std::vector<uint64_t>){
  if(1 != strArg.size()) {
    return CommandReturn::BAD_ARGS;
  }

  if(boost::algorithm::iequals(strArg[0],"CM1")) {
    SM->UART_Terminal("/dev/ttyUL1");    
  } else if(boost::algorithm::iequals(strArg[0],"CM2")) {
    SM->UART_Terminal("/dev/ttyUL2");
  } else if(boost::algorithm::iequals(strArg[0],"ESM")) {
    SM->UART_Terminal("/dev/ttyUL3");
  } else {
    return CommandReturn::BAD_ARGS;
  }
  
  return CommandReturn::OK;
}

CommandReturn::status ApolloSMDevice::UART_CMD(std::vector<std::string> strArg,std::vector<uint64_t>){
  // Need at least the base node and one string to send
  if(2 > strArg.size()) {
    return CommandReturn::BAD_ARGS;
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
    return CommandReturn::BAD_ARGS;
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

  return CommandReturn::OK;
} 

CommandReturn::status ApolloSMDevice::svfplayer(std::vector<std::string> strArg, std::vector<uint64_t>) {

  if(2 != strArg.size()) {
    return CommandReturn::BAD_ARGS;
  }

  SM->svfplayer(strArg[0],strArg[1]);
  
  return CommandReturn::OK;
}

CommandReturn::status ApolloSMDevice::GenerateHTMLStatus(std::vector<std::string> strArg, std::vector<uint64_t> level) {
  if (strArg.size() < 1) {
    return CommandReturn::BAD_ARGS;
  }

  std::string strOut;

  //Grab a possible level
  size_t verbosity;
  if (level.size() == 1) {
    verbosity = level[0];
  }else {
    verbosity = 1;
  }

  if (strArg.size() == 1) {
    strOut = SM->GenerateHTMLStatus(strArg[0],verbosity,"HTML");
  }else{
    strOut = SM->GenerateHTMLStatus(strArg[0],verbosity,strArg[1]);
  }

  if (strOut == "ERROR") {
    return CommandReturn::BAD_ARGS;
  }
  
  return CommandReturn::OK;
}

CommandReturn::status ApolloSMDevice::DumpDebug(std::vector<std::string> /*strArg*/,
						std::vector<uint64_t> /*intArg*/){
  std::stringstream outfileName;
  outfileName << "Apollo_debug_dump_";  

  char buffer[128];
  time_t unixTime=time(NULL);
  struct tm * timeinfo = localtime(&unixTime);
  strftime(buffer,128,"%F-%T-%Z",timeinfo);
  outfileName << buffer;

  outfileName << ".dat";
  
  std::ofstream outfile(outfileName.str().c_str(),std::ofstream::out);
  outfile << outfileName.str() << std::endl;
  SM->DebugDump(outfile);
  outfile.close();  
  return CommandReturn::OK;
}

CommandReturn::status ApolloSMDevice::unblockAXI(std::vector<std::string> /*strArg*/,
						std::vector<uint64_t> /*intArg*/){

  SM->unblockAXI();  

  return CommandReturn::OK;						   
}
						 
