#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/svfplayer.hh>

int ApolloSM::svfplayer(std::string const & svfFile, std::string const & XVCLabel,bool displayProgress) {

  SVFPlayer SVF;
  if(displayProgress){
    SVF.EnableDisplayProgress();
  }
  //std::string lock = "PL_MEM.XVC_LOCK."+XVCLabel;
  std::string lock = XVCLabel+".LOCK";
  WriteRegister(lock,1);
  sleep(1);
  //Make sure any previous JTAG commands finished
  while(ReadRegister(XVCLabel+".BUSY")){}
  //Make sure we are in reset
  //send 32 TMS '1's
  WriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  WriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  WriteRegister(XVCLabel+".LENGTH",32);
  WriteAction(XVCLabel+".GO");//,1);
  while(ReadRegister(XVCLabel+".BUSY")){}
  //send 32 TMS '1's
  WriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  WriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  WriteRegister(XVCLabel+".LENGTH",32);
  WriteAction(XVCLabel+".GO");//,1);
  while(ReadRegister(XVCLabel+".BUSY")){}

  //uint32_t in 32bit words
  uint32_t offset = GetRegAddress(XVCLabel) - GetRegAddress(XVCLabel.substr(0,XVCLabel.find('.')));
  int rc = SVF.play(svfFile, XVCLabel.substr(0,XVCLabel.find('.')), offset);

  //Make sure we are in reset
  //send 32 TMS '1's
  WriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  WriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  WriteRegister(XVCLabel+".LENGTH",32);
  WriteAction(XVCLabel+".GO");//,1);
  while(ReadRegister(XVCLabel+".BUSY")){}
  //send 32 TMS '1's
  WriteRegister(XVCLabel+".TDI_VECTOR",0x0);
  WriteRegister(XVCLabel+".TMS_VECTOR",0xFFFFFFFF);
  WriteRegister(XVCLabel+".LENGTH",32);
  WriteAction(XVCLabel+".GO");//,1);
  while(ReadRegister(XVCLabel+".BUSY")){}

  WriteRegister(lock,0);
  return rc;
} 
