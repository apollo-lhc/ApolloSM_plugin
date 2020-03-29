#include <ApolloSM/ApolloSM.hh>
#include <fstream> //std::ofstream

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


std::string ApolloSM::GenerateHTMLStatus(std::string filename, size_t level = size_t(1), std::string type = std::string("HTML")) {

  //SETUP
  std::ofstream HTML;
  HTML.open(filename);
  if(!HTML.is_open()) {
    fprintf(stderr, "Failed to open file\n");
    return "ERROR";
  }
  std::string BareReport; //For ReportBare

  //Setting Status Display
  if (type == "HTML") {statusDisplay->SetHTML();}
  else if(type == "Bare") {}
  else {
    fprintf(stderr, "ERROR: invalid HTML type\n");
    fprintf(stderr, "Valid HTML types are; HTML, Bare, or "" for HTML\n");
    return "ERROR";
  }

  //Get report
  if (type == "HTML") {statusDisplay->Report(level, HTML, "");}
  else {
    BareReport = statusDisplay->ReportBare(level, "");
    HTML.write(BareReport.c_str(),BareReport.size());
    HTML.close();
  }

  //END
  HTML.close();
  statusDisplay->UnsetHTML();
  return "GOOD";
}

bool ApolloSM::PowerUpCM(int CM_ID, int wait /*seconds*/){
  const uint32_t RUNNING_STATE = 3;
  if((CM_ID < 1) || (CM_ID > 2)){
    BUException::APOLLO_SM_BAD_VALUE e;
    e.Append("Bad CM_ID");
    throw e;
  }
  std::string CM_CTRL("CM.CM");
  switch (CM_ID){
  case 2:
    CM_CTRL+="2";
    break;
  default:
    CM_CTRL+="1";
  }
  CM_CTRL+=".CTRL.";

  //Check that the uC is powered up, power up if needed
  if(!RegReadRegister(CM_CTRL+"ENABLE_UC")){
    RegWriteRegister(CM_CTRL+"ENABLE_UC",1);
  }
  //Power up the CM 
  RegWriteRegister(CM_CTRL+"ENABLE_PWR",1);
  usleep(10000); //Wait 10ms
  
  wait*=1000000; //convert wait time to us from s
  do{
    if(RegReadRegister(CM_CTRL+"STATE") == RUNNING_STATE){
      return true;
     }
    int dt = 10000;//10ms
    usleep(dt);
    wait-=dt;
  }while(wait >= 0);

  RegWriteRegister(CM_CTRL+"ENABLE_PWR",0);
  
  return false;
}


bool ApolloSM::PowerDownCM(int CM_ID, int wait /*seconds*/){
  const uint32_t PWR_DOWN_STATE = 4;
  const uint32_t RESET_STATE    = 1;
  if((CM_ID < 1) || (CM_ID > 2)){
    BUException::APOLLO_SM_BAD_VALUE e;
    e.Append("Bad CM_ID");
    throw e;
  }
  std::string CM_CTRL("CM.CM");
  switch (CM_ID){
  case 2:
    CM_CTRL+="2";
    break;
  default:
    CM_CTRL+="1";
  }
  CM_CTRL+=".CTRL.";

  RegWriteRegister(CM_CTRL+"ENABLE_PWR",0);
  usleep(10000); //Wait 10ms
  
  wait*=1000000; //convert wait time to us from s
  do{
    if( RegReadRegister(CM_CTRL+"STATE") == RESET_STATE ){
      //PWR GOOD went off
      break;
    }
    int dt = 10000;//10ms
    usleep(dt);
    wait-=dt;
  }while(wait >= 0);  

  uint32_t state = RegReadRegister(CM_CTRL+"STATE");

  if(PWR_DOWN_STATE == state){
    //We just shut off the uC before power good went down.  
    //We gave up
    return false;
  }
  //everything is good
  return true;
}

void ApolloSM::unblockAXI() {
  RegWriteAction("C2C1_AXI_FW.UNBLOCK");
  RegWriteAction("C2C1_AXILITE_FW.UNBLOCK");
  RegWriteAction("C2C2_AXI_FW.UNBLOCK");
  RegWriteAction("C2C2_AXILITE_FW.UNBLOCK");  
  return;
}
