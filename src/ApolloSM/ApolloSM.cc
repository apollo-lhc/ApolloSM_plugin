#include <ApolloSM/ApolloSM.hh>

ApolloSM::ApolloSM():IPBusConnection("ApolloSM"){  
}

uint32_t ApolloSM::ExampleFunction(uint32_t number){
  //do something here
  uint32_t data = RegReadAddress(number);
  return ~data; //invert bits
}
