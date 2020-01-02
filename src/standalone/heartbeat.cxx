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

#include <boost/program_options.hpp>  //for configfile parsing
#include <fstream>

#include <tclap/CmdLine.h> //TCLAP parser

#include <syslog.h>  ///for syslog

#define SEC_IN_US  1000000
#define NS_IN_US 1000

#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_CONFIG_FILE "/etc/heartbeat"
#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_PID_FILE    "/var/run/heartbeat.pid"


// ====================================================================================================
// signal handling
bool static volatile loop;
void static signal_handler(int const signum) {
  if(SIGINT == signum || SIGTERM == signum) {
    loop = false;
  }
}



// ====================================================================================================
// Read from config files and set up all parameters
// For further information see https://theboostcpplibraries.com/boost.program_options

boost::program_options::variables_map loadConfig(std::string const & configFileName,
						 boost::program_options::options_description const & fileOptions) {
  // This is a container for the information that fileOptions will get from the config file
  boost::program_options::variables_map vm;  

  // Check if config file exists
  std::ifstream ifs{configFileName};
  syslog(LOG_INFO, "Config file \"%s\" %s\n",configFileName.c_str(), (!ifs.fail()) ? "exists" : "does not exist");

  if(ifs) {
    // If config file exists, parse ifs into fileOptions and store information from fileOptions into vm
    boost::program_options::store(parse_config_file(ifs, fileOptions), vm);
  }

  return vm;
}




// ====================================================================================================
long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec)/NS_IN_US);
}




// ====================================================================================================
int main(int argc, char** argv) { 

  TCLAP::CmdLine cmd("ApolloSM PS Heartbeat");
  TCLAP::ValueArg<std::string> configFile("c",                 //one char flag
					  "config_file",       // full flag name
					  "config file",       //description
					  false,               //required argument
					  DEFAULT_CONFIG_FILE, //Default value
					  "string",            //type
					  cmd);
  TCLAP::ValueArg<std::string>    runPath    ("r","run_path","run path",false,DEFAULT_RUN_DIR ,"string",cmd);
  TCLAP::ValueArg<std::string>    pidFileName("p","pid_file","pid file",false,DEFAULT_PID_FILE,"string",cmd);

  try {
    cmd.parse(argc, argv);
  }catch (TCLAP::ArgException &e) {
    fprintf(stderr, "Failed to Parse Command Line\n");
    return -1;
  }



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
    FILE * pidFile = fopen(pidFileName.getValue().c_str(),"w");
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
  if ((chdir(runPath.getValue().c_str())) < 0) {
    syslog(LOG_ERR,"Failed to change path to \"%s\"\n",runPath.getValue().c_str());    
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO,"Changed path to \"%s\"\n", runPath.getValue().c_str());    

  //Everything looks good, close the standard file fds.
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  
  // ============================================================================
  // Read from configuration file and set up parameters
  syslog(LOG_INFO,"Reading from config file now\n");
  int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;
 
  // fileOptions is for parsing config files
  boost::program_options::options_description fileOptions{"File"};
  //sigh... with boost comes compilcated c++ magic
  fileOptions.add_options() 
    ("polltime", 
     boost::program_options::value<int>()->default_value(DEFAULT_POLLTIME_IN_SECONDS), 
     "polling interval");
  boost::program_options::variables_map configOptions;  
  try{
    configOptions = loadConfig(configFile.getValue(),fileOptions);
    // Check for information in configOptions
    if(configOptions.count("polltime")) {
      polltime_in_seconds = configOptions["polltime"].as<int>();
    }  
    syslog(LOG_INFO,
	   "Setting poll time to %d seconds (%s)\n",
	   polltime_in_seconds, 
	   configOptions.count("polltime") ? "CONFIG FILE" : "DEFAULT");
        
  }catch(const boost::program_options::error &ex){
    syslog(LOG_INFO, "Caught exception in function loadConfig(): %s \n", ex.what());    
  }


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

  long update_period_us = polltime_in_seconds*SEC_IN_US; //sleep time in microseconds


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

    // ==================================
    // Main DAEMON loop
    syslog(LOG_INFO,"Starting heartbeat\n");
    
    while(loop) {
      // loop start time
      clock_gettime(CLOCK_REALTIME, &startTS);

      //=================================
      //Do work
      //=================================

      //PS heartbeat
      SM->RegReadRegister("SLAVE_I2C.HB_SET1");
      SM->RegReadRegister("SLAVE_I2C.HB_SET2");

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
    syslog(LOG_ERR,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());          
  }catch(std::exception const & e){
    syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
  }
  
  //PS heartbeat
  SM->RegReadRegister("SLAVE_I2C.HB_SET1");
  SM->RegReadRegister("SLAVE_I2C.HB_SET2");
  
  //Clean up
  if(NULL != SM) {
    delete SM;
  }
  
  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"heartbeat Daemon ended\n");
  
  return 0;
}
