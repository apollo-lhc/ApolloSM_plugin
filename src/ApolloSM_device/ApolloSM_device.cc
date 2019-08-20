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

APOLLOSMDevice::APOLLOSMDevice(std::vector<std::string> arg)
  : CommandList<APOLLOSMDevice>("AolloSM"),
    IPBusRegHelper(),
    stream(NULL){
  
  SM = new ApolloSM();
  SM->Connect(arg);
  SetHWInterface(SM->GetHWInterface()); //Pass the inherited version of IPBusIO inside of IPBusREgHelper a pointer to the real hw interface
  
  //setup commands
  LoadCommandList();
}

APOLLOSMDevice::~APOLLOSMDevice(){
  if(NULL != SM){
    delete SM;
  }
}

  
void APOLLOSMDevice::LoadCommandList(){
    // general commands (Launcher_commands)
    AddCommand("read",&APOLLOSMDevice::Read,
	       "Read from ApolloSM\n"          \
	       "Usage: \n"                     \
	       "  read addr <count> <FLAGS>\n" \
	       "Flags: \n"                     \
	       "  D:  64bit words\n"           \
	       "  N:  suppress zero words\n",
	       &APOLLOSMDevice::RegisterAutoComplete);
    AddCommandAlias("r","read");

    AddCommand("readFIFO",&APOLLOSMDevice::ReadFIFO,
	       "Read from a FIFO\n"      \
	       "Usage: \n"               \
	       "  readFIFO addr count\n",
	       &APOLLOSMDevice::RegisterAutoComplete);
    AddCommandAlias("rf","readFIFO");

    AddCommand("readoffset",&APOLLOSMDevice::ReadOffset,
	       "Read from an offset to an address\n" \
	       "Usage: \n"                           \
	       "  readoffset addr offset <count>\n",
	       &APOLLOSMDevice::RegisterAutoComplete);
    AddCommandAlias("ro","readoffset");



    AddCommand("write",&APOLLOSMDevice::Write,
	       "Write to ApolloSM\n"           \
	       "Usage: \n"                     \
	       "  write addr <data> <count> \n",
	       &APOLLOSMDevice::RegisterAutoComplete);
    AddCommandAlias("w","write");

    AddCommand("writeFIFO",&APOLLOSMDevice::WriteFIFO,
	       "Write to ApolloSM FIFO\n"      \
	       "Usage: \n"                     \
	       "  writeFIFO addr data count\n",
	       &APOLLOSMDevice::RegisterAutoComplete);
    AddCommandAlias("wf","writeFIFO");

    AddCommand("writeoffset",&APOLLOSMDevice::WriteOffset,
	       "Write from an offset to an address\n"   \
	       "Usage: \n"                              \
	       "  writeoffset addr offset data count\n",
	       &APOLLOSMDevice::RegisterAutoComplete);
    AddCommandAlias("wo","writeoffset");


    AddCommand("nodes", &APOLLOSMDevice::ListRegs, 
	       "List matching address table items\n",
	       &APOLLOSMDevice::RegisterAutoComplete);

    AddCommand("ExampleCommand",&APOLLOSMDevice::ExampleCommand,
	       "An example of a command");

}



//If there is a file currently open, it closes it                                                             
void APOLLOSMDevice::setStream(const char* file) {
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
void APOLLOSMDevice::closeStream() {
  if (stream != NULL) {
    stream->close();
    delete stream;
    stream = NULL;
    fileName = "";
  }
}



CommandReturn::status APOLLOSMDevice::ExampleCommand(std::vector<std::string> strArg,std::vector<uint64_t> intArg){
  if(1 != strArg.size()){
    //This command takes exactly one argument
    return CommandReturn::BAD_ARGS;
  }

  if(!isdigit(strArg[0][0])){
    //The argument must be a number
    return CommandReturn::BAD_ARGS;
  }

  uint32_t foo = SM->ExampleFunction(uint32_t(intArg[0]));
  printf("Got: 0x%08X\n",foo);
  return CommandReturn::OK;
}
