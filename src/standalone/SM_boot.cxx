#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <standalone/CM.hh>
#include <standalone/FPGA.hh>
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

#define SEC_IN_US 1000000
#define NS_IN_US  1000

// value doesn't matter, as long as it is defined
//#define SAY_STATUS_DONE_ANYWAY 1

#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_CONFIG_FILE "/etc/SM_boot"
#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_PID_FILE    "/var/run/sm_boot.pid"
#define DEFAULT_POWERUP_TIME 5
#define DEFAULT_SENSORS_THROUGH_ZYNQ true // This means: by default, read the sensors through the zynq
//#define DEFAULT_PROGRAM_KINTEX false
//#define DEFAULT_PROGRAM_VIRTEX false

// ====================================================================================================
// // These are indices for CMs and FPGAs. For readability
// #define CM_ID        0
// #define CM_PWR_GOOD  1
// #define CM_PWR_UP    2
// 
// #define FPGA_NAME    0 
// #define FPGA_CM      1
// #define FPGA_XVC     2
// #define FPGA_SVF     3
// #define FPGA_C2C     4
// #define FPGA_DONE    5
// #define FPGA_INIT    6 
// #define FPGA_AXI     7
// #define FPGA_AXILITE 8
// 
// // FPGA done bit
// #define FPGA_PROGRAMMED     1
// #define FPGA_PROGRAM_FAILED 0
// 
// // ====================================================================================================
// // Parse a long string into a vector of strings
// std::vector<std::string> split_string(std::string str, std::string delimiter){
//   
//   size_t position = 0;
//   std::string token;
//   std::vector<std::string> vec;
//   while( (position = str.find(delimiter)) != std::string::npos) {
//     token = str.substr(0, position);
//     vec.push_back(token);
//     str.erase(0, position+delimiter.length());
//   }
//   vec.push_back(str);
// 
//   return vec;
// }

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
// Checks register/node values
bool checkNode(ApolloSM * SM, std::string node, uint32_t correctVal) {
  bool GOOD = true;
  bool BAD  = false;

  uint32_t readVal;
  if(correctVal != (readVal = SM->RegReadRegister(node))) {
    syslog(LOG_ERR, "%s is, incorrectly, %d\n", node.c_str(), readVal);     
    return BAD;
  }
  return GOOD;
}

// ====================================================================================================
// Read firmware build date and hash
void printBuildDate(ApolloSM * SM, int CM) {

  // The masks according to BUTool "nodes"
  uint32_t yearMask  = 0xffff0000;
  uint32_t monthMask = 0x0000ff00;
  uint32_t dayMask   = 0x000000ff;

  if(1 == CM) {
    // Kintex
    std::string CM_K_BUILD = "CM_K_INFO.BUILD_DATE";
    uint32_t CM_K_BUILD_DATE = SM->RegReadRegister(CM_K_BUILD);
      
    uint16_t yearK;
    uint8_t  monthK;
    uint8_t  dayK;
    // Convert Kintex build dates from strings to ints
    yearK  = (CM_K_BUILD_DATE & yearMask) >> 16;
    monthK = (CM_K_BUILD_DATE & monthMask) >> 8;
    dayK   = (CM_K_BUILD_DATE & dayMask);
      
    syslog(LOG_INFO, "Kintex firmware build date\nYYYY MM DD\n");
    syslog(LOG_INFO, "%x %x %x", yearK, monthK, dayK);

  } else if(2 == CM) {
    // Virtex
    std::string CM_V_BUILD = "CM_V_INFO.BUILD_DATE";
    uint32_t CM_V_BUILD_DATE = SM->RegReadRegister(CM_V_BUILD);
      
    uint16_t yearV;
    uint8_t  monthV;
    uint8_t  dayV;    
    // Convert Virtex build dates from strings to ints
    yearV  = (CM_V_BUILD_DATE & yearMask) >> 16;
    monthV = (CM_V_BUILD_DATE & monthMask) >> 8;
    dayV   = (CM_V_BUILD_DATE & dayMask);
      
    syslog(LOG_INFO, "Virtex firmware build date\nYYYY/MM/DD\n");
    syslog(LOG_INFO, "%x %x %x", yearV, monthV, dayV);
  }
}

