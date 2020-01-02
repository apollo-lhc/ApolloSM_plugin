#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <vector>
#include <string>

int main(int, char**) {
  
  ApolloSM * SM = NULL;
  SM = new ApolloSM();
  std::vector<std::string> arg;
  arg.push_back("connection.xml");
  SM->Connect(arg);

  // All SM_INFO registers
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.GIT_VALID"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.GIT_HASH_1"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.GIT_HASH_2"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.GIT_HASH_3"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.GIT_HASH_4"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.GIT_HASH_5"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.BUILD_DATE"));
  printf("0x%08X\n", SM->RegReadRegister("SM_INFO.BUILD_TIME"));
  
  if(NULL != SM) {
    delete SM;
  }
  return 0;
}
