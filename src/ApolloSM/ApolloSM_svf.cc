#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/svfplayer.hh>

int ApolloSM::svfplayer(std::string const & svfFile, std::string const & XVCLabel,bool displayProgress) {


  SVFPlayer SVF;
  if(displayProgress){
    SVF.EnableDisplayProgress();
  }
  //std::string lock = "PL_MEM.XVC_LOCK."+XVCLabel;
  std::string regLOCK = XVCLabel+".LOCK";
  std::string regBUSY = XVCLabel+".BUSY";
  std::string regTDI = XVCLabel+".TDI_VECTOR";
  std::string regTMS = XVCLabel+".TMS_VECTOR";
  std::string regLENGTH = XVCLabel+".LENGTH";
  std::string regGO = XVCLabel+".GO";
  


  WriteRegister(regLOCK,1);
  //Make sure any previous JTAG commands finished
  while(ReadRegister(regBUSY)){}
  //Make sure we are in reset
  //send 32 TMS '1's
  WriteRegister(regTDI,0x0);
  WriteRegister(regTMS,0xFFFFFFFF);
  WriteRegister(regLENGTH,32);
  WriteAction(regGO);//,1);
  while(ReadRegister(regBUSY)){}
  //send 32 TMS '1's
  WriteRegister(regTDI,0x0);
  WriteRegister(regTMS,0xFFFFFFFF);
  WriteRegister(regLENGTH,32);
  WriteAction(regGO);//,1);
  while(ReadRegister(regBUSY)){}

  //uint32_t in 32bit words
  uint32_t offset = GetRegAddress(XVCLabel) - GetRegAddress(XVCLabel.substr(0,XVCLabel.find('.')));
  int rc = SVF.play(svfFile, XVCLabel.substr(0,XVCLabel.find('.')), offset);

  //Make sure we are in reset
  //send 32 TMS '1's
  WriteRegister(regTDI,0x0);
  WriteRegister(regTMS,0xFFFFFFFF);
  WriteRegister(regLENGTH,32);
  WriteAction(regGO);//,1);
  while(ReadRegister(regBUSY)){}
  //send 32 TMS '1's
  WriteRegister(regTDI,0x0);
  WriteRegister(regTMS,0xFFFFFFFF);
  WriteRegister(regLENGTH,32);
  WriteAction(regGO);//,1);
  while(ReadRegister(regBUSY)){}

  WriteRegister(regLOCK,0);
  return rc;
} 
