#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <standalone/CM.hh>
#include <vector>
#include <string>
#include <boost/algorithm/string/predicate.hpp> // for iequals

// ================================================================================
// Setup for boost program_options
#include <boost/program_options.hpp>
#include <standalone/progOpt.hh>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/uartCmd"
#define DEFAULT_CONN_FILE "/opt/address_table/connections.xml"
namespace po = boost::program_options;

// ================================================================================
int main(int argc, char** argv) { 

  int const numberGoodArgs = 3;  

  if(argc < numberGoodArgs) {
    printf("Wrong number of arguments. Please include at least a device and a command\n");
    return -1;
  }

  //=======================================================================
  // Set up program options
  //=======================================================================
  //Command Line options
  po::options_description cli_options("cmpwrdown options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("CONN_FILE,C", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file");

  //Config File options
  po::options_description cfg_options("cmpwrdown options");
  cfg_options.add_options()
    ("CONN_FILE", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file");
  
  //variable_maps for holding program options
  po::variables_map cli_map;
  po::variables_map cfg_map;

  //Store command line and config file arguments into cli_map and cfg_map
  try {
    cli_map = storeCliArguments(cli_options, argc, argv);
  } catch (std::exception &e) {
    std::cout << cli_options << std::endl;
    return 0;
  }

  try {
    cfg_map = storeCfgArguments(cfg_options, DEFAULT_CONFIG_FILE);  
  } catch (std::exception &e) {}
  
  //Help option - ends program
  if(cli_map.count("help")){
    std::cout << cli_options << '\n';
    return 0;
  }

  //Set connection file
  std::string connectionFile = DEFAULT_CONN_FILE;
  setOptionValue(connectionFile, "CONN_FILE", cli_map, cfg_map);

  //=======================================================================
  // Run UART command
  //=======================================================================
  // Make an ApolloSM
  ApolloSM * SM = NULL;
  try{
    SM = new ApolloSM();
    if(NULL == SM){
      fprintf(stderr, "Failed to create new ApolloSM. Terminating program\n");
      exit(EXIT_FAILURE);
    }
    std::vector<std::string> arg;
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
    if(boost::algorithm::iequals(strArg[0],"CM_1")) {
      ttyDev.append("/dev/ttyUL1");    
      promptChar = '%';
    } else if(boost::algorithm::iequals(strArg[0],"CM_2")) {
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
