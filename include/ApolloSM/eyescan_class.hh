#ifndef __EYESCANCLASS_HH__
#define __EYESCANCLASS_HH__
#include <ApolloSM/ApolloSM.hh>
#include <queue>


// Correct eye scan attribute values
#define ES_EYE_SCAN_EN 0x1
#define ES_ERRDET_EN 0x1
#define PMA_CFG 0x000 // Actually 10 0s: 10b0000000000
//#define PMA_RSV2 0x1
//#define ES_QUALIFIER 0x0000
//#define ES_QUAL_MASK 0xFFFF

//#define RX_DATA_WIDTH_GTX 0x4 // zynq
//#define RX_INT_DATAWIDTH_GTX 0x1 // We use 32 bit
//
//#define RX_DATA_WIDTH_GTH 0x4 // kintex
//#define RX_INT_DATAWIDTH_GTH 0x0 //16 bit
//
//#define RX_DATA_WIDTH_GTY 0x6 // virtex
//#define RX_INT_DATAWIDTH_GTY 0x1 //32 bit
#define DFE 0
#define LPM 1

#define MAX_UI_MAG 0.5

#define MAX_PRESCALE_VALUE ((1<<5)-1)

#define MAX_Y_BIN_MAG ((1<<7)-1)

#define PRESCALE_START 3
#define PRESCALE_STEP 3


class eyescan
{
public:
  typedef enum { UNINIT, SCAN_INIT, SCAN_READY, SCAN_START, SCAN_PIXEL, SCAN_DONE, SCAN_ERR } ES_state_t;
  typedef enum { FIRST, SECOND, LPM_MODE} DFE_state_t;
  enum class SERDES_t : uint8_t {UNKNOWN=0,GTH_USP=1,GTX_7S=2,GTY_USP=3,GTH_7S=4};

  // All necessary information to plot an eyescan
  struct eyescanCoords {
    eyescanCoords(){clear();};
    double   voltage;
    bool     voltageReal;
    double   phase;
    uint16_t horzWriteVal;
    uint16_t vertWriteVal;

    uint8_t  prescale;
    double   BER;
    uint64_t sample0;
    uint64_t error0;
    uint64_t sample1;
    uint64_t error1;    
    void clear(){
      //clear all values
      voltage     = 0;
      voltageReal= 0;
      phase       = 0;
      horzWriteVal= 0;
      vertWriteVal= 0;
      reset();
    };
    void reset(){
      //reset the data output values
      BER         = 0;
      sample0     = 0;
      error0      = 0;
      sample1     = 0;
      error1      = 0;
      prescale=PRESCALE_START;
    };
  };


private:
  //SERDES parameters
  SERDES_t xcvrType;
  uint8_t   rxDataWidth;
  uint8_t   rxIntDataWidth;
  uint16_t  rxOutDiv;
  double    linkSpeedGbps;
  uint32_t  rxlpmen;

  //scan parameters
  uint16_t maxXBinMag;
  uint16_t binXIncr;
  int16_t  binXBoundary;
  uint16_t binXCount;

  uint16_t maxYBinMag;
  uint16_t binYIncr;
  int16_t  binYBoundary;
  double   binYdV;
  
  ES_state_t es_state;
  DFE_state_t dfe_state;

  //  uint8_t prescale;
  uint8_t maxPrescale;

  //access parameters
  std::string lpmNode;
  std::string DRPBaseNode;
  double firstBER;
  std::vector<eyescanCoords> Coords_vect;
  std::vector<eyescanCoords>::iterator it;
  std::vector<double> volt_vect;
  std::vector<double> phase_vect;




public:
  eyescan(ApolloSM*SM, 
	  std::string const & DRPBaseNode_set, 
	  std::string const & lpmNode_set, 
	  int nBinsX_set, int nBinsY_set, int max_prescale);
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
