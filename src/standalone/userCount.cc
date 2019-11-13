#include <standalone/userCount.hh>

#include <string>

/* According to POSIX.1-2001 */
#include <sys/select.h>

/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <utmp.h>

#include <stdexcept>

#include <sys/inotify.h>

#define ID_OF_A_USER 7

#define _UTMP_FILENAME "/var/run/utmp"

userCount::userCount():iNotifyFD(-1),watchFD(-1){  
}

userCount::~userCount(){
  shutdownNotify();
}

int userCount::initNotify(){
  if(-1 == iNotifyFD){ //Check if we have already setup the fd
    iNotifyFD = inotify_init();
    if(iNotifyFD < 0) {
      iNotifyFD = -1; //force this state
      throw std::runtime_error("Could not create inotify instance\n");
    }
    
  }
  //Start watch for user file
  SetupWatch();

  return iNotifyFD; //Set in fucntion above
}

void userCount::SetupWatch(){
  if(-1 == iNotifyFD){
    throw std::runtime_error("Bad iNotifyFD.  Can't create watch FD.\n");
  }
  //Add a watch to /var/run/utmp
  watchFD = inotify_add_watch(iNotifyFD, _UTMP_FILENAME , IN_MODIFY);
  if(-1 == watchFD){
    throw std::runtime_error("Unable to create watch for " _UTMP_FILENAME);
  }  
}

bool userCount::ProcessWatchEvent(){
  if(-1 == watchFD){
    return false;
  }
  //to be sure, make sure the FD is readable. 
  fd_set readCheckSet;
  FD_ZERO(&readCheckSet);
  FD_SET(iNotifyFD,&readCheckSet);
  struct timespec zeroTimeout = {0,0};
  
  int readCheck = pselect(iNotifyFD+1,&readCheckSet,NULL,NULL,&zeroTimeout,NULL);
  if(readCheck <= 0){
    //timeout or error
    return false;
  }else{
    if(FD_ISSET(iNotifyFD,&readCheckSet)){
      //we are read ready so read one
      size_t const bufferSize = 4096;
      uint8_t buffer[bufferSize];
      int readSize = read(iNotifyFD,buffer,bufferSize);
      if(readSize <= 0){
	//bad read
	return false;
      }
      //loop over the buffer and process events
      struct inotify_event const  * event = NULL;
      for(event = (struct inotify_event const *) buffer;
	  (uint8_t *)event < buffer+readSize;
	  event += sizeof(struct inotify_event) + event->len){
	if((event->wd == watchFD) && (event->mask & IN_MODIFY)){
	  //we've already read out the events, and we only care if
	  //there has been atleast 1 IN_MODIFY event, so we don't 
	  //need to process anymore. 
	  return true;
	}
      }
    }
    return false;
  }
  return false;
}

void userCount::shutdownNotify(){
  if((watchFD != 0) && (iNotifyFD != 0)){
    inotify_rm_watch(iNotifyFD, watchFD);
    watchFD = -1;
    iNotifyFD = -1;
  }
}


// ==================================================
// Count number of users and send to SM
void userCount::GetUserCounts(uint32_t & superUsers,uint32_t & normalUsers) {
  superUsers = 0;
  normalUsers = 0;

  // set file path
  utmpname(_UTMP_FILENAME);
  // Put pointer at beginning of file
  setutent();  
  struct utmp * user;
 
  std::string const rootUser("root");
  // Count total number of users line by line (getuxtent).
  // getutxent returns NULL after last line 
  while((user = getutent())) {
    if(ID_OF_A_USER == user->ut_type) {
      if(!rootUser.compare(user->ut_user)) {
	superUsers++;
      } else {
	normalUsers++;
      }
    }
  }  
  // close utmp file
  endutent();
}
