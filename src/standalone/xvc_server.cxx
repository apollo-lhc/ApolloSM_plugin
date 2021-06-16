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
#include <arpa/inet.h>  //for inet_ntoa
#include <pthread.h> // Why is this here?

#include <sys/stat.h> //for umask
#include <sys/types.h> //for umask
                                                                                                                                         
#include <syslog.h>
#include <errno.h>

#include <vector>
#include <string>

#include <ApolloSM/uioLabelFinder.hh>
#include <ApolloSM/ApolloSM.hh>

#include <boost/program_options.hpp>
#include <standalone/optionParsing.hh>
#include <standalone/optionParsing_bool.hh>
#include <standalone/daemon.hh>

#include <fstream>
#include <iostream>

// ================================================================================
// Setup for boost program_options
#define DEFAULT_CONFIG_FILE "/etc/xvc_server"
#define DEFAULT_RUN_DIR "/opt/address_table/"
#define DEFAULT_PID_DIR "/var/run/"
#define DEFAULT_XVCPREFIX " "
#define DEFAULT_XVCPORT -1
namespace po = boost::program_options;


#define MAP_SIZE      0x10000

typedef struct  {
  uint32_t length_offset;
  uint32_t tms_offset;
  uint32_t tdi_offset;
  uint32_t tdo_offset;
  uint32_t ctrl_offset; //bit 1 is a go signal, bit 2 is a busy signal
  uint32_t lock_offset;
  uint32_t IP;
  uint32_t port;
} sXVC;

sXVC volatile * pXVC = NULL;
uint32_t volatile * XVCLock = NULL;


#define CHECK_LOCK        \
  if(XVCLock && *XVCLock){      \
    syslog(LOG_INFO,"Breaking due to Lock\n");  \
    return -1;          \
  }           \

