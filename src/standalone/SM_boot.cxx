#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <uhal/uhal.hpp>
#include <vector>
#include <string>
#include <boost/tokenizer.hpp>
#include <unistd.h> // usleep, execl
#include <signal.h>
#include <time.h>

#include <sys/stat.h> //for umask
#include <sys/types.h> //for umask

#include <BUException/ExceptionBase.hh>


#include <boost/program_options.hpp>
#include <fstream>

#include <syslog.h> ///for syslog
#include <standalone/parseOptions.hh> // setOptions // setParamValues // loadConfig
#include <standalone/daemon.hh>       // daemonizeThisProgram // changeSignal // loop

#define SEC_IN_US  1000000
#define NS_IN_US 1000

// value doesn't matter, as long as it is defined
#define SAY_STATUS_DONE_ANYWAY 1

#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_CONFIG_FILE "/etc/SM_boot"
#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_PID_FILE    "/var/run/sm_boot.pid"
#define DEFAULT_POWERUP_TIME 5

#define DEFAULT_SENSORS_THROUGH_ZYNQ true // This means: by default, read the sensors through the zynq

// ====================================================================================================
long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec)/NS_IN_US);
}

// ====================================================================================================
// Definitions

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

struct temperatures {
  uint8_t MCUTemp;
  uint8_t FIREFLYTemp;
  uint8_t FPGATemp;
  uint8_t REGTemp;
  bool    validData;
};
// ====================================================================================================
temperatures sendAndParse(ApolloSM* SM) {
  temperatures temps {0,0,0,0,false};
  std::string recv;

  // read and print
  try{
    recv = (SM->UART_CMD("/dev/ttyUL1", "simple_sensor", '%'));
    temps.validData = true;
  }catch(BUException::IO_ERROR &e){
    //ignore this     
  }
  
  if (temps.validData){
    // Separate by line
    boost::char_separator<char> lineSep("\r\n");
    tokenizer lineTokens{recv, lineSep};

    // One vector for each line 
    std::vector<std::vector<std::string> > allTokens;

    // Separate by spaces
    boost::char_separator<char> space(" ");
    int vecCount = 0;
    // For each line
    for(tokenizer::iterator lineIt = lineTokens.begin(); lineIt != lineTokens.end(); ++lineIt) {
      tokenizer wordTokens{*lineIt, space};
      // We don't yet own any memory in allTokens so we append a blank vector
      std::vector<std::string> blankVec;
      allTokens.push_back(blankVec);
      // One vector per line
      for(tokenizer::iterator wordIt = wordTokens.begin(); wordIt != wordTokens.end(); ++wordIt) {
	allTokens[vecCount].push_back(*wordIt);
      }
      vecCount++;
    }

    // Check for at least one element 
    // Check for two elements in first element
    // Following lines follow the same concept
    std::vector<float> temp_values;
    for(size_t i = 0; 
	i < allTokens.size() && i < 4;
	i++){
      if(2 == allTokens[i].size()) {
	float temp;
	if( (temp = std::atof(allTokens[i][1].c_str())) < 0) {
	  temp = 0;
	}
	temp_values.push_back(temp);
      }
    }
    switch (temp_values.size()){
    case 4:
      temps.REGTemp = (uint8_t)temp_values[3];  
      //fallthrough
    case 3:
      temps.FPGATemp = (uint8_t)temp_values[2];  
      //fallthrough
    case 2:
      temps.FIREFLYTemp = (uint8_t)temp_values[1];  
      //fallthrough
    case 1:
      temps.MCUTemp = (uint8_t)temp_values[0];  
      //fallthrough
      break;
    default:
      break;
    }
  }
  return temps;
}

// ====================================================================================================
void updateTemp(ApolloSM * SM, std::string const & base,uint8_t temp){
  uint32_t oldValues = SM->RegReadRegister(base);
  oldValues = (oldValues & 0xFFFFFF00) | ((temp)&0x000000FF);
  if(0 == temp){    
    SM->RegWriteRegister(base,oldValues);
    return;
  }

  //Update max
  if(temp > (0xFF&(oldValues>>8))){
    oldValues = (oldValues & 0xFFFF00FF) | ((temp<< 8)&0x0000FF00);
  }
  //Update min
  if((temp < (0xFF&(oldValues>>16))) || 
     (0 == (0xFF&(oldValues>>16)))){
    oldValues = (oldValues & 0xFF00FFFF) | ((temp<<16)&0x00FF0000);
  }
  SM->RegWriteRegister(base,oldValues);
}

