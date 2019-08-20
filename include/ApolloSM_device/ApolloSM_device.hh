#ifndef __APOLLO_SM_DEVICE_HPP__
#define __APOLLO_SM_DEVICE_HPP__

//For tool device base class
#include <BUTool/CommandList.hh>

#include <BUTool/DeviceFactory.hh>

#include <string>
#include <vector>

#include <uhal/uhal.hpp>

#include <iostream>
#include <fstream>

#include <IPBusRegHelper/IPBusRegHelper.hh>
#include <ApolloSM/ApolloSM.hh>

namespace BUTool{
  
  class APOLLOSMDevice: public CommandList<APOLLOSMDevice>, private IPBusRegHelper{
  public:
    APOLLOSMDevice(std::vector<std::string> arg); 
    ~APOLLOSMDevice();




  private:
    ApolloSM * SM;
    
    std::ofstream* stream;
    std::string fileName;

    std::string Show();
    std::ofstream& getStream();
    bool isFileOpen() { return stream != NULL;}
    void setStream(const char* file);
    void closeStream();
    std::string getFileName() { return fileName; }





    //Here is where you update the map between string and function
    void LoadCommandList();

    //Add new command functions here
    
    //   CommandReturn::status ListNodes(std::vector<std::string>,std::vector<uint64_t>);	   

    CommandReturn::status OpenFile(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status CloseFile(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status MrWuRegisterDump(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status ExampleCommand(std::vector<std::string>,std::vector<uint64_t>);

    //Add new command (sub command) auto-complete files here
    std::string autoComplete_Help(std::vector<std::string> const &,std::string const &,int);


  };
  RegisterDevice(APOLLOSMDevice,
		 "ApolloSM",
		 "file/SM_SN",
		 "a",
		 "ApolloSM",
		 "\"connection file\"  or \"SM_SN\""
		 ); //Register APOLLOSMDevice with the DeviceFactory  
}

#endif
