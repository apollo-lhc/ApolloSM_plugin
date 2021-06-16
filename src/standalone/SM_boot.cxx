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

#include <syslog.h>  ///for syslog

#include <boost/program_options.hpp>
#include <standalone/optionParsing.hh>
#include <standalone/optionParsing_bool.hh>
#include <standalone/daemon.hh>

#include <fstream>
#include <iostream>


// ====================================================================================================
// Constants
#define SEC_IN_US  1000000
#define NS_IN_US 1000

// ====================================================================================================
// Set up for boost program_options
#define DEFAULT_CONFIG_FILE "/etc/SM_boot"
#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_RUN_DIR     "/opt/address_table/"
#define DEFAULT_PID_FILE    "/var/run/sm_boot.pid"
#define DEFAULT_POWERUP_TIME 5
#define DEFAULT_SENSORS_THROUGH_ZYNQ false // This means: by default, read the sensors through the zynq
#define DEFAULT_CM_POWERUP false
namespace po = boost::program_options; //Making life easier for boost
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


int main(int argc, char** argv) { 

  // parameters to get from command line or config file (config file itself will not be in the config file, obviously)
  std::string runPath     = DEFAULT_RUN_DIR;
  std::string pidFileName = DEFAULT_PID_FILE;
  int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;
  bool powerupCMuC        = DEFAULT_CM_POWERUP;
  int powerupTime         = DEFAULT_POWERUP_TIME;
  bool sensorsThroughZynq = DEFAULT_SENSORS_THROUGH_ZYNQ;

  //Mikey - finish
  po::options_description cli_options("SM_boot options");
  cli_options.add_options()
    ("help,h", "Help screen")
    ("run_path,r",           po::value<std::string>(), "Path to run directory")
    ("pid_file,f",           po::value<std::string>(), "pid file")
    ("polltime,P",           po::value<int>(),         "Polltime in seconds")
    ("cm_powerup,P",         po::value<bool>(),        "Powerup CM")
    ("cm_powerup_time,t",    po::value<int>(),         "Powerup time in seconds")
    ("sensorsThroughZynq,s", po::value<bool>(),        "Read sensors through the Zynq")
    ("config_file",          po::value<std::string>(), "config file"); // This is the only option not also in the file option (obviously); 
   
  po::options_description cfg_options("SM_boot options");
  cfg_options.add_options()
    ("run_path",           po::value<std::string>(), "Path to run directory")
    ("pid_file",           po::value<std::string>(), "pid file")
    ("polltime",           po::value<int>(),         "Polltime in seconds")
    ("cm_powerup",         po::value<bool>(),        "Powerup CM")
    ("cm_powerup_time",    po::value<int>(),         "Powerup time in seconds")
    ("sensorsThroughZynq", po::value<bool>(),        "Read sensors through the Zynq"); // This means: by default, read the sensors through the zynq

  std::map<std::string,std::vector<std::string> > allOptions;
  
  //Do a quick search of the command line only to look for a new config file.
  //Get options from command line,
  try { 
    FillOptions(parse_command_line(argc, argv, cli_options),
    allOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    return 0;
  }
  //Help option - ends program 
  if(allOptions.find("help") != allOptions.end()){
    std::cout << cli_options << '\n';
    return 0;
  }  
  
  std::string configFileName = GetFinalParameterValue(std::string("config_file"),allOptions,std::string(DEFAULT_CONFIG_FILE));
  
  //Get options from config file
  std::ifstream configFile(configFileName.c_str());   
  if(configFile){
    try { 
      FillOptions(parse_config_file(configFile,cfg_options,true),
      allOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
    }
    configFile.close();
  }
  
  runPath=             GetFinalParameterValue(std::string("run_path"),          allOptions,std::string(DEFAULT_RUN_DIR));
  pidFileName=         GetFinalParameterValue(std::string("pid_file"),          allOptions,std::string(DEFAULT_PID_FILE));
  polltime_in_seconds= GetFinalParameterValue(std::string("polltime"),          allOptions,DEFAULT_POLLTIME_IN_SECONDS);
  powerupCMuC=         GetFinalParameterValue(std::string("cm_powerup"),        allOptions,DEFAULT_CM_POWERUP);
  powerupTime=         GetFinalParameterValue(std::string("cm_powerup_time"),   allOptions,DEFAULT_POWERUP_TIME);
  sensorsThroughZynq=  GetFinalParameterValue(std::string("sensorsThroughZynq"),allOptions,DEFAULT_SENSORS_THROUGH_ZYNQ);
  
  // ============================================================================
  // Deamon book-keeping
  Daemon daemon;
  daemon.daemonizeThisProgram(pidFileName, runPath);

  // ============================================================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  daemon.changeSignal(&sa_INT , &old_sa, SIGINT);
  daemon.changeSignal(&sa_TERM, NULL   , SIGTERM);
  daemon.SetLoop(true);


  //Update parameters

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
      SM->RegWriteRegister("CM.CM_1.CTRL.ENABLE_UC",1);
      syslog(LOG_INFO,"Powering up CM uC\n");
      sleep(powerupTime);
    }
  
    //Set uC temp sensors as disabled
    if(!sensorsThroughZynq){
      temperatures temps;  
      temps = {0,0,0,0,false};
      sendTemps(SM, temps);
      syslog(LOG_INFO,"No reading out CM sensors via zynq\n");
    }else{
      syslog(LOG_INFO,"Reading out CM sensors via zynq\n");
    }


    // ==================================
    // Main DAEMON loop
    syslog(LOG_INFO,"Starting Monitoring loop\n");
    

    uint32_t CM_running = 0;
    while(daemon.GetLoop()) {
      // loop start time
      clock_gettime(CLOCK_REALTIME, &startTS);

      //=================================
      //Do work
      //=================================

      //Process CM temps
      if(sensorsThroughZynq) {
  temperatures temps;  
        if(SM->RegReadRegister("CM.CM_1.CTRL.ENABLE_UC")){
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
    CM_running = SM->RegReadRegister("CM.CM_1.CTRL.PWR_GOOD");
    
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
  SM->RegWriteRegister("CM.CM_1.CTRL.ENABLE_UC",0);

  
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
  syslog(LOG_INFO,"eyescan Daemon ended\n");
  return 0;
}