#ifndef __APOLLO_SM_HH__
#define __APOLLO_SM_HH__

#include <IPBusIO/IPBusConnection.hh>
#include <IPBusStatus/IPBusStatus.hh>
#include <BUException/ExceptionBase.hh>

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

  void UART_Terminal(std::string baseNode);

  std::string UART_CMD(std::string baseNode, std::string sendline);

  bool PowerUpCM(int CM_ID,int wait = -1);

private:  
  IPBusStatus * statusDisplay;
};


#endif
