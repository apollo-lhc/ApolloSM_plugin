/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd) 
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/). 
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/) 
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 *  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <time.h>
#include <stdint.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h> 
#include <pthread.h>

#include <sys/stat.h> //for umask
#include <sys/types.h> //for umask
                                                                                                                                                
#include <syslog.h>
#include <errno.h>

#include <vector>
#include <string>

#include <fstream>

#include <ApolloSM/uioLabelFinder.hh>
#include <standalone/daemon.hh>       // daemonizeThisProgram // changeSignal // loop
#include <standalone/parseOptions.hh> // setOptions // setParamValues // loadConfig

//extern int errno;

#define DEFAULT_CONFIG_FILE "/etc/xvc_server"
#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_PID_FILE    "/var/run/"
#define DEFAULT_XVCPREFIX   ""
#define DEFAULT_XVCPORT     -1

// ====================================================================================================
// signal handling
bool static volatile * loop; // xvc_server, unlike the other daemons, uses loop in two diferent places, so we need this to point to xvc_serverDaemon loop
// void static signal_handler(int const signum) {
//   if(SIGINT == signum || SIGTERM == signum) {
//     loop = false;
//   }
// }

// ==================================================

#define MAP_SIZE      0x10000

typedef struct  {
  uint32_t length_offset;
  uint32_t tms_offset;
  uint32_t tdi_offset;
  uint32_t tdo_offset;
  uint32_t ctrl_offset;
} sXVC;

sXVC volatile * pXVC = NULL;
uint32_t volatile * XVCLock = NULL;


#define CHECK_LOCK				\
  if(XVCLock && *XVCLock){			\
    syslog(LOG_INFO,"Breaking due to Lock\n");  \
    return -1;					\
  }						\


static int sread(int fd, void *target, int len) {
  unsigned char *t = (unsigned char *) target;
  while (len) {
    int r = read(fd, t, len);
    if (r <= 0)
      return r;
    t += r;
    len -= r;
  }
  return 1;
}