// ====================================================================================================
// Bring-up CM FPGAs
int bringupCMFPGAs(ApolloSM * SM, FPGA const myFPGA) {
  int const success =  0;
  int const fail    = -1;
  int const nofile  = -2;

//std::string const fpga_name    = FPGA[FPGA_NAME];
//std::string const fpga_cm      = FPGA[FPGA_CM];
//std::string const fpga_xvc     = FPGA[FPGA_XVC];
//std::string const fpga_svf     = FPGA[FPGA_SVF];
//std::string const fpga_c2c     = FPGA[FPGA_C2C];
//// done bit not needed
//std::string const fpga_init    = FPGA[FPGA_INIT];
//std::string const fpga_axi     = FPGA[FPGA_AXI];
//std::string const fpga_axilite = FPGA[FPGA_AXILITE];

  try {
    // ==============================    
    syslog(LOG_INFO, "Programming %s FPGA associated with CM%s using XVC label %s and svf file %s and checking clock locks at %s\n", myFPGA.name.c_str(), myFPGA.cm.c_str(), myFPGA.xvc.c_str(), myFPGA.svfFile.c_str(), myFPGA.c2c.c_str());
    // Check CM is actually powered up and "good". 
    std::string CM_CTRL = "CM." + myFPGA.cm + ".CTRL.";
    if(!checkNode(SM, CM_CTRL + "PWR_GOOD"   , 1)) {return fail;}
    if(!checkNode(SM, CM_CTRL + "ISO_ENABLED", 1)) {return fail;}
    if(!checkNode(SM, CM_CTRL + "STATE"      , 4)) {return fail;}
    // Check that svf file exists
    FILE * f = fopen(myFPGA.svfFile.c_str(), "rb");
    if(NULL == f) {return nofile;}
    fclose(f);
    // program
    SM->svfplayer(myFPGA.svfFile, myFPGA.xvc);
    // Check CM.CM*.C2C clocks are locked
    if(!checkNode(SM, myFPGA.c2c + ".CPLL_LOCK"      , 1)) {return fail;}
    if(!checkNode(SM, myFPGA.c2c + ".PHY_GT_PLL_LOCK", 1)) {return fail;}
    syslog(LOG_INFO, "Successfully programmed %s FPGA\n", myFPGA.name.c_str());
    
    if(myFPGA.init.compare("")) {
      // non empty initialize bit, so we initialize
      syslog(LOG_INFO, "Initializing %s fpga with %s\n", myFPGA.name.c_str(), myFPGA.c2c.c_str());
      // Get FPGA out of error state
      SM->RegWriteRegister(myFPGA.init, 1);
      usleep(1000000);
      SM->RegWriteRegister(myFPGA.init, 0);
      
      // Check that phy lane is up, link is good, and that there are no errors
      if(!checkNode(SM, myFPGA.c2c + ".MB_ERROR"    , 0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".CONFIG_ERROR", 0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".LINK_ERROR",   0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".PHY_HARD_ERR", 0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".PHY_SOFT_ERR", 0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".PHY_MMCM_LOL", 0)) {return fail;} 
      if(!checkNode(SM, myFPGA.c2c + ".PHY_LANE_UP" , 1)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".LINK_GOOD"   , 1)) {return fail;}
      syslog(LOG_INFO, "Initialized %s fpga with %s. Lanes up, links good, and no errors.\n", myFPGA.name.c_str(), myFPGA.c2c.c_str());
    }
     
    // unblock axi
    if(myFPGA.axi.compare("")) {
      SM->RegWriteAction(myFPGA.axi.c_str());      
      syslog(LOG_INFO, "%s: %s unblocked\n", myFPGA.name.c_str(), myFPGA.axi.c_str());    
    }
    if(myFPGA.axilite.compare("")) {
      SM->RegWriteAction(myFPGA.axilite.c_str());
      syslog(LOG_INFO, "%s: %s unblocked\n", myFPGA.name.c_str(), myFPGA.axilite.c_str());
    }
    
//    std::string CM_CTRL = "CM.CM" + CM_IDstr + ".CTRL.";
//
//    // Check CM is actually powered up and "good". 1 is good 0 is bad.
//    if(!checkNode(SM, CM_CTRL + "PWR_GOOD"   , 1)) {return fail;}
//    if(!checkNode(SM, CM_CTRL + "ISO_ENABLED", 1)) {return fail;}
//    if(!checkNode(SM, CM_CTRL + "STATE"      , 4)) {return fail;}
//
//    // ==============================
//    // Optionally run svfplayer commands to program CM FPGAs    
//    if(svfProgram) {
//      std::string svfFile = "/fw/CM/CM_" + FPGAinitial + ".svf";
//      // Check that CM file exists
//      FILE * f = fopen(svfFile.c_str(), "rb");
//      if(NULL == f) {return nofile;}
//      fclose(f);
//    
//      // Program FPGA
//      syslog(LOG_INFO, "Programming %s with %s\n", FPGA.c_str(), svfFile.c_str());
//      SM->svfplayer(svfFile.c_str(), "XVC1");
//    
//      std::string CM_C2C = "CM.CM" + CM_IDstr + ".C2C.";
//    
//      // Check CM.CM*.C2C clocks are locked
//      if(!checkNode(SM, CM_C2C + "CPLL_LOCK"      , 1)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "PHY_GT_PLL_LOCK", 1)) {return fail;}
//      
//      // Get FPGA out of error state
//      SM->RegWriteRegister(CM_C2C + "INITIALIZE", 1);
//      usleep(1000000);
//      SM->RegWriteRegister(CM_C2C + "INITIALIZE", 0);
//      
//      // Check that phy lane is up, link is good, and that there are no errors.
//      if(!checkNode(SM, CM_C2C + "MB_ERROR"    , 0)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "CONFIG_ERROR", 0)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "LINK_ERROR",   0)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "PHY_HARD_ERR", 0)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "PHY_SOFT_ERR", 0)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "PHY_MMCM_LOL", 0)) {return fail;} 
//      if(!checkNode(SM, CM_C2C + "PHY_LANE_UP" , 1)) {return fail;}
//      if(!checkNode(SM, CM_C2C + "LINK_GOOD"   , 1)) {return fail;}
//    
//      // Write to the "unblock" bits of the AXI*_FW slaves
////      SM->RegWriteRegister("C2C" + CM_IDstr + "_AXI_FW.UNBLOCK", 1);
////      SM->RegWriteRegister("C2C" + CM_IDstr + "_AXILITE_FW.UNBLOCK", 1);
//      SM->RegWriteAction("C2C" + CM_IDstr + "_AXI_FW.UNLOCK");
//      SM->RegWriteAction("C2C" + CM_IDstr + "_AXILITE_FW.UNLOCK");
      
  } catch(BUException::exBase const & e) {
    syslog(LOG_ERR, "Caught BUException: %s\n   Info: %s\n", e.what(), e.Description());
    return fail;
  } catch (std::exception const & e) {
    syslog(LOG_ERR, "Caught std::exception: %s\n", e.what());
    return fail;
  }
  
  return success;
}

