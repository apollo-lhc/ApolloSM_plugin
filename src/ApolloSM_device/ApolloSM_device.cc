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
	       "Usave: \n"                          \
	       "  status level <table name>\n");

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
  switch (strArg.size()) {
    case 0:
      return CommandReturn::BAD_ARGS;
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
      break;
    }
  SM->GenerateStatusDisplay(intArg[0],std::cout,table);
  return CommandReturn::OK;
}
