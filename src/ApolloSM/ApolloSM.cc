#include <ApolloSM/ApolloSM.hh>
#include <fstream> //std::ofstream

#include <boost/algorithm/string/predicate.hpp> //for iequals

ApolloSM::ApolloSM(std::vector<std::string> const & args):
  IPBusConnection("ApolloSM", args),
  IPBusIO(      ((IPBusConnection*)this)->GetHWInterface()),
  statusDisplay((IPBusIO*)this){
  //Set case sensistive
  SetCase(RegisterHelperIO::RegisterNameCase::CASE_SENSITIVE);
}

ApolloSM::~ApolloSM(){
}

void ApolloSM::GenerateStatusDisplay(size_t level,
				     std::ostream & stream=std::cout,
				     std::string const & singleTable = std::string("")){
  statusDisplay.Clear();
  statusDisplay.Report(level,stream,singleTable);
  statusDisplay.Clear();
}


std::string ApolloSM::GenerateHTMLStatus(std::string filename,
					 size_t level = size_t(1),
					 std::string type = std::string("HTML")) {
  statusDisplay.Clear();
  //SETUP
  std::ofstream HTML;
  HTML.open(filename);
  if(!HTML.is_open()) {
    fprintf(stderr, "Failed to open file\n");
    return "ERROR";
  }
  std::string BareReport; //For ReportBare

  //Setting Status Display
  if (type == "HTML") {statusDisplay.SetHTML();}
  else if(type == "Bare") {}
  else {
    fprintf(stderr, "ERROR: invalid HTML type %s\n", type.c_str());
    fprintf(stderr, "Valid HTML types are; 'HTML', 'Bare', or '' for HTML\n");
    return "ERROR";
  }

  //Get report
  if (type == "HTML") {statusDisplay.Report(level, HTML, "");}
  else {
    BareReport = statusDisplay.ReportBare(level, "");
    HTML.write(BareReport.c_str(),BareReport.size());
    HTML.close();
  }

  //END
  HTML.close();
  statusDisplay.UnsetHTML();
  statusDisplay.Clear();
  return "GOOD";
}

std::string ApolloSM::GenerateGraphiteStatus(size_t level = size_t(1), std::string table="") {
  statusDisplay.Clear();
  //SETUP
  std::stringstream output;

  //Setting Status Display
  statusDisplay.SetGraphite();

  //Get report
  statusDisplay.Report(level,output,table);

  //END
  statusDisplay.UnsetGraphite();
  statusDisplay.Clear();
  return output.str();
}



bool ApolloSM::PowerUpCM(int CM_ID, int wait /*seconds*/){
  const uint32_t RUNNING_STATE = 3;
  if((CM_ID < 1) || (CM_ID > 2)){
    BUException::APOLLO_SM_BAD_VALUE e;
    e.Append("Bad CM_ID");
    throw e;
  }
  std::string CM_CTRL("CM.CM_");
  switch (CM_ID){
  case 2:
    CM_CTRL+="2";
    break;
  default:
    CM_CTRL+="1";
  }
  CM_CTRL+=".CTRL.";

  //Check that the uC is powered up, power up if needed
  if(!ReadRegister(CM_CTRL+"ENABLE_UC")){
    WriteRegister(CM_CTRL+"ENABLE_UC",1);
  }
  //Power up the CM 
  WriteRegister(CM_CTRL+"ENABLE_PWR",1);
  usleep(10000); //Wait 10ms
  
  wait*=1000000; //convert wait time to us from s
  do{
    if(ReadRegister(CM_CTRL+"STATE") == RUNNING_STATE){
      return true;
     }
    int dt = 10000;//10ms
    usleep(dt);
    wait-=dt;
  }while(wait >= 0);

  WriteRegister(CM_CTRL+"ENABLE_PWR",0);
  
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
  std::string CM_CTRL("CM.CM_");
  switch (CM_ID){
  case 2:
    CM_CTRL+="2";
    break;
  default:
    CM_CTRL+="1";
  }
  CM_CTRL+=".CTRL.";

  WriteRegister(CM_CTRL+"ENABLE_PWR",0);
  usleep(10000); //Wait 10ms
  
  wait*=1000000; //convert wait time to us from s
  do{
    if( ReadRegister(CM_CTRL+"STATE") == RESET_STATE ){
      //PWR GOOD went off
      break;
    }
    int dt = 10000;//10ms
    usleep(dt);
    wait-=dt;
  }while(wait >= 0);  

  uint32_t state = ReadRegister(CM_CTRL+"STATE");

  if(PWR_DOWN_STATE == state){
    //We just shut off the uC before power good went down.  
    //We gave up
    return false;
  }
  //everything is good
  return true;
}

void ApolloSM::unblockAXI(std::string unblockName) {
  std::vector<std::string> Names = GetRegsRegex("*");
  uMap::iterator unblockNode;
  for(auto it = Names.begin();it != Names.end();it++){
    //Get the list of parameters for this node
    uMap parameters = GetParameters(*it);
    if( (unblockNode = parameters.find("Unblock")) != parameters.end()){
      if(unblockName.size() == 0){	
	//The unblockName is empty, so every node with unblock gets a write
	WriteAction(*it);
      }else{
	if(boost::iequals(unblockName,unblockNode->second)){
	  WriteAction(*it);
	}
      }
    }

  }
  return;
}

void ApolloSM::restartCMuC(std::string CM_ID) {
  std::string command_string = "CM.CM_" + CM_ID + ".CTRL.ENABLE_UC";
  WriteRegister(command_string,1);
  usleep(10000); //Wait 10ms
  WriteRegister(command_string,0);
}
