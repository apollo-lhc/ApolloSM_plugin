#ifndef __LINUX_SYSTEM_MONITOR_HH__
#define __LINUX_SYSTEM_MONITOR_HH__
#include <stdint.h>
float MemUsage();
float CPUUsage();
uint32_t Uptime(float & days, float &hours, float & minutes);
int networkMonitor(int &inRate, int&outRate);
#endif
