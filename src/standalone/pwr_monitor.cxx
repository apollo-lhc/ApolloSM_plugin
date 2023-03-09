#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <ctime>

#include <vector> //for vectors
#include <string>

#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>

//pselect stuff
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

//signals
#include <signal.h>

#include <syslog.h>  ///for syslog


#include <boost/program_options.hpp>
#include <standalone/optionParsing.hh>
#include <standalone/optionParsing_bool.hh>
#include <standalone/daemon.hh>


#include <fstream>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp> //ieqals
#include <boost/algorithm/string/case_conv.hpp> //to_upper
#include <boost/filesystem.hpp>

int GetSysFSValue(std::string filename){
  int ret = -1;
  FILE * inFile = fopen(filename.c_str(),"r");
  if(NULL == inFile){
    return ret;
  }
  fscanf(inFile,"%d",&ret);
  fclose(inFile);
  return ret;
}

float GetCurrent(std::string basePath,size_t iSensor){
  float ret = -1;
  if (iSensor == 0 || iSensor > 3){
    BUException::BAD_PARAMETER e;
    throw e;
  }
  basePath+="/curr"+std::to_string(iSensor)+"_input";
  ret = GetSysFSValue(basePath);
  return ret;
}

float GetVoltage(std::string basePath,size_t iSensor){
  float ret = -1;
  if (iSensor == 0 || iSensor > 3){
    BUException::BAD_PARAMETER e;
    throw e;
  }
  basePath+="/in"+std::to_string(iSensor)+"_input";
  ret = GetSysFSValue(basePath);
  return ret;
}

//Get the label from a path and an iSensor
//iSensor goes from 1 to 3.
//iSensor = 0 means use basePath as the full path
std::string GetLabel(std::string basePath,size_t iSensor){
  if ( iSensor > 3){
    BUException::BAD_PARAMETER e;
    throw e;
  }else if (iSensor > 0){
    basePath+="/in"+std::to_string(iSensor)+"_label";
  }  
  
  size_t const strArgSize = 64;
  char strArg[strArgSize+1];
  memset(strArg,0,strArgSize+1);

  FILE * inFile = fopen(basePath.c_str(),"r");
  if(NULL == inFile){
    return std::string("");
  }

  fscanf(inFile, "%20s", strArg);
  fclose(inFile);
  
  return std::string(strArg);
}

void UpdateINA3221Sensor(ApolloSM * sm,
			 std::string const & baseTablePath,
			 std::string const & baseFilePath,
			 size_t iSense){
  std::string label = GetLabel(baseFilePath,iSense);
  float voltage = GetVoltage(baseFilePath,iSense);
  float current = GetCurrent(baseFilePath,iSense);
  try{
    sm->WriteRegister(baseTablePath+"."+label+".I",current);
    sm->WriteRegister(baseTablePath+"."+label+".V",voltage);
  }catch(BUException::exBase &e){
  }    
}

#define SEC_IN_US 1000000
#define NS_IN_US 1000

// ================================================================================
#define DEFAULT_CONFIG_FILE "/etc/pwr_monitor"

#define DEFAULT_POLLTIME_IN_SECONDS 10
#define DEFAULT_RUN_DIR "/opt/address_table"
#define DEFAULT_PID_FILE "/var/run/pwr_monitor.pid"
#define DEFAULT_CONN_FILE "/fw/SM/address_table/connections.xml"

namespace po = boost::program_options;


// ====================================================================================================
long us_difftime(struct timespec cur, struct timespec end){ 
  return ( (end.tv_sec  - cur.tv_sec )*SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec)/NS_IN_US);
}

// ==================================================

