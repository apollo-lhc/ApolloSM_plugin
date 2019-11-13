#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

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
