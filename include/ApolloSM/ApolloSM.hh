#ifndef __APOLLO_SM_HH__
#define __APOLLO_SM_HH__

#include <IPBusIO/IPBusConnection.hh>
#include <IPBusStatus/IPBusStatus.hh>


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
private:  
  IPBusStatus * statusDisplay;
};


#endif
