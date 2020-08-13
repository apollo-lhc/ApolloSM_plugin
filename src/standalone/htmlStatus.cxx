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
#define DEFAULT_PID_FILE    "htmlStatus.pid"
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
    po::store(po::parse_command_line(argc, argv, options), progOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    std::cout << options << std::endl;
    return 0;
  }

  //If configFile opens, get options from config file
  if(File) { 
    try{ 
      po::store(po::parse_config_file(File,options,true), progOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
      std::cout << options << std::endl;
      return 0; 
    }
  }

  //help option, this assumes help is a member of options_description
  if(progOptions.count("help")){
    std::cout << options << '\n';
    return 0;
  }
 
  return progOptions;
}

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
 
  //Set up program options
  po::options_description options("cmpwrdown options");
  options.add_options()
    ("help,h",    "Help screen")
    ("RUN_DIR",     po::value<std::string>()->default_value("/opt/address_table"),           "run path")
    ("PID_DIR",     po::value<std::string>()->default_value("/var/run/"),                    "pud path")
    ("POLLTIME_IN_SECONDS",    po::value<int>()->default_value(10),                                     "polling interval")
    ("OUTFILE",     po::value<std::string>()->default_value("/var/www/lighttpd/index.html"), "html output file")
    ("LOG_LEVEL",   po::value<int>()->default_value(1),                                      "status display log level")
    ("OUTPUT_TYPE", po::value<std::string>()->default_value("HTML"),                         "html output type");

  //setup for loading program options
  //std::ifstream configFile(DEFAULT_CONFIG_FILE);
  po::variables_map progOptions = getVariableMap(argc, argv, options, DEFAULT_CONFIG_FILE);

  //Set pidPath
  std::string pidPath = "";
  if(progOptions.count("PID_DIR")){
    pidPath = progOptions["PID_DIR"].as<std::string>();
  }
  std::string pidFileName = pidPath + DEFAULT_PID_FILE;
  //Set runpath
  std::string runPath = "";
  if(progOptions.count("RUN_DIR")) {
    runPath = progOptions["RUN_DIR"].as<std::string>();
  }
  //Set polltime_in_seconds
  int polltime_in_seconds = 0;
  if(progOptions.count("POLLTIME_IN_SECONDS")) {
    polltime_in_seconds = progOptions["POLLTIME_IN_SECONDS"].as<int>();
  }
  syslog(LOG_INFO, "Setting poll time to %d seconds (%s)\n", polltime_in_seconds, progOptions.count("polltime") ? "CONFIG FILE" : "DEFAULT");
  //Set logLevel
  int logLevel = 0;
  if(progOptions.count("LOG_LEVEL")) {
    logLevel = progOptions["LOG_LEVEL"].as<int>();
  }
  syslog(LOG_INFO, "Setting log level to %d (%s)\n", logLevel, progOptions.count("log_level") ? "CONFIG FILE" : "DEFAULT");
  //Set outfile
  std::string outfile = "";
  if(progOptions.count("OUTFILE")) {
    outfile = progOptions["OUTFILE"].as<std::string>();
  }
  syslog(LOG_INFO, "Sending output to %s (%s)\n", outfile.c_str(), progOptions.count("outfile") ? "CONFIG FILE" : "DEFAULT");
  //Set outputType
    std::string outputType = "";
  if(progOptions.count("OUTPUT_TYPE")) {
    outputType = progOptions["OUTPUT_TYPE"].as<std::string>();
  }
  syslog(LOG_INFO, "Sending output type to %s (%s)\n", outputType.c_str(), progOptions.count("output_type") ? "CONFIG FILE" : "DEFAULT");

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
