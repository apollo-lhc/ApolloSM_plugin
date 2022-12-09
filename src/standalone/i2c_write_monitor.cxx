#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <uhal/uhal.hpp>
#include <vector>
#include <string>
#include <boost/tokenizer.hpp>
#include <unistd.h> // usleep, execl
#include <signal.h>
#include <time.h>

#include <syslog.h>  ///for syslog

#include <systemd/sd-daemon.h> // for sd_notify

#include <boost/program_options.hpp>
#include <standalone/optionParsing.hh>
#include <standalone/optionParsing_bool.hh>
#include <standalone/daemon.hh>

#include <fstream>
#include <iostream>

#define SEC_IN_US 1000000
#define NS_IN_US  1000

// ================================================================================
// Setup for boost program_options
#define DEFAULT_CONFIG_FILE "/etc/i2c_write_monitor"

// Define defaults for the daemon
#define DEFAULT_POLLTIME_IN_SECONDS 0.5
#define DEFAULT_TIMEOUT_IN_SECONDS 60
#define DEFAULT_CONN_FILE "/opt/address_table/connections.xml"
#define DEFAULT_RUN_DIR "/opt/address_table/"
#define DEFAULT_PID_FILE "/var/run/i2c_write_monitor.pid"

namespace po = boost::program_options;


long us_difftime(struct timespec cur, struct timespec end) { 
    /*
     * Helper function to compute the time difference between
     * cur and end in microseconds.
     */
    return ( (end.tv_sec  - cur.tv_sec ) * SEC_IN_US + 
	   (end.tv_nsec - cur.tv_nsec) / NS_IN_US);
}


