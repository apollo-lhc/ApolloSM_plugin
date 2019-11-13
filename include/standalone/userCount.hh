#ifndef __USER_COUNT_HH__
#define __USER_COUNT_HH__

#include <stdint.h>

class userCount{
public:
  userCount();
  ~userCount();

  int initNotify(); 
  int GetNotifyFD(){return iNotifyFD;};
  bool ProcessWatchEvent(); //For clearing out notify data (call before GetUserCounts

  void GetUserCounts(uint32_t & superUsers,uint32_t & normalUsers);
  
private:
  void SetupWatch();
  void shutdownNotify();
  int iNotifyFD;  
  int watchFD;
};
  
#endif
