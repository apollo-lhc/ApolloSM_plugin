#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
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

#define SEC_IN_USEC 10000000
#define NSEC_IN_USEC 1000
// ====================================================================================================
// Definitions

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

struct temperatures {
  uint8_t MCUTemp;
  uint8_t FIREFLYTemp;
  uint8_t FPGATemp;
  uint8_t REGTemp;
};

// ====================================================================================================
// Kill program if it is in background
bool static volatile loop;

void static signal_handler(int const signum) {
  if(SIGINT == signum || SIGTERM == signum) {
    loop = false;
  }
}

// ====================================================================================================

temperatures sendAndParse(ApolloSM* SM) {
  temperatures temps {0,0,0,0};
  
  // read and print
  std::string recv(SM->UART_CMD("CM.CM1", "simple_sensor", '%'));
  
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

  return temps;
}

// ====================================================================================================

void sendTemps(ApolloSM* SM, temperatures temps) {
  SM->RegWriteRegister("SLAVE_I2C.S2.0", temps.MCUTemp);
  SM->RegWriteRegister("SLAVE_I2C.S3.0", temps.FIREFLYTemp);
  SM->RegWriteRegister("SLAVE_I2C.S4.0", temps.FPGATemp);
  SM->RegWriteRegister("SLAVE_I2C.S5.0", temps.REGTemp);
}

// ====================================================================================================

long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_USEC + 
	   (end.tv_nsec - cur.tv_nsec)/NSEC_IN_USEC);
}

// ====================================================================================================
// Checks value of nodes and quits program if value was not the expected value
bool checkNode(ApolloSM * SM, FILE * logFile, std::string node, int correctVal) {
  bool GOOD = 1;
  bool BAD = 0;

  bool readVal;
  if(correctVal != (readVal = SM->RegReadRegister(node))) {
    fprintf(logFile, "%s was %d\n", node.c_str(), readVal);
    fflush(logFile);
    return BAD;
  }
  return GOOD;
}

// ====================================================================================================
// Read firmware build date and hash
void printBuildDate(ApolloSM * SM, FILE * logFile, int CM) {

    // The masks according to BUTool "nodes"
    uint32_t yearMask = 0xffff0000;
    uint32_t monthMask = 0x0000ff00;
    uint32_t dayMask = 0x000000ff;

    if(1 == CM) {
      // Kintex
      std::string CM_K_BUILD = "CM_K_INFO.BUILD_DATE";
      uint32_t CM_K_BUILD_DATE = SM->RegReadRegister(CM_K_BUILD);
      
      uint16_t yearK;
      uint8_t monthK;
      uint8_t dayK;
      // Convert Kintex build dates from strings to ints
      yearK = (CM_K_BUILD_DATE & yearMask) >> 16;
      monthK = (CM_K_BUILD_DATE & monthMask) >> 8;
      dayK = (CM_K_BUILD_DATE & dayMask);
      
      fprintf(logFile, "Kintex firmware build date\nYYYY MM DD\n");
      fprintf(logFile, "%x %x %x", yearK, monthK, dayK);

    } else if(2 == CM) {
      // Virtex
      std::string CM_V_BUILD = "CM_V_INFO.BUILD_DATE";
      uint32_t CM_V_BUILD_DATE = SM->RegReadRegister(CM_V_BUILD);
      
      uint16_t yearV;
      uint8_t monthV;
      uint8_t dayV;    
      // Convert Virtex build dates from strings to ints
      yearV = (CM_V_BUILD_DATE & yearMask) >> 16;
      monthV = (CM_V_BUILD_DATE & monthMask) >> 8;
      dayV = (CM_V_BUILD_DATE & dayMask);
      
      fprintf(logFile, "Virtex firmware build date\nYYYY/MM/DD\n");
      fprintf(logFile, "%x %x %x", yearV, monthV, dayV);
    }
}

