
#ifndef __EYESCAN_HH__
#define __EYESCAN_HH__

// All necessary information to plot an eyescan
struct eyescanCoords {
  int voltage;
  double phase;
  double BER;
  unsigned long int sample0;
  unsigned long int error0;
  unsigned long int sample1;
  unsigned long int error1;
  uint8_t voltageReg;
  uint16_t phaseReg;
};
struct SESout{ //single eyescan output
	double BER;
	unsigned long int sample0;
	unsigned long int error0;
	unsigned long int sample1;
	unsigned long int error1;
};

#endif
