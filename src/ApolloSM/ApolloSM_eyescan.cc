#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/eyescan.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh> // EYESCAN_ERROR
#include <vector>

#include <math.h> // pow
#include <map>

// ================================================================================
// Definitions

#define VERT_INDEX 0
#define HORZ_INDEX 1

// Correct eye scan attribute values
#define ES_EYE_SCAN_EN 0x1
#define ES_ERRDET_EN 0x1
#define PMA_CFG 0x000 // Actually 10 0s: 10b0000000000
#define PMA_RSV2 0x1
#define ES_QUALIFIER 0x0000
#define ES_QUAL_MASK 0xFFFF
#define RX_DATA_WIDTH 0x4 // We use 32 bit
#define RX_INT_DATAWIDTH 0x1 // We use 32 bit

#define SEVEN_FPGA 1
#define SEVEN_BUS_SIZE 3
#define USP_FPGA 2
#define USP_BUS_SIZE 4
// ==================================================
// identifies what FPGA to scan

//std::map<std::string, int> static const FPGA_IDMap = 
//  {
//    {"seven", 1},
//    {"usp", 2}
//  };
//
int static volatile FPGA_ID;
//
//int static getFPGA_ID() {
//  return FPGA_ID;
//}
//
//void static setFPGA_ID(std::string ID) {
//  it = FPGA_IDMap.find(ID);
//  if(it != FPGA_IDMap.end()) {
//    FPGA_ID = it->second;
//  }
//}
//
//void static zeroFPGA_ID() {
//  FPGA_ID = 0;
//} 
// ==================================================
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

// Identifies RXOUT_DIV to use for max phase 
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
// ==================================================

// Does not need to be an ApolloSM function, only assertNode and confirmNode (below) will use this
void throwException(std::string message) {
  BUException::EYESCAN_ERROR e;
  e.Append(message);
  throw e;
}

// assert to the node the correct value. Must be an ApolloSM function to use RegWriteRegister and RegReadRegister
void ApolloSM::assertNode(std::string node, uint32_t correctVal) {
  RegWriteRegister(node, correctVal);
  // Might be able to just put confirmNode here
  if(correctVal != RegReadRegister(node)) {
    throwException("Unable to set " + node + " correctly to: " + std::to_string(correctVal));
  }
}

// confirm that the node value is correct. Must be an ApolloSM function to use RegReadRegister 
void ApolloSM::confirmNode(std::string node, uint32_t correctVal) {
  if(correctVal != RegReadRegister(node)) {
    throwException(node + " is not set correctly to: " + std::to_string(correctVal));
  }
}

