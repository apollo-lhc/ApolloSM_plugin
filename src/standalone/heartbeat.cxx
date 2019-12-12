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

#include <boost/program_options.hpp>
#include <fstream>

#define SEC_IN_USEC 1000000
#define NSEC_IN_USEC 1000

// ====================================================================================================
// Read from config files and set up all parameters                                                                                                                                                                                         
// For further information see https://theboostcpplibraries.com/boost.program_options

// global polltime variable
int polltime_in_seconds;
#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_POLLTIME_STR "10"

#define CONFIG_FILE "/tmp/bin/heartbeatConfig.txt"

void setup(FILE * logFile) {
  
  try {
    // fileOptions is for parsing config files
    boost::program_options::options_description fileOptions{"File"};
    // Let fileOptions know what information you want it to take out of the config file. Here you want it to take "polltime"
    // Second argument is the type of the information. Second argument has many features, we use the default_value feature. 
    // Third argument is description of the information
    fileOptions.add_options()
      ("polltime", boost::program_options::value<int>()->default_value(DEFAULT_POLLTIME_IN_SECONDS), "amount of time to wait before reading heartbeat nodes again");

    // This is a container for the information that fileOptions will get from the config file
    boost::program_options::variables_map vm;

    // Check if config file exists
    std::ifstream ifs{CONFIG_FILE};
    if(ifs) {
      // If config file exists, parse ifs into fileOptions and store information from fileOptions into vm
      // Note that if the config file does not exist, store will not be called and vm will be empty
      fprintf(logFile, "Config file " CONFIG_FILE " exists\n");
      fflush(logFile);
      // Using just parse_config_file without boost::program_options:: works. This may be a boost bug or the compiler may just be very smart
      boost::program_options::store(parse_config_file(ifs, fileOptions), vm);
    } else {
      fprintf(logFile, "Config file " CONFIG_FILE " does not exist\n");
      fflush(logFile);
    }

    // For some reason neither of the following two lines will evaluate to exist. Even if ifs evaluates true in if(ifs) (as it does above), the second line here still will not evaluate to exist.
    // If we can fix either of these two lines we can get rid of the above else statement (not the if, just the else)
    //    fprintf(logFile, "Config file at " CONFIG_FILE " %s\n", !ifs.fail() ? "exists" : "does not exist");
    //    fprintf(logFile, "Config file at " CONFIG_FILE " %s\n", ifs ? "exists" : "does not exist");
    //    fflush(logFile);

    // Notify is not needed but it is powerful. Commeneted out for future references
    //boost::program_options::notify(vm);

    // Check for information in vm
    if(vm.count("polltime")) {
      polltime_in_seconds = vm["polltime"].as<int>();
    }

    std::string polltime_str(std::to_string(polltime_in_seconds));

    fprintf(logFile, "Setting poll time as %s seconds from %s\n", vm.count("polltime") ? polltime_str.c_str() : DEFAULT_POLLTIME_STR, vm.count("polltime") ? "CONFIGURATION FILE" : "DEFAULT VALUE");
    fflush(logFile);

  } catch (const boost::program_options::error &ex) {
    fprintf(logFile, "Caught exception in function setup(): %s \n", ex.what());
    fflush(logFile);
  }
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
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_USEC + 
	   (end.tv_nsec - cur.tv_nsec)/NSEC_IN_USEC);
}





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
    FILE * pidFile = fopen("/var/run/heartbeat.pid","w");
    fprintf(pidFile,"%d\n",pid);
    fclose(pidFile);
    exit(EXIT_SUCCESS);
  }else{
    // I'm the child!
  }
  
  //Change the file mode mask to allow read/write
  umask(0);

  //Create log file
  FILE * logFile = fopen("/var/log/heartbeat.log","w");
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
  // Read from configuration file and set up parameters
  fprintf(logFile,"Reading from config file now\n");
  fflush(logFile);
  setup(logFile);
  fprintf(logFile,"Finished reading from config file\n");
  fflush(logFile);


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

  long update_period_us = polltime_in_seconds*SEC_IN_USEC; //sleep time in microseconds


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
    // Main DAEMON loop
    fprintf(logFile,"Starting heartbeat\n");
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
  
  //PS heartbeat
  SM->RegReadRegister("SLAVE_I2C.HB_SET1");
  SM->RegReadRegister("SLAVE_I2C.HB_SET2");
  
  //Clean up
  if(NULL != SM) {
    delete SM;
  }
  
  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  fprintf(logFile,"heartbeat Daemon ended\n");
  fclose(logFile);
  
  return 0;
}
