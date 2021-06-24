
#ifndef __EYESCAN_HH__
#define __EYESCAN_HH__

// All necessary information to plot an eyescan
struct eyescanCoords {
  int voltage;
  double phase;
  double BER;
  int sample;
  int errors;
  uint8_t voltageReg;
  uint16_t phaseReg;
};

#endif