// To set up all attributes for an eye scan
void ApolloSM::EnableEyeScan(std::string baseNode, uint32_t prescale) {
  // ** must do this
  // *** not quite sure 

//  zeroFPGA_ID();
//  setFPGA_ID(fpga_id);
//  if(0 == getFPGA_ID()) {
//    throwException("invalid fpga id");
//  }

  uint32_t mask = GetRegMask(baseNode + "RX_DATA_WIDTH");
  int count;
  for(count = 0; mask; mask >>= 1) {
    count += mask&1;
  }
  printf("count is %d\n", count);

//  FPGA_ID = 0;
//  uint32_t busSize = GetRegSize(baseNode+"RX_DATA_WIDTH");
//  if(SEVEN_BUS_SIZE == busSize) {
//    FPGA_ID = SEVEN_FPGA;
//  } else if (USP_BUS_SIZE == busSize) {
//    FPGA_ID = USP_FPGA;
//  }
//  if(0 == FPGA_ID) {
//    throwException("bus size does not match any known FPGA bus size\n");
//  }

  // For reading, so I use does not equal, !=, or mask, &? 

  // ** ES_EYE_SCAN_EN assert 1
  assertNode(baseNode + "EYE_SCAN_EN", ES_EYE_SCAN_EN);

  // ** ES_ERRDET_EN assert 1
  assertNode(baseNode + "ERRDET_EN", ES_ERRDET_EN);

  // ** ES_PRESCALE set prescale
  //  uint32_t prescale32 = std::stoi(prescale); 
  assertNode(baseNode + "PRESCALE", prescale);

  // *** PMA_CFG confirm all 0s
  if(SEVEN_BUS_SIZE == count) {
    confirmNode(baseNode + "PMA_CFG", PMA_CFG);
  }
  
  // ** PMA_RSV2[5] assert 1 
  if(SEVEN_BUS_SIZE == count) {
    assertNode(baseNode + "PMA_RSV2", PMA_RSV2);
  }
  
  int qualSize = 0;
  if(SEVEN_BUS_SIZE == count) {
    qualSize = 5;
  } else {
    qualSize = 10;
  }

  // *** ES_QUALIFIER I think assert all 0s
  for(int i = 0; i < qualSize; i++) {
    assertNode(baseNode + "QUALIFIER_" + std::to_string(i), ES_QUALIFIER);
  }

  // ** ES_QUAL_MASK assert all 1s
  for(int i = 0; i < qualSize; i++) {
    assertNode(baseNode + "QUAL_MASK_" + std::to_string(i), ES_QUAL_MASK);
  }
  
  // ** ES_SDATA_MASK
  // use a for loop or something
  if(SEVEN_BUS_SIZE == count) {
    assertNode(baseNode + "OFFSET_DATA_MASK_0", 0x00FF);
    assertNode(baseNode + "OFFSET_DATA_MASK_1", 0x0000);
    assertNode(baseNode + "OFFSET_DATA_MASK_2", 0xFF00);
    assertNode(baseNode + "OFFSET_DATA_MASK_3", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_4", 0xFFFF);
  } else {
    assertNode(baseNode + "OFFSET_DATA_MASK_0", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_1", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_2", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_3", 0x0000);
    assertNode(baseNode + "OFFSET_DATA_MASK_4", 0x0000);
    assertNode(baseNode + "OFFSET_DATA_MASK_5", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_6", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_7", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_8", 0xFFFF);
    assertNode(baseNode + "OFFSET_DATA_MASK_9", 0xFFFF);
  }

  // ** RX_DATA_WIDTH confirm 0x4 
  confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH);
  
  // ** RX_INT_DATAWIDTH confirm 0x1
  confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH);
}

// ==================================================

float GetEyeScanVoltage() {
  // Change hex to int
  // return the int
  float i = 1;
  return i;
}

void ApolloSM::SetEyeScanVoltage(std::string baseNode, uint8_t vertOffset, uint32_t sign) {
  // change int to hex
  // write the hex

  // write the hex
  RegWriteRegister(baseNode + "VERT_OFFSET_MAG", vertOffset);
  RegWriteRegister(baseNode + "VERT_OFFSET_SIGN", sign);
}


float GetEyeScanPhase() {
  // change hex to int
  // return the int

  float i = 1;
  return i;
}

void ApolloSM::SetEyeScanPhase(std::string baseNode, /*uint16_t*/ int horzOffset) {//, uint32_t /*sign*/) {

  // change int to hex
  //  uint16_t horz_offset = 

  // write the hex
  //  RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", horzOffset);
  //  RegWriteRegister(baseNode + "PHASE_UNIFICATION", sign);
  // Only the last twelve bits are allowed. 
  RegWriteRegister(baseNode + "HORZ_OFFSET", (horzOffset + 4096)&0x0FFF);
}
 
void ApolloSM::SetOffsets(std::string /*baseNode*/, uint8_t /*vertOffset*/, uint16_t /*horzOffset*/) {
  // Set offsets

  // set voltage offset
  //  SetEyeScanVoltage(baseNode, vertOffset);
  // set phase offset
  //  SetEyeScanPhase(baseNode, horzOffset);
}
 
// ==================================================

#define WAIT 0x1
#define END 0x5
#define RUN 0x1
#define STOP_RUN 0x0

#define PRECISION 0.00000001 // 10^-9

#define PRESCALE_STEP 3
#define MAX_PRESCALE 3

// Performs a single eye scan and returns the BER
float ApolloSM::SingleEyeScan(std::string baseNode) {

  float BER;
 
  bool loop;

  loop = true;

  // Re-zero prescale
  assertNode(baseNode + "PRESCALE", 0x0);

  while(loop) {
    // confirm we are in WAIT, if not, stop scan
    //  confirmNode(baseNode + "CTRL_STATUS", WAIT);
    RegWriteRegister(baseNode + "RUN", STOP_RUN);
    
    // assert RUN
    //  assertNode(baseNode + "RUN", RUN);
    RegWriteRegister(baseNode + "RUN", RUN);  
    
    // poll END
    int count = 0;
    // one second max
    while(1000000 > count) {
      if(END == RegReadRegister(baseNode + "CTRL_STATUS")) {
	// Scan has ended
	break;
      }
      // sleep 1 millisecond
      usleep(1000);
      count++;
      if(1000000 == count) {
	throwException("BER sequence did not reach end in one second\n");
      }
    }	  
    
    // read error and sample count
    float errorCount = RegReadRegister(baseNode + "ERROR_COUNT");
    float sampleCount = RegReadRegister(baseNode + "SAMPLE_COUNT");
    
    // Should sleep for some time before de-asserting run. Can be a race condition if we don't sleep
    
    // Figure out the prescale and data width to calculate BER
    uint32_t prescale = RegReadRegister(baseNode + "PRESCALE");
    uint32_t regDataWidth = RegReadRegister(baseNode + "RX_DATA_WIDTH");
    int regDataWidthInt = (int)regDataWidth;
    //std::map<int, int>::iterator it = busWidthMap.find(regDataWidthInt);
    // should check if int is at the end
    //  int actualDataWidth = it->second;
    int actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
    
    // de-assert RUN (aka go back to WAIT)
    //  assertNode(baseNode + "RUN", STOP_RUN);
    RegWriteRegister(baseNode + "RUN", STOP_RUN);
        
    // calculate BER
    //    BER = errorCount/(pow(2,(1+prescale))*sampleCount*(float)actualDataWidth);
    BER = errorCount/((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
    
    // If BER is lower than precision we need to check with a higher prescale to ensure that
    // that is believable. pg 231 https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty-transceivers.pdf
    if((BER < PRECISION) && prescale != MAX_PRESCALE) {
      prescale+=PRESCALE_STEP;
      if(prescale > MAX_PRESCALE) {
	prescale = MAX_PRESCALE;
      }
      assertNode(baseNode + "PRESCALE", prescale);
      // useless but just to be paranoid
      loop = true;
    } else {
      loop = false;
    }
  }

  return BER;
}

// ==================================================

#define MAXUI 0.5
#define MINUI -0.5
 
std::vector<eyescanCoords> ApolloSM::EyeScan(std::string baseNode, double horzIncrement, int vertIncrement) {
  
//  if(1/horzIncrement != 0) {
//    throwException("Please enter a horizontal increment divisible into 1\n");
//  }

  // Make sure all DRP attributes are set up for eye scan 
  //EnableEyeScan(baseNode, prescale);
  //EnableEyeScan(baseNode, 0x1);
  
  // declare vector of all eye scan plot coordinates
  std::vector<eyescanCoords> esCoords;

  // index for vector of coordinates
  int coordsIndex = 0;
  int resizeCount = 1;

  // =========================
  uint8_t maxVoltage = 127;
  int minVoltage = -127;


  // Figure out RXOUT_DIV to set max phase
  uint32_t rxoutDiv = RegReadRegister(baseNode + "RXOUT_DIV");
  // should check if int is at the end
  int maxPhase = rxoutDivMap.find(rxoutDiv)->second;

  printf("The max phase is: %d\n", maxPhase);

  double phaseMultiplier = maxPhase/MAXUI;

  // =========================

  // Set offsets and perform eyescan
  for(int voltage = minVoltage; voltage <= maxVoltage; voltage+=vertIncrement) {

    // https://www.xilinx.com/support/documentation/user_guides/ug476_7Series_Transceivers.pdf#page=300 go to ES_VERT_OFFSET description
    // For bit 7 (8th bit) of ES_VERT_OFFSET
    uint32_t POSITIVE = 0;
    uint32_t NEGATIVE = 1;

    if(voltage < 0) {
      SetEyeScanVoltage(baseNode, (uint8_t)(-1*voltage), NEGATIVE); 
    } else {
      SetEyeScanVoltage(baseNode, voltage, POSITIVE);
    }

    for(double phase = MINUI; phase <= MAXUI; phase+=horzIncrement) {
      
      int phaseInt;

      if(phase < 0) {
	phaseInt = ceil(phase*phaseMultiplier);
      } else {
	phaseInt = floor(phase*phaseMultiplier);
      }
      
      SetEyeScanPhase(baseNode, phaseInt);
      esCoords.resize(resizeCount);

      // record voltage and phase coordinates
      esCoords[coordsIndex].voltage = voltage; 
      esCoords[coordsIndex].phase = phase;
      //      printf("%d %d\n", voltage, phaseInt);      

      esCoords[coordsIndex].BER = SingleEyeScan(baseNode);
      
      // Vert sign mask is 0x80 so we need to shift right by 7
      esCoords[coordsIndex].voltageReg = RegReadRegister(baseNode + "VERT_OFFSET_MAG") | (RegReadRegister(baseNode + "VERT_OFFSET_SIGN") << 7); 
      esCoords[coordsIndex].phaseReg = RegReadRegister(baseNode + "HORZ_OFFSET") & 0x0FFF;

      //      printf("%.9f\n", esCoords[coordsIndex].BER);
      printf("%d\n", esCoords[coordsIndex].voltage);
      
      // going to next coordinate/scan 
      coordsIndex++;
      resizeCount++;
    }
  }

//  // reset FPGA_ID
//  zeroFPGA_ID();
//  
  return esCoords;
}

