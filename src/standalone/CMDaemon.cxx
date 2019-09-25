#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <uhal/uhal.hpp>
#include <vector>
#include <string>
#include <boost/tokenizer.hpp>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <sys/stat.h>
#include <stdlib.h>
//#include <chrono>

// ====================================================================================================
// Definitions

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

struct temperatures {
  uint8_t MCUTemp;
  uint8_t FIREFLYTemp;
  uint8_t FPGATemp;
  uint8_t REGTemp;
};

//typedef std::chrono::high_resolution_clock clk;
//
//typedef std::chrono::high_resolution_clock::time_point tp;
//
//typedef std::chrono::duration<double, std::milli> doubleDur;

// ====================================================================================================
// Kill program if it is in background
bool volatile loop;

void static signal_handler(int const signum) {
  if(SIGINT == signum) {
    loop = false;
  }
}

// ====================================================================================================

struct temperatures sendAndParse(ApolloSM * const SM) {
  struct temperatures temps;
  
  // read and print
  std::string recv((SM)->UART_CMD("CM.CM1", "simple_sensor", '%'));
  //printf("\nReceived:\n\n%s\n\n", recv.c_str());
  
  // Separate by line
  boost::char_separator<char> lineSep("\r\n");
  tokenizer lineTokens{recv, lineSep};

  // One vector for each line 
  std::vector<std::vector<std::string> > allTokens;

  // Separate by spaces
  boost::char_separator<char> space(" ");
  int vecCount = 0;
  // For each line
  for(tokenizer::iterator lineIt = lineTokens.begin(); lineIt != lineTokens.end(); ++lineIt) {
    tokenizer wordTokens{*lineIt, space};
    // We don't yet own any memory in allTokens so we append a blank vector
    std::vector<std::string> blankVec;
    allTokens.push_back(blankVec);
    // One vector per line
    for(tokenizer::iterator wordIt = wordTokens.begin(); wordIt != wordTokens.end(); ++wordIt) {
      allTokens[vecCount].push_back(*wordIt);
    }
    vecCount++;
  }

  // Check for at least one element 
  // Check for two elements in first element
  // Following lines follow the same concept
  if(0 < allTokens.size()) {
    if(2 == allTokens[0].size()) {
      float ftemp1 = std::atof(allTokens[0][1].c_str());
      if(0 > ftemp1) {
	ftemp1 = 0;
      }
      temps.MCUTemp = (uint8_t)ftemp1;
    }
  }

  if(1 < allTokens.size()) {
    if(2 == allTokens[1].size()) {
      float ftemp2 = std::atof(allTokens[1][1].c_str());
      if(0 > ftemp2) {
	ftemp2 = 0;
      }
      temps.FIREFLYTemp = (uint8_t)ftemp2;
    }
  }

  if(2 < allTokens.size()) {
    if(2 == allTokens[2].size()) {
      float ftemp3 = std::atof(allTokens[2][1].c_str());
      if(0 > ftemp3) {
	ftemp3 = 0;
      }
    temps.FPGATemp = (uint8_t)ftemp3;
    }
  }    

  if(3 < allTokens.size()) {
    if(2 == allTokens[3].size()) {
      float ftemp4 = std::atof(allTokens[3][1].c_str());
      if(0 > ftemp4) {
	ftemp4 = 0;
      }
      temps.REGTemp = (uint8_t)ftemp4;  
    }
  }

  return temps;
}

// ====================================================================================================

void sendTemps(ApolloSM * const SM, struct temperatures const temps) {
  (SM)->RegWriteRegister("SLAVE_I2C.S2.0", temps.MCUTemp);
  (SM)->RegWriteRegister("SLAVE_I2C.S3.0", temps.FIREFLYTemp);
  (SM)->RegWriteRegister("SLAVE_I2C.S4.0", temps.FPGATemp);
  (SM)->RegWriteRegister("SLAVE_I2C.S5.0", temps.REGTemp);
}

// ====================================================================================================

  long elapsed(long const startS, long const startNS, long const stopS, long const stopNS) {
    // 1000000 converts seconds to microseconds. 1000 converts nanoseconds to microseconds
    printf("s: %ld\n", (stopS - startS)*1000000);
    printf("ns: %ld\n", (stopNS - startNS)/1000);
    return (stopS - startS)*1000000 + (stopNS - startNS)/1000;
  }