//Daemon class;
Daemon daemonInst;

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
  //          /* Switch this to interrupt in next revision */
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

  } while (daemonInst.GetLoop());
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main(int argc, char **argv) {
  int i;
  int listenFD;

  int fdUIO = -1;
  struct sockaddr_in address;

  int uioN = -1;


  //=======================================================================
  // Set up program options
  //=======================================================================
  //Command Line options
  po::options_description cli_options("XVC options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("RUN_DIR,r",   po::value<std::string>(), "Path to default run directory")
    ("PID_FILE,d",  po::value<std::string>(), "Path to default pid directory")
    ("xvcPrefix,v", po::value<std::string>(), "xvc prefix")
    ("xvcPort,p",   po::value<int>(),         "xvc_port number")
    ("config_file", po::value<std::string>(), "config file");
  //Config File options
  po::options_description cfg_options("XVC options");
  cfg_options.add_options()
    ("RUN_DIR",   po::value<std::string>(), "Path to default run directory")
    ("PID_DIR",   po::value<std::string>(), "Path to default pid directory")
    ("xvcPrefix", po::value<std::string>(), "xvc prefix")
    ("xvcPort",   po::value<int>(),         "xvc_port number");

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

  //Set port
  int port            = GetFinalParameterValue(std::string("xvcPort"),  allOptions,DEFAULT_XVCPORT);
  //setxvcName
  std::string xvcName = GetFinalParameterValue(std::string("xvcPrefix"),allOptions,std::string(DEFAULT_XVCPREFIX));
  //set PID_DIR
  std::string PID_DIR = GetFinalParameterValue(std::string("PID_DIR"),  allOptions,std::string(DEFAULT_PID_DIR));
  //Set RUN_DIR
  std::string RUN_DIR = GetFinalParameterValue(std::string("RUN_DIR"),  allOptions,std::string(DEFAULT_RUN_DIR));

  //use xvcName to get uiLabel
  std::string uioLabel = xvcName;

  //now that we know the port, setup the log
  char daemonName[] = "xvc_server.XXXXXY"; //The 'Y' is so strlen is properly padded
  snprintf(daemonName,strlen(daemonName),"xvc_server.%u",port);
  std::string pidFileName = PID_DIR+std::string(daemonName)+std::string(".pid");

  // ============================================================================
  // Deamon book-keeping
  daemonInst.daemonizeThisProgram(pidFileName, RUN_DIR);

  // ============================================================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  daemonInst.changeSignal(&sa_INT , &old_sa, SIGINT);
  daemonInst.changeSignal(&sa_TERM, NULL   , SIGTERM);
  daemonInst.SetLoop(true);

  //Find UIO number
  std::string parentLabel = uioLabel.substr(0,uioLabel.find('.'));
  uioN = label2uio(parentLabel.c_str());//uioLabel.c_str().);
  if(uioN < 0){
    syslog(LOG_ERR,"Failed to find UIO device with label %s.\n",parentLabel.c_str());//uioLabel.c_str());
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

  //Getting offset
  ApolloSM * SM = new ApolloSM();
  if(NULL == SM){
    syslog(LOG_ERR,"Failed to create new ApolloSM\n");
  } else{
    syslog(LOG_INFO,"Created new ApolloSM\n");
  }
  std::vector<std::string> arg;
  arg.push_back("connections.xml");
  SM->Connect(arg);

  uint32_t uio_offset = SM->GetRegAddress(xvcName) - SM->GetRegAddress(xvcName.substr(0,xvcName.find('.')));
  if(NULL != SM){
    delete SM;
  }
   
  pXVC = (sXVC volatile*) mmap(NULL, sizeof(sXVC) + uio_offset*sizeof(uint32_t),
             PROT_READ|PROT_WRITE, MAP_SHARED,
             fdUIO, 0x0);// + uio_offset*sizeof(uint32_t));

  pXVC += (uio_offset * 4) / sizeof(sXVC);


  if(MAP_FAILED == pXVC){
    syslog(LOG_ERR,"Failed to mmap %s.\n",uioFileName);
    return 1;            
  }
  delete [] uioFileName;
    


  //Setup XVCLock UIO
  //checking plxvc vs xvc local
  if((xvcName.compare("XVC_LOCAL"))!=0){ //If xvcName is not 'XVC_LOCAL'
    XVCLock = &pXVC->lock_offset;
    if (XVCLock == NULL) {
      syslog(LOG_ERR,"Failed to assign XVCLock\n");
    } else {
      syslog(LOG_ERR,"XVCLock found");
    }
  } else {

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
  }


  opterr = 0;
  listenFD = socket(AF_INET, SOCK_STREAM, 0);
               
  if (listenFD < 0) {
    syslog(LOG_ERR,"socket: %s",strerror(errno));
    return 1;
  }
   
  i = 1;
  setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  address.sin_family = AF_INET;

  if (bind(listenFD, (struct sockaddr*) &address, sizeof(address)) < 0) {
    syslog(LOG_ERR,"bind: %s",strerror(errno));
    return 1;
  }

  if (listen(listenFD, 5) < 0) {
    syslog(LOG_ERR,"listen: %s",strerror(errno));
    return 1;
  }




  //zero remote info
  pXVC->IP =0;
  pXVC->port = 0;

  int maxFD = 0;

  //setup the listen FDset for select
  fd_set listenFDSet;
  FD_ZERO(&listenFDSet);
  FD_SET(listenFD, &listenFDSet);

  //setup the connection FD set for select
  int connectionFD = -1;
  fd_set connectionFDSet;
  FD_ZERO(&connectionFDSet);

  //start by waiting on the listen socket
  fd_set activeFDSet = listenFDSet;
  maxFD = listenFD;
  while (daemonInst.GetLoop()) {
    fd_set read = activeFDSet, except = listenFDSet;

    if (select(maxFD + 1, &read, 0, &except, 0) < 0) {
      syslog(LOG_ERR,"select: %s",strerror(errno));
      break;
    }
    
    //---------------------------------------------------------------------------
    if (FD_ISSET(listenFD,&except)){
      syslog(LOG_ERR,"Exceptional condition on listen socket\n");
      break;
    //---------------------------------------------------------------------------
    } else if (FD_ISSET(listenFD, &read)) {
      socklen_t nsize = sizeof(address);

      connectionFD = accept(listenFD, (struct sockaddr*) &address, &nsize);

    
      syslog(LOG_INFO,"connection accepted - fd %d %s:%u\n", connectionFD,inet_ntoa(address.sin_addr),address.sin_port);
      if (connectionFD < 0) {
  syslog(LOG_ERR,"accept: %s",strerror(errno)); 
      } else {
  syslog(LOG_INFO,"setting TCP_NODELAY to 1\n");
  int flag = 1;
  int optResult = setsockopt(connectionFD,
           IPPROTO_TCP,
           TCP_NODELAY,
           (char *)&flag,
           sizeof(int));
  if (optResult < 0)
    syslog(LOG_ERR,"TCP_NODELAY error: %s",strerror(errno));
  
  
  pXVC->IP =address.sin_addr.s_addr;
  pXVC->port = address.sin_port;

  //set the activeFDSet for read to now be the connection
  FD_ZERO(&connectionFDSet);
  FD_SET(connectionFD,&connectionFDSet);
  activeFDSet = connectionFDSet;
  maxFD = connectionFD;
      }
    //---------------------------------------------------------------------------
    }else if((connectionFD > 0) &&
       (FD_ISSET(connectionFD,&read))){
      if(handle_data(connectionFD)){
  //error happened
  syslog(LOG_INFO,"connection closed - fd %d %s:%u\n", connectionFD,inet_ntoa(address.sin_addr),address.sin_port);
  pXVC->IP =0;
  pXVC->port = 0;
  
  close(connectionFD);
  activeFDSet = listenFDSet;
  maxFD = listenFD; 
  connectionFD = -1;
      }
    }
  }  
  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"%s Daemon ended\n",daemonName);

  return 0;
}