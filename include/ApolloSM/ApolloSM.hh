#ifndef __APOLLO_SM_HH__
#define __APOLLO_SM_HH__

#include <IPBusIO/IPBusConnection.hh>
#include <IPBusStatus/IPBusStatus.hh>
#include <BUException/ExceptionBase.hh>
#include <ApolloSM/eyescan.hh>

#include <iostream>

namespace BUException{
  ExceptionClassGenerator(APOLLO_SM_BAD_VALUE,"Bad value use in Apollo SM code\n");
}

#include <stdint.h>

class ApolloSM : public IPBusConnection{
public:
  ApolloSM(); //User should call Connect inhereted from IPBusConnection
  ~ApolloSM();

  //The IPBus connection and read/write functions come from the IPBusConnection class.
  //Look there for the details. 
  void GenerateStatusDisplay(size_t level,
			     std::ostream & stream,
			     std::string const & singleTable);

  std::string GenerateHTMLStatus(std::string filename, size_t level, std::string);
  
  void UART_Terminal(std::string const & ttyDev);

  std::string UART_CMD(std::string const & ttyDev, std::string sendline, char const promptChar = '%');

  int svfplayer(std::string const & svfFile, std::string const & XVCReg);
  
  bool PowerUpCM(int CM_ID,int wait = -1);
  bool PowerDownCM(int CM_ID,int wait = -1);

  void DebugDump(std::ostream & output = std::cout);

  //  void throwException(std::string message);
  
  float SingleEyeScan(std::string baseNode);
  void EnableEyeScan(std::string baseNode, uint32_t prescale);
  std::vector<eyescanCoords> EyeScan(std::string baseNode, float maxVoltage, float maxPhase, uint16_t prescale);
  

private:  
  IPBusStatus * statusDisplay;

  void assertNode(std::string node, uint32_t correctVal);
  void confirmNode(std::string node, uint32_t correctVal);

};


#endif
