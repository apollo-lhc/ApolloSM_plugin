#ifndef __EYESCANCLASS_HH__
#define __EYESCANCLASS_HH__
#include <ApolloSM/ApolloSM.hh>
#include <queue>


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
#define DFE 0
#define LPM 1

#define MAXUI 0.5
#define MINUI -0.5

#define MAX_PRESCALE_VALUE ((1<<5)-1)

//#define MAX_X_BINS ((1<<11)-1)
//#define MAX_Y_BINS ((1<<8))-1)

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
    {8, 128},
    {9, 160}
  };
std::map<uint32_t, int> static const rxoutDivMap = 
  {
    // RXOUT_DIV hex value (DRP encoding) vs max horizontal offset
    // https://www.xilinx.com/support/documentation/application_notes/xapp1198-eye-scan.pdf pgs 8 and 9
    {0, 32},
    {1, 64},
    {2, 128},
    {3, 256},
    {4, 512}
  };

class eyescan
{
public:
  typedef enum { UNINIT, SCAN_INIT, SCAN_READY, SCAN_START, SCAN_PIXEL, SCAN_DONE  } ES_state_t;
  typedef enum { FIRST, SECOND, LPM_MODE} DFE_state_t;

  // All necessary information to plot an eyescan
  struct eyescanCoords {
    double voltage;
    double phase;
    int32_t voltageInt;
    int32_t phaseInt;
    double BER;
    uint32_t sample0;
    uint32_t error0;
    uint32_t sample1;
    uint32_t error1;
    uint8_t voltageReg;
    uint16_t phaseReg;
  };


private:
  enum class SERDES_t : uint8_t {UNKNOWN=0,GTH_USP=1,GTX_7S=2,GTY_USP=3,GTH_7S=4};
  SERDEST_t xcvrType;
  uint8_t   rxDataWidth;
  uint8_t   rsIntDataWidth;


  std::string lpmNode;
  std::string DRPBaseNode;
  ES_state_t es_state;
  DFE_state_t dfe_state;
  double firstBER;
  std::vector<eyescanCoords> Coords_vect;
  std::vector<eyescanCoords>::iterator it;
  std::vector<double> volt_vect;
  std::vector<double> phase_vect;
  uint8_t maxPrescale;
  float volt;
  float phase;
  uint32_t cur_prescale=0;
  uint16_t nBinsX;
  uint16_t nBinsY;
  //uint32_t rxoutDiv;
  //int maxPhase;
  //double phaseMultiplier;
  //make these #defines

  uint32_t rxlpmen;



public:
  eyescan(ApolloSM*SM, 
	  std::string const & DRPBaseNode_set, 
	  std::string const & lpmNode_set, 
	  int nBinsX_set, int nBinsY_set, int max_prescale);
  ~eyescan();
  ES_state_t check();
  //void check();
  void update(ApolloSM*SM);
  std::vector<eyescanCoords>const & dataout();
  void throwException(std::string message);
  //make function to dump to file
  void fileDump(std::string outputFile);
  void reset();
  void start();

private:
  eyescan();
  void scan_pixel(ApolloSM*SM);
  void initialize(ApolloSM*SM);

  ES_state_t EndPixelLPM(ApolloSM*SM);
  ES_state_t EndPixelDFE(ApolloSM*SM);

  void SetEyeScanVoltage(ApolloSM*SM, std::string baseNode, uint8_t vertOffset, uint32_t sign);

  void SetEyeScanPhase(ApolloSM*SM, std::string baseNode, /*uint16_t*/ int horzOffset, uint32_t sign);

};


#endif
