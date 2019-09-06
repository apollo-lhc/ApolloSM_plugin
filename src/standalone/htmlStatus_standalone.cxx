#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <vector>
#include <string>

int main(int, char**) {

  //Create ApolloSM class & connect
  ApolloSM * SM = NULL;
  SM = new ApolloSM();
  std::vector<std::string> arg;
  arg.push_back("connections.xml");
  SM->Connect(arg);

  SM->GenerateHTMLStatus(1,"testfile");
  
  //Close ApolloSM class and END
  if(NULL != SM) {
    delete SM;
  }
  return 0;
}
