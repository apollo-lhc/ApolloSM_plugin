#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/svfplayer.hh>

int ApolloSM::svfplayer(std::string const & svfFile, std::string const & XVCLabel) {

  SVFPlayer SVF;
  std::string lock = "PL_MEM.XVC_LOCK."+XVCLabel;
  RegWriteRegister(lock,1);
  sleep(1);
  //Make sure any previous JTAG commands finished
  while(RegReadRegister(XVCLabel+".GO")){}
  //Make sure we are in reset
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".GO")){}
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".GO")){}

  int rc = SVF.play(svfFile, XVCLabel);

  //Make sure we are in reset
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".GO")){}
  //send 32 TMS '1's
  RegWriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  RegWriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  RegWriteRegister(XVCLabel+".LENGTH",32);
  RegWriteRegister(XVCLabel+".GO",1);
  while(RegReadRegister(XVCLabel+".GO")){}

  RegWriteRegister(lock,0);
  return rc;
} 
