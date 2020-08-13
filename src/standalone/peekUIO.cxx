#include <ApolloSM/uioLabelFinder.hh>
#include <string>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>

// ================================================================================
// Setup for boost program_options
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#define DEFAULT_CONFIG_FILE "/etc/BUTool"
namespace po = boost::program_options;

po::variables_map getVariableMap(int argc, char** argv, po::options_descriptiom options, std::ifstream configFile) {
  //container for prog options grabbed from commandline and config file
  po::variables_map progOptions;

  //Get options from command line
  try { 
    po::store(parse_command_line(argc, argv, options), progOptions);
  } catch (std::exception &e) {
    fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
    std::cout << options << std::endl;
    return 0;
  }

  //If configFile opens, get options from config file
  if(configFile) { 
    try{ 
      po::store(parse_config_file(configFile,options,true), progOptions);
    } catch (std::exception &e) {
      fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
      std::cout << options << std::endl;
      return 0; 
    }
  }

  //help option, this assumes help is a member of options_description
  if(progOptions.count("help")){
    std::cout << options << '\n';
    return 0;
  }
 
  return progOptons;
}

// ================================================================================

int main(int argc, char ** argv){
  std::string label;
  uint32_t address;
  uint32_t count = 1;

  // ===== Work in progress
    //Set up program options
  po::options_description options("cmpwrdown options");
  options.add_options()
    ("help,h",    "Help screen")
    ("DEFAULT_CONNECTION_FILE,C", po::value<std::string>()->default_value("/opt/address_table/connections.xml"), "Path to the default config file")
    ("DEFAULT_CM_ID,c",           po::value<int>()->default_value(1),                                             "Default CM to power down");
  
  //setup for loading program options
  std::ifstream configFile(DEFAULT_CONFIG_FILE);
  po::variables_map progOptions = getVariableMap(argc, argv, options, configFile);


  switch (argc){
  case 4:
    //Get count
    count = strtoul(argv[3],NULL,0);
    //fallthrough
  case 3:
    //Get address
    address = strtoul(argv[2],NULL,0);
    //set UIO label
    label.assign(argv[1]);
    break;
  default:
    printf("Usage: %s uio_label addr <count=1>\n",argv[0]);
    return 1;
    break;
  }

  //Find UIO for label
  int uio = label2uio(argv[1]);
  if(uio < 0){
    fprintf(stderr,"%s not found\n",argv[1]);
    return 1;
  }else{
    printf("UIO: %d\n",uio);
  }
  char UIOFilename[] = "/dev/uioXXXXXXXX";
  snprintf(UIOFilename,strlen(UIOFilename),
	   "/dev/uio%d",uio);

  //Open UIO
  int fdUIO = open(UIOFilename,O_RDWR);
  if(fdUIO < 0){
    fprintf(stderr,"Error opening %s\n",label.c_str());
    return 1;
  }

  uint32_t * ptr = (uint32_t *) mmap(NULL,sizeof(uint32_t)*(address+count),
				   PROT_READ|PROT_WRITE, MAP_SHARED,
				   fdUIO,0x0);
  
  if(MAP_FAILED == ptr){
    fprintf(stderr,"Failed to mmap\n");
    return 1;
  }

  uint32_t endAddress =  address+count;
  for(uint32_t startAddress = address&(~0x7);
      startAddress < endAddress;
      ){
    printf("0x%08X: ",startAddress);
    for(int WordCount = 7; WordCount >= 0;WordCount--){
      if(startAddress < endAddress){
	if(startAddress < address){
	  printf("         ");
	}else{
	  printf("0x%08X ",ptr[startAddress+WordCount]);
	}
      }
    }
    startAddress +=8;
    printf("\n");
  }

  return 0;
}
