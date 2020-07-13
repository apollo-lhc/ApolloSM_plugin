#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <ctime>

float MemUsage(){
  uint64_t totalMem,freeMem;
  //Open meminfo file
  FILE * memFile = fopen("/proc/meminfo","r");
  if(NULL == memFile){
    return -1;
  }

  //read in the first line the total line
  char line[100];
  char line2[100];
  fscanf(memFile,"%s %" SCNu64 " %s",line,&totalMem,line2);
  if(strncmp("MemTotal:",line,9)){
    return -1;
  }

  //read in the second line the total line
  fscanf(memFile,"%s %" SCNu64 " %s",line,&freeMem,line2);
  if(strncmp("MemFree:",line,8)){
    return -1;
  }

  fclose(memFile);
  
  float ret = 100.0*((double)(totalMem-freeMem))/((double) totalMem);
  return ret;
}

float CPUUsage(){
  uint64_t user,nice,system,idle,iowait,irq,sofirq;

  //static storage of values for diff
  static uint64_t luser,lnice,lsystem,lidle,liowait,lirq,lsofirq;
  static double lsum;

  //Open stat file
  FILE * statFile = fopen("/proc/stat","r");
  if(NULL == statFile){
    return -1;
  }
  
  //read in the first line (the CPU line)
  char cpu_line[100];
  fscanf(statFile,"%s %" SCNu64 " %"  SCNu64 " %"  SCNu64 " %"  SCNu64 " %"  SCNu64 " %"  SCNu64 " %"  SCNu64 ,
	 cpu_line,&user,&nice,&system,&idle,&iowait,&irq,&sofirq);
  fclose(statFile);

  if((cpu_line[0] != 'c') ||
     (cpu_line[1] != 'p') ||
     (cpu_line[2] != 'u')){
    return -1;
  }
  
  //compute the cpu usage
  double sum  = user+nice+system+idle+iowait+irq+sofirq;
  lsum = luser+lnice+lsystem+lidle+liowait+lirq+lsofirq;    
  float ret = 100.0*(double)(user - luser)/(sum-lsum);

  //store for next call
  luser   = user;  
  lnice   = nice;  
  lsystem = system;
  lidle   = idle;  
  liowait = iowait;
  lirq    = irq;   
  lsofirq = sofirq;
  lsum    = sum;

  return ret;
}


void Uptime(float & days, float &hours, float & minutes){
  //return the real value, but use reference passes to make human reable values
  float fullValue = 0;
  days = hours = minutes = fullValue;

  //Open uptime file
  FILE * uptimeFile = fopen("/proc/uptime","r");
  if(NULL == uptimeFile){
    return;
  }
  //Read in the first entry (number of seconds uptime)
  fscanf(uptimeFile,"%f",&fullValue);
  fclose(uptimeFile);

  //blank out values that are trivially small
  if(fullValue < 1){
    return;
  }
  //Display values in useful units (zero other types)
  if(fullValue < 60*60){
    minutes = fullValue/60.0;
  }else if(fullValue < 24*60*60){
    hours = fullValue/(60.0*60);
  }else{
    days = fullValue/(24*60.0*60);
  }
  return;
}

