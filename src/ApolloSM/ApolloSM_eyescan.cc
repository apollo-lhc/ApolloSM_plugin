#include <ApolloSM/ApolloSM.hh>
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
#define RX_DATA_WIDTH 0x4
#define RX_INT_DATAWDITH 0x1

// ==================================================

void throwException(std::string message) {
  BUException::EYESCAN_ERROR e;
  e.append(message);
  throw e;
}


void ApolloSM::EnableEyeScan(std::string baseNode, uint16_t prescale) {
  // ** must do this
  // *** not quite sure
  
  // For reading, so I use does not equal, !=, or mask, &? 

  // ** ES_EYE_SCAN_EN assert 1  
  RegWriteRegister(baseNode + "EYE_SCAN_EN", 0x1);
  if(0x1 != RegRead(baseNode + "EYE_SCAN_EN")) {
    throwException("unable to set EYE_SCAN_EN correctly to 0x1");
  }

  // ** ES_ERRDET_EN assert 1
  RegWriteRegister(baseNode + "ERRDET_EN", 0x1);
  if(0x1 != RegRead(baseNode + "ERRDET_EN")) {
    throwException("unable to set ERRDET_EN correctly to 0x1");
  }

  // ** ES_PRESCALE set prescale
  RegWriteRegister(baseNode + "PRESCALE", prescale);
  if(0x1 != RegRead(baseNode + "PRESCALE")) {
    throwException("unable to set PRESCALE correctly to " + std::to_string(prescale));
  }

  // *** PMA_CFG confirm all 0s
  if(0x000 != RegReadRegister(baseNode + "PMA_CFG")) {
    // PMA_CFG is actually only 10 bits
    throwException("PMA_CFG is not set correctly to 10b0000000000");
  }

  // ** PMA_RSV2[5] assert 1 
  RegWriteRegister()
  
  // *** ES_QUALIFIER I think assrt all 0s
  // ** ES_QUAL_MASK assert all 1s
  // ** ES_SDATA_MASK 
  // ** RX_DATA_WIDTH confirm 0x4 
  // ** RX_INT_DATAWIDTH confirm 0x1

}

int ApolloSM::GetEyeScanVoltage() {
  // Change hex to int
  // return the int
}

void ApolloSM::SetEyeScanVoltage() {
  // change int to hex
  // write the hex
}

int ApolloSM::GetEyeScanPhase() {
  // change hex to int
  // return the int
}

void ApolloSM::SetEyeScanPhase() {
  // change int to hex
  // write the hex
}

// ==================================================

// All necessary information to calculate bit error rate (BER)
//struct eyescanData {
//  uint16_t sampleCount;
//  uint16_t errorCount;
//};

// All necessary information to plot an eyescan
struct eyescanCoords {
  float voltage;
  float phase;
  float BER;
};
  
// ==================================================

void SingleEyeScan(uint16_t prescale) {
  // confirm we are in WAIT, if not, stop scan

  // assert RUN

  // poll END

  // read error and sample count

  // de-assert RUN (aka go back to WAIT)

  // calculate BER

  // return BER
}

std::vector<<eyescanCoords> ApolloSM::EyeScan(std::string baseNode, float maxVoltage, float maxPhase, uint16_t prescale) {
  
  // declare vector of vector of eye scan data
  //  std::vector<std::vector<eyescanData> esData;
  
  // Make sure all DRP attributes are set up for eye scan 
  EnableEyeScan(baseNode, prescale);
  
  // declare vector of all eye scan plot coordinates
  std::vector<eyescanCoords> esCoords;
  // empty coordinate
  eyescanCoords emptyCoord;
  // index for vector of coordinates
  int coordsIndex = 0;

  // Generate all voltage and phase offsets to be used in eyescan based on range (maxes) specified
  //std::vector<std::vector<float>> offsets = GenerateOffsets(maxVoltage, maxPhase);
  
  // Calculate min voltage and phase from max
  float minVoltage = -1*maxVoltage;
  float minPhase = -1*maxPhase;

  // Set offsets and perform eyescan
  for(float voltage = minVoltage; voltage <= maxVoltage; voltage++) {
    // Allocate memory for new coordinate
    esCoords.push_back(emptyCoord);
    
    // set voltage offset
    SetEyeScanVoltage(voltage);
    // check that voltage offset is actually set correctly
    if(GetEyeScanVoltage != voltage) {
      // something went wrong, stop scan
    }    
    
    for(float phase = minPhase; phase <= maxPhase; phase++) {
      // set phase offset
      SetEyeScanPhase(phase);
      // check that phase offset is actually set correctly
      if(GetEyeScanPhase != phase) {
	// something went wrong, stop scan
      }
      
      // set voltage coordinate
      esCoords[coordsIndex].voltage = voltage; 
      // set phase coordinate
      esCoords[coordsIndex].phase = phase;
      // Perform a single scan and set BER coordinate
      esCoords[coordsIndex].BER = SingleEyeScan(prescale);

      // going to next coordinate/scan 
      coordsIndex++;
    }
  }

  // IMPORTANT: in 2D array, if top left is [0][0], then negative phases are on the left and negative voltages are on the TOP. remember this when plotting. (even though eyes are symmetrical)  

  return esCoords;
}

// ================================================================================
// Functions

// Generate all voltage and phase offsets to be used in eyescan based on range (maxes) specified
//std::vector<std::vector<float>> ApolloSM::GenerateOffsets(float maxVoltage, float maxPhase) {
//  // declare vectors
//  std::vector<std::vector<float>> offsets;
//  std::vector<float> empty;
//  
//  offsets.push_back(empty); // allocate memory for voltages
//  offsets.push_back(empty); // allocate memory for phases  
//  
//  // calculate minimum voltage and phase
//  float minVoltage = -1*maxVoltage;
//  float minPhase = -1*maxPhase;
//  
//  // generate all voltage offsets
//  for(i = minVoltage; i <= maxVoltage; i++) {
//    offsets[VERT_INDEX].push_back(i);
//  }
//  
//  // generate all phase offsets
//  for(i = minPhase; i <= maxPhase; i++) {
//    offsets[HORZ_INDEX].push_back(i);
//  }
//  
//  return offsets;
//}

