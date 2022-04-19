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
#include <boost/lexical_cast.hpp> //for lexical_cast

std::vector<std::string> grabLine(FILE * file){
  
  std::vector<std::string> line;
  bool running = true;
  while(running){
    char charArg;
    char strArg[64] = "";
    fscanf(file, "%s%c", strArg, &charArg);
    std::string lineArg(strArg);
    line.push_back(lineArg);
    if(charArg == '\n') {running = false;} //end loop at newline
  }
  return line;
}

float MemUsage(){
  // Computes and returns the % memory usage by comparing the 
  // available and total memory values, read from /proc/meminfo
  uint64_t totalMem,freeMem,availableMem;
  // Open meminfo file
  FILE * memFile = fopen("/proc/meminfo","r");
  if(NULL == memFile){
    return -1;
  }

  // Read in the first line: Total memory
  char line[100];
  char line2[100];
  fscanf(memFile,"%s %" SCNu64 " %s",line,&totalMem,line2);
  if(strncmp("MemTotal:",line,9)){
    return -1;
  }

  // Read in the second line: Free memory
  fscanf(memFile,"%s %" SCNu64 " %s",line,&freeMem,line2);
  if(strncmp("MemFree:",line,8)){
    return -1;
  }
  
  // Read in the third line: Available memory
  fscanf(memFile,"%s %" SCNu64 " %s",line,&availableMem,line2);
  if(strncmp("MemAvailable:",line,13)){
    return -1;
  }

  fclose(memFile);
  
  // Compute the memory usage by comparing the available memory and total memory
  float ret = 100.0*((double)(totalMem-availableMem))/((double) totalMem);
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

  //static storage of values for calculating differences
  static uint64_t InOctets_running, OutOctets_running;
  static time_t time_running;

  //open stat file
  FILE * statFile = fopen("/proc/net/netstat","r");
  if(NULL == statFile){return 1;}

  /* Read lineOne from statFile */
  time_t time_current = time(NULL); //get current time
  char lineTitle[10]; //to store line header
  std::vector<std::string> lineOne;
  bool line_captured = false;
  fscanf(statFile /*file being scanned*/, "%s "/*store string*/, lineTitle /*store string into lineTitle*/); //first scan
  while(!line_captured){
    if((lineTitle[0] == 'I') && (lineTitle[1] == 'p') && (lineTitle[2] == 'E') && (lineTitle[3] == 'x') && (lineTitle[4] == 't') && (lineTitle[5] == ':')) { //lineTitle == "IpExt:"
      lineOne = grabLine(statFile); //store line
      line_captured = true; //end while loop
    } else {
      //move filepointer to newline
      char throwAway = 'x';
      while (throwAway != '\n'){
  	throwAway = fgetc(statFile);
      }
    }
    fscanf(statFile, "%s ", lineTitle); //get next line header
  }

  /* Read lineTwo from statFile */
  std::vector<std::string> lineTwo;
  line_captured = false;
  while(!line_captured){
    if((lineTitle[0] == 'I') && (lineTitle[1] == 'p') && (lineTitle[2] == 'E') && (lineTitle[3] == 'x') && (lineTitle[4] == 't') && (lineTitle[5] == ':')) { //lineTitle == "IpExt:"
      lineTwo = grabLine(statFile); //store line
      line_captured = true; //end while loop
    } else {
      //move filepointer to newline
      char throwAway = 'x';
      while (throwAway != '\n'){
  	throwAway = fgetc(statFile);
      }
    }
    fscanf(statFile, "%s ", lineTitle); //get next line header
  }
  fclose(statFile); //close file

  /* Error checking */
  if(lineOne.size() != lineTwo.size()){
    return 2;
  }

  /* Determine which line is made of headers and which is made of values */
  std::vector<std::string> headers, values;
  if(isdigit(lineOne[0][0]) != 0){
    headers = lineTwo;
    values = lineOne;
  } else {
    headers = lineOne;
    values = lineTwo;
  }

  /* Search headers for matching strings, then assign InOctet and OutOctet from the corresponding index in values */
  uint64_t InOctets = 0;
  uint64_t OutOctets = 0;
  for (uint i = 0; i < headers.size(); i++) { //iterate through headers
    if (headers[i] == "InOctets") {
      InOctets = boost::lexical_cast<uint64_t>(values[i].c_str());
    }
    if (headers[i] == "OutOctets") {
      OutOctets = boost::lexical_cast<uint64_t>(values[i].c_str());
    }      
  } 

  /* calculate rate of change for InOctets and OutOctets */
  //Get differences
  float time_diff = float(time_current - time_running);
  uint64_t InOctets_diff = InOctets - InOctets_running;
  uint64_t OutOctets_diff = OutOctets - OutOctets_running;
  //Re-assign running totals
  time_running = time_current;
  InOctets_running = InOctets;
  OutOctets_running = OutOctets;
  //Get rate of change
  if (time_diff != 0) { //Do not divide by 0
    inRate = InOctets_diff / time_diff;
    outRate = OutOctets_diff / time_diff;
  } else { //this should make it obvious in the charts that something went wrong
    inRate = -1;
    outRate = -1;
  }
  return 0;
}
