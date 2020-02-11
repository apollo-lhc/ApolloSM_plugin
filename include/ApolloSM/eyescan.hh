#ifndef __EYESCAN_HH__
#define __EYESCAN_HH__

// All necessary information to plot an eyescan
struct eyescanCoords {
  uint8_t voltage;
  float phase;
  float BER;
};

#endif
