#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/svfplayer.hh>

int ApolloSM::svfplayer(std::string const & svfFile, std::string const & XVCLabel) {

  SVFPlayer SVF;
  //std::string lock = "PL_MEM.XVC_LOCK."+XVCLabel;
  std::string lock = XVCLabel+".LOCK";
  RegWriteRegister(lock,1);
  sleep(1);
  //Make sure any previous JTAG commands finished
  while(RegReadRegister(XVCLabel+".BUSY")){}
  //Make sure we are in reset
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".BUSY")){}
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".BUSY")){}

  //uint32_t in 32bit words
  uint32_t offset = GetRegAddress(XVCLabel) - GetRegAddress(XVCLabel.substr(0,XVCLabel.find('.')));
  int rc = SVF.play(svfFile, XVCLabel.substr(0,XVCLabel.find('.')), offset);

  //Make sure we are in reset
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".BUSY")){}
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".BUSY")){}

  RegWriteRegister(lock,0);
  return rc;
} 