int handle_data(int fd) {

  const char xvcInfo[] = "xvcServer_v1.0:2048\n"; 

  do {    
    CHECK_LOCK
    char cmd[16];
    unsigned char buffer[2048], result[1024];
    memset(cmd, 0, 16);

    if (sread(fd, cmd, 2) != 1)
      return 1;

    if (memcmp(cmd, "ge", 2) == 0) {
      if (sread(fd, cmd, 6) != 1)
	return 1;
      memcpy(result, xvcInfo, strlen(xvcInfo));
      ssize_t writeRet = write(fd, result, strlen(xvcInfo));
      if ((writeRet < 0) || (((size_t)writeRet) != strlen(xvcInfo))) {
	syslog(LOG_ERR,"write: %s",strerror(errno));
	return 1;
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
	return 1;
      memcpy(result, cmd + 5, 4);
      if (write(fd, result, 4) != 4) {
	syslog(LOG_ERR,"write: %s",strerror(errno));
	return 1;
      }
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
	return 1;
    } else {

      syslog(LOG_ERR,"invalid cmd '%s'\n", cmd);
      return 1;
    }

    int len;
    if (sread(fd, &len, 4) != 1) {
      syslog(LOG_ERR,"reading length failed\n");
      return 1;
    }

    int nr_bytes = (len + 7) / 8;
    if (((size_t)nr_bytes) * 2 > sizeof(buffer)) {
      syslog(LOG_ERR,"buffer size exceeded\n");
      return 1;
    }

    if (sread(fd, buffer, nr_bytes * 2) != 1) {
      syslog(LOG_ERR,"reading data failed\n");
      return 1;
    }
    memset(result, 0, nr_bytes);


    int bytesLeft = nr_bytes;
    int bitsLeft = len;
    int byteIndex = 0;
    int tdi, tms, tdo;

    while (bytesLeft > 0) {      
      CHECK_LOCK
      tms = 0;
      tdi = 0;
      tdo = 0;
      if (bytesLeft >= 4) {
	memcpy(&tms, &buffer[byteIndex], 4);
	memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);
	
	pXVC->length_offset = 32;        
	pXVC->tms_offset = tms;         
	pXVC->tdi_offset = tdi;       
	pXVC->ctrl_offset = 0x01;

	/* Switch this to interrupt in next revision */
	while (pXVC->ctrl_offset)
	  {
	  }

	tdo = pXVC->tdo_offset;
	memcpy(&result[byteIndex], &tdo, 4);

	bytesLeft -= 4;
	bitsLeft -= 32;         
	byteIndex += 4;


      } else {
	memcpy(&tms, &buffer[byteIndex], bytesLeft);
	memcpy(&tdi, &buffer[byteIndex + nr_bytes], bytesLeft);
          
	pXVC->length_offset = bitsLeft;        
	pXVC->tms_offset = tms;         
	pXVC->tdi_offset = tdi;       
	pXVC->ctrl_offset = 0x01;
	//					/* Switch this to interrupt in next revision */
	while (pXVC->ctrl_offset)
	  {
	  }

	tdo = pXVC->tdo_offset;          
	memcpy(&result[byteIndex], &tdo, bytesLeft);

	break;
      }
    }
    if (write(fd, result, nr_bytes) != nr_bytes) {
      syslog(LOG_ERR,"write: %s",strerror(errno));
      return 1;
    }

  } while (*loop);
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main(int argc, char **argv) {
  int i;
  int s;

  int fdUIO = -1;
  struct sockaddr_in address;

  int uioN = -1;
  std::string uioLabel;
 
  int port;
  std::string xvcName;

  // parameters to get from command line or config file (config file itself will not be in the config file, obviously)
  std::string configFile  = DEFAULT_CONFIG_FILE;
  std::string runPath     = DEFAULT_RUN_DIR;
  std::string pidFileName = DEFAULT_PID_FILE;
  std::string xvcPreFix   = DEFAULT_XVCPREFIX;
  int xvcPort             = DEFAULT_XVCPORT;

  // parse command line and config file to set parameters
  boost::program_options::options_description fileOptions{"File"}; // for parsing config file
  boost::program_options::options_description commandLineOptions{"Options"}; // for parsing command line
  commandLineOptions.add_options()
    ("config_file",
     boost::program_options::value<std::string>(),
     "config_file"); // This is the only option not also in the file option (obviously)
  setOption(&fileOptions, &commandLineOptions, "run_path", "run path"         , runPath);
  setOption(&fileOptions, &commandLineOptions, "pid_file", "pid file"         , pidFileName);
  setOption(&fileOptions, &commandLineOptions, "xvc"     , "xvc prefix"       , xvcPreFix);
  setOption(&fileOptions, &commandLineOptions, "port"    , "xvc port number"  , xvcPort);
  boost::program_options::variables_map configFileVM; // for parsing config file
  boost::program_options::variables_map commandLineVM; // for parsing command line

  // The command line must be parsed before the config file so that we know if there is a command line specified config file
  fprintf(stdout, "Parsing command line now\n");
  try {
    // parse command line
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, commandLineOptions), commandLineVM);
  } catch(const boost::program_options::error &ex) {
    fprintf(stderr, "Caught exception while parsing command line: %s. Try (--). ex: --polltime \nTerminating xvc_server\n", ex.what());       
    return -1;
  }

  // Check for non default config file
  if(commandLineVM.count("config_file")) {
    configFile = commandLineVM["config_file"].as<std::string>();
  }  
  fprintf(stdout, "config file path: %s\n", configFile.c_str());

  // Now the config file may be loaded
  fprintf(stdout, "Reading from config file now\n");
  try {
    // parse config file
    configFileVM = loadConfig(configFile, fileOptions);
  } catch(const boost::program_options::error &ex) {
    fprintf(stdout, "Caught exception in function loadConfig(): %s \nTerminating xvc_server\n", ex.what());        
    return -1;
  }
 

  // Look at the config file and command line and determine if we should change the parameters from their default values
  setParamValue(&runPath    , "run_path", configFileVM, commandLineVM, false);
  setParamValue(&pidFileName, "pid_file", configFileVM, commandLineVM, false);
  setParamValue(&xvcPort    , "port"    , configFileVM, commandLineVM, false);
  setParamValue(&xvcPreFix  , "xvc"     , configFileVM, commandLineVM, false);

  port     = xvcPort;
  xvcName  = xvcPreFix;
  uioLabel = xvcPreFix;

  //now that we know the port, setup the log
  char daemonName[] = "xvc_server.XXXXXY"; //The 'Y' is so strlen is properly padded
  snprintf(daemonName,strlen(daemonName),"xvc_server.%u",port);
  pidFileName+=daemonName;
  pidFileName+=".pid";
 
  // ============================================================================
  // Deamon book-keeping
  // Every daemon program should have one Daemon object. Daemon class functions are functions that all daemons progams have to perform. That is why we made the class.
  Daemon xvc_serverDaemon;
  xvc_serverDaemon.daemonizeThisProgram(pidFileName, runPath);

