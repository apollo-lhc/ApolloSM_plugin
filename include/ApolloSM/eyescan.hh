
#ifndef __EYESCAN_HH__
#define __EYESCAN_HH__

// All necessary information to plot an eyescan
struct eyescanCoords {
  int voltage;
  double phase;
  double BER;
  int sample0;
  int error0;
  int sample1;
  int error1;
  uint8_t voltageReg;
  uint16_t phaseReg;
};
struct SESout{ //single eyescan output
	double BER;
	int sample0;
	int error0;
	int sample1;
	int error1;
};

#endif
