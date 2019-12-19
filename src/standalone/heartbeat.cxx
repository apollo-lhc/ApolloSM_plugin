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

#define SEC_IN_USEC 10000000
#define NSEC_IN_USEC 1000

#define RUN_DIR "/opt/address_tables/"


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

  //Move to RUN_DIR
  if ((chdir(RUN_DIR)) < 0) {
    fprintf(logFile,"Failed to change path to \"" RUN_DIR "\"\n");
    fflush(logFile);
    exit(EXIT_FAILURE);
  }else{
    fprintf(logFile,"Changed path to \"" RUN_DIR "\"\n");
    fflush(logFile);
  }


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

  long update_period_us = 1*SEC_IN_USEC; //sleep time in microseconds


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
