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
#include <time.h>
#include <stdint.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h> 
#include <pthread.h>

                                                                                                                                                
#include <syslog.h>
#include <errno.h>

#include <vector>
#include <string>

//TCLAP parser
#include <tclap/CmdLine.h>

#include <standalone/uioLabelFinder.hh>

extern int errno;

#define MAP_SIZE      0x10000

typedef struct  {
  uint32_t length_offset;
  uint32_t tms_offset;
  uint32_t tdi_offset;
  uint32_t tdo_offset;
  uint32_t ctrl_offset;
} sXVC;

sXVC volatile * pXVC = NULL;

static int verbose = 0;

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
	perror("write");
	return 1;
      }
      if (verbose) {
	printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
	syslog(LOG_ERR,"%u : Received command: 'getinfo'\n", (int)time(NULL));
	printf("\t Replied with %s\n", xvcInfo);
	syslog(LOG_ERR,"\t Replied with %s\n", xvcInfo);
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
	return 1;
      memcpy(result, cmd + 5, 4);
      if (write(fd, result, 4) != 4) {
	perror("write");
	return 1;
      }
      if (verbose) {
	printf("%u : Received command: 'settck'\n", (int)time(NULL));
	syslog(LOG_ERR,"%u : Received command: 'settck'\n", (int)time(NULL));
	printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
	syslog(LOG_ERR,"\t Replied with '%.*s'\n\n", 4, cmd + 5);
      }
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
	return 1;
      if (verbose) {
	printf("%u : Received command: 'shift'\n", (int)time(NULL));
	syslog(LOG_ERR,"%u : Received command: 'shift'\n", (int)time(NULL));
      }
    } else {

      fprintf(stderr, "invalid cmd '%s'\n", cmd);
      syslog(LOG_ERR,"invalid cmd '%s'\n", cmd);
      return 1;
    }

    int len;
    if (sread(fd, &len, 4) != 1) {
      fprintf(stderr, "reading length failed\n");
      syslog(LOG_ERR,"reading length failed\n");
      return 1;
    }

    int nr_bytes = (len + 7) / 8;
    if (((size_t)nr_bytes) * 2 > sizeof(buffer)) {
      fprintf(stderr, "buffer size exceeded\n");
      syslog(LOG_ERR,"buffer size exceeded\n");
      return 1;
    }

    if (sread(fd, buffer, nr_bytes * 2) != 1) {
      fprintf(stderr, "reading data failed\n");
      syslog(LOG_ERR,"reading data failed\n");
      return 1;
    }
    memset(result, 0, nr_bytes);

    if (verbose) {
      printf("\tNumber of Bits  : %d\n", len);
      syslog(LOG_ERR,"\tNumber of Bits  : %d\n", len);
      printf("\tNumber of Bytes : %d \n", nr_bytes);
      syslog(LOG_ERR,"\tNumber of Bytes : %d \n", nr_bytes);
      printf("\n");
      syslog(LOG_ERR,"\n");
    }

    int bytesLeft = nr_bytes;
    int bitsLeft = len;
    int byteIndex = 0;
    int tdi, tms, tdo;

    while (bytesLeft > 0) {
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

	if (verbose) {
	  printf("LEN : 0x%08x\n", 32);
	  syslog(LOG_ERR,"LEN : 0x%08x\n", 32);
	  printf("TMS : 0x%08x\n", tms);
	  syslog(LOG_ERR,"TMS : 0x%08x\n", tms);
	  printf("TDI : 0x%08x\n", tdi);
	  syslog(LOG_ERR,"TDI : 0x%08x\n", tdi);
	  printf("TDO : 0x%08x\n", tdo);
	  syslog(LOG_ERR,"TDO : 0x%08x\n", tdo);
	}

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

	if (verbose) {
	  printf("LEN : 0x%08x\n", bitsLeft);
	  syslog(LOG_ERR,"LEN : 0x%08x\n", bitsLeft);
	  printf("TMS : 0x%08x\n", tms);
	  syslog(LOG_ERR,"TMS : 0x%08x\n", tms);
	  printf("TDI : 0x%08x\n", tdi);
	  syslog(LOG_ERR,"TDI : 0x%08x\n", tdi);
	  printf("TDO : 0x%08x\n", tdo);
	  syslog(LOG_ERR,"TDO : 0x%08x\n", tdo);
	}
	break;
      }
    }
    if (write(fd, result, nr_bytes) != nr_bytes) {
      perror("write");
      return 1;
    }

  } while (1);
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main(int argc, char **argv) {
  int i;
  int s;

  int fdUIO = -1;
  struct sockaddr_in address;
    

  openlog("xvcServer",LOG_PERROR|LOG_PID|LOG_ODELAY,LOG_DAEMON);
  syslog(LOG_INFO,"starting %s", argv[0]);                                                                                                                                                                         
  int port;
   
  try {
    TCLAP::CmdLine cmd("Apollo XVC.",
		       ' ',
		       "XVC");
   
    // XVC name base
    TCLAP::ValueArg<std::string> xvcPreFix("v",              //one char flag
					       "xvc",      // full flag name
					       "xvc prefix",//description
					       true,            //required
					       std::string(""),  //Default is empty
					       "string",         // type
					       cmd);

    // port number
    TCLAP::ValueArg<int> xvcPort("p",              //one char flag
				 "port",      // full flag name
				 "xvc port number",//description
				 true,            //required
				 -1,  //Default is empty
				 "int",         // type
				 cmd);

  
    //Parse the command line arguments
    cmd.parse(argc,argv);
    port = xvcPort.getValue();

    //Find UIO number
    int uioN = label2uio(xvcPreFix.getValue());
    if(uioN < 0){
      fprintf(stderr,"Failed to find UIO device with label %s.\n",xvcPreFix.getValue().c_str());
      syslog(LOG_ERR,"Failed to find UIO device with label %s.\n",xvcPreFix.getValue().c_str());
      return 1;      
    }
    size_t const uioFileNameLength = 1024;
    char * uioFileName = new char[uioFileNameLength+1];
    memset(uioFileName,0x0,uioFileNameLength+1);
    snprintf(uioFileName,uioFileNameLength,"/dev/uio%d",uioN);
    
    fprintf(stderr,"Found %s @ %s.\n",xvcPreFix.getValue().c_str(),uioFileName);
    syslog(LOG_ERR,"Found %s @ %s.\n",xvcPreFix.getValue().c_str(),uioFileName);
    //Open UIO device
    fdUIO = open(uioFileName,O_RDWR);
    if(fdUIO < 0){
      fprintf(stderr,"Failed to open %s.\n",uioFileName);
      syslog(LOG_ERR,"Failed to open %s.\n",uioFileName);
      return 1;            
    }
    
    pXVC = (sXVC volatile*) mmap(NULL,sizeof(sXVC),
				 PROT_READ|PROT_WRITE, MAP_SHARED,
				 fdUIO, 0x0);
    if(MAP_FAILED == pXVC){
      fprintf(stderr,"Failed to mmap %s.\n",uioFileName);
      syslog(LOG_ERR,"Failed to mmap %s.\n",uioFileName);
      return 1;            
    }

    
  }catch (TCLAP::ArgException &e) {
    fprintf(stderr, "Error %s for arg %s\n",
	    e.error().c_str(), e.argId().c_str());
    syslog(LOG_ERR, "Error %s for arg %s\n",
	   e.error().c_str(), e.argId().c_str());
    return 0;
  }

  opterr = 0;
  s = socket(AF_INET, SOCK_STREAM, 0);
               
  if (s < 0) {
    perror("socket");
    return 1;
  }
   
  i = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  address.sin_family = AF_INET;

  if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(s, 5) < 0) {
    perror("listen");
    return 1;
  }

  fd_set conn;
  int maxfd = 0;

  FD_ZERO(&conn);
  FD_SET(s, &conn);

  maxfd = s;

  while (1) {
    fd_set read = conn, except = conn;
    int fd;

    if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
      perror("select");
      break;
    }

    for (fd = 0; fd <= maxfd; ++fd) {
      if (FD_ISSET(fd, &read)) {
	if (fd == s) {
	  int newfd;
	  socklen_t nsize = sizeof(address);

	  newfd = accept(s, (struct sockaddr*) &address, &nsize);

	  //               if (verbose)
	  printf("connection accepted - fd %d\n", newfd);
	  if (newfd < 0) {
	    perror("accept");
	  } else {
	    printf("setting TCP_NODELAY to 1\n");
	    int flag = 1;
	    int optResult = setsockopt(newfd,
				       IPPROTO_TCP,
				       TCP_NODELAY,
				       (char *)&flag,
				       sizeof(int));
	    if (optResult < 0)
	      perror("TCP_NODELAY error");
	    if (newfd > maxfd) {
	      maxfd = newfd;
	    }
	    FD_SET(newfd, &conn);
	  }
	}
	else if (handle_data(fd)) {

	  if (verbose)
	    printf("connection closed - fd %d\n", fd);
	  close(fd);
	  FD_CLR(fd, &conn);
	}
      }
      else if (FD_ISSET(fd, &except)) {
	if (verbose)
	  printf("connection aborted - fd %d\n", fd);
	close(fd);
	FD_CLR(fd, &conn);
	if (fd == s)
	  break;
      }
    }
  }  
  return 0;
}