// ====================================================================================================
int main(int argc, char** argv) { 

  // parameters to get from command line or config file (config file itself will not be in the config file, obviously)
  std::string configFile  = DEFAULT_CONFIG_FILE;
  std::string runPath     = DEFAULT_RUN_DIR;
  std::string pidFileName = DEFAULT_PID_FILE;
  int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;
  bool powerupCMuC        = true;
  int powerupTime         = DEFAULT_POWERUP_TIME;
  bool sensorsThroughZynq = DEFAULT_SENSORS_THROUGH_ZYNQ;
  //std::vector<std::vector<std::string> > CMs;
  // An example of what CMs may look like
  //     | (0) ID  | (1) power good node      | (2) power up |
  // CMs |     CM1 |     CM.CM1.CTRL.PWR_GOOD |     true     |
  //     |     CM2 |     CM.CM2.CTRL.PWR_GOOD |     false    |
  //std::vector<std::vector<std::string> > FPGAs;
  // An example of what FPGAs may look like. I made a vector of vector of strings to prevent having to declare 24 different variables (along with CMs). Not sure if this is the best decision -Felex
  //       |     | (0) name   | (1) CM  | (2) XVC label | (3) svf file  | (4) init node             | (5) C2C base node | (6) done bit                    | (7) axi unblock node | (8) axilite unblock node
  // FPGAs | (0) |     kintex |     CM1 |     XVC1      |     top_K.svf |     CM.CM1.C2C.INITIALIZE |     CM.CM1.C2C    | PL_MEM.S1.CM.STATUS.DONE_KINTEX |     C2C1_AXI.UNBLOCK |     C2C1_AXILITE.UNBLOCK   
  //       | (1) |     virtex |     CM2 |     XVC1      |     top_V.svf |     CM.CM2.C2C.INITIALIZE |     CM.CM2.C2C    | PL_MEM.S1.CM.STATUS.DONE_VIRTEX |     C2C2_AXI.UNBLOCK |     C2C2_AXILITE.UNBLOCK   

  boost::program_options::options_description fileOptions{"File"}; // for parsing config file
  boost::program_options::options_description commandLineOptions{"Options"}; // for parsing command line
  commandLineOptions.add_options()
    ("config_file"        , boost::program_options::value<std::string>()              , "config file"                  ) // Not in fileOptions (obviously)
    ("run_path"           , boost::program_options::value<std::string>()              , "run path"                     )
    ("pid_file"           , boost::program_options::value<std::string>()              , "pid file"                     )
    ("polltime"           , boost::program_options::value<int>()                      , "polling interval"             )
    ("cm_powerup"         , boost::program_options::value<bool>()                     , "power up CM uC"               )
    ("cm_powerup_time"    , boost::program_options::value<int>()                      , "uC power up wait time"        )
    ("sensorsThroughZynq" , boost::program_options::value<bool>()                     , "read sensor data through Zynq");
    //    ("CM"                 , boost::program_options::value<std::vector<std::string> >(), "power up command module"      )  // commented out for now
    //    ("FPGA"               , boost::program_options::value<std::vector<std::string> >(), "FPGAs: program, init, unblock"); // commented out for now               
  fileOptions.add_options()
    ("run_path"           , boost::program_options::value<std::string>()              , "run path"                     )
    ("pid_file"           , boost::program_options::value<std::string>()              , "pid file"                     )
    ("polltime"           , boost::program_options::value<int>()                      , "polling interval"             )
    ("cm_powerup"         , boost::program_options::value<bool>()                     , "power up CM uC"               )
    ("cm_powerup_time"    , boost::program_options::value<int>()                      , "uC power up wait time"        )
    ("sensorsThroughZynq" , boost::program_options::value<bool>()                     , "read sensor data through Zynq")
    //("CM"                 , boost::program_options::value<std::vector<std::string> >(), "command module: power up"     )
    //("FPGA"               , boost::program_options::value<std::vector<std::string> >(), "FPGAs: program, init, unblock")
    /*
      The following add_options lines would not be necessary if we weren't using variables map and store. 
      Actually, if we don't use variables map and store as well as allow for unrecognized options, we don't need 
      to add_options at all. However, since we are already relying heavily on variables maps for overriding
      config file options with command line options, we will continue to use variables maps. With that being said, 
      the problem with using store and variables maps is that store does not allow for any options to have more than one
      value in the config file. This is most likely because it doesn't make sense to store key value pairs in maps
      where the same key can have more than one value. The current "solution" to this is to add_options any options 
      that we already know will have more than one value as vectors. This is clearly not desirable since, for example,
      underneath we have CM1.FPGA as a option and it isn't very robust to be required to know ahead of time what 
      options will definitely be specified more than once ahead of time. Since maps can't have the same key twice,
      there probably is no boost::program_options solution to this. So for now we will use the vector "solution"
      but later on we will most likely have to completely get rid of variables map and store and parse the fileOptions
      and commandOptions ourselves (not to be confused with the command line and config file, we can still use the 
      parse_config_file and parse_command_line functions for those).
    */
    ("CM.NAME"            , boost::program_options::value<std::vector<std::string> >(), "command module names"         )
    ("CM1.FPGA"           , boost::program_options::value<std::vector<std::string> >(), "CM1's FPGAs names"            );
    
  // The different options we will retrieve from the config file/command line. setOption is equivalent to calling commandLineOptions.add() and then fileOptions.add()
//  setOption(&fileOptions, &commandLineOptions, "run_path"          , "run path"                     , runPath);
//  setOption(&fileOptions, &commandLineOptions, "pid_file"          , "pid file"                     , pidFileName);
//  setOption(&fileOptions, &commandLineOptions, "polltime"          , "polling interval"             , polltime_in_seconds);
//  setOption(&fileOptions, &commandLineOptions, "cm_powerup"        , "power up CM uC"               , powerupCMuC);
//  setOption(&fileOptions, &commandLineOptions, "cm_powerup_time"   , "uC power up wait time"        , powerupTime);
//  setOption(&fileOptions, &commandLineOptions, "sensorsThroughZynq", "read sensor data through Zynq", sensorsThroughZynq);
  //  setOption(&fileOptions, &commandLineOptions, "program_kintex"    , "program kintex"               , program_kintex);
  //  setOption(&fileOptions, &commandLineOptions, "program_virtex"    , "program virtex"               , program_virtex);
  //  setOption(&fileOptions, &commandLineOptions, "bringup_FPGA"      , "to program CM FPGAs"          , std::string("dummy"));
  int totalNumConfigFileOptions = 0; 
  boost::program_options::parsed_options configFilePO(&fileOptions); // compiler won't let me merely declare it configFilePO so I initialized it with fileOptions; would be nice to fix this
  boost::program_options::variables_map configFileVM; // for parsing config file
  boost::program_options::variables_map commandLineVM; // for parsing command line
  bool caughtCommandLineException = false;
  bool caughtConfigFileException  = false;

  // The command line must be parsed before the config file so that we know if there is a command line specified config file 
  fprintf(stdout, "Parsing command line now\n");
  try {
    // parse command line
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, commandLineOptions), commandLineVM);
  } catch(const boost::program_options::error &ex) {
    fprintf(stderr, "Caught exception while parsing command line: %s. Try (--). ex: --polltime\n Ignoring all command line arguments\n", ex.what());       
    caughtCommandLineException = true;
  }

  // Check for non default config file
  if((false == caughtCommandLineException) && commandLineVM.count("config_file")) {
    configFile = commandLineVM["config_file"].as<std::string>();
    fprintf(stdout, "config file path: %s (COMMAND LINE)\n", configFile.c_str());
  } else {
    fprintf(stdout, "config file path: %s (DEFAULT)\n"     , configFile.c_str());
  }
  
  // Now the config file may be loaded
  fprintf(stdout, "Reading from config file now\n");
  try {
    // parse config file
    // not using loadConfig() because I need more than just the variables map when looking for CMs and FPGAs
    std::ifstream ifs{configFile};
    fprintf(stderr, "Config file \"%s\" %s\n",configFile.c_str(), (!ifs.fail()) ? "exists" : "does not exist");
    if(ifs) {
      configFilePO = boost::program_options::parse_config_file(ifs, fileOptions, true); // true is great. It allows for unregistered options. ie. if it finds option "foo" in the config file but "foo" is not a registered option boost won't yell at you
      boost::program_options::store(configFilePO, configFileVM);
      totalNumConfigFileOptions = configFilePO.options.size();
    }
  } catch(const boost::program_options::error &ex) {
    fprintf(stdout, "Caught exception in function loadConfig(): %s \n Ignoring config file parameters\n", ex.what());        
    caughtConfigFileException = true;
  }

  // Look at the config file and command line and determine if we should change the parameters from their default values
  // Only run path and pid file are needed for the next bit of code. The other parameters can and should wait until syslog is available.
  setParamValue(&runPath    , "run_path", configFileVM, commandLineVM, false, caughtCommandLineException, caughtConfigFileException); // false for no syslog
  setParamValue(&pidFileName, "pid_file", configFileVM, commandLineVM, false, caughtCommandLineException, caughtConfigFileException);
  // if((false == caughtCommandLineException) && (false == caughtConfigFileException))
  //   {
  //     // no exceptions caught

  //   } 
  // else if((true == caughtCommandLineException) && (true == caughtConfigFileException))
  //   {
  //     // Two exceptions caught, ignore both command line and config file options, aka use default, aka do nothing
  //   }
  // // At this point we know they are opposite signs
  // else if(true == caughtCommandLineException)
  //   {
  //     // Only use config file
  //     boost::program_options::variables_map dummyVM; // I wonder if this will segfault
  //     setParamValue(&runPath    , "run_path", configFileVM, dummyVM, false); // false for no syslog
  //     setParamValue(&pidFileName, "pid_file", configFileVM, dummyVM, false);
  //   }
  // // caughtConfigFileException is definitely true
  // else
  //   {
  //     // Only use command line
  //     boost::program_options::variables_map dummyVM; // I wonder if this will segfault
  //     setParamValue(&runPath    , "run_path", dummyVM, commandLineVM, false); // false for no syslog
  //     setParamValue(&pidFileName, "pid_file", dummyVM, commandLineVM, false);
  //   }
  
