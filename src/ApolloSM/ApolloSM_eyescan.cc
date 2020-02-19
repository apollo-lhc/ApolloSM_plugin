#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/eyescan.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh> // EYESCAN_ERROR
#include <vector>

#include <math.h> // pow

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

  printf("the size of RX_DATA_WIDTH is: %x", GetRegSize(baseNode + "RX_DATA_WIDTH"));
  printf("the mask of RX_DATA_WIDTH is: %x", GetRegMask(baseNode + "RX_DATA_WIDTH"));

  // For reading, so I use does not equal, !=, or mask, &? 

  // ** ES_EYE_SCAN_EN assert 1
  assertNode(baseNode + "EYE_SCAN_EN", ES_EYE_SCAN_EN);

  // ** ES_ERRDET_EN assert 1
  assertNode(baseNode + "ERRDET_EN", ES_ERRDET_EN);

  // ** ES_PRESCALE set prescale
  //  uint32_t prescale32 = std::stoi(prescale); 
  assertNode(baseNode + "PRESCALE", prescale);

  // *** PMA_CFG confirm all 0s
  confirmNode(baseNode + "PMA_CFG", PMA_CFG);

  // ** PMA_RSV2[5] assert 1 OB
  assertNode(baseNode + "PMA_RSV2", PMA_RSV2);
  
  // *** ES_QUALIFIER I think assert all 0s
  for(int i = 0; i < 5; i++) {
    assertNode(baseNode + "QUALIFIER_" + std::to_string(i), ES_QUALIFIER);
  }

  // ** ES_QUAL_MASK assert all 1s
  for(int i = 0; i < 5; i++) {
    assertNode(baseNode + "QUAL_MASK_" + std::to_string(i), ES_QUAL_MASK);
  }
  
  // ** ES_SDATA_MASK 
  assertNode(baseNode + "OFFSET_DATA_MASK_0", 0x00FF);
  assertNode(baseNode + "OFFSET_DATA_MASK_1", 0x0000);
  assertNode(baseNode + "OFFSET_DATA_MASK_2", 0xFF00);
  assertNode(baseNode + "OFFSET_DATA_MASK_3", 0xFFFF);
  assertNode(baseNode + "OFFSET_DATA_MASK_4", 0xFFFF);
 
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

void ApolloSM::SetEyeScanVoltage(std::string baseNode, uint8_t vertOffset) {
  // change int to hex
  // write the hex

  // write the hex
  RegWriteRegister(baseNode + "VERT_OFFSET", vertOffset);
}


float GetEyeScanPhase() {
  // change hex to int
  // return the int

  float i = 1;
  return i;
}

void ApolloSM::SetEyeScanPhase(std::string baseNode, uint16_t horzOffset) {

  // change int to hex
  //  uint16_t horz_offset = 

  // write the hex
  RegWriteRegister(baseNode + "HORZ_OFFSET", horzOffset);
}
 
void ApolloSM::SetOffsets(std::string baseNode, uint8_t vertOffset, uint16_t horzOffset) {
  // Set offsets

  // set voltage offset
  SetEyeScanVoltage(baseNode, vertOffset);
  // check that voltage offset is actually set correctly
//  if(GetEyeScanVoltage() != vertOffset) {
//    throwException("Cannot set voltage offset properly\n");
//  }    
//  
  // set phase offset
  SetEyeScanPhase(baseNode, horzOffset);
  // check that phase offset is actually set correctly
//  if(GetEyeScanPhase() != horzOffset) {
//    throwException("Cannot set voltage phase properly\n");
//  }
//    
}
 
// ==================================================

#define WAIT 0x1
#define END 0x5
#define RUN 0x1
#define STOP_RUN 0x0

