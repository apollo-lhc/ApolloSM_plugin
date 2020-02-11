#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/eyescan.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh> // EYESCAN_ERROR
#include <vector>

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
  RegWriteRegister(baseNode + "CONTROL", STOP_RUN);

  // assert RUN
  //  assertNode(baseNode + "RUN", RUN);
  RegWriteRegister(baseNode + "CONTROL", RUN);  

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
  
  // de-assert RUN (aka go back to WAIT)
  //  assertNode(baseNode + "RUN", STOP_RUN);
  RegWriteRegister(baseNode + "CONTROL", STOP_RUN);

  // Figure out the prescale to calculate BER
  uint32_t prescale = RegReadRegister(baseNode + "PRESCALE");

  // return BER
  return errorCount/(prescale*sampleCount);
}

// ==================================================
 
std::vector<eyescanCoords> ApolloSM::EyeScan(std::string baseNode) {//, float /*maxVoltage*/, float /*maxPhase*/, uint16_t /*prescale*/) {
  
  // declare vector of vector of eye scan data
  //  std::vector<std::vector<eyescanData> esData;
  
  // Make sure all DRP attributes are set up for eye scan 
  //EnableEyeScan(baseNode, prescale);
  //EnableEyeScan(baseNode, 0x1);
  
  // declare vector of all eye scan plot coordinates
  std::vector<eyescanCoords> esCoords;
  // empty coordinate
  //  eyescanCoords emptyCoord;
  // index for vector of coordinates
  int coordsIndex = 0;
  int resizeCount = 1;

  // Generate all voltage and phase offsets to be used in eyescan based on range (maxes) specified
  //std::vector<std::vector<float>> offsets = GenerateOffsets(maxVoltage, maxPhase);
  
  // Calculate min voltage and phase from max
  //  float minVoltage = -1*maxVoltage;
  //  float minPhase = -1*maxPhase;

  // For compiler error of unused argument
  std::string bootleg = baseNode;

  uint8_t maxVoltage = 7;
  //uint8_t minVoltage = -1*maxVoltage;
  int minVoltage = -7;
  uint16_t maxPhase = 3;
  //uint16_t minPhase = -31;
  int minPhase = -3;
  
  // Set offsets and perform eyescan
  for(int voltage = minVoltage; voltage <= maxVoltage; voltage++) {
    // Allocate memory for new coordinate
    //    esCoords.push_back(emptyCoord);
    


    // set voltage offset
    printf("writing voltage %d\n", voltage);
    if(0 > voltage) {
      SetEyeScanVoltage(baseNode, (uint8_t)((-1*voltage) | (0x80)));
    } else {
      SetEyeScanVoltage(baseNode, voltage);
    }
    //check that voltage offset is actually set correctly
//    if(GetEyeScanVoltage() != voltage) {
//      // something went wrong, stop scan
//    }    
//    
    for(int phase = minPhase; phase <= maxPhase; phase++) {
      // set phase offset
      SetEyeScanPhase(baseNode, phase & 0xFFF);
      
      printf("writing phase %d\n", phase);
      // check that phase offset is actually set correctly
//      if(GetEyeScanPhase() != phase) {
// 	// something went wrong, stop scan
//      }
//      

      esCoords.resize(resizeCount);

      //      printf("%d ", (uint8_t)((-1*voltage) | 0x80));
      //printf("%d\n", phase & 0xFFF);

      // set voltage coordinate
      esCoords[coordsIndex].voltage = voltage; 
      // set phase coordinate
      esCoords[coordsIndex].phase = phase/(float)(maxPhase*2); // Normalized to 1, 0.5 on each side
      // Perform a single scan and set BER coordinate
      esCoords[coordsIndex].BER = SingleEyeScan(baseNode);
      //esCoords[coordsIndex].BER = 0;
      // going to next coordinate/scan 
      coordsIndex++;
      resizeCount++;
    }
  }
  
  // IMPORTANT: in 2D array, if top left is [0][0], then negative phases are on the left and negative voltages are on the TOP. remember this when plotting. (even though eyes are symmetrical)  
  
  return esCoords;
}


// ================================================================================
/*
// Functions

// Generate all voltage and phase offsets to be used in eyescan based on range (maxes) specified
  std::vector<std::vector<float>> ApolloSM::GenerateOffsets(float maxVoltage, float maxPhase) {
   // declare vectors
   std::vector<std::vector<float>> offsets;
   std::vector<float> empty;
   
   offsets.push_back(empty); // allocate memory for voltages
   offsets.push_back(empty); // allocate memory for phases  
   
   // calculate minimum voltage and phase
   float minVoltage = -1*maxVoltage;
   float minPhase = -1*maxPhase;
   
   // generate all voltage offsets
   for(i = minVoltage; i <= maxVoltage; i++) {
     offsets[VERT_INDEX].push_back(i);
   }
   
   // generate all phase offsets
   for(i = minPhase; i <= maxPhase; i++) {
     offsets[HORZ_INDEX].push_back(i);
   }
   
   return offsets;
 }
*/