// ============================================================================
  // Deamon book-keeping
  // Every daemon program should have one Daemon object. Daemon class functions are functions that all daemons progams have to perform. That is why we made the class.
  Daemon SM_bootDaemon;
  SM_bootDaemon.daemonizeThisProgram(pidFileName, runPath);

  // ============================================================================
  // Now that syslog is available, we can continue to look at the config file and command line and determine if we should change the parameters from their default values.
  setParamValue(&polltime_in_seconds, "polltime"          , configFileVM, commandLineVM, true, caughtCommandLineException, caughtConfigFileException); // true for syslog
  setParamValue(&powerupCMuC        , "cm_powerup"        , configFileVM, commandLineVM, true, caughtCommandLineException, caughtConfigFileException);
  setParamValue(&powerupTime        , "cm_powerup_time"   , configFileVM, commandLineVM, true, caughtCommandLineException, caughtConfigFileException);
  setParamValue(&sensorsThroughZynq , "sensorsThroughZynq", configFileVM, commandLineVM, true, caughtCommandLineException, caughtConfigFileException);

  //  int numberCMs = 0;
  //  int numberFPGAs = 0;
  // find all command modules
  std::vector<CM> allCMs;
  if(false == caughtConfigFileException) {
    for(int i = 0; i < totalNumConfigFileOptions; i++) {
      std::string option = configFilePO.options[i].string_key;
      if(option.compare("CM.NAME") == 0) {
	// found a command module
	std::string cmName = configFilePO.options[i].value[0].c_str();
	CM newCM(cmName, configFilePO);
	allCMs.push_back(newCM);
      }
    }
  }
