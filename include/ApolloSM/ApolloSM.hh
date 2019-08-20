#ifndef __APOLLO_SM_HH__
#define __APOLLO_SM_HH__

#include <IPBusIO/IPBusConnection.hh>

#include <stdint.h>

class ApolloSM : public IPBusConnection{
public:
  ApolloSM(); //User should call Connect inhereted from IPBusConnection

  //The IPBus connection and read/write functions come from the IPBusConnection class.
  //Look there for the details. 

  uint32_t ExampleFunction(uint32_t number);
private:  

};


#endif
