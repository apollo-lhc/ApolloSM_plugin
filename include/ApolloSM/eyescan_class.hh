#ifndef __EYESCANCLASS_HH__
#define __EYESCANCLASS_HH__
#include <ApolloSM/ApolloSM.hh>
//#include <BUException/ExceptionBase.hh>
#include <BUTool/ToolException.hh>
#include <IPBusIO/IPBusIO.hh>
#include <ApolloSM/eyescan.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh> // EYESCAN_ERROR
#include <vector>
#include <stdlib.h>
//#include <math.h> // pow
#include <map>
#include <syslog.h>
#include <time.h>
class eyescan
{
public:
	typedef enum { UNINIT, BUSY, WAITING_PRESCALE, DONE  } ES_state_t;

	// All necessary information to plot an eyescan
struct eyescanCoords {
  double voltage;
  double phase;
  double BER;
  uint32_t sample0;
  uint32_t error0;
  uint32_t sample1;
  uint32_t error1;
  uint8_t voltageReg;
  uint16_t phaseReg;
};

struct Coords {
  double voltage;
  double phase;
};
// struct pixel_out{ //single eyescan output
// 	double BER;
// 	uint32_t sample0;
// 	uint32_t error0;
// 	uint32_t sample1;
// 	uint32_t error1;
// };

private:
	ES_state_t es_state;
	std::vector<eyescanCoords> scan_output;
	std::vector<double> volt_vect;
	std::vector<double> phase_vect;
	int Max_prescale;
	float volt;
	float phase;



public:
	eyescan(std::string basenode, std::string lpmNode, int nBinsX, int nBinsY, int max_prescale);
	~eyescan();

	ES_state_t check();
	void update();
	std::vector<eyescanCoords> const & dataout();

private:
	eyescan();
	eyescanCoords scan_pixel(float phase, float volt, int prescale);
	//void ApolloSM::SetEyeScanPhase(std::string baseNode, /*uint16_t*/ int horzOffset, uint32_t sign);
	//void ApolloSM::SetEyeScanVoltage(std::string baseNode, uint8_t vertOffset, uint32_t sign);
	

};
	



#endif