// Performs a single eye scan and returns the BER
float ApolloSM::SingleEyeScan(std::string baseNode) {
  // confirm we are in WAIT, if not, stop scan
  //  confirmNode(baseNode + "CTRL_STATUS", WAIT);
  RegWriteRegister(baseNode + "RUN", STOP_RUN);

  // assert RUN
  //  assertNode(baseNode + "RUN", RUN);
  RegWriteRegister(baseNode + "RUN", RUN);  

  // poll END
  int count = 0;
  while(1000 > count) {
    if(END == RegReadRegister(baseNode + "CTRL_STATUS")) {
      // Scan has ended
      break;
    }
    // sleep 1 millisecond
    usleep(1000);
    count++;
    if(1000 == count) {
      throwException("BER sequence did not reach end in one second\n");
    }
  }	  

  // read error and sample count
  float errorCount = RegReadRegister(baseNode + "ERROR_COUNT");
  float sampleCount = RegReadRegister(baseNode + "SAMPLE_COUNT");
  
  // Should sleep for some time before de-asserting run. Can be a race condition if we don't sleep

  // Figure out the prescale to calculate BER
  uint32_t prescale = RegReadRegister(baseNode + "PRESCALE");

  // de-assert RUN (aka go back to WAIT)
  //  assertNode(baseNode + "RUN", STOP_RUN);
  RegWriteRegister(baseNode + "RUN", STOP_RUN);

  // Figure out the prescale to calculate BER
  //  uint32_t prescale = RegReadRegister(baseNode + "PRESCALE");

  // return BER
  return errorCount/(pow(2,(1+prescale))*sampleCount);
}

// ==================================================
 
std::vector<eyescanCoords> ApolloSM::EyeScan(std::string baseNode) {//, float /*maxVoltage*/, float /*maxPhase*/, uint16_t /*prescale*/) {
  
  // Make sure all DRP attributes are set up for eye scan 
  //EnableEyeScan(baseNode, prescale);
  //EnableEyeScan(baseNode, 0x1);
  
  // declare vector of all eye scan plot coordinates
  std::vector<eyescanCoords> esCoords;

  // index for vector of coordinates
  int coordsIndex = 0;
  int resizeCount = 1;

  // For compiler error of unused argument
  std::string bootleg = baseNode;

  uint8_t maxVoltage = 126;
  //uint8_t minVoltage = -1*maxVoltage;
  int minVoltage = -126;
  uint16_t maxPhase = 254;
  //uint16_t minPhase = -31;
  int minPhase = -254;
  
  // Set offsets and perform eyescan
  for(int voltage = minVoltage; voltage <= maxVoltage; voltage++) {
    
    // set voltage offset
    if(0 > voltage) {
      uint8_t unsignedV = (uint8_t)((-1*voltage) | (0x80));
      //      SetEyeScanVoltage(baseNode, (uint8_t)((-1*voltage) | (0x80)));
      SetEyeScanVoltage(baseNode, unsignedV);
    } else {
      SetEyeScanVoltage(baseNode, voltage);
    }
    //check that voltage offset is actually set correctly
//    if(GetEyeScanVoltage() != voltage) {
//      // something went wrong, stop scan
//    }    
//    
    for(int phase = minPhase; phase <= maxPhase; phase+=8) {
      // set phase offset
      SetEyeScanPhase(baseNode, phase & 0xFFF);
      
      //printf("writing phase %d\n", phase);
      // check that phase offset is actually set correctly
//      if(GetEyeScanPhase() != phase) {
// 	// something went wrong, stop scan
//      }
//      

      esCoords.resize(resizeCount);

      // record voltage coordinate
      esCoords[coordsIndex].voltage = voltage; 
      // record phase coordinate
      esCoords[coordsIndex].phase = phase/(float)(maxPhase*2); // Normalized to 1 UI, 0.5 UI on each side
      // Perform a single scan and record BER coordinate
      esCoords[coordsIndex].BER = SingleEyeScan(baseNode);
      printf("%f\n", esCoords[coordsIndex].BER);
      
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

