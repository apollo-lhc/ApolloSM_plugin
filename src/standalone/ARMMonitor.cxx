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

#define UPDATE_PERIOD 10

// ====================================================================================================
// Kill program if it is in background
bool static volatile loop;

void static signal_handler(int const signum) {
  if(SIGINT == signum || SIGTERM == signum) {
    loop = false;
  }
}


// ==================================================

int main() {
  // ==================================================
  // If /var/run/utmp does not exist we are done
  {
    FILE * file = fopen("/var/run/utmp","r");
    if(NULL == file) {
      printf("Utmp file does not exist. Terminating countUsers\n");
      return -1;
    }
  }

  // ==================================================
  // Daemon
  pid_t pid, sid;
  pid = fork();
  if(pid < 0) {
    // Something went wrong 
    // log
    exit(EXIT_FAILURE);
  } else if(0 < pid) {
    // We are the parent and created a child with pid pid
    FILE * pidFile = fopen("/var/run/countUsers.pid","w");
    fprintf(pidFile,"%d\n",pid);
    fclose(pidFile);
    exit(EXIT_SUCCESS);
  } else {
    // I'm the child!
  }
  // Change the file mode mask to allow read/write
  umask(0);
  // Create log file
  FILE * logFile = fopen("/var/log/ARMMonitor.log","w");
  if(NULL == logFile) {
    fprintf(stderr,"Failed to create log file for htmlStatus\n");
    exit(EXIT_FAILURE);
  }
  fprintf(logFile,"Opened log file\n");
  fflush(logFile);
  // create new SID for daemon
  sid = setsid();
  if(0 > sid) {
    fprintf(logFile,"Failed to change SID\n");
    fflush(logFile);
    exit(EXIT_FAILURE);
  } else {
    fprintf(logFile, "Set SID to %d\n",sid);
    fflush(logFile);
  }
  // Move to /tmp/bin
  if(0 > (chdir("/tmp/bin"))) {
    fprintf(logFile,"Failed to change path to /tmp/bin \n");
    fflush(logFile);
    exit(EXIT_FAILURE);
  } else {
    fprintf(logFile, "Changed path to /tmp/bin \n");
    fflush(logFile);
  }
  // Everything looks good, close standard file fds.
  // Daemons don't use them, so close for security purposes
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // ============================================================================
  // Daemon code setup

  // ====================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa_INT,old_sa_TERM;
  memset(&sa_INT ,0,sizeof(sa_INT)); //Clear struct
  memset(&sa_TERM,0,sizeof(sa_TERM)); //Clear struct
  //setup SA
  sa_INT.sa_handler  = signal_handler;
  sa_TERM.sa_handler = signal_handler;
  sigemptyset(&sa_INT.sa_mask);
  sigemptyset(&sa_TERM.sa_mask);
  sigaction(SIGINT,  &sa_INT , &old_sa_INT);
  sigaction(SIGTERM, &sa_TERM, &old_sa_TERM);  
  loop = true;

  // ==================================================
  // Make ApolloSM
  ApolloSM * SM;
  try {
    SM = new ApolloSM();
    if(NULL == SM) {
      fprintf(logFile, "Failed to create new ApolloSM\n");
      fflush(logFile);
      return -1;
    } else {
      fprintf(logFile, "Created new ApolloSM\n");
      fflush(logFile);
    }
    std::vector<std::string> arg;
    arg.push_back("connections.xml");
    SM->Connect(arg);
  } catch(BUException::exBase const & e) {
    fprintf(logFile, "Caught BUException: %s\n    Info: %s\n", e.what(), e.Description());
    fflush(logFile);
    return -1;
  } catch(std::exception const & e) {
    fprintf(logFile, "Caught std::exception: %s\n", e.what());
    fflush(logFile);
    return -1;
  }


  //vars for pselect
  fd_set readSet,readSet_ret;
  FD_ZERO(&readSet);
  struct timespec timeout = {UPDATE_PERIOD,0};
  int maxFDp1 = 0;

  //Create a usercount process
  userCount uCnt;
  int fdUserCount = uCnt.initNotify();
  fprintf(logFile,"iNotify setup on FD %d\n",fdUserCount);
  FD_SET(fdUserCount,&readSet);
  if(fdUserCount >= maxFDp1){
    maxFDp1 = fdUserCount+1;
  }

  
  // ==================================================
  // All the work

  //Do one read of users file before we start our loop
  uint32_t superUsers,normalUsers;
  uCnt.GetUserCounts(superUsers,normalUsers);
  SM->RegWriteRegister("PL_MEM.USERS_INFO.SUPER_USERS.COUNT",superUsers);
  SM->RegWriteRegister("PL_MEM.USERS_INFO.USERS.COUNT",normalUsers);	  
 
  while(loop){
    readSet_ret = readSet;
    int pselRet = pselect(maxFDp1,&readSet_ret,NULL,NULL,&timeout,NULL);
    if(0 == pselRet){
      //timeout, do CPU/mem monitoring
      uint32_t mon;
      mon = MemUsage()*100; //Scale the value by 100 to get two decimal places for reg   
      SM->RegWriteRegister("PL_MEM.ARM.MEM_USAGE",mon);
      mon = CPUUsage()*100; //Scale the value by 100 to get two decimal places for reg   
      SM->RegWriteRegister("PL_MEM.ARM.CPU_LOAD",mon);
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
      fprintf(logFile,"Error in pselect %d(%s)",errno,strerror(errno));
    }
  }
  // ==================================================
  // Clean up. Close and delete everything.
    
  // Delete SM
  if(NULL != SM) {
    delete SM;
  }


  // ==================================================
}
