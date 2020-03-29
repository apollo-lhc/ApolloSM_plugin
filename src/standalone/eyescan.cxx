#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <BUException/ExceptionBase.hh>
#include <string>
#include <signal.h>
#include <syslog.h>  
#include <boost/program_options.hpp>
#include <standalone/daemon.hh>
#include <standalone/parseOptions.hh>

// ====================================================================================================
// Define all your defaults here
#define DEFAULT_CONFIG_FILE         "/etc/eyescan"
#define DEFAULT_RUN_DIR             "/opt/address_tables/"
#define DEFAULT_PID_FILE            "/var/run/eyescan.pid"
#define DEFAULT_POLLTIME_IN_MINUTES 30 
#define DEFAULT_BASE_NODE           "C2C1_PHY."
#define DEFAULT_FILE_NAME           "c2c1_phy.txt"
#define DEFAULT_HORZ_INCREMENT      0.02 // UI is from -0.5 to 0.5
#define DEFAULT_VERT_INCREMENT      2    // Voltage is from -127 to 127

// ====================================================================================================
// Define any functions you need here

// ====================================================================================================
int main(int argc, char** argv) { 

  // parameters to get from command line or config file (config file itself will not be in the config file, obviously)
  std::string configFile          = DEFAULT_CONFIG_FILE;
  std::string runPath             = DEFAULT_RUN_DIR;
  std::string pidFileName         = DEFAULT_PID_FILE;
  int         polltime_in_minutes = DEFAULT_POLLTIME_IN_MINUTES;
  std::string baseNode            = DEFAULT_BASE_NODE;
  std::string fileName            = DEFAULT_FILE_NAME;
  double      horzIncrement       = DEFAULT_HORZ_INCREMENT;
  int         vertIncrement       = DEFAULT_VERT_INCREMENT;       

  // parse command line and config file to set parameters
  boost::program_options::options_description fileOptions{"File"}; // for parsing config file
  boost::program_options::options_description commandLineOptions{"Options"}; // for parsing command line
  commandLineOptions.add_options()
    ("config_file",
     boost::program_options::value<std::string>(),
     "config file"); // This is the only option not also in the file option (obviously)
  setOption(&fileOptions, &commandLineOptions, "run_path" , "run path" , runPath);
  setOption(&fileOptions, &commandLineOptions, "pid_file" , "pid file" , pidFileName);
  setOption(&fileOptions, &commandLineOptions, "polltime" , "polling interval" , polltime_in_minutes);
  setOption(&fileOptions, &commandLineOptions, "baseNode" , "link to scan" , baseNode);
  setOption(&fileOptions, &commandLineOptions, "data_file" , "data file" , fileName);
  setOption(&fileOptions, &commandLineOptions, "horzIncrement" , "phase increment" , horzIncrement);
  setOption(&fileOptions, &commandLineOptions, "vertIncrement" , "voltage increment" , vertIncrement);

  boost::program_options::variables_map configFileVM; // for parsing config file
  boost::program_options::variables_map commandLineVM; // for parsing command line

  // The command line must be parsed before the config file so that we know if there is a command line specified config file 
  fprintf(stdout, "Parsing command line now\n");
  try {
    // parse command line
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, commandLineOptions), commandLineVM);
  } catch(const boost::program_options::error &ex) {
    fprintf(stderr, "Caught exception while parsing command line: %s \nTerminating eyescan\n", ex.what());       
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
    fprintf(stdout, "Caught exception in function loadConfig(): %s \nTerminating eyescan\n", ex.what());        
    return -1;
  }

  // Look at the config file and command line and see if we should change the parameters from their default values
  // Only run path and pid file are needed for the next bit of code. The other parameters can and should wait until syslog is available.
  setParamValue(&runPath    , "run_path", configFileVM, commandLineVM, false);
  setParamValue(&pidFileName, "pid_file", configFileVM, commandLineVM, false);

  // ============================================================================
  // Deamon book-keeping
  // Every daemon program should have one Daemon object. Daemon class functions are functions that all daemons progams have to perform. That is why we made the class.
  Daemon eyescanDaemon;
  eyescanDaemon.daemonizeThisProgram(pidFileName, runPath);

  // ============================================================================
  // Now that syslog is available, we can continue to look at the config file and command line and determine if we should change the parameters from their default values.
  setParamValue(&polltime_in_minutes, "polltime" , configFileVM, commandLineVM, true);
  setParamValue(&baseNode, "baseNode" , configFileVM, commandLineVM, true);
  setParamValue(&fileName, "data_file" , configFileVM, commandLineVM, true);
  setParamValue(&horzIncrement, "horzIncrement" , configFileVM, commandLineVM, true);
  setParamValue(&vertIncrement, "vertIncrement" , configFileVM, commandLineVM, true);
  
  // ============================================================================
  // Signal handling
  struct sigaction sa_INT,sa_TERM,old_sa;
  // struct sigaction sa_INT,sa_TERM,oldINT_sa,oldTERM_sa;
  eyescanDaemon.changeSignal(&sa_INT , &old_sa, SIGINT);
  eyescanDaemon.changeSignal(&sa_TERM, NULL   , SIGTERM);
  eyescanDaemon.loop = true;

  // ============================================================================
  // More set up if needed.

  // ============================================================================
  ApolloSM * SM = NULL;
  try{
    // Initialize ApolloSM
    SM = new ApolloSM();
    if(NULL == SM){
      syslog(LOG_ERR,"Failed to create new ApolloSM\n");
      exit(EXIT_FAILURE);
    }else{
      syslog(LOG_INFO,"Created new ApolloSM\n");      
    }
    std::vector<std::string> arg;
    arg.push_back("connections.xml");
    SM->Connect(arg);    
    // ==================================
    // More set up if needed.

    // ==================================

    syslog(LOG_INFO, "Starting main eye scan loop\n");

    while(eyescanDaemon.loop) {
      
      // 1. Eye scan and write to file.
      std::vector<eyescanCoords> esCoords = SM->EyeScan(baseNode, horzIncrement, vertIncrement);

      FILE * dataFile = fopen(fileName.c_str(), "w");
      
      for(int i = 0; i < (int)esCoords.size(); i++) {
	fprintf(dataFile, "%.9f ", esCoords[i].phase);
	fprintf(dataFile, "%d "  , esCoords[i].voltage);
	fprintf(dataFile, "%f "  , esCoords[i].BER);
	fprintf(dataFile, "%x "  , esCoords[i].voltageReg & 0xFF);
	fprintf(dataFile, "%x\n" , esCoords[i].phaseReg & 0xFFF);
      }

      fclose(dataFile);
      
      // 2. Graph it with Python.

      std::string runPython = "python eyescan.py " + dataFile;
      system(runPython.c_str());
      // python ./root/felex/eyescan.py .txt

      // 3. Upload it.

      std::string png = dataFile;
      png.pop_back();
      png.pop_back();
      png.pop_back();
      png.append("png");
     
      std::string copy = "cp " + png + "/var/www/lighttpd";
      system(copy);

      // cp basenode.png /var/www/lighttpd

      // 4. Sleep

      for(int i = 0; i < polltime_in_minutes; i++) {
	usleep(1000000);
      }

    }
  }catch(BUException::exBase const & e){
    syslog(LOG_INFO,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());
          
  }catch(std::exception const & e){
    syslog(LOG_INFO,"Caught std::exception: %s\n",e.what());
          
  }
  
  // ============================================================================
  // Clean up  
  
  // Pre-delete SM clean up stuff if nedeed.
  
  // ==================================
  if(NULL != SM) {
    delete SM;
  }

  // Restore old action of receiving SIGINT (which is to kill program) before returning 
  sigaction(SIGINT, &old_sa, NULL);
  syslog(LOG_INFO,"eyescan Daemon ended\n");

  // ============================================================================  
  return 0;
}