//  printf("Listing %d command modules found:\n", (int)allCMs.size());
//  for(int i = 0;l i < (int)allCMs.size(); i++) {
//    printf("   %s\n", allCMs[i].name.c_str());
//  }

  // print all cm and fpga info
  for(int c = 0; c < (int)allCMs.size(); c++) {
    syslog(LOG_INFO, "Found CM: %s with info:\n", allCMs[c].name.c_str());
    syslog(LOG_INFO, "power good: %s\n", allCMs[c].powerGood.c_str());
    std::stringstream ss;
    std::string str;
    ss << allCMs[c].powerUp;
    ss >> str;
    syslog(LOG_INFO, "power up  : %s\n", str.c_str());
    //    std::cout << "power up  : " << allCMs[c].powerUp << std::endl;
    for(int f = 0; f < (int)allCMs[c].FPGAs.size(); f++) {
      syslog(LOG_INFO, "In %s found FPGA: %s with info:\n", allCMs[c].name.c_str(), allCMs[c].FPGAs[f].name.c_str());
      syslog(LOG_INFO, "cm     : %s\n", allCMs[c].FPGAs[f].cm.c_str());
      syslog(LOG_INFO, "svfFile: %s\n", allCMs[c].FPGAs[f].svfFile.c_str());
      syslog(LOG_INFO, "xvc    : %s\n", allCMs[c].FPGAs[f].xvc.c_str());
      syslog(LOG_INFO, "c2c    : %s\n", allCMs[c].FPGAs[f].c2c.c_str());
      syslog(LOG_INFO, "done   : %s\n", allCMs[c].FPGAs[f].done.c_str());
      syslog(LOG_INFO, "init   : %s\n", allCMs[c].FPGAs[f].init.c_str());
      syslog(LOG_INFO, "axi    : %s\n", allCMs[c].FPGAs[f].axi.c_str());
      syslog(LOG_INFO, "axilite: %s\n", allCMs[c].FPGAs[f].axilite.c_str()); 
    }
    syslog(LOG_INFO, "\n\n");
  }