//  pid_t pid, sid;
//  pid = fork();
//  if(pid < 0){
//    //Something went wrong.
//    //log something
//    exit(EXIT_FAILURE);
//  }else if(pid > 0){
//    //We are the parent and created a child with pid pid
////    std::string pidFileName = DEFAULT_PID_FILE;
////    pidFileName+=daemonName;
////    pidFileName+=".pid";
//    FILE * pidFile = fopen(pidFileName.c_str(),"w");
//    fprintf(pidFile,"%d\n",pid);
//    fclose(pidFile);
//    exit(EXIT_SUCCESS);
//  }else{
//    // I'm the child!
//    openlog(daemonName,LOG_PERROR|LOG_PID|LOG_ODELAY,LOG_DAEMON);
//  }
//  
//  
//  //Change the file mode mask to allow read/write
//  umask(0);
//  
//  //  openlog(daemonName,LOG_PERROR|LOG_PID|LOG_ODELAY,LOG_DAEMON);
//  syslog(LOG_INFO,"starting %s", daemonName);
//  
//
//  // create new SID for the daemon.
//  sid = setsid();
//  if (sid < 0) {
//    syslog(LOG_ERR,"Failed to change SID\n");
//    exit(EXIT_FAILURE);
//  }
//  syslog(LOG_INFO,"Set SID to %d\n",sid);
//
//  //Move to RUN_DIR
//  if ((chdir(DEFAULT_RUN_DIR)) < 0) {
//    syslog(LOG_ERR,"Failed to change path to \"%s\"\n",DEFAULT_RUN_DIR);    
//    exit(EXIT_FAILURE);
//  }
//  syslog(LOG_INFO,"Changed path to \"%s\"\n",DEFAULT_RUN_DIR);    
//
//  //Everything looks good, close the standard file fds.
//  close(STDIN_FILENO);
//  close(STDOUT_FILENO);
//  close(STDERR_FILENO);

  // ============================================================================


  //Find UIO number
  uioN = label2uio(uioLabel.c_str());
  if(uioN < 0){
    syslog(LOG_ERR,"Failed to find UIO device with label %s.\n",uioLabel.c_str());
    return 1;      
  }
  size_t const uioFileNameLength = 1024;
  char * uioFileName = new char[uioFileNameLength+1];
  memset(uioFileName,0x0,uioFileNameLength+1);
  snprintf(uioFileName,uioFileNameLength,"/dev/uio%d",uioN);
  
  syslog(LOG_ERR,"Found %s @ %s.\n",uioLabel.c_str(),uioFileName);
  //Open UIO device
  fdUIO = open(uioFileName,O_RDWR);
  if(fdUIO < 0){
    syslog(LOG_ERR,"Failed to open %s.\n",uioFileName);
    return 1;            
  }
  
  pXVC = (sXVC volatile*) mmap(NULL,sizeof(sXVC),
			       PROT_READ|PROT_WRITE, MAP_SHARED,
			       fdUIO, 0x0);
  if(MAP_FAILED == pXVC){
    syslog(LOG_ERR,"Failed to mmap %s.\n",uioFileName);
    return 1;            
  }
  delete [] uioFileName;
    


  //Setup XVCLock UIO
  int fdXVCLock = -1;
  int uioNXVCLock = label2uio("PL_MEM");
  if(uioNXVCLock < 0){
    syslog(LOG_ERR,"Failed to find UIO device with label %s.\n","PL_MEM");
    return 1;      
  }
  uioFileName = new char[uioFileNameLength+1];
  memset(uioFileName,0x0,uioFileNameLength+1);
  snprintf(uioFileName,uioFileNameLength,"/dev/uio%d",uioNXVCLock);
    
  syslog(LOG_ERR,"Found %s @ %s.\n","PL_MEM",uioFileName);
  //Open UIO device
  fdXVCLock = open(uioFileName,O_RDWR);
  if(fdXVCLock < 0){
    syslog(LOG_ERR,"Failed to open %s.\n",uioFileName);
    return 1;            
  }
  uint32_t offset = 0;
  if(!xvcName.compare("XVC1")){
    offset=0x7e5;
  }else if(!xvcName.compare("XVC2")){
    offset=0x7e6;
  }else if(!xvcName.compare("XVC_LOCAL")){
    offset=0x7e7;
  }
  if(offset){  
    XVCLock = ((uint32_t volatile*) mmap(NULL,sizeof(uint32_t)*0x800,
					 PROT_READ|PROT_WRITE, MAP_SHARED,
					 fdXVCLock,0x0))  + offset;
    if(MAP_FAILED == XVCLock){
      syslog(LOG_ERR,"Failed to mmap %s.\n",uioFileName);
      return 1;            
    }
    syslog(LOG_ERR,"Found XVC lock register @ 0x%04X\n",offset);    
  }else{
    syslog(LOG_ERR,"No lock register found.\n");        
    XVCLock = &offset;
  }
  delete [] uioFileName;  

  opterr = 0;
  s = socket(AF_INET, SOCK_STREAM, 0);
               
  if (s < 0) {
    syslog(LOG_ERR,"socket: %s",strerror(errno));
    return 1;
  }
   
  i = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  address.sin_family = AF_INET;

  if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
    syslog(LOG_ERR,"bind: %s",strerror(errno));
    return 1;
  }

  if (listen(s, 5) < 0) {
    syslog(LOG_ERR,"listen: %s",strerror(errno));
    return 1;
  }

  fd_set conn;
  int maxfd = 0;

  FD_ZERO(&conn);
  FD_SET(s, &conn);

  maxfd = s;


  // ============================================================================
  // Daemon code setup

  // ====================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  xvc_serverDaemon.changeSignal(&sa_INT , &old_sa, SIGINT);
  xvc_serverDaemon.changeSignal(&sa_TERM, NULL   , SIGTERM);

