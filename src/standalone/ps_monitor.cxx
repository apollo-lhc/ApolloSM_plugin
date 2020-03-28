#include <utmpx.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <ApolloSM/ApolloSM.hh>
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

//umask
#include <sys/types.h>
#include <sys/stat.h>

#include <BUException/ExceptionBase.hh>

#include <boost/program_options.hpp>  //for configfile parsing
#include <fstream>

#include <syslog.h>  ///for syslog
#include <standalone/daemon.hh>       // daemonizeThisProgram // changeSignal // loop
#include <standalone/parseOptions.hh> // setOptions // setParamValues // loadConfig

#define SEC_IN_US  1000000
#define NS_IN_US 1000

#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_CONFIG_FILE "/etc/ps_monitor"
#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_PID_FILE    "/var/run/ps_monitor.pid"



// ====================================================================================================
// Kill program if it is in background
//bool static volatile loop;
//
//void static signal_handler(int const signum) {
//  if(SIGINT == signum || SIGTERM == signum) {
//    loop = false;
//  }
//}

// ====================================================================================================
long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec)/NS_IN_US);
}

// ==================================================

int main(int argc, char ** argv) {

  // parameters to get from command line or config file (config file itself will not be in the config file, obviously)
  std::string configFile  = DEFAULT_CONFIG_FILE;
  std::string runPath     = DEFAULT_RUN_DIR;
  std::string pidFileName = DEFAULT_PID_FILE;
  int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;

  // parse command line and config file to set parameters
  boost::program_options::options_description fileOptions{"File"}; // for parsing config file
  boost::program_options::options_description commandLineOptions{"Options"}; // for parsing command line
  commandLineOptions.add_options()
    ("config_file",
     boost::program_options::value<std::string>(),
     "config_file"); // This is the only option not also in the file option (obviously)
  setOption(&fileOptions, &commandLineOptions, "run_path", "run path"         , runPath);
  setOption(&fileOptions, &commandLineOptions, "pid_file", "pid file"         , pidFileName);
  setOption(&fileOptions, &commandLineOptions, "polltime", "polling interval" , polltime_in_seconds);
  boost::program_options::variables_map configFileVM; // for parsing config file
  boost::program_options::variables_map commandLineVM; // for parsing command line

  // The command line must be parsed before the config file so that we know if there is a command line specified config file
  fprintf(stdout, "Parsing command line now\n");
  try {
    // parse command line
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, commandLineOptions), commandLineVM);
  } catch(const boost::program_options::error &ex) {
    fprintf(stderr, "Caught exception while parsing command line: %s. Try (--). ex: --polltime \nTerminating ps_monitor\n", ex.what());       
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
    fprintf(stdout, "Caught exception in function loadConfig(): %s \nTerminating ps_monitor\n", ex.what());        
    return -1;
  }
 
  // Look at the config file and command line and determine if we should change the parameters from their default values
  // Only run path and pid file are needed for the next bit of code. The other parameters can and should wait until syslog is available.
  setParamValue(&runPath    , "run_path", configFileVM, commandLineVM, false);
  setParamValue(&pidFileName, "pid_file", configFileVM, commandLineVM, false);
//  setParamValue(&polltime_in_seconds, "polltime"   , configFileVM, commandLineVM, true);

  // ============================================================================
  // Deamon book-keeping
  // Every daemon program should have one Daemon object. Daemon class functions are functions that all daemons progams have to perform. That is why we made the class.
  Daemon ps_monitorDaemon;
  ps_monitorDaemon.daemonizeThisProgram(pidFileName, runPath);

