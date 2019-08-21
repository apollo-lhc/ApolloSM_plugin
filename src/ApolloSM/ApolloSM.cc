#include <ApolloSM/ApolloSM.hh>

ApolloSM::ApolloSM():IPBusConnection("ApolloSM"),statusDisplay(NULL){  
  statusDisplay= new IPBusStatus(GetHWInterface());
}

ApolloSM::~ApolloSM(){
  if(statusDisplay != NULL){
    delete statusDisplay;
  }
}

void ApolloSM::GenerateStatusDisplay(size_t level,
				     std::ostream & stream=std::cout,
				     std::string const & singleTable = std::string("")){
  statusDisplay->Report(level,stream,singleTable);
}