//  memset(&sa_INT ,0,sizeof(sa_INT)); //Clear struct
//  memset(&sa_TERM,0,sizeof(sa_TERM)); //Clear struct
//  //setup SA
//  sa_INT.sa_handler  = signal_handler;
//  sa_TERM.sa_handler = signal_handler;
//  sigemptyset(&sa_INT.sa_mask);
//  sigemptyset(&sa_TERM.sa_mask);
//  sigaction(SIGINT,  &sa_INT , &old_sa);
//  sigaction(SIGTERM, &sa_TERM, NULL);
  loop = &(xvc_serverDaemon.loop);
  xvc_serverDaemon.loop = true;

  while (xvc_serverDaemon.loop) {
    fd_set read = conn, except = conn;
    int fd;

    if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
      syslog(LOG_ERR,"select: %s",strerror(errno));
      break;
    }
    
    for (fd = 0; fd <= maxfd; ++fd) {
      if (FD_ISSET(fd, &read)) {
	if (fd == s) {
	  int newfd;
	  socklen_t nsize = sizeof(address);

	  newfd = accept(s, (struct sockaddr*) &address, &nsize);

	  syslog(LOG_INFO,"connection accepted - fd %d\n", newfd);
	  if (newfd < 0) {
	    syslog(LOG_ERR,"accept: %s",strerror(errno));
	  } else {
	    syslog(LOG_INFO,"setting TCP_NODELAY to 1\n");
	    int flag = 1;
	    int optResult = setsockopt(newfd,
				       IPPROTO_TCP,
				       TCP_NODELAY,
				       (char *)&flag,
				       sizeof(int));
	    if (optResult < 0)
	      syslog(LOG_ERR,"TCP_NODELAY error: %s",strerror(errno));
	    if (newfd > maxfd) {
	      maxfd = newfd;
	    }
	    FD_SET(newfd, &conn);
	  }
	}
	else if (handle_data(fd)) {

	  syslog(LOG_INFO,"connection closed - fd %d\n", fd);
	  close(fd);
	  FD_CLR(fd, &conn);
	}
      }
      else if (FD_ISSET(fd, &except)) {
	syslog(LOG_INFO,"connection aborted - fd %d\n", fd);
	close(fd);
	FD_CLR(fd, &conn);
	if (fd == s)
	  break;
      }
    }
  }  

  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"%s Daemon ended\n",daemonName);

  return 0;
}