//   // In config file: 1. look for command modules and power them up if required 2. look for FPGAs and program, initialize, and unblock if required
//   for(int i = 0; i < totalNumConfigFileOptions; i++) {
//     // The first argument to add_options() (ex. "polltime" or "run_path")
//     std::string optionName = configFilePO.options[i].string_key;    
//    
//     std::string delimeter = " ";
//     
//     // Find command modules
//     if(optionName.compare("CM") != 0) {
//       std::string CMstr = configFilePO.options[i].value[0].c_str();
//       std::vector<std::string> CMvec = split_string(CMstr, delimeter);
//       // Two possibilities 
//       size_t const dontpowerup = 1;
//       size_t const powerup     = 3;
//       if((CMvec.size() == dontpowerup) || (CMvec.size() == powerup)) {
// 	// allocate some more space
// 	std::vector<std::string> dummyVS;
// 	CMs.push_back(dummyVS);
// //	size_t const CMvecSize = CMvec.size();
// //	switch(true) { // all fallthrough
// //	case CMvecSize >= dontpowerup:
// //	  CMs[numberCMs].push_back(CMvec[CM_ID]);
// //	case CMvecSize >= powerup:
// //	  CMs[numberCMs].push_back(CMvec[CM_PWR_GOOD]);
// //	  CMs[numberCMs].push_back(CMvec[CM_PWR_UP]);
// //	}
// 	// See ***** for FPGAs
// 	if(CMvec.size() >= dontpowerup) {
// 	  CMs[numberCMs].push_back(CMvec[CM_ID]);
// 	}
// 	if(CMvec.size() == powerup) {
// 	  CMs[numberCMs].push_back(CMvec[CM_PWR_GOOD]);
// 	  CMs[numberCMs].push_back(CMvec[CM_PWR_UP]);
// 	}
// 	numberCMs++;
//       } else {
// 	syslog(LOG_ERR, "CM in config file only accepts 1 or 3 arguments. You have: %lu\n", CMvec.size());	
//       }
//     }      
//     
//     // Find FPGAs
//     if(optionName.compare("FPGA") != 0) {
//       std::string FPGAstr = configFilePO.options[i].value[0].c_str();
//       std::vector<std::string> FPGAvec = split_string(FPGAstr, delimeter);
//       // Four possibilities
//       size_t const fpgaAndCM      = 2;
//       size_t const alsoProgram    = 6;
//       size_t const alsoInitialize = 7;
//       size_t const alsoUnblock    = 9;
//       if((FPGAvec.size() == fpgaAndCM) || (FPGAvec.size() == alsoProgram) || (FPGAvec.size() == alsoInitialize) || (FPGAvec.size() == alsoUnblock)) {
// 	// allocate some more space
// 	std::vector<std::string> dummyVS;
// 	FPGAs.push_back(dummyVS);
// 	// ***** Don't erase this line. Related to other ***** lines.
//  	// Algorithm: If at least n elems, add the n elems. Then, if at least n1 elems, add the next n1-n elems. Up to some max.
//  	// Ex: If at least two elems, add both. Then, if at least five elems, add next three. For now, up to eight.
//  	// Kind of like a switch case. It'd be nice to figure out how to actually use a switch case since it looks (kind of) nicer instead of if.
// 	if(FPGAvec.size() >= fpgaAndCM) 
// 	  {
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_NAME]);
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_CM]);
// 	  } 	 
// 	if(FPGAvec.size() >= alsoProgram)
// 	  {
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_XVC]);
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_SVF]);
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_C2C]);
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_DONE]);
// 	  }
// 	if(FPGAvec.size() >= alsoInitialize)
// 	  {
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_INIT]);
// 	  }
// 	if(FPGAvec.size() == alsoUnblock)
// 	  {
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_AXI]);
// 	    FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_AXILITE]);
// 	  }
// 	
// 	//	//	size_t const FPGAvecSize = FPGAvec.size();
// // 	switch(FPGAvec.size()) { // all fallthrough
// // 	case fpgaAndCM ... (alsoProgram-1):
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_NAME]);
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_CM]);
// // 	case alsoProgram ... (alsoInitialize-1):
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_XVC]);
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_SVF]);
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_C2C]);
// // 	case alsoInitialize ... (alsoUnblock-1):
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_INIT]);
// // 	case alsoUnblock:
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_AXI]);
// // 	  FPGAs[numberFPGAs].push_back(FPGAvec[FPGA_AXILITE]);
// // 	}
// 	numberFPGAs++;
//       }	else {
// 	syslog(LOG_ERR, "FPGA in config file only accepts %lu, %lu, %lu, or %lu arguments. You have %lu\n", fpgaAndCM, alsoProgram, alsoInitialize, alsoUnblock, FPGAvec.size());
//       }
//     }
//   }
//  syslog(LOG_INFO, "%d valid CMs and %d valid FPGAs found\n", numberCMs, numberFPGAs);
//  syslog(LOG_INFO, "More information about the CMs and FPGAs found coming soon...\n");
  // Should print more information about the CMs and FPGAs found
  
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
  SM_bootDaemon.SetLoop(true);

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
    // Power up CM(s) and program, initialize, and unblock FPGA(s), if required
    
    // Power up CMs and program FPGAs
    for(int i = 0; i < (int)allCMs.size(); i++) {
      int wait_time = 5; // 5 is 1 second
      //      std::string tempCM_ID = CMs[i][CM_ID];
      //      tempCM_ID.erase(0,2); // drop the 'CM' (ex. for CM1, tempCM_ID is 1)
      if(allCMs[i].powerUp) {
	bool success = SM->PowerUpCM(allCMs[i].ID, wait_time);      
	if(success) {
	  syslog(LOG_INFO, "%s is powered up\n", allCMs[i].name.c_str());
	} else {
	  syslog(LOG_ERR, "%s failed to powered up in time\n", allCMs[i].name.c_str());
	}
	// check that PWR_GOOD is 1
	if(checkNode(SM, allCMs[i].powerGood, 1)) {
	  std::string str;
	  std::stringstream ss;
	  ss << allCMs[i].powerGood;
	  ss >> str;
	  syslog(LOG_INFO, "%s is 1\n", str.c_str());
	}
      } else {
	syslog(LOG_INFO, "No power up required for: %s\n", allCMs[i].name.c_str());
      }
    
      // program FPGAs
      if(allCMs[i].powerUp) {
	for(int f = 0; f < (int)allCMs[i].FPGAs.size(); f++) {
	  if(allCMs[f].FPGAs[f].program) {
	    syslog(LOG_INFO, "%s has program = true. Attempting to program...\n", allCMs[i].FPGAs[f].name.c_str());
	    int const success =  0;
	    int const fail    = -1;
	    int const nofile  = -2;
	    
//	    int const programmingFailed     = 0;
//	    int const programmingSuccessful = 1;
	    // assert 0 to done bit
	    //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
	    switch(bringupCMFPGAs(SM, allCMs[i].FPGAs[f])) {
	    case success:
	      syslog(LOG_ERR, "Bringing up %s: %s FPGA succeeded. Setting %s to 1\n", allCMs[i].name.c_str(), allCMs[i].FPGAs[f].name.c_str(), allCMs[i].FPGAs[f].done.c_str());
	      // write 1 to done bit
	      //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingSuccessful);
	      break;
	    case fail:
	      // assert 0 to done bit (paranoid)
	      syslog(LOG_ERR, "Bringing up %s: %s FPGA failed. Setting %s to 0\n", allCMs[i].name.c_str(), allCMs[i].FPGAs[f].name.c_str(), allCMs[i].FPGAs[f].done.c_str());
	      //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
	      break;
	    case nofile:
	      // assert 0 to done bit (paranoid)
	      syslog(LOG_ERR, "svf file %s does not exist for %s FPGA. Setting %s to 0\n", allCMs[i].FPGAs[f].svfFile.c_str(), allCMs[i].FPGAs[f].name.c_str(), allCMs[i].FPGAs[f].done.c_str());
	      //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
	      break;
	    }
	  } else {
	    syslog(LOG_INFO, "%s will not be programmed because it has program = false\n", allCMs[i].FPGAs[f].name.c_str());
	  }
	}
      } else {
	syslog(LOG_INFO, "%s has powerUp = false. None of its FPGAs will be powered up\n", allCMs[i].name.c_str());
      }

      // Print firmware build date for FPGA
      printBuildDate(SM, allCMs[i].ID);
    }
    
    //    // Programming
