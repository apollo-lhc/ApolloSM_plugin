#include <standalone/daemon.hh>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

// this allows sig_handler to access the class variable "loop" without being a class function
bool static volatile * globalLoop;
  
Daemon::Daemon(){
  globalLoop = &loop;
}

Daemon::~Daemon(){
}

//void daemonizeMyself(std::string pidFileName, std::string runPath) {
void Daemon::daemonizeThisProgram(std::string pidFileName, std::string runPath) {
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
    openlog(NULL,LOG_PID,LOG_DAEMON);
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
  //Since something might still try to use them, use dup to link them all to /dev/null
  int nullFD = open("/dev/null",O_RDWR);
  if(-1 == nullFD){
    syslog(LOG_ERR,"Failed to open /dev/null for std fd closing");
    exit(EXIT_FAILURE);
  }  
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  //remap the st file descriptors to nullFD
  dup2(nullFD,STDIN_FILENO);
  dup2(nullFD,STDOUT_FILENO);
  dup2(nullFD,STDERR_FILENO);
  
}

//void static signal_handler(int const signum) {
// sigaction does not seem happy when this is a class function
void signal_handler(int const signum) {
  if(SIGINT == signum || SIGTERM == signum) {
    *globalLoop = false;
  }
}

void Daemon::changeSignal(struct sigaction * newAction, struct sigaction * oldAction, int const signum) {
  memset(newAction,0,sizeof(*newAction)); //Clear struct
  // set up the action
  newAction->sa_handler = signal_handler;
  sigemptyset(&(newAction->sa_mask));
  sigaction(signum, newAction, oldAction);
}

void Daemon::SetLoop(bool b) {
  loop = b;
}

bool Daemon::GetLoop() {
  return loop;
}
