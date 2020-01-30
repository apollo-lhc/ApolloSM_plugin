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
  
  class ApolloSMDevice: public CommandList<ApolloSMDevice>, private IPBusRegHelper{
  public:
    ApolloSMDevice(std::vector<std::string> arg); 
    ~ApolloSMDevice();




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
    
    CommandReturn::status OpenFile(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status CloseFile(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status MrWuRegisterDump(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status StatusDisplay(std::vector<std::string>,std::vector<uint64_t>);

    CommandReturn::status svfplayer(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status UART_Term(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status UART_CMD(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status GenerateHTMLStatus(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status unblockAXI(std::vector<std::string>,std::vector<uint64_t>);
    
    //Add new command (sub command) auto-complete files here
    std::string autoComplete_Help(std::vector<std::string> const &,std::string const &,int);


    //Command Module
    CommandReturn::status CMPowerUP(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status CMPowerDown(std::vector<std::string>,std::vector<uint64_t>);

    CommandReturn::status DumpDebug(std::vector<std::string>,std::vector<uint64_t>);

  };
  RegisterDevice(ApolloSMDevice,
		 "ApolloSM",
		 "file/SM_SN",
		 "a",
		 "ApolloSM",
		 "\"connection file\"  or \"SM_SN\""
		 ); //Register ApolloSMDevice with the DeviceFactory  
}

#endif
