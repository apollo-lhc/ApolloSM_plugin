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

#include <syslog.h>  ///for syslog

#define SEC_IN_US 1000000
#define NS_IN_US 1000
// ================================================================================
// Setup for boost program_options
#include <boost/program_options.hpp>
#include <standalone/progOpt.hh>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/htmlStatus"
#define DEFAULT_RUN_DIR "/opt/address_table/"
#define DEFAULT_PID_FILE "/var/run/htmlStatus.pid"
#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_OUTFILE "/var/www/lighttpd/index.html"
#define DEFAULT_LOG_LEVEL 1
#define DEFAULT_OUTPUT_TYPE "HTML"
namespace po = boost::program_options;

// ====================================================================================================
// signal handling
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

// ====================================================================================================
// MAIN
// ====================================================================================================
int main(int argc, char** argv) {

  // ============================================================================
  // Read from configuration file and set up parameters
  syslog(LOG_INFO,"Reading from config file now\n");
 
  //=======================================================================
  // Set up program options
  //=======================================================================
  //Command Line options
  po::options_description cli_options("cmpwrdown options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("RUN_DIR",             po::value<std::string>()->implicit_value(""), "run path")
    ("PID_FILE",             po::value<std::string>()->implicit_value(""), "pid path")
    ("POLLTIME_IN_SECONDS", po::value<int>()->implicit_value(0),          "polling interval")
    ("OUTFILE",             po::value<std::string>()->implicit_value(""), "html output file")
    ("LOG_LEVEL",           po::value<int>()->implicit_value(0),          "status display log level")
    ("OUTPUT_TYPE",         po::value<std::string>()->implicit_value(""), "html output type");

  //Config File options
  po::options_description cfg_options("cmpwrdown options");
  cfg_options.add_options()
    ("RUN_DIR",             po::value<std::string>(),  "run path")
    ("PID_FILE",             po::value<std::string>(),  "pud path")
    ("POLLTIME_IN_SECONDS", po::value<int>(),          "polling interval")
    ("OUTFILE",             po::value<std::string>(),  "html output file")
    ("LOG_LEVEL",           po::value<int>(),          "status display log level")
    ("OUTPUT_TYPE",         po::value<std::string>(), "html output type");

  //variable_maps for holding program options
  po::variables_map cli_map;
  po::variables_map cfg_map;

  //Store command line and config file arguments into cli_map and cfg_map
  try {
    cli_map = storeCliArguments(cli_options, argc, argv);
    cfg_map = storeCfgArguments(cfg_options, DEFAULT_CONFIG_FILE);
  } catch (std::exception &e) {
    std::cout << cli_options << std::endl;
    return 0;
  }

  //Help option - ends program
  if(cli_map.count("help")){
    std::cout << cli_options << '\n';
    return 0;
  }
 
  //Set run dir
  std::string runPath = DEFAULT_RUN_DIR;
  setOptionValue(runPath, "RUN_DIR", cli_map, cfg_map);
  //set pidFileName
  std::string pidFileName = DEFAULT_PID_FILE;
  setOptionValue(pidFileName, "PID_FILE", cli_map, cfg_map);
  //Set polltime
  int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;
  setOptionValue(polltime_in_seconds, "POLLTIME_IN_SECONDS", cli_map, cfg_map);
  syslog(LOG_INFO, "Setting poll time to %d seconds (%s)\n", polltime_in_seconds, cli_map.count("polltime") ? "CONFIG FILE" : "DEFAULT");
  //Set outfile
  std::string outfile = DEFAULT_OUTFILE;
  setOptionValue(outfile, "OUTFILE", cli_map, cfg_map);
  syslog(LOG_INFO, "Sending output to %s (%s)\n", outfile.c_str(), cli_map.count("outfile") ? "CONFIG FILE" : "DEFAULT");
  //Set log level
  int logLevel = DEFAULT_LOG_LEVEL;
  setOptionValue(logLevel, "LOG_LEVEL", cli_map, cfg_map);
  syslog(LOG_INFO, "Setting log level to %d (%s)\n", logLevel, cli_map.count("log_level") ? "CONFIG FILE" : "DEFAULT");
  //Set output type
  std::string outputType = DEFAULT_OUTPUT_TYPE;
  setOptionValue(outputType, "OUTPUT_TYPE", cli_map, cfg_map);
  syslog(LOG_INFO, "Sending output type to %s (%s)\n", outputType.c_str(), cli_map.count("output_type") ? "CONFIG FILE" : "DEFAULT");

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

  // ====================================
  // for counting time
  struct timespec startTS;
  struct timespec stopTS;

  long update_period_us = polltime_in_seconds*SEC_IN_US; //sleep time in microseconds


  //=======================================================================
  // Generate HTML status
  //=======================================================================
  //Create ApolloSM class
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
    syslog(LOG_INFO,"Starting htmlStatus\n");

    while(loop) {
      // loop start time
      clock_gettime(CLOCK_REALTIME, &startTS);

      //=================================
      //Do work
      //=================================
      std::string strOut;
      //Generate HTML Status
      strOut = SM->GenerateHTMLStatus(outfile, logLevel, outputType);
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

  //Close ApolloSM and END
  if(NULL != SM) {
    delete SM;
  }

  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"htmlStatus Daemon ended\n");
  

  return 0;
}