//    for(int i = 0; i < (int)FPGAs.size(); i++) {
//      int const success =  0;
//      int const fail    = -1;
//      int const nofile  = -2;
//      // assert 0 to done bit
//      //	SM->RegWriteRegister(FPGAs[i][FPGA_DONE].c_str(), FPGA_PROGRAM_FAILED);
//      switch(bringupCMFPGAs(SM, FPGAs[i])) {
//      case success:
//	syslog(LOG_ERR, "Bringing up %s FPGA succeeded. Setting %s to 1\n", FPGAs[i][FPGA_NAME].c_str(), FPGAs[i][FPGA_DONE].c_str());
//	// write 1 to done bit
//	//	SM->RegWriteRegister(FPGAs[i][FPGA_DONE].c_str(), FPGA_PROGRAMMED);
//	break;
//      case fail:
//	// assert 0 to done bit (paranoid)
//	syslog(LOG_ERR, "Bringing up %s FPGA failed. Setting %s to 0\n", FPGAs[i][FPGA_NAME].c_str(), FPGAs[i][FPGA_DONE].c_str());
//      	//	SM->RegWriteRegister(FPGAs[i][FPGA_DONE].c_str(), FPGA_PROGRAM_FAILED);
//	break;
//      case nofile:
//	// assert 0 to done bit (paranoid)
//	syslog(LOG_ERR, "svf file %s does not exist. Setting %s to 0\n", FPGAs[i][FPGA_SVF].c_str(), FPGAs[i][FPGA_DONE].c_str());
//      	//	SM->RegWriteRegister(FPGAs[i][FPGA_DONE].c_str(), FPGA_PROGRAM_FAILED);
//	break;
//      }
//    }

//int kintex = 1;
//    switch(programCMFPGA(SM, kintex, program_kintex)) {
//    case 0:
//      // fail
//      return -1;
//    case 1: 
//      // success
//      break;
//    case 2:
//      // A CM svf file does not exist
//      syslog(LOG_ERR, "CM%d svf file does not exist\n", kintex);
//      return -1;
//    }
//    
//    int virtex = 2;
//    switch(programCMFPGA(SM, kintex, program_virtex)) {
//    case 0:
//      // fail
//      return -1;
//    case 1: 
//      // success
//      break;
//    case 2:
//      // A CM svf file does not exist
//      syslog(LOG_ERR, "CM%d svf file does not exist\n", virtex);
//      return -1;
//    }

    // Do we set this bit if the CM files do not exist?
    //Set the power-up done bit to 1 for the IPMC to read
// #ifdef SAY_STATUS_DONE_ANYWAY
//     SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",1);
//     syslog(LOG_INFO,"Set STATUS.DONE to 1\n");
// #endif

    // ==================================
    // Main DAEMON loop
    syslog(LOG_INFO,"Starting Monitoring loop\n");    
    
    uint32_t CM_running = 0;
    while(SM_bootDaemon.GetLoop()) {
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