void sendTemps(ApolloSM* SM, temperatures temps) {
  updateTemp(SM,"SLAVE_I2C.S2.VAL", temps.MCUTemp);
  updateTemp(SM,"SLAVE_I2C.S3.VAL", temps.FIREFLYTemp);
  updateTemp(SM,"SLAVE_I2C.S4.VAL", temps.FPGATemp);
  updateTemp(SM,"SLAVE_I2C.S5.VAL", temps.REGTemp);
}

// ====================================================================================================
// Program CM FPGAs
int programCMFPGA(ApolloSM * SM, FILE * logFile, int CM_ID) {
  bool NOFILE = 2;
  bool SUCCESS = 1;
  bool FAIL = 0;   
  
  int wait_time = 5; // 1 second
  
  std::string FPGA;
  std::string FPGAinitial;
  switch(CM_ID) {
  case 1: 
    // Kintex
    FPGA.append("Kintex");
    FPGAinitial.append("K");
  case 2:
    // Virtex
    FPGA.append("Virtex");
    FPGAinitial.append("V");
  default:
    fprintf(logFile, "Invalid CM ID: %d\n", CM_ID);
    fflush(logFile);
    // Maybe something less harsh
    return FAIL;
  }

  try {
    // ==============================
    // Power up CM. Currently not doing anything about time out.    
    bool success = SM->PowerUpCM(CM_ID,wait_time);
    if(success) {
      fprintf(logFile, "CM %d is powered up\n", CM_ID);
      fflush(logFile);
    } else {
      fprintf(logFile, "CM %d failed to powered up in time\n", CM_ID);
      fflush(logFile);
    }

    std::string CM_CTRL = "CM.CM" + std::to_string(CM_ID) + ".CTRL.";

    // Check CM is actually powered up and "good". 1 is good 0 is bad.
    if(!checkNode(SM, logFile, CM_CTRL + "PWR_GOOD",    1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_CTRL + "ISO_ENABLED", 1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_CTRL + "STATE",       4)) {return FAIL;}

    // Optionally run svfplayer commands to program CM FPGAs    

    // Check that CM file exists
    FILE * f = fopen(("/fw/CM/CM_" + FPGA + ".svf").c_str(), "rb");
    if(NULL == f) {
      return NOFILE;
    } 
    fclose(f);
    
    // Program FPGA
    SM->svfplayer("/fw/CM/CM_" + FPGA +".svf", "XVC1");
    
    std::string CM_C2C = "CM.CM" + std::to_string(CM_ID) + ".C2C.";
    
    // Check CM.CM*.C2C clocks are locked
    if(!checkNode(SM, logFile, CM_C2C + "CPLL_LOCK",       1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_C2C + "PHY_GT_PLL_LOCK", 1)) {return FAIL;}
    
    // Get FPGA out of error state
    SM->RegWriteRegister(CM_C2C + "INITIALIZE", 1);
    usleep(1000000);
    SM->RegWriteRegister(CM_C2C + "INITIALIZE", 0);
    
    // Check that phy lane is up, link is good, and that there are no errors.
    if(!checkNode(SM, logFile, CM_C2C + "MB_ERROR",     0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_C2C + "CONFIG_ERROR", 0)) {return FAIL;}
    //if(!checkNode(SM, logFile, CM_C2C + "LINK_ERROR",   0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_C2C + "PHY_HARD_ERR", 0)) {return FAIL;}
    //if(!checkNode(SM, logFile, CM_C2C + "PHY_SOFT_ERR", 0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_C2C + "PHY_MMCM_LOL", 0)) {return FAIL;} 
    if(!checkNode(SM, logFile, CM_C2C + "PHY_LANE_UP",  1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM_C2C + "LINK_GOOD",    1)) {return FAIL;}
    
    // Write to the "unblock" bits of the AXI*_FW slaves
    SM->RegWriteRegister("C2C" + std::to_string(CM_ID) + "_AXI_FW.UNBLOCK", 1);
    SM->RegWriteRegister("C2C" + std::to_string(CM_ID) + "_AXILITE_FW.UNBLOCK", 1);
    
    // Print firmware build date for FPGA
    printBuildDate(SM, logFile, CM_ID);
    
  } catch(BUException::exBase const & e) {
    fprintf(logFile, "Caught BUException: %s\n   Info: %s\n", e.what(), e.Description());
    fflush(logFile);
    return FAIL;
  } catch (std::exception const & e) {
    fprintf(logFile, "Caught std::exception: %s\n", e.what());
    fflush(logFile);
    return FAIL;
    }
    
  return SUCCESS;
}