int main(int argc, char ** argv) {
  //=======================================================================
  // Set up program options
  //=======================================================================
  //Command Line options
  po::options_description cli_options("pwr_monitor options");
  cli_options.add_options()
    ("help,h",    "Help screen")
    ("POLLTIME_IN_SECONDS,s", po::value<int>(),         "polltime in seconds")
    ("RUN_DIR,r",             po::value<std::string>(), "run path")
    ("CONN_FILE,c",           po::value<std::string>(), "Path to the XML connections file")
    ("PID_FILE,d",            po::value<std::string>(), "pid file")
    ("config_file",           po::value<std::string>(), "config file");


  //Config File options
  po::options_description cfg_options("pwr_monitor options");
  cfg_options.add_options()
    ("POLLTIME_IN_SECONDS", po::value<int>(),         "polltime in seconds")
    ("RUN_DIR",             po::value<std::string>(), "run path")
    ("CONN_FILE",           po::value<std::string>(), "Path to the XML connections file")
    ("PID_FILE",            po::value<std::string>(), "pid file")
    ("SESNOR_NAME",            po::value<std::string>(), "Sensor to read out")
    ("TABLE_PATH_ROOT",            po::value<std::string>(), "Address table path");


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
  
  
  //Set polltime_in_seconds
  int polltime_in_seconds = GetFinalParameterValue(std::string("POLLTIME_IN_SECONDS"), allOptions, DEFAULT_POLLTIME_IN_SECONDS);
  //Set runPath
  std::string runPath     = GetFinalParameterValue(std::string("RUN_DIR"),             allOptions, std::string(DEFAULT_RUN_DIR));
  //set pidFileName
  std::string pidFileName = GetFinalParameterValue(std::string("PID_FILE"),            allOptions, std::string(DEFAULT_PID_FILE));
  // Get XML connection file for ApolloSM
  std::string connectionFile = GetFinalParameterValue(std::string("CONN_FILE"),        allOptions, std::string(DEFAULT_CONN_FILE));

  // ============================================================================
  // Deamon book-keeping
  Daemon daemon;
  daemon.daemonizeThisProgram(pidFileName, runPath);
  
  // ============================================================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  
  daemon.changeSignal(&sa_INT , &old_sa, SIGINT);
  daemon.changeSignal(&sa_TERM, NULL   , SIGTERM);
  daemon.SetLoop(true);


  syslog(LOG_INFO,"Using \"%s\" for the table name base ",allOptions["TABLE_PATH_ROOT"][0].c_str());


  // ==================================
  // Find all the sensors we are looking for and save their paths
  struct sSensor {std::string name; std::string filePath; size_t index;};
  std::vector<sSensor> sensorList;
  for(auto itSensor = allOptions["SENSOR_NAME"].begin();
      itSensor != allOptions["SENSOR_NAME"].end();
      itSensor++){
    //Search for the current sensor
    sSensor sen;
    sen.name = *itSensor;
    boost::algorithm::to_upper(sen.name);
    syslog(LOG_INFO,"Looking for sensor %s\n",sen.name.c_str());
    for (boost::filesystem::directory_iterator hwmonPath("/sys/class/hwmon/"); 
	 hwmonPath!=boost::filesystem::directory_iterator(); ++hwmonPath) {
      //loop over hwmon devices
      if (boost::filesystem::is_directory(hwmonPath->path())) {
	for (boost::filesystem::directory_iterator ihwmonPath(hwmonPath->path().string()); 
	     ihwmonPath!=boost::filesystem::directory_iterator(); ++ihwmonPath) {
	  //loop over files for each hwmonitor
	  if (boost::filesystem::is_directory(ihwmonPath->path())) {
	    continue;
	  }else if(ihwmonPath->path().filename().string().find("_label") 
		   != std::string::npos){
	    //we found a label file, check if it is 
	    std::string currentLabel = GetLabel(ihwmonPath->path().native(),0);	  
	    if(boost::iequals(sen.name,currentLabel)){
	      //correct sensor
	      //get file path
	      sen.filePath=ihwmonPath->path().parent_path().string();
	      //determine the sensor number
	      sen.index = ihwmonPath->path().filename().string()[2] - 48;   // '0' is ascii code 48
	      syslog(LOG_INFO,"  Found sensor %s at %s:%zu\n",currentLabel.c_str(),sen.filePath.c_str(),sen.index);
	      sensorList.push_back(sen);
	      break;
	    }
	  }
	}
      }
    }
  }



  ApolloSM * SM = NULL;
  try{
    // ==================================
    // Initialize ApolloSM
    std::vector<std::string> arg;
    arg.push_back(connectionFile);
    SM = new ApolloSM(arg);
    if(NULL == SM){
      syslog(LOG_ERR,"Failed to create new ApolloSM\n");
      exit(EXIT_FAILURE);
    }else{
      syslog(LOG_INFO,"Created new ApolloSM\n");      
    }

  
    
    //vars for pselect
    struct timespec timeout = {polltime_in_seconds,0};
    int maxFDp1 = 0;
    
    // ==================================
    // Main DAEMON loop
    syslog(LOG_INFO,"Starting PWR monitor\n");
    
    // ==================================================
    // All the work
    while(daemon.GetLoop()){
      int pselRet = pselect(maxFDp1,NULL,NULL,NULL,&timeout,NULL);
      if(0 == pselRet){
	//timeout
	for(auto itSensor = sensorList.begin();
	    itSensor != sensorList.end();
	    itSensor++){
	  UpdateINA3221Sensor(SM,
			      allOptions["TABLE_PATH_ROOT"][0],
			      itSensor->filePath,
			      itSensor->index);
			      
	}
      }
    }
  }catch(BUException::exBase const & e){
    syslog(LOG_ERR,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());          
  }catch(std::exception const & e){
    syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
  }
  
  // ==================================================
  // Clean up. Close and delete everything.

  // Delete SM
  if(NULL != SM) {
    delete SM;
  }

  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"PWR Monitor Daemon ended\n");
  return 0;
}
