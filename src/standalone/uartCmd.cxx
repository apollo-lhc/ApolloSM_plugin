#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
//#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <standalone/CM.hh>
//#include <uhal/uhal.hpp>
#include <vector>
#include <string>
//#include <sys/stat.h> //for umask
//#include <sys/types.h> //for umask
//#include <BUException/ExceptionBase.hh>
#include <boost/algorithm/string/predicate.hpp> // for iequals

// ================================================================================
//#define DEFAULT_RUN_DIR     "/opt/address_tables/"
#define DEFAULT_CONNECTION_FILE "/opt/address_tables/connections.xml"

// ================================================================================
int main(int argc, char** argv) { 

  int const numberGoodArgs = 3;  

  if(argc < numberGoodArgs) {
    printf("Wrong number of arguments. Please include at least a device and a command\n");
    return -1;
  }
  
  // Make an ApolloSM
  ApolloSM * SM = NULL;
  try{
    SM = new ApolloSM();
    if(NULL == SM){
      fprintf(stderr, "Failed to create new ApolloSM. Terminating program\n");
      exit(EXIT_FAILURE);
    }//else{
    //  fprintf(stdout,"Created new ApolloSM\n");      
    //}
    // load connection file
    std::vector<std::string> arg;
    std::string connectionFile = DEFAULT_CONNECTION_FILE;
    printf("Using %s\n", connectionFile.c_str());
    arg.push_back(connectionFile);
    SM->Connect(arg);
    
    // ==============================
    // parse command line
    
    // load argv into this vector
    std::vector<std::string> strArg;
    for(int i = 1; i < numberGoodArgs; i++) {
      strArg.push_back(argv[i]);
    }

    std::string ttyDev;
    char promptChar;
    if(boost::algorithm::iequals(strArg[0],"CM1")) {
      ttyDev.append("/dev/ttyUL1");    
      promptChar = '%';
    } else if(boost::algorithm::iequals(strArg[0],"CM2")) {
      ttyDev.append("/dev/ttyUL2");
      promptChar = '%';
    } else if(boost::algorithm::iequals(strArg[0],"ESM")) {
      ttyDev.append("/dev/ttyUL3");
      promptChar = '>';
    } else {
      printf("Invalid device\n");
      return -1;
    }

    //make one string to send
    std::string sendline;
    for(int i = 1; i < (int)strArg.size(); i++) {
      sendline.append(strArg[i]);
      sendline.push_back(' ');
    }
    //get rid of last space
    sendline.pop_back();
    
    // so the output from a uart_cmd consists of, foremost, a bunch of control sequences, then a new line, and then
    // the output of what we want, version or help menu, etc. What we will do is throw away everything before the 
    // first new line
    int const firstNewLine = 10;
    size_t firstNewLineIndex;
    
    // send the command
    std::string recvline = SM->UART_CMD(ttyDev, sendline, promptChar);  
  
    // find the first new line
    for(size_t i = 0; i < recvline.size(); i++) {
      if(firstNewLine == (int)recvline[i]) {
	firstNewLineIndex = i;
	break;
      }
    }
    
    // Erase the newline and everything before it. Then print
    recvline.erase(recvline.begin(), recvline.begin()+firstNewLineIndex);
    printf("Received:\n\n%s\n\n", recvline.c_str());
    
  }catch(BUException::exBase const & e){
    fprintf(stdout,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());
  }catch(std::exception const & e){
    fprintf(stdout,"Caught std::exception: %s\n",e.what());
  }
  
  // Clean up
  //printf("Deleting ApolloSM\n");
  if(NULL != SM) {
    delete SM;
  }

  return 0;
}