int main(int argc, char** argv) { 

  // parameters to get from command line or config file (config file itself will not be in the config file, obviously)
  std::string configFile  = DEFAULT_CONFIG_FILE;
  std::string runPath     = DEFAULT_RUN_DIR;
  std::string pidFileName = DEFAULT_PID_FILE;
  int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;
  bool powerupCMuC        = true;
  int powerupTime         = DEFAULT_POWERUP_TIME;
  bool sensorsThroughZynq = DEFAULT_SENSORS_THROUGH_ZYNQ;
  
  // parse command line and config file to set parameters
  boost::program_options::options_description fileOptions{"File"}; // for parsing config file
  boost::program_options::options_description commandLineOptions{"Options"}; // for parsing command line
  commandLineOptions.add_options()
    ("config_file",
     boost::program_options::value<std::string>(),
     "config file"); // This is the only option not also in the file option (obviously)
  setOption(&fileOptions, &commandLineOptions, "run_path"          , "run path"                     , runPath);
  setOption(&fileOptions, &commandLineOptions, "pid_file"          , "pid file"                     , pidFileName);
  setOption(&fileOptions, &commandLineOptions, "polltime"          , "polling interval"             , polltime_in_seconds);
  setOption(&fileOptions, &commandLineOptions, "cm_powerup"        , "power up CM uC"               , powerupCMuC);
  setOption(&fileOptions, &commandLineOptions, "cm_powerup_time"   , "uC power up wait time"        , powerupTime);
  setOption(&fileOptions, &commandLineOptions, "sensorsThroughZynq", "read sensor data through Zynq", sensorsThroughZynq);
  boost::program_options::variables_map configFileVM; // for parsing config file
  boost::program_options::variables_map commandLineVM; // for parsing command line

  // The command line must be parsed before the config file so that we know if there is a command line specified config file 
  fprintf(stdout, "Parsing command line now\n");
  try {
    // parse command line
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, commandLineOptions), commandLineVM);
  } catch(const boost::program_options::error &ex) {
    fprintf(stderr, "Caught exception while parsing command line: %s. Try (--). ex: --polltime \nTerminating SM_boot\n", ex.what());       
    return -1;
  }

  // Check for non default config file
  if(commandLineVM.count("config_file")) {
    configFile = commandLineVM["config_file"].as<std::string>();
  }  
  fprintf(stdout, "config file path: %s\n", configFile.c_str());

  // Now the config file may be loaded
  fprintf(stdout, "Reading from config file now\n");
  try {
    // parse config file
    configFileVM = loadConfig(configFile, fileOptions);
  } catch(const boost::program_options::error &ex) {
    fprintf(stdout, "Caught exception in function loadConfig(): %s \nTerminating SM_boot\n", ex.what());        
    return -1;  
  }

  // Look at the config file and command line and determine if we should change the parameters from their default values
  // Only run path and pid file are needed for the next bit of code. The other parameters can and should wait until syslog is available.
  setParamValue(&runPath            , "run_path"          , configFileVM, commandLineVM, false);
  setParamValue(&pidFileName        , "pid_file"          , configFileVM, commandLineVM, false);