//doubleDur elapsed(tp const start) {
//  // The difference between two time point objects is a duration object. This duration
//  // in particular is in milliseconds so multiply by one million to get nano
//  return ((clk::now() - start)*1000000);
//}

// ====================================================================================================

int main(int, char**) {

  // ====================================================================================================
  // Initialize ApolloSM
  ApolloSM * SM = NULL;
  SM = new ApolloSM();
  std::vector<std::string> arg;
  arg.push_back("connections.xml");
  SM->Connect(arg);

  // ====================================================================================================
  // Signal handling
  // Catch SIGINT to set loop = false
  struct sigaction sa;
  // Restore old SIGINT action
  struct sigaction oldsa;
  // Instantiate sigaction struct member with signal handler function
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, &oldsa);
  loop = true;

  // ====================================================================================================
  // for counting time
  struct timespec startTS;
  struct timespec stopTS;
  
  // second to microsecond
  // long stous = 1000000;
  // 10 seconds in microseconds  
  //long sleepTime = 10*stous;

  //  tp start;

//// ====================================================================================================
//// For daemonizing
//// Process and session ids
//pid_t pid, sid;
//
//// Fork current process
//pid = fork();
//// Parent process continues with a process ID greater than 0
//if(0 < pid) {
//  exit(EXIT_SUCCESS);
//}
//// Process ID lower than 0 indicates failure in either process
//else if(0 > pid) {
//  exit(EXIT_FAILURE);
//}
//// Parent process has now terminated and forked child process will continue
//// PID of child process was 0
//
//// Since the child process is a daemon, the umask needs to be set so files and logs can be written to
//umask(0);
//// Open system logs for child process
//openlog("CMDaemon", LOG_NOWAIT | LOG_PID, LOG_USER);
//syslog(LOG_NOTICE, "Successfully started CMDaemon");
//
//// Generate a session ID for child process
//sid = setsid();
//// Check valid SID
//if(0 > sid) {
//  syslog(LOG_ERR, "Could not generate session ID for child process");
//  exit(EXIT_FAILURE);
//}
//
//// Change current working directory to a directory guaranteed to exist
//if(0 > chdir("/")) {
//  syslog(LOG_ERR, "Could not change working directory to /");
//  exit(EXIT_FAILURE);
//}
//
//// Daemons cannot use the terminal so close standard file descriptors for security reasons
//close(STDIN_FILENO);
//close(STDOUT_FILENO);
//close(STDERR_FILENO);
//
//// ====================================================================================================

  struct temperatures temps;  

  //Set the power-up done bit to 1 for the IPMC to read
  SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE",1);

  while(loop) {
    // start time
    clock_gettime(CLOCK_REALTIME, &startTS);
    //start = clk::now();

    if(!loop) {break;}
    
    if(SM->RegReadRegister("CM.CM1.CTRL.PWR_GOOD")){
      temps = sendAndParse(SM);
      sendTemps(SM, temps);
    }else{
      temps = {0,0,0,0};
      sendTemps(SM, temps);
    }

    //printf("Read done\n");
    
    // end time
    clock_gettime(CLOCK_REALTIME, &stopTS);
    
    if(!loop) {break;}
    
    printf("No kill\n");

    // sleep for 10 seconds minus how long it took to read and send temperature
    //usleep(sleepTime - elapsed(startTS.tv_sec, startTS.tv_nsec, stopTS.tv_sec, stopTS.tv_nsec));
    //usleep(sleepTime - elapsed(start).count());
    usleep(10000000);
  }

  printf("Out of loop\n");
  
  // ====================================================================================================
  
  if(NULL != SM) {
    delete SM;
  }

  // ====================================================================================================
    
  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &oldsa, NULL);

  // ====================================================================================================

  SM->RegWriteRegister("SLAVE_I2C.S1.SM.STATUS.DONE:",0);
  
  // ====================================================================================================
  
//syslog(LOG_NOTICE, "Successful kill");
//closelog();
////  exit(EXIT_SUCCESS);

  // ====================================================================================================
  
  return 0;
}
