#include <ApolloSM/ApolloSM.hh>
#include <BUTool/ToolException.hh>
#include <ProtocolUIO.hpp>

int ApolloSM::GetSerialNumber(){
  int ret = -1; //default value
  try{
    ret = ReadRegister("SLAVE_I2C.S1.SM.INFO.SN");
  }catch(BUException::BAD_REG_NAME & e2){
    //eat this exception and just return -1;
  }
  return ret;
}
int ApolloSM::GetRevNumber(){
  int ret = -1; //default value
  try{
    ret = ReadRegister("SLAVE_I2C.S1.SM.INFO.RN");
  }catch(BUException::BAD_REG_NAME & e2){
    //eat this exception and just return -1;
  }
  return ret;
};
int ApolloSM::GetShelfID(){
  int ret = -1; //default value
  try{
    ret = ReadRegister("SLAVE_I2C.S1.SM.INFO.SLOT");
  }catch(BUException::BAD_REG_NAME & e2){
    //eat this exception and just return -1;
  }
  return ret;
};
int ApolloSM::GetSlot(){
  int ret = -1; //default value
  try{
    ret = ReadRegister("SLAVE_I2C.S1.SM.INFO.SLOT");
  }catch(BUException::BAD_REG_NAME & e2){
    //eat this exception and just return -1;
  }
  return ret;
};

uint32_t ApolloSM::GetZynqIP(){
  uint32_t ret = 0; //default value
  //Not yet implemented
  return ret;
};
uint32_t ApolloSM::GetIPMCIP(){
  uint32_t ret = 0; //default value
  try{
    ret = ReadRegister("SLAVE_I2C.S8.IPMC_IP");
  }catch(BUException::BAD_REG_NAME & e2){
    //eat this exception and just return -1;
  }
  return ret;
};
