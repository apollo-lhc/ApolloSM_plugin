#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <uhal/uhal.hpp>
#include <vector>
#include <string>
#include <boost/tokenizer.hpp>
#include <unistd.h> // usleep
#include <signal.h>
#include <time.h>

// ====================================================================================================
// Definitions

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

struct temperatures {
  uint8_t MCUTemp;
  uint8_t FIREFLYTemp;
  uint8_t FPGATemp;
  uint8_t REGTemp;
};

// ====================================================================================================
// Kill program if it is in background
bool volatile loop;

void static signal_handler(int const signum) {
  //fprintf(stderr, "hello\n");
  //printf("SIGUSR1 is: %d\n", SIGUSR1);
  //printf("signum is: %d\n", signum);
  //  if(SIGUSR1 == signum) {
  if(SIGINT == signum) {
    loop = false;
  }
}

// ====================================================================================================

temperatures sendAndParse(ApolloSM* SM) {
  temperatures temps;
  
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

void sendTemps(ApolloSM* SM, temperatures temps) {

  // TO DO use RegWriteRegister

  (SM)->RegWriteRegister("SLAVE_I2C.S2.1", temps.MCUTemp);
  (SM)->RegWriteRegister("SLAVE_I2C.S3.1", temps.FIREFLYTemp);
  (SM)->RegWriteRegister("SLAVE_I2C.S4.1", temps.FPGATemp);
  (SM)->RegWriteRegister("SLAVE_I2C.S5.1", temps.REGTemp);

//(SM)->RegWriteNode((SM)->GetNode("SLAVE_I2C.S2.1"), temps.MCUTemp);
//(SM)->RegWriteNode((SM)->GetNode("SLAVE_I2C.S3.1"), temps.FIREFLYTemp);
//(SM)->RegWriteNode((SM)->GetNode("SLAVE_I2C.S4.1"), temps.FPGATemp);
//(SM)->RegWriteNode((SM)->GetNode("SLAVE_I2C.S5.1"), temps.REGTemp);
}

// ====================================================================================================

long elapsed(long startS, long startNS, long stopS, long stopNS) {
  // 1000000 converts seconds to microseconds. 1000 converts nanoseconds to microseconds
  return (stopS - startS)*1000000 + (stopNS - startNS)/1000;
}

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
  //sigaction(SIGUSR1, &sa, &oldsa);
  loop = true;

  // ====================================================================================================
  // for counting time
  struct timespec startTS;
  struct timespec stopTS;

  // second to microsecond
  long stous = 1000000;
  // nanosecond to microsecond
  //long nstous = 1000;
  // 10 seconds in microseconds  
  long sleepTime = 10*stous;

  // ====================================================================================================

  temperatures temps;

  while(loop) {
    // start time
    clock_gettime(CLOCK_REALTIME, &startTS);

    temps = sendAndParse(SM);
    sendTemps(SM, temps);

    // end time
    clock_gettime(CLOCK_REALTIME, &stopTS);
    //printf("%d\n", clock_gettime(CLOCK_MONOTONIC, &stopTS));
      
    // sleep for 10 seconds minus how long it took to read and send temperature
    //    usleep(sleepTime - (stopTS.tv_sec - startTS.tv_sec)*stous - (stopTS.tv_nsec - startTS.tv_nsec)/nstous);
    usleep(sleepTime - elapsed(startTS.tv_sec, startTS.tv_nsec, stopTS.tv_sec, stopTS.tv_nsec));
    //usleep(10000000);
    //printf("Done sleeping\n");
  }
  
  //  printf("Deleting SM\n");
  if(NULL != SM) {
    delete SM;
  }

  //  printf("restoring\n");
  
  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &oldsa, NULL);
  //sigaction(SIGUSR1, &oldsa, NULL);
  
  printf("Successful kill\n");
  
  return 0;
}