//returns inRate and outRate in bytes/second
int networkMonitor(int &inRate, int &outRate){
  //used to store values from file
  uint64_t InNoRoutes, InTruncatedPkts, InMcastPkts, OutMcastPkts, InBcastPkts, OutBcastPkts, InOctets, OutOctets;
  time_t time_current;
  //static storage of values for diff
  static uint64_t InOctets_running, OutOctets_running;
  static time_t time_running;

  //open stat file
  FILE * statFile = fopen("/proc/net/netstat","r");
  if(NULL == statFile){
    fprintf(stderr, "error opening /proc/net/netstat\n");
    return 1;
  }

  //Read line 4 from statFile
  char bufferOne[1000]; //buffers must be greater than line one characters ~2000
  char bufferTwo[1000];
  char bufferThree[1000];
  char cpu_line[7];
  fgets(bufferOne, 2000, statFile); //using fgets to skip past line one
  fgets(bufferTwo, 2000, statFile); //using fgets to skip past line two
  fgets(bufferThree, 2000, statFile); //using fgets to skip past line three
  time_current = time(NULL);
  fscanf(statFile, //file being scanned
  	 "%s %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, //Formatting of line 3
  	 cpu_line, &InNoRoutes, &InTruncatedPkts, &InMcastPkts, &OutMcastPkts, &InBcastPkts, &OutBcastPkts, &InOctets, &OutOctets); //store into variables
  fclose(statFile); //close file

  //get rate
  float time_dif = float(time_current - time_running);
  time_running = time_current;
  uint64_t inDiff = InOctets - InOctets_running;
  uint64_t outDiff = OutOctets - OutOctets_running;
  inRate = inDiff / time_dif;
  outRate = outDiff / time_dif;

  //re-assign running totals
  InOctets_running = InOctets;
  OutOctets_running = OutOctets;
  return 0;
}
cp /home/mikekremer/work/SM_ZYNQ_FW/os/centos/mods/build_Graphite.sh /home/mikekremer/work/SM_ZYNQ_FW/os/centos/image/tmp//Graphite/
sudo cp /home/mikekremer/work/misc/ApolloMonitor.cc /home/mikekremer/work/SM_ZYNQ_FW/os/centos/image/tmp//Graphite/src/ApolloMonitor/ApolloMonitor.cc
sudo chroot /home/mikekremer/work/SM_ZYNQ_FW/os/centos/image /usr/local/bin/qemu-arm-static /bin/bash /tmp/Graphite/build_Graphite.sh
g++ -c -o build/ApolloMonitor/ApolloMonitor.o src/ApolloMonitor/ApolloMonitor.cc -Iinclude -std=c++11 -fPIC -Wall -g -O3 -Werror -Wpedantic -I/opt/BUTool/include -I/opt/cactus/include 
  In file included from include/ApolloMonitor/ApolloMonitor.hh:3:0,
  from src/ApolloMonitor/ApolloMonitor.cc:1:
  /opt/BUTool/include/ApolloSM/ApolloSM.hh:12:83: error: extra ‘;’ [-Werror=pedantic]
  ExceptionClassGenerator(APOLLO_SM_BAD_VALUE,"Bad value use in Apollo SM code\n");
                                                                                   ^
  In file included from include/ApolloMonitor/ApolloMonitor.hh:1:0,
  from src/ApolloMonitor/ApolloMonitor.cc:1:
  include/base/SensorFactory.hh:18:16: error: ‘{anonymous}::registered’ defined but not used [-Werror=unused-variable]
  const bool registered = SensorFactory::Instance()->Register(type,             \
                ^
							      include/ApolloMonitor/ApolloMonitor.hh:19:1: note: in expansion of macro ‘RegisterSensor’
							      RegisterSensor(ApolloMonitor,"ApolloMonitor")
 ^
							      cc1plus: all warnings being treated as errors
							      make: *** [build/ApolloMonitor/ApolloMonitor.o] Error 1
g++ -c -o build/graphite_monitor.o src/graphite_monitor.cxx -Iinclude -std=c++11 -fPIC -Wall -g -O3 -Werror -Wpedantic
g++ -c -o build/base/Sensor.o src/base/Sensor.cc -Iinclude -std=c++11 -fPIC -Wall -g -O3 -Werror -Wpedantic
g++ -c -o build/base/daemon.o src/base/daemon.cc -Iinclude -std=c++11 -fPIC -Wall -g -O3 -Werror -Wpedantic
g++ -c -o build/base/SensorFactory.o src/base/SensorFactory.cc -Iinclude -std=c++11 -fPIC -Wall -g -O3 -Werror -Wpedantic
							      src/base/SensorFactory.cc:8:13: error: ‘bool CLIArgsValid(const string&, const string&)’ defined but not used [-Werror=unused-function]
							      static bool CLIArgsValid(std::string const & flag,std::string const & full_flag){
             ^
	     cc1plus: all warnings being treated as errors
	     make: *** [build/base/SensorFactory.o] Error 1
	     make: *** [/home/mikekremer/work/SM_ZYNQ_FW/os/centos/image/opt//Graphite_Monitor] Error 2
FAILED_MAKE_CENTOS
[mikekremer@tesla centos]$ 
