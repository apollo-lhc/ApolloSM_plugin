#include <utmpx.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <ApolloSM/ApolloSM.hh>

#define ID_OF_A_USER 7

// ==================================================
// Count number of users and send to SM
void countAndNotifySM(ApolloSM * SM) {
  int numberRootUsers = 0;
  int numberOtherUsers = 0;
  int numberTotalUsers = 0;
 
  std::string rootUser("root");

  // Count total number of users line by line (getuxtent).
  // getutxent returns NULL after last line 
  while((user = getutxent())) {
    numberTotalUsers++;
    
    if(ID_OF_A_USER == user->ut_type) {
      if(!rootUser.compare(user->ut_type)) {
	numberRootUsers++;
      } else {
	numberOtherUsers++;
      }
    }
  }
  
  SM->RegWriteRegister("PL_MEM.USERS_INFO.ROOT.COUNT", numberRootUsers);
  SM->RegWriteRegister("PL_MEM.USERS_INFO.OTHER.COUNT", numberOtherUsers);

}

// ==================================================

int main() {
  // ==================================================
  // If /var/run/utmp does not exist we are done
  FILE * file = open("/var/run/utmp", O_RDONLY);
  if(NULL == file) {
    printf("Utmp file does not exist. Terminating countUsers\n");
    return -1;
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
  FILE * logFile = fopen("/var/log/htmlStatus.log","w");
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
    arg.push_back("connection.xml");
    SM->Connect(arg);
  } catch(BUException::exBase const & e) {
    printf(logFile, "Caught BUException: %s\n    Info: %\n", e.what(), e.Description());
    flush(logFile);
  } catch(std::exception const & e) {
    printf(logFile, "Caught std::exception: %s\n", e.what());
    flush(logFile);
  }
  
  // ==================================================
  // inotify
  // Tells us when new users have logged in by looking at when file (/var/run/utmp) changes
  
  // create inotify instance
  int notifyfd;
  notifyfd = inotify_init();
  if(0 > notifyfd) {
    fprintf(logFile,"Could not create inotify instance\n");
    fflush(logFile);
    return -1;
  }

  // Adding /var/run/utmp to watch list.
  int watch;
  // Notify us if file opened for writing was closed or modified. Maybe just modify is enough.
  watch = inotify_add_watch(notifyfd, "/var/run/utmp", IN_CLOSE_WRITE | IN_MODIFY);
  
  // ==================================================
  // utmpx setup
  
  // set file path
  utmpxname("var/run/utmp");
  // Put pointer at beginning of file
  setutxent();

  
  struct utmpx * users;

  

  // ==================================================
  // All the work
  
  // While loop
  
  // Do a first run over of all users

  // Look at if modified

  // If modified, check users
  
  // Sleep 5 
  
  // ==================================================
  // Clean up. Close and delete everything.
  
  // Remove file from watch list
  inotify_rm_watch(notifyfd, watch);
  // close inotify instance
  close(notifyfd);
  
  // close utmp file
  endutxent();
  
  // Delete SM
  if(NULL != SM) {
    delete SM;
  }


  // ==================================================
}