//  pid_t pid, sid;
//  pid = fork();
//  if(pid < 0){
//    //Something went wrong.
//    //log something
//    exit(EXIT_FAILURE);
//  }else if(pid > 0){
//    //We are the parent and created a child with pid pid
//    FILE * pidFile = fopen(pidFileName.c_str(),"w");
//    fprintf(pidFile,"%d\n",pid);
//    fclose(pidFile);
//    exit(EXIT_SUCCESS);
//  }else{
//    // I'm the child!
//    //open syslog
//    openlog(NULL,LOG_CONS|LOG_PID,LOG_DAEMON);
//  }
//
//  
//  //Change the file mode mask to allow read/write
//  umask(0);
//
//  //Start logging
//  syslog(LOG_INFO,"Opened log file\n");
//
//  // create new SID for the daemon.
//  sid = setsid();
//  if (sid < 0) {
//    syslog(LOG_ERR,"Failed to change SID\n");
//    exit(EXIT_FAILURE);
//  }
//  syslog(LOG_INFO,"Set SID to %d\n",sid);
//
//  //Move to RUN_DIR
//  if ((chdir(runPath.c_str())) < 0) {
//    syslog(LOG_ERR,"Failed to change path to \"%s\"\n",runPath.c_str());    
//    exit(EXIT_FAILURE);
//  }
//  syslog(LOG_INFO,"Changed path to \"%s\"\n", runPath.c_str());    
//
//  //Everything looks good, close the standard file fds.
//  close(STDIN_FILENO);
//  close(STDOUT_FILENO);
//  close(STDERR_FILENO);

  
  // ============================================================================
  // Now that syslog is available, we can continue to look at the config file and command line and determine if we should change the parameters from their default values.
  setParamValue(&polltime_in_seconds, "polltime", configFileVM, commandLineVM, true);

  // ============================================================================
  // Daemon code setup

  // ====================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  ps_monitorDaemon.changeSignal(&sa_INT , &old_sa, SIGINT);
  ps_monitorDaemon.changeSignal(&sa_TERM, NULL   , SIGTERM);
//  memset(&sa_INT ,0,sizeof(sa_INT)); //Clear struct
//  memset(&sa_TERM,0,sizeof(sa_TERM)); //Clear struct
//  //setup SA
//  sa_INT.sa_handler  = signal_handler;
//  sa_TERM.sa_handler = signal_handler;
//  sigemptyset(&sa_INT.sa_mask);
//  sigemptyset(&sa_TERM.sa_mask);
//  sigaction(SIGINT,  &sa_INT , &old_sa);
//  sigaction(SIGTERM, &sa_TERM, NULL);
  ps_monitorDaemon.loop = true;

  // ==================================================
  // If /var/run/utmp does not exist we are done
  {
    FILE * file = fopen("/var/run/utmp","r");
    if(NULL == file) {
      syslog(LOG_INFO,"Utmp file does not exist. Terminating countUsers\n");
      return -1;
    }
  }

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
    uCnt.GetUserCounts(superUsers,normalUsers);
    SM->RegWriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
    SM->RegWriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);	  
 
    while(ps_monitorDaemon.loop){
      readSet_ret = readSet;
      int pselRet = pselect(maxFDp1,&readSet_ret,NULL,NULL,&timeout,NULL);
      if(0 == pselRet){
	//timeout, do CPU/mem monitoring
	uint32_t mon;
	mon = MemUsage()*100; //Scale the value by 100 to get two decimal places for reg   
	SM->RegWriteRegister("PL_MEM.ARM.MEM_USAGE",mon);
	mon = CPUUsage()*100; //Scale the value by 100 to get two decimal places for reg   
	SM->RegWriteRegister("PL_MEM.ARM.CPU_LOAD",mon);
	float days,hours,minutes;
	Uptime(days,hours,minutes);
	SM->RegWriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.DAYS",uint32_t(100.0*days));
	SM->RegWriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.HOURS",uint32_t(100.0*hours));
	SM->RegWriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.MINS",uint32_t(100.0*minutes));
	
      }else if(pselRet > 0){
	//a FD is readable. 
	if(FD_ISSET(fdUserCount,&readSet_ret)){
	  if(uCnt.ProcessWatchEvent()){
	    uCnt.GetUserCounts(superUsers,normalUsers);
	    SM->RegWriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
	    SM->RegWriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);
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
