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


void ApolloSM::GenerateHTMLStatus(size_t level, std::string filename) {

  //Create file
  std::ofstream HTML;
  HTML.open(filenameHTML);
  //Set statusDisplay to HTML
  statusDisplay->SetHTML();
  statusDisplay->Report(level, HTML, "");
  //Unset HTML
  statusDisplay->UnsetHTML();

}


bool ApolloSM::PowerUpCM(int CM_ID, int wait /*seconds*/){
  const uint32_t RUNNING_STATE = 4;
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

  RegWriteRegister(CM_CTRL+"ENABLE_UC",1);
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
  RegWriteRegister(CM_CTRL+"ENABLE_UC",0);
  
  return false;
}


bool ApolloSM::PowerDownCM(int CM_ID, int wait /*seconds*/){
  const uint32_t PWR_DOWN_STATE = 5;
  const uint32_t STARTUP_STATE  = 2;
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
    if((RegReadRegister(CM_CTRL+"STATE") == STARTUP_STATE) ||
       (RegReadRegister(CM_CTRL+"STATE") == RESET_STATE)){
      //PWR GOOD went off
      break;
    }
    int dt = 10000;//10ms
    usleep(dt);
    wait-=dt;
  }while(wait >= 0);  

  uint32_t state = RegReadRegister(CM_CTRL+"STATE");
  RegWriteRegister(CM_CTRL+"ENABLE_UC",0);      
  

  if(PWR_DOWN_STATE == state){
    //We just shut off the uC before power good went down.  
    //We gave up
    return false;
  }
  //everything is good
  return true;
}

