#ifndef __DAEMON_HH__
#define __DAEMON_HH__

#include <string>

void daemonizeMyself(std::string pidFileName, std::string runPath);

#endif
