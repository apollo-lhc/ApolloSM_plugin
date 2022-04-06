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
#include <ApolloSM/eyescan_class.hh>

namespace BUTool{

  //This holder class is used to force the ApolloSM class that would normally
  //be in the ApolloSMDevice class to be initialized before the IPBusRegHelper
  //so that it can be past to the IPBusRegHelper's constructor. 
  class ApolloSMHolder{
  public:
    ApolloSMHolder(std::vector<std::string> const & arg){
      SM = std::make_shared<ApolloSM>(arg);
    };
  protected:
    std::shared_ptr<ApolloSM> SM;
  private:
    ApolloSMHolder();
  };
  
  class ApolloSMDevice: public CommandList<ApolloSMDevice>,
			public ApolloSMHolder,
			public IPBusRegHelper{
  public:
    ApolloSMDevice(std::vector<std::string> arg); 
    ~ApolloSMDevice();




  private:
    
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
    CommandReturn::status DumpGraphite(std::vector<std::string>,std::vector<uint64_t>);

    CommandReturn::status svfplayer(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status UART_Term(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status UART_CMD(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status GenerateHTMLStatus(std::vector<std::string>,std::vector<uint64_t>);

    CommandReturn::status unblockAXI(std::vector<std::string>,std::vector<uint64_t>);

    //CommandReturn::status EnableEyeScan(std::vector<std::string>,std::vector<uint64_t>);
    //CommandReturn::status SetOffsets(std::vector<std::string>,std::vector<uint64_t>);
    //CommandReturn::status SingleEyeScan(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status EyeScan(std::vector<std::string> strArg, std::vector<uint64_t>);
    //CommandReturn::status Bathtub(std::vector<std::string>,std::vector<uint64_t>);
    //Add new command (sub command) auto-complete files here
    std::string autoComplete_Help(std::vector<std::string> const &,std::string const &,int);


    //Command Module
    CommandReturn::status CMPowerUP(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status CMPowerDown(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status restartCMuC(std::vector<std::string>,std::vector<uint64_t>);
    CommandReturn::status DumpDebug(std::vector<std::string>,std::vector<uint64_t>);

  };
  RegisterDevice(ApolloSMDevice,
		 "ApolloSM",
		 "file/SM_SN",
		 "a",
		 "ApolloSM",
		 "Connection file for creating an ApolloSM device"
		 ); //Register ApolloSMDevice with the DeviceFactory  
}

#endif
