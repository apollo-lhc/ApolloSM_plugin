#ifndef __LINUX_SYSTEM_MONITOR_HH__
#define __LINUX_SYSTEM_MONITOR_HH__
float MemUsage();
float CPUUsage();
void Uptime(float & days, float &hours, float & minutes);
int networkMonitor(int &inRate, int&outRate);
#endif
