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

#include <syslog.h>  ///for syslog

#define SEC_IN_US 1000000
#define NS_IN_US 1000

#define DEFAULT_PID_FILE "ps_monitor.pid"
// ================================================================================
// Setup for boost program_options
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/BUTool"
namespace po = boost::program_options;

po::variables_map getVariableMap(int argc, char** argv, po::options_description options, std::string configFile) {
  //container for prog options grabbed from commandline and config file
  po::variables_map progOptions;
  //open config file
  std::ifstream File(configFile);

  //Get options from command line
  try { 
    po::store(parse_command_line(argc, argv, options), progOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    std::cout << options << std::endl;
    return 0;
  }

  //If configFile opens, get options from config file
  if(File) { 
    try{ 
      po::store(parse_config_file(File,options,true), progOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
      std::cout << options << std::endl;
      return 0; 
    }
  }
 
  return progOptions;
}

// ====================================================================================================
// Kill program if it is in background
bool static volatile loop;

void static signal_handler(int const signum) {
  if(SIGINT == signum || SIGTERM == signum) {
    loop = false;
  }
}

// ====================================================================================================
long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec)/NS_IN_US);
}

// ==================================================

int main(int argc, char ** argv) {

  // ============================================================================
  // Read from configuration file and set up parameters

  //Set up program options
  po::options_description options("cmpwrdown options");
  options.add_options()
    ("help,h",    "Help screen")
    ("POLLTIME_IN_SECONDS,s",     po::value<int>()->default_value(10), "Default polltime in seconds")
    ("RUN_DIR,r",                 po::value<std::string>()->default_value("/opt/address_table"),           "run path")
    ("PID_DIR,d",                 po::value<std::string>()->default_value("/var/run/"),                    "pud path");
    //("DEFAULT_CONNECTION_FILE,C", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file")
  
  //setup for loading program options
  po::variables_map progOptions = getVariableMap(argc, argv, options, DEFAULT_CONFIG_FILE);

  //help option
  if(progOptions.count("help")){
    std::cout << options << '\n';
    return 0;
  }

  //Set polltime_in_seconds
  int polltime_in_seconds = 0;
  if (progOptions.count("POLLTIME_IN_SECONDS")){polltime_in_seconds = progOptions["POLLTIME_IN_SECONDS"].as<int>();}
  //Set runPath
  std::string runPath = "";
  if (progOptions.count("RUN_DIR")){runPath = progOptions["RUN_DIR"].as<std::string>();}
  //set pidFileName
  std::string pidPath = "";
  if (progOptions.count("PID_DIR")){pidPath = progOptions["PID_DIR"].as<std::string>();}
  std::string pidFileName = pidPath + DEFAULT_PID_FILE;
  //Set connectionFile ??
    

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
    FILE * pidFile = fopen(pidFileName.c_str(),"w");
    fprintf(pidFile,"%d\n",pid);
    fclose(pidFile);
    exit(EXIT_SUCCESS);
  }else{
    // I'm the child!
    //open syslog
    openlog(NULL,LOG_CONS|LOG_PID,LOG_DAEMON);
  }

  
  //Change the file mode mask to allow read/write
  umask(0);

  //Start logging
  syslog(LOG_INFO,"Opened log file\n");

  // create new SID for the daemon.
  sid = setsid();
  if (sid < 0) {
    syslog(LOG_ERR,"Failed to change SID\n");
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO,"Set SID to %d\n",sid);

  //Move to RUN_DIR
  if ((chdir(runPath.c_str())) < 0) {
    syslog(LOG_ERR,"Failed to change path to \"%s\"\n",runPath.c_str());    
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO,"Changed path to \"%s\"\n", runPath.c_str());    

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
    int inRate, outRate;
    int networkMon_return = networkMonitor(inRate, outRate); //run once to burn invalid first values
    uCnt.GetUserCounts(superUsers,normalUsers);
    try {
      SM->RegWriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
      SM->RegWriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);	  
    }catch(std::exception const & e){
      syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
    }
    while(loop){
      readSet_ret = readSet;
      int pselRet = pselect(maxFDp1,&readSet_ret,NULL,NULL,&timeout,NULL);
      if(0 == pselRet){
	//timeout, do CPU/mem monitoring
	uint32_t mon;
	mon = MemUsage()*100; //Scale the value by 100 to get two decimal places for reg   
	try {
	  SM->RegWriteRegister("PL_MEM.ARM.MEM_USAGE",mon);
	}catch(std::exception const & e){
	  syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	}
	mon = CPUUsage()*100; //Scale the value by 100 to get two decimal places for reg   
	try {
	  SM->RegWriteRegister("PL_MEM.ARM.CPU_LOAD",mon);
	}catch(std::exception const & e){
	  syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	}
	networkMon_return = networkMonitor(inRate, outRate);
	if(!networkMon_return){ //networkMonitor was successful
	  try {
	    SM->RegWriteRegister("PL_MEM.NETWORK.ETH0.RX",uint32_t(inRate));
	    SM->RegWriteRegister("PL_MEM.NETWORK.ETH0.TX",uint32_t(outRate));
	  }catch(std::exception const & e){
	    syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	  }
	} else { //networkMonitor failed
	  syslog(LOG_ERR, "Error in networkMonitor, return %d\n", networkMon_return);
	}
	float days,hours,minutes;
	Uptime(days,hours,minutes);
	try {
	  SM->RegWriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.DAYS",uint32_t(100.0*days));
	  SM->RegWriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.HOURS",uint32_t(100.0*hours));
	  SM->RegWriteRegister("PL_MEM.ARM.SYSTEM_UPTIME.MINS",uint32_t(100.0*minutes));
	} catch(std::exception const & e){
	  syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
	}
      }else if(pselRet > 0){
	//a FD is readable. 
	if(FD_ISSET(fdUserCount,&readSet_ret)){
	  if(uCnt.ProcessWatchEvent()){
	    uCnt.GetUserCounts(superUsers,normalUsers);
	    try {
	      SM->RegWriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
	      SM->RegWriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);
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