//  setParamValue(&polltime_in_seconds, "polltime"          , configFileVM, commandLineVM, true);
//  setParamValue(&powerupCMuC        , "cm_powerup"        , configFileVM, commandLineVM, true);
//  setParamValue(&powerupTime        , "cm_powerup_time"   , configFileVM, commandLineVM, true);
//  setParamValue(&sensorsThroughZynq , "sensorsThroughZynq", configFileVM, commandLineVM, true);

  // ============================================================================
  // Deamon book-keeping
  // Every daemon program should have one Daemon object. Daemon class functions are functions that all daemons progams have to perform. That is why we made the class.
  Daemon SM_bootDaemon;
  SM_bootDaemon.daemonizeThisProgram(pidFileName, runPath);

  // ============================================================================
  // Now that syslog is available, we can continue to look at the config file and command line and determine if we should change the parameters from their default values.
  setParamValue(&polltime_in_seconds, "polltime"          , configFileVM, commandLineVM, true);
  setParamValue(&powerupCMuC        , "cm_powerup"        , configFileVM, commandLineVM, true);
  setParamValue(&powerupTime        , "cm_powerup_time"   , configFileVM, commandLineVM, true);
  setParamValue(&sensorsThroughZynq , "sensorsThroughZynq", configFileVM, commandLineVM, true);

  // ============================================================================
  // Daemon code setup

  // ====================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  // struct sigaction sa_INT,sa_TERM,oldINT_sa,oldTERM_sa;
  SM_bootDaemon.changeSignal(&sa_INT , &old_sa, SIGINT);
  SM_bootDaemon.changeSignal(&sa_TERM, NULL   , SIGTERM);
  //  memset(&sa_INT ,0,sizeof(sa_INT)); //Clear struct
 //  memset(&sa_TERM,0,sizeof(sa_TERM)); //Clear struct
 //  //setup SA
 //  sa_INT.sa_handler  = signal_handler;
 //  sa_TERM.sa_handler = signal_handler;
 //  sigemptyset(&sa_INT.sa_mask);
 //  sigemptyset(&sa_TERM.sa_mask);
 //  sigaction(SIGINT,  &sa_INT , &old_sa);
 //  sigaction(SIGTERM, &sa_TERM, NULL);
  // sigaction(SIGTERM, &sa_TERM, &oldTERM_sa);
  SM_bootDaemon.loop = true;

  // ====================================
  // for counting time
  struct timespec startTS;
  struct timespec stopTS;

  long update_period_us = polltime_in_seconds*SEC_IN_US; //sleep time in microseconds

  bool inShutdown = false;
  ApolloSM * SM = NULL;
  try{
    // ==================================
    // Initialize ApolloSM
    SM = new ApolloSM();
    if(NULL == SM){
      syslog(LOG_ERR,"Failed to create new ApolloSM\n");
      exit(EXIT_FAILURE);
    }else{
      syslog(LOG_INFO,"Created new ApolloSM\n");      
    }
    std::vector<std::string> arg;
    arg.push_back("connections.xml");
    SM->Connect(arg);
    //Set the power-up done bit to 1 for the IPMC to read
    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",1);    
    syslog(LOG_INFO,"Set STATUS.DONE to 1\n");
  
    // ====================================
    // Turn on CM uC      
    if (powerupCMuC){
      SM->RegWriteRegister("CM.CM1.CTRL.ENABLE_UC",1);
      syslog(LOG_INFO,"Powering up CM uC\n");
      sleep(powerupTime);
    }
  
    //Set uC temp sensors as disabled
    if(!sensorsThroughZynq){
      temperatures temps;  
      temps = {0,0,0,0,false};
      sendTemps(SM, temps);
    }

    // ==================================
    // Program CM FPGAs and kill program upon failure
    int firstCM_ID = 1;
    int lastCM_ID = 2;
    for(int i = firstCM_ID; i <= lastCM_ID; i++) {
      switch(programCMFPGA(SM, logFile, i)) {
      case 0:
	      //  fail
	      return 0;
      case 1: 
	      // success
	      break;
      case 2:
	      // A CM file does not exist
	      fprintf(logFile, "CM %dfile does not exist\n", i);
	      fflush(logFile);
	      break;
      }
    }

    // Do we set this bit if the CM files do not exist?
    //Set the power-up done bit to 1 for the IPMC to read
#ifdef SAY_STATUS_DONE_ANYWAY
    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",1);
    fprintf(logFile,"Set STATUS.DONE to 1\n");
    fflush(logFile);
#endif

    // ==================================
    // Main DAEMON loop
    syslog(LOG_INFO,"Starting Monitoring loop\n");
    

    uint32_t CM_running = 0;
    while(SM_bootDaemon.loop) {
      // loop start time
      clock_gettime(CLOCK_REALTIME, &startTS);

      //=================================
      //Do work
      //=================================

      //Process CM temps
      //      if(true == sensorsThroughZynq) {
      if(sensorsThroughZynq) {
	temperatures temps;  
      	if(SM->RegReadRegister("CM.CM1.CTRL.ENABLE_UC")){
	  try{
	    temps = sendAndParse(SM);
	  }catch(std::exception & e){
	    syslog(LOG_INFO,e.what());
	    //ignoring any exception here for now
	    temps = {0,0,0,0,false};
	  }
	  
	  if(0 == CM_running ){
	    //Drop the non uC temps
	    temps.FIREFLYTemp = 0;
	    temps.FPGATemp = 0;
	    temps.REGTemp = 0;
	  }
	  CM_running = SM->RegReadRegister("CM.CM1.CTRL.PWR_GOOD");
	  
	  sendTemps(SM, temps);
	  if(!temps.validData){
	    syslog(LOG_INFO,"Error in parsing data stream\n");
	  }
	}else{
	  temps = {0,0,0,0,false};
	  sendTemps(SM, temps);
	}
      }

      //Check if we are shutting down
      if((!inShutdown) && SM->RegReadRegister("SLAVE_I2C.S1.SM.STATUS.SHUTDOWN_REQ")){
	syslog(LOG_INFO,"Shutdown requested\n");
	inShutdown = true;
	//the IPMC requested a re-boot.
	pid_t reboot_pid;
	if(0 == (reboot_pid = fork())){
	  //Shutdown the system
	  execlp("/sbin/shutdown","/sbin/shutdown","-h","now",NULL);
	  exit(1);
	}
	if(-1 == reboot_pid){
	  inShutdown = false;
	  syslog(LOG_INFO,"Error! fork to shutdown failed!\n");
	}else{
	  //Shutdown the command module (if up)
	  SM->PowerDownCM(1,5);
	}
      }
      //=================================


      // monitoring sleep
      clock_gettime(CLOCK_REALTIME, &stopTS);
      // sleep for 10 seconds minus how long it took to read and send temperature    
      useconds_t sleep_us = update_period_us - us_difftime(startTS, stopTS);
      if(sleep_us > 0){
	usleep(sleep_us);
      }
    }
  }catch(BUException::exBase const & e){
    syslog(LOG_INFO,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());
          
  }catch(std::exception const & e){
    syslog(LOG_INFO,"Caught std::exception: %s\n",e.what());
          
  }


  //make sure the CM is off
  //Shutdown the command module (if up)
  SM->PowerDownCM(1,5);
  SM->RegWriteRegister("CM.CM1.CTRL.ENABLE_UC",0);

  
  //If we are shutting down, do the handshanking.
  if(inShutdown){
    syslog(LOG_INFO,"Tell IPMC we have shut-down\n");
    //We are no longer booted
    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",0);
    //we are shut down
    //    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.SHUTDOWN",1);
    // one last HB
    //PS heartbeat
    SM->RegReadRegister("SLAVE_I2C.HB_SET1");
    SM->RegReadRegister("SLAVE_I2C.HB_SET2");

  }

  //Dump registers on power down
  std::stringstream outfileName;
  outfileName << "/var/log/Apollo_debug_dump_";  

  char buffer[128];
  time_t unixTime=time(NULL);
  struct tm * timeinfo = localtime(&unixTime);
  strftime(buffer,128,"%F-%T-%Z",timeinfo);
  outfileName << buffer;

  outfileName << ".dat";
  
  std::ofstream outfile(outfileName.str().c_str(),std::ofstream::out);
  outfile << outfileName.str() << std::endl;
  SM->DebugDump(outfile);
  outfile.close();  

  //Clean up
  if(NULL != SM) {
    delete SM;
  }

  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
//  sigaction(SIGTERM, &oldTERM_sa, NULL);
  syslog(LOG_INFO,"SM boot Daemon ended\n");

  return 0;
}