int main(int argc, char** argv) { 

    // Set poll time, this daemon will poll the I2C DONE register
    // every X seconds.
    int polltime_in_seconds = DEFAULT_POLLTIME_IN_SECONDS;

    // Set timeout
    int timeout_in_seconds = DEFAULT_TIMEOUT_IN_SECONDS;

    // Set connection file
    std::string connectionFile = DEFAULT_CONN_FILE;

    // Set running directory
    std::string runPath = DEFAULT_RUN_DIR;

    // Set PID file name
    std::string pidFileName = DEFAULT_PID_FILE;

    /*
     * Set up program options.
     * We'll read options from command line and the configuration file.
     */
    
    // Command line options
    po::options_description cli_options("I2C Write Monitor CLI Options");
    cli_options.add_options()
        ("help,h",    "Help screen")
        ("POLLTIME_IN_SECONDS,s", po::value<int>(),         "Default polltime in seconds")
        ("TIMEOUT_IN_SECONDS,s",  po::value<int>(),         "Default timeout in seconds")
        ("CONN_FILE,c",           po::value<std::string>(), "Path to the default connections file")
        ("RUN_DIR,r",             po::value<std::string>(), "Run path")
        ("PID_FILE,p",            po::value<std::string>(), "PID file");

    // Define options from the configuration file
    po::options_description cfg_options("I2C Write Monitor Config File Options");
    cfg_options.add_options()
        ("POLLTIME_IN_SECONDS", po::value<int>(),         "Default polltime in seconds")
        ("TIMEOUT_IN_SECONDS",  po::value<int>(),         "Default timeout in seconds")
        ("CONN_FILE",           po::value<std::string>(), "Path to the default connections file")
        ("RUN_DIR",             po::value<std::string>(), "Run path")
        ("PID_FILE",            po::value<std::string>(), "PID file");


    std::map<std::string,std::vector<std::string> > allOptions;  
    // Parse options from command line
    try { 
        FillOptions(parse_command_line(argc, argv, cli_options),
            allOptions);
    } catch (std::exception &e) {
        fprintf(stderr, "Error in BOOST parse_command_line: %s\n", e.what());
        return -1;
    }

    // Help option - ends program
    if (allOptions.find("help") != allOptions.end()) {
        std::cout << cli_options << std::endl;
        return 0;
    }

    // Get the final config file path
    std::string configFileName = GetFinalParameterValue(
        std::string("config_file"),
        allOptions,
        std::string(DEFAULT_CONFIG_FILE));

    // Get options from config file
    std::ifstream configFile(configFileName.c_str());
    if (configFile) {
        try { 
            FillOptions(parse_config_file(configFile,cfg_options,true),
                allOptions);
        } catch (std::exception &e) {
            fprintf(stderr, "Error in BOOST parse_config_file: %s\n", e.what());
        }
        configFile.close();
    }

    // Get final values of parameters
    polltime_in_seconds = GetFinalParameterValue(std::string("POLLTIME_IN_SECONDS"),allOptions,DEFAULT_POLLTIME_IN_SECONDS);  
    timeout_in_seconds  = GetFinalParameterValue(std::string("TIMEOUT_IN_SECONDS") ,allOptions,DEFAULT_TIMEOUT_IN_SECONDS);  
    connectionFile      = GetFinalParameterValue(std::string("CONN_FILE")          ,allOptions,std::string(DEFAULT_CONN_FILE));
    runPath             = GetFinalParameterValue(std::string("RUN_DIR")            ,allOptions,std::string(DEFAULT_RUN_DIR));
    pidFileName         = GetFinalParameterValue(std::string("PID_FILE")           ,allOptions,std::string(DEFAULT_PID_FILE));

    /*
     * Initialize and configure the daemon.
     */
    Daemon daemon;
    daemon.daemonizeThisProgram(pidFileName, runPath);

    // Signal handling
    struct sigaction sa_INT,sa_TERM,old_sa;

    daemon.changeSignal(&sa_INT , &old_sa, SIGINT);
    daemon.changeSignal(&sa_TERM, NULL   , SIGTERM);
    daemon.SetLoop(true);

    // For counting time
    struct timespec daemonStartTS;
    struct timespec loopStartTS;
    struct timespec loopStopTS;

    // Poll time and timeout in microseconds
    long polltime_in_us = polltime_in_seconds * SEC_IN_US; 
    long timeout_in_us  = timeout_in_seconds * SEC_IN_US; 

    bool didSuccessfulRead = false;

    // Set up ApolloSM instance
    ApolloSM * SM;
    try {
        // Initialize ApolloSM
        std::vector<std::string> arg;
        arg.push_back("connections.xml");
        SM = new ApolloSM(arg);
        
        // Check if we failed to allocate an ApolloSM
        if (NULL == SM) {
            syslog(LOG_ERR,"Failed to create new ApolloSM\n");
            exit(EXIT_FAILURE);
        } else {
            syslog(LOG_INFO,"Created new ApolloSM\n");      
        }

        /*
         * Go into the main daemon loop.
         */
        syslog(LOG_INFO,"Starting I2C write monitor\n");

        // Get initial time
        clock_gettime(CLOCK_REALTIME, &daemonStartTS);

        while(daemon.GetLoop()) {
            
            // Get loop starting time
            clock_gettime(CLOCK_REALTIME, &loopStartTS);

            // Check for timeout
            long time_since_start = us_difftime(daemonStartTS, loopStartTS);
            // Timeout, break out of the loop
            if (time_since_start > timeout_in_us) {
                daemon.SetLoop(false);
            }

            // Read the register
            // If this value is 0x1, we know that I2C writes are complete
            std::string registerName = "SLAVE_I2C.S1.SM.STATUS.I2C_DONE";
            
            uint32_t writesDone = SM->ReadRegister(registerName);

            // We've read a 0x1, notify systemd that startup is complete
            if ((writesDone == 0x1) && (!didSuccessfulRead)) {
                didSuccessfulRead = true;
                sd_notify(0, "READY=1");
            }

            // Sleep until the next iteration
            clock_gettime(CLOCK_REALTIME, &loopStopTS);
            useconds_t sleep_us = polltime_in_us - us_difftime(loopStartTS, loopStopTS);
            if (sleep_us > 0) {
                usleep(sleep_us);
            }
        }

    } catch (BUException::exBase const & e) {
        syslog(LOG_ERR,"Caught BUException: %s\n   Info: %s\n",e.what(),e.Description());          
    } catch (std::exception const & e) {
        syslog(LOG_ERR,"Caught std::exception: %s\n",e.what());          
    }

    // If shutting down, clean up
    if(NULL != SM) {
        delete SM;
    }
    
    // Restore old action of receiving SIGINT (which is to kill program) before returning 
    sigaction(SIGINT, &old_sa, NULL);
    syslog(LOG_INFO,"I2C Write Monitor Daemon ended\n");

    return 0;
}

