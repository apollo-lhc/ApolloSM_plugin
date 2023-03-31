#include <utmpx.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>

#include <standalone/userCount.hh>
#include <standalone/lnxSysMon.hh>

#include <errno.h>
#include <string.h>

//pselect stuff
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

//signals
#include <signal.h>

#include <syslog.h>  ///for syslog


#include <boost/program_options.hpp>
#include <standalone/optionParsing.hh>
#include <standalone/optionParsing_bool.hh>
#include <standalone/daemon.hh>


#include <fstream>
#include <iostream>


#define SEC_IN_US 1000000
#define NS_IN_US 1000

// ================================================================================
#define DEFAULT_CONFIG_FILE "/etc/ps_monitor"

#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_RUN_DIR "/fw/SM/address_table"
#define DEFAULT_PID_FILE "/var/run/ps_monitor.pid"
#define DEFAULT_CONN_FILE "/fw/SM/address_table/connections.xml"

namespace po = boost::program_options;


// ====================================================================================================
long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec)/NS_IN_US);
}

// ==================================================

int main(int argc, char ** argv) {


  //=======================================================================
  // Set up program options
  //=======================================================================
  //Command Line options
  po::options_description cli_options("ps_monitor options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("POLLTIME_IN_SECONDS,s", po::value<int>(),         "polltime in seconds")
    ("RUN_DIR,r",             po::value<std::string>(), "run path")
    ("CONN_FILE,c",           po::value<std::string>(), "Path to the XML connections file")
    ("PID_FILE,d",            po::value<std::string>(), "pid file")
    ("config_file",           po::value<std::string>(), "config file");


  //Config File options
  po::options_description cfg_options("ps_monitor options");
  cfg_options.add_options()
    ("POLLTIME_IN_SECONDS", po::value<int>(),         "polltime in seconds")
    ("RUN_DIR",             po::value<std::string>(), "run path")
    ("CONN_FILE",           po::value<std::string>(), "Path to the XML connections file")
    ("PID_FILE",            po::value<std::string>(), "pid file");


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


  //Set polltime_in_seconds
  int polltime_in_seconds = GetFinalParameterValue(std::string("POLLTIME_IN_SECONDS"), allOptions, DEFAULT_POLLTIME_IN_SECONDS);
  //Set runPath
  std::string runPath     = GetFinalParameterValue(std::string("RUN_DIR"),             allOptions, std::string(DEFAULT_RUN_DIR));
  //set pidFileName
  std::string pidFileName = GetFinalParameterValue(std::string("PID_FILE"),            allOptions, std::string(DEFAULT_PID_FILE));
  // Get XML connection file for ApolloSM
  std::string connectionFile = GetFinalParameterValue(std::string("CONN_FILE"),        allOptions, std::string(DEFAULT_CONN_FILE));

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


  // ==================================================
  // If /var/run/utmp does not exist we are done
  {
    FILE * file = fopen("/var/run/utmp","r");
    if(NULL == file) {
      syslog(LOG_INFO,"Utmp file does not exist. Terminating countUsers\n");
      return -1;
    }
  }

  //=======================================================================
  // Start ps monitor
  //=======================================================================
  ApolloSM * SM = NULL;
  try{
    // ==================================
    // Initialize ApolloSM
    syslog(LOG_INFO,"Using connections file: %s\n", connectionFile.c_str());      
    std::vector<std::string> arg;
    arg.push_back(connectionFile);
    SM = new ApolloSM(arg);
    if(NULL == SM){
      syslog(LOG_ERR,"Failed to create new ApolloSM\n");
      exit(EXIT_FAILURE);
    }else{
      syslog(LOG_INFO,"Created new ApolloSM\n");      
    }

    //vars for pselect
    fd_set readSet,readSet_ret;
    FD_ZERO(&readSet);
    struct timespec timeout = {polltime_in_seconds,0};
    int maxFDp1 = 0;
    
    //Create a usercount process
    userCount uCnt;
    int fdUserCount = uCnt.initNotify();
    syslog(LOG_INFO,"iNotify setup on FD %d\n",fdUserCount);
    FD_SET(fdUserCount,&readSet);
    if(fdUserCount >= maxFDp1){
      maxFDp1 = fdUserCount+1;
    }


    // ==================================
    // Main DAEMON loop
    syslog(LOG_INFO,"Starting PS monitor\n");

    // ==================================================
    // All the work

    //Do one read of users file before we start our loop
    uint32_t superUsers,normalUsers;
    int inRate, outRate;
    int networkMon_return = networkMonitor(inRate, outRate); //run once to burn invalid first values
    uCnt.GetUserCounts(superUsers,normalUsers);
    try {
      SM->WriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
      SM->WriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);	  
    }catch(std::exception const & e){
      syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
    }
    while(daemon.GetLoop()){
      readSet_ret = readSet;
      int pselRet = pselect(maxFDp1,&readSet_ret,NULL,NULL,&timeout,NULL);
      if(0 == pselRet){
	//timeout, do CPU/mem monitoring

	//Memory Usage
	uint32_t mon;
	mon = MemUsage()*100; //Scale the value by 100 to get two decimal places for reg   
	try {
	  SM->WriteRegister("PL_MEM.ARM.MEM_USAGE",mon);
	}catch(std::exception const & e){
	  syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	}

	//CPU usage
	mon = CPUUsage()*100; //Scale the value by 100 to get two decimal places for reg   
	try {
	  SM->WriteRegister("PL_MEM.ARM.CPU_LOAD",mon);
	}catch(std::exception const & e){
	  syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	}

	//Network usage
	networkMon_return = networkMonitor(inRate, outRate);
	if(!networkMon_return){ //networkMonitor was successful
	  try {
	    SM->WriteRegister("PL_MEM.NETWORK.ETH0.RX",uint32_t(inRate));
	    SM->WriteRegister("PL_MEM.NETWORK.ETH0.TX",uint32_t(outRate));
	  }catch(std::exception const & e){
	    syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	  }
	} else { //networkMonitor failed
	  syslog(LOG_ERR, "Error in networkMonitor, return %d\n", networkMon_return);
	}

	//Uptime
	float raw_sec,days,hours,minutes;
	raw_sec = Uptime(days,hours,minutes);
	try {
	  SM->WriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.DAYS",uint32_t(100.0*days));
	  SM->WriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.HOURS",uint32_t(100.0*hours));
	  SM->WriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.MINS",uint32_t(100.0*minutes));
	  SM->WriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.RAW",uint32_t(raw_sec));
	} catch(std::exception const & e){
	  syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	}

	//Zynq temp
	double tempValue = 0;
	SM->ReadConvert("MONITOR.TEMP",tempValue);
	SM->WriteRegister("SLAVE_I2C.S1.SM.TEMP.TEMP",uint8_t(tempValue));
	SM->ReadConvert("MONITOR.TEMP_MAX",tempValue);
	SM->WriteRegister("SLAVE_I2C.S1.SM.TEMP.TEMP_MAX",uint8_t(tempValue));
	SM->ReadConvert("MONITOR.TEMP_MIN",tempValue);
	SM->WriteRegister("SLAVE_I2C.S1.SM.TEMP.TEMP_MIN",uint8_t(tempValue));

      }else if(pselRet > 0){
	//a FD is readable. 
	if(FD_ISSET(fdUserCount,&readSet_ret)){
	  //User count change
	  if(uCnt.ProcessWatchEvent()){
	    uCnt.GetUserCounts(superUsers,normalUsers);
	    try {
	      SM->WriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
	      SM->WriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);
	    }catch(std::exception const & e){
	      syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	    }
	  }
	}
      }else{
	syslog(LOG_ERR,"Error in pselect %d(%s)",errno,strerror(errno));
      }
    }
  }catch(BUException::exBase const & e){
    syslog(LOG_ERR,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());          
  }catch(std::exception const & e){
    syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
  }

  // ==================================================
  // Clean up. Close and delete everything.

  // Delete SM
  if(NULL != SM) {
    delete SM;
  }

  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"PS Monitor Daemon ended\n");
  return 0;

}