// ====================================================================================================
// Program CM FPGAs
int programCMFPGAs(ApolloSM * SM, FILE * logFile) {
  bool NOFILE = 2;
  bool SUCCESS = 1;
  bool FAIL = 0; 

  int wait_time = 5; // 1 second

  try {
    // ==============================
    // CM1 Kintex

    int CM1_ID = 1;

    // Power up CM1. Currently not doing anything about time out.    
    bool success = SM->PowerUpCM(CM1_ID,wait_time);
    if(success) {
      fprintf(logFile, "CM %d is powered up\n", CM1_ID);
      fflush(logFile);
    } else {
      fprintf(logFile, "CM %d failed to powered up in time\n", CM1_ID);
      fflush(logFile);
    }

    std::string CM1_CTRL = "CM.CM1.CTRL.";

    // Check CM1 is actually powered up and "good". 1 is good 0 is bad.
    if(!checkNode(SM, logFile, CM1_CTRL + "PWR_GOOD",    1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_CTRL + "ISO_ENABLED", 1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_CTRL + "STATE",       4)) {return FAIL;}

    // Optionally run svfplayer commands to program CM FPGAs    

    // Check that Kintex file exists
    FILE * fk = fopen("/fw/CM/CM_Kintex.svf", "rb");
    if(NULL == fk) {
      return NOFILE;
    } 
    fclose(fk);
    
    // Program Kintex FPGA
    SM->svfplayer("/fw/CM/CM_Kintex.svf", "XVC1");
    
    std::string CM1_C2C = "CM.CM1.C2C.";
    
    // Check CM.CM1.C2C clocks are locked
    if(!checkNode(SM, logFile, CM1_C2C + "CPLL_LOCK",       1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_C2C + "PHY_GT_PLL_LOCK", 1)) {return FAIL;}
    
    // Get Kintex FPGA out of error state
    SM->RegWriteRegister(CM1_C2C + "INITIALIZE", 1);
    usleep(1000000);
    SM->RegWriteRegister(CM1_C2C + "INITIALIZE", 0);
    
    // Check that phy lane is up, link is good, and that there are no errors.
    if(!checkNode(SM, logFile, CM1_C2C + "MB_ERROR",     0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_C2C + "CONFIG_ERROR", 0)) {return FAIL;}
    //if(!checkNode(SM, logFile, CM1_C2C + "LINK_ERROR",   0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_C2C + "PHY_HARD_ERR", 0)) {return FAIL;}
    //if(!checkNode(SM, logFile, CM1_C2C + "PHY_SOFT_ERR", 0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_C2C + "PHY_MMCM_LOL", 0)) {return FAIL;} 
    if(!checkNode(SM, logFile, CM1_C2C + "PHY_LANE_UP",  1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM1_C2C + "LINK_GOOD",    1)) {return FAIL;}
    
    // Write to the "unblock" bits of the AXI*_FW slaves
    SM->RegWriteRegister("C2C1_AXI_FW.UNBLOCK", 1);
    SM->RegWriteRegister("C2C1_AXILITE_FW.UNBLOCK", 1);
    
    // Print firmware build date for Kintex (CM = 1)
    printBuildDate(SM, logFile, 1);
    
    // ==============================
    // CM2 Virtex

    int CM2_ID = 2;

    // Power up CM2. Currently not doing anything about time out.    
    success = SM->PowerUpCM(CM2_ID,wait_time);
    if(success) {
      fprintf(logFile, "CM %d is powered up\n", CM2_ID);
      fflush(logFile);
    } else {
      fprintf(logFile, "CM %d failed to powered up in time\n", CM2_ID);
      fflush(logFile);
    }

    std::string CM2_CTRL = "CM.CM2.CTRL.";

    // Check CM2 is actually powered up and "good". 1 is good 0 is bad.
    if(!checkNode(SM, logFile, CM2_CTRL + "PWR_GOOD",    1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_CTRL + "ISO_ENABLED", 1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_CTRL + "STATE",       4)) {return FAIL;}
    
    // Optionally run svfplayer commands to program CM FPGAs    

    // Check that Virtex file exists
    FILE * fv = fopen("/fw/CM/CM_Kintex.svf", "rb");
    if(NULL == fv) {
      return NOFILE;
    } 
    fclose(fv);

    // Program Virtex FPGA
    SM->svfplayer("/fw/CM/CM_Virtex.svf", "XVC1");
    
    std::string CM2_C2C = "CM.CM2.C2C.";
    
    // Check CM.CM1.C2C clocks are locked
    if(!checkNode(SM, logFile, CM2_C2C + "CPLL_LOCK",       1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_C2C + "PHY_GT_PLL_LOCK", 1)) {return FAIL;}
    
    // Get Virtex FPGA out of error state
    SM->RegWriteRegister(CM2_C2C + "INITIALIZE", 1);
    usleep(1000000);
    SM->RegWriteRegister(CM2_C2C + "INITIALIZE", 0);
    
    // Check that phy lane is up, link is good, and that there are no errors.
    if(!checkNode(SM, logFile, CM2_C2C + "MB_ERROR",     0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_C2C + "CONFIG_ERROR", 0)) {return FAIL;}
    //if(!checkNode(SM, logFile, CM2_C2C + "LINK_ERROR",   0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_C2C + "PHY_HARD_ERR", 0)) {return FAIL;}
    //if(!checkNode(SM, logFile, CM2_C2C + "PHY_SOFT_ERR", 0)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_C2C + "PHY_MMCM_LOL", 0)) {return FAIL;} 
    if(!checkNode(SM, logFile, CM2_C2C + "PHY_LANE_UP",  1)) {return FAIL;}
    if(!checkNode(SM, logFile, CM2_C2C + "LINK_GOOD",    1)) {return FAIL;}
    
    SM->RegWriteRegister("C2C2_AXI_FW.UNBLOCK", 1);
    SM->RegWriteRegister("C2C2_AXILITE_FW.UNBLOCK", 1);

    // Print firmware build date for Virtex (CM = 2)
    printBuildDate(SM, logFile, 2);

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

// ====================================================================================================
// Main program
int main(int, char**) { 

  // ============================================================================
  // Deamon book-keeping
  pid_t pid, sid;
  pid = fork();
  if(pid < 0){
    //Something went wrong.
    //log something
    exit(EXIT_FAILURE);
  }else if(pid > 0){
    //We are the parent and created a child with pid pid
    FILE * pidFile = fopen("/var/run/sm_boot.pid","w");
    fprintf(pidFile,"%d\n",pid);
    fclose(pidFile);
    exit(EXIT_SUCCESS);
  }else{
    // I'm the child!
  }
  
  //Change the file mode mask to allow read/write
  umask(0);

  //Create log file
  FILE * logFile = fopen("/var/log/sm_boot.log","w");
  if(logFile == NULL){
    fprintf(stderr,"Failed to create log file\n");
    exit(EXIT_FAILURE);
  }
  fprintf(logFile,"Opened log file\n");
  fflush(logFile);

  // create nbew SID for the daemon.
  sid = setsid();
  if (sid < 0) {
    fprintf(logFile,"Failed to change SID\n");
    fflush(logFile);
    exit(EXIT_FAILURE);
  }else{
    fprintf(logFile,"Set SID to %d\n",sid);
    fflush(logFile);    
  }

  //Move to "/tmp/bin"
  if ((chdir("/tmp/bin")) < 0) {
    fprintf(logFile,"Failed to change path to \"/tmp/bin\"\n");
    fflush(logFile);
    exit(EXIT_FAILURE);
  }else{
    fprintf(logFile,"Changed path to \"/tmp/bin\"\n");
    fflush(logFile);
  }


  //Everything looks good, close the standard file fds.
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);


  
  // ============================================================================
  // Daemon code setup

  // ====================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  memset(&sa_INT ,0,sizeof(sa_INT)); //Clear struct
  memset(&sa_TERM,0,sizeof(sa_TERM)); //Clear struct
  //setup SA
  sa_INT.sa_handler  = signal_handler;
  sa_TERM.sa_handler = signal_handler;
  sigemptyset(&sa_INT.sa_mask);
  sigemptyset(&sa_TERM.sa_mask);
  sigaction(SIGINT,  &sa_INT , &old_sa);
  sigaction(SIGTERM, &sa_TERM, NULL);
  loop = true;

  // ====================================
  // for counting time
  struct timespec startTS;
  struct timespec stopTS;

  long update_period_us = 1*SEC_IN_USEC; //sleep time in microseconds


  bool inShutdown = false;
  ApolloSM * SM = NULL;
  try{
    // ==================================
    // Initialize ApolloSM
    SM = new ApolloSM();
    if(NULL == SM){
      fprintf(logFile,"Failed to create new ApolloSM\n");
      fflush(logFile);
    }else{
      fprintf(logFile,"Created new ApolloSM\n");
      fflush(logFile);
    }
    std::vector<std::string> arg;
    arg.push_back("connections.xml");
    SM->Connect(arg);
  
    // ==================================
    // Program CM FPGAs and kill program upon failure
    switch(programCMFPGAs(SM, logFile)) {
    case 0:
      //  fail
      return 0;
    case 1: 
      // success
      break;
    case 2:
      // A CM file does not exist
      fprintf(logFile, "A CM file does not exist\n");
      fflush(logFile);
      break;
    }

    // Do we set this bit if the CM files do not exist?
    //Set the power-up done bit to 1 for the IPMC to read
    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",1);
    fprintf(logFile,"Set STATUS.DONE to 1\n");
    fflush(logFile);

    // ==================================
    // Main DAEMON loop
    fprintf(logFile,"Starting Monitoring loop\n");
    fflush(logFile);
    while(loop) {
      // loop start time
      clock_gettime(CLOCK_REALTIME, &startTS);

      //=================================
      //Do work
      //=================================

      //PS heartbeat
      SM->RegReadRegister("SLAVE_I2C.HB_SET1");
      SM->RegReadRegister("SLAVE_I2C.HB_SET2");

      //Process CM temps
      temperatures temps;  
      if(SM->RegReadRegister("CM.CM1.CTRL.IOS_ENABLED")){
	temps = sendAndParse(SM);
	sendTemps(SM, temps);
      }else{
	temps = {0,0,0,0};
	sendTemps(SM, temps);
      }

      //Check if we are shutting down
      if((!inShutdown) && SM->RegReadRegister("SLAVE_I2C.S1.SM.STATUS.SHUTDOWN_REQ")){
	fprintf(logFile,"Shutdown requested\n");
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
	  fprintf(logFile,"Error! fork to shutdown failed!\n");
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
    fprintf(logFile,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());
    fflush(logFile);      
  }catch(std::exception const & e){
    fprintf(logFile,"Caught std::exception: %s\n",e.what());
    fflush(logFile);      
  }
  
  //If we are shutting down, do the handshanking.
  if(inShutdown){
    fprintf(logFile,"Tell IPMC we have shut-down\n");
    //We are no longer booted
    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",0);
    //we are shut down
    //    SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.SHUTDOWN",1);
    // one last HB
    //PS heartbeat
    SM->RegReadRegister("SLAVE_I2C.HB_SET1");
    SM->RegReadRegister("SLAVE_I2C.HB_SET2");

  }
  
  //Clean up
  if(NULL != SM) {
    delete SM;
  }
  
  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  fprintf(logFile,"SM boot Daemon ended\n");
  fclose(logFile);
  
  return 0;
}
