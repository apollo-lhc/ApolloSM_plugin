#ifndef __EYESCANCLASS_HH__
#define __EYESCANCLASS_HH__
#include <ApolloSM/ApolloSM.hh>
#include <queue>
// //#include <BUException/ExceptionBase.hh>
// #include <BUTool/ToolException.hh>
// #include <IPBusIO/IPBusIO.hh>
// //#include <ApolloSM/eyescan.hh>
// #include <ApolloSM/ApolloSM_Exceptions.hh> // EYESCAN_ERROR
// #include <vector>
// #include <stdlib.h>
// //#include <math.h> // pow
// #include <map>
// #include <syslog.h>
// #include <time.h>

// Correct eye scan attribute values
#define ES_EYE_SCAN_EN 0x1
#define ES_ERRDET_EN 0x1
#define PMA_CFG 0x000 // Actually 10 0s: 10b0000000000
#define PMA_RSV2 0x1
#define ES_QUALIFIER 0x0000
#define ES_QUAL_MASK 0xFFFF

#define RX_DATA_WIDTH_GTX 0x4 // zynq
#define RX_INT_DATAWIDTH_GTX 0x1 // We use 32 bit

#define RX_DATA_WIDTH_GTH 0x4 // kintex
#define RX_INT_DATAWIDTH_GTH 0x0 //16 bit

#define RX_DATA_WIDTH_GTY 0x6 // virtex
#define RX_INT_DATAWIDTH_GTY 0x1 //32 bit

// identifies bus data width
std::map<int, int> static const busWidthMap = 
  {
    // read hex value (DRP encoding) vs bus width (attribute encoding)
    {2, 16},
    {3, 20},
    {4, 32},
    {5, 40},
    {6, 64},
    {7, 80}
    // currently unsupported values
    //,
    //{8, 128},
    //{9, 160}
  };

class eyescan
{
public:
  typedef enum { SCAN_INIT, SCAN_RESET, SCAN_START, SCAN_PIXEL, SCAN_DONE  } ES_state_t;
  typedef enum { FIRST, SECOND, LPM} DFE_state_t;

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

  // struct Coords {
  //   double voltage;
  //   double phase;
  // };

private:
  std::string lpmNode;
  std::string baseNode;
  ES_state_t es_state;
  //std::vector<std::vector<Coords>> Coords_vect;
  //std::queue<eyescan::Coords> Coords_queue;
  std::vector<eyescanCoords> Coords_vect;
  std::vector<eyescanCoords>::iterator it = Coords_vect.begin();
  std::vector<double> volt_vect;
  std::vector<double> phase_vect;
  uint32_t Max_prescale;
  float volt;
  float phase;
  int Max_prescale;
  int cur_prescale;
  int nBinsX
  int nBinsY

  //make these #defines
  uint32_t const dfe = 0;
  uint32_t const lpm = 1;
  uint32_t rxlpmen;



public:
  eyescan(ApolloSM*SM, std::string baseNode_set, std::string lpmNode_set, int nBinsX_set, int nBinsY_set, int max_prescale);
  ~eyescan();
  //ES_state_t check();
  void check();
  void update(ApolloSM*SM);
  std::vector<eyescanCoords>const & dataout();
  void throwException(std::string message);
  //make function to dump to file

private:
  eyescan();
  eyescanCoords scan_pixel(ApolloSM*SM);
  void initialize();
  void EndPixelLPM(ApolloSM*SM);
  void EndPixelDFE(ApolloSM*SM);

  void SetEyeScanVoltage(ApolloSM*SM, std::string baseNode, uint8_t vertOffset, uint32_t sign);

  void SetEyeScanPhase(ApolloSM*SM, std::string baseNode, /*uint16_t*/ int horzOffset, uint32_t sign);

};


#endif
