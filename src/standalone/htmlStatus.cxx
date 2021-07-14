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
#define DEFAULT_CONFIG_FILE "/etc/htmlStatus"
#define DEFAULT_RUN_DIR "/opt/address_table/"
#define DEFAULT_PID_FILE "/var/run/htmlStatus.pid"
#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_OUTFILE "/var/www/lighttpd/index.html"
#define DEFAULT_LOG_LEVEL 1
#define DEFAULT_OUTPUT_TYPE "HTML"

namespace po = boost::program_options;

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
  po::options_description cli_options("htmlStatus options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("RUN_DIR",             po::value<std::string>(), "run path")
    ("PID_FILE",            po::value<std::string>(), "pid path")
    ("POLLTIME_IN_SECONDS", po::value<int>(),         "polling interval")
    ("OUTFILE",             po::value<std::string>(), "html output file")
    ("LOG_LEVEL",           po::value<int>(),         "status display log level")
    ("OUTPUT_TYPE",         po::value<std::string>(), "html output type")
    ("config_file",         po::value<std::string>(), "config file");
  //Config File options
  po::options_description cfg_options("htmlStatus options");
  cfg_options.add_options()
    ("RUN_DIR",             po::value<std::string>(),  "run path")
    ("PID_FILE",            po::value<std::string>(),  "pud path")
    ("POLLTIME_IN_SECONDS", po::value<int>(),          "polling interval")
    ("OUTFILE",             po::value<std::string>(),  "html output file")
    ("LOG_LEVEL",           po::value<int>(),          "status display log level")
    ("OUTPUT_TYPE",         po::value<std::string>(),  "html output type");


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

 
  //Set run dir
  std::string runPath     = GetFinalParameterValue(std::string("RUN_DIR"),             allOptions,std::string(DEFAULT_RUN_DIR));
  //set pidFileName							         
  std::string pidFileName = GetFinalParameterValue(std::string("PID_FILE"),            allOptions,std::string(DEFAULT_PID_FILE));
  //Set polltime
  int polltime_in_seconds = GetFinalParameterValue(std::string("POLLTIME_IN_SECONDS"), allOptions, DEFAULT_POLLTIME_IN_SECONDS);
  //Set outfile
  std::string outfile     = GetFinalParameterValue(std::string("OUTFILE"),             allOptions,std::string(DEFAULT_OUTFILE));
  //Set log level
  int logLevel            = GetFinalParameterValue(std::string("LOG_LEVEL"),           allOptions,DEFAULT_LOG_LEVEL);
  //Set output type
  std::string outputType  = GetFinalParameterValue(std::string("OUTPUT_TYPE"),         allOptions,std::string(DEFAULT_OUTPUT_TYPE));

  syslog(LOG_INFO, "Setting poll time to %d seconds\n",polltime_in_seconds);
  syslog(LOG_INFO, "Sending output to %s\n", outfile.c_str());
  syslog(LOG_INFO, "Setting log level to %d\n", logLevel);
  syslog(LOG_INFO, "Sending output type to %s\n", outputType.c_str());



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


    while(daemon.GetLoop()) {

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
