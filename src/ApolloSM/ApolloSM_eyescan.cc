#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/eyescan.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh> // EYESCAN_ERROR
#include <vector>
#include <stdlib.h>
//#include <math.h> // pow
#include <map>
#include <syslog.h>
#include <time.h>

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

#define RX_DATA_WIDTH_GTX 0x4 // zynq
#define RX_INT_DATAWIDTH_GTX 0x1 // We use 32 bit

#define RX_DATA_WIDTH_GTH 0x4 // kintex
#define RX_INT_DATAWIDTH_GTH 0x0 //16 bit

#define RX_DATA_WIDTH_GTY 0x6 // virtex
#define RX_INT_DATAWIDTH_GTY 0x1 //32 bit

#define SEVEN_FPGA 1
#define SEVEN_BUS_SIZE 3
#define USP_FPGA 2
#define USP_BUS_SIZE 4
#define GTH_CHECK_COUNT 16
#define GTY_CHECK_COUNT 10
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

  syslog(LOG_INFO, "appending\n");
  // check for a '.' at the end of baseNode and add it if it isn't there 
  if(baseNode.compare(baseNode.size()-1,1,".") != 0) {
    baseNode.append(".");
  }

  uint32_t data_width_mask = GetRegMask(baseNode + "RX_DATA_WIDTH");
  int count;
  for(count = 0; data_width_mask; data_width_mask >>= 1) {
    count += data_width_mask&1;
  }
  printf("data_width count is %d\n", count);

  uint32_t check_mask = GetRegMask(baseNode + "GTY_GTH_CHECK");
  int count2;
  for(count2 = 0; check_mask; check_mask >>= 1) {
    count2 += check_mask&1;
  }
  printf("transceiver check count is %d\n", count2);

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
  // Both 7 series and kintex USP are 0x4 (32 bit bus width) virtex USP is 0x6 
  if(SEVEN_BUS_SIZE == count) {
    confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH_GTX);
  } else if(GTH_CHECK_COUNT == count2) {
    confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH_GTH);
  } else {
    confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH_GTY);
  }  

  // ** RX_INT_DATAWIDTH confirm
  // 7 series is 1
  // usp is either 0 or 1 but the default (when powered up) seems to be 0. 
  // https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty-transceivers.pdf pg 317 gives only a little more info
  // look for "internal data width"
  if(SEVEN_BUS_SIZE == count) {
    confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH_GTX);
  } else if(GTH_CHECK_COUNT == count2) {
    confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH_GTH);
  } else {
    confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH_GTY);
  }
}

// ==================================================

float GetEyeScanVoltage() {
  // Change hex to int
  // return the int
  float i = 1;
  return i;
}

void ApolloSM::SetEyeScanVoltage(std::string baseNode, uint8_t vertOffset, uint32_t sign) {
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


void ApolloSM::SetEyeScanPhase(std::string baseNode, /*uint16_t*/ int horzOffset, uint32_t sign) {
  // write the hex
  RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", horzOffset);
  //printf("Set phase stop 1 \n");
  RegWriteRegister(baseNode + "PHASE_UNIFICATION", sign); 
  //RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", (horzOffset + 4096)&0x0FFF);
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

// use the precision map in EyeScanLink.cc
#define PRECISION 0.00000001 // 10^-9

#define PRESCALE_STEP 3
//#define MAX_PRESCALE 3

// Performs a single eye scan and returns the BER
SESout ApolloSM::SingleEyeScan(std::string const baseNode, std::string const lpmNode, uint32_t const maxPrescale) {

  SESout singleScanOut;
  double BER;
  float errorCount;
  float sampleCount;
  float errorCount0;
  float actualsample0;
  uint32_t prescale = 0;
  uint32_t const regDataWidth = RegReadRegister(baseNode + "RX_DATA_WIDTH");
  int const regDataWidthInt = (int)regDataWidth;
  int const actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
  
  // Check if we're DFE or LPM
  uint32_t const dfe = 0;
  uint32_t const lpm = 1;

  //uint32_t const rxlpmen = RegReadRegister("CM.CM_1.C2C.RX.LPM_EN");
  uint32_t const rxlpmen = RegReadRegister(lpmNode);
  
  //RegReadRegister(0x1900003B);
  //  uint32_t const rxlpmenmasked = RegReadRegister(0x1900003B) & 0x00000100; 

  if(lpm == rxlpmen) {
    //printf("Looks like we have LPM. The register is %u\n", rxlpmen);
  } else if(dfe == rxlpmen) {
    //printf("Looks like we have DFE. The register is %u\n", rxlpmen);
  } else {
    printf("Something is wrong. We don't have lpm or dfe\n");
  }

  // Re-zero prescale
  assertNode(baseNode + "PRESCALE", 0x0);

  bool loop;
  loop = true;

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
    errorCount = RegReadRegister(baseNode + "ERROR_COUNT");
    sampleCount = RegReadRegister(baseNode + "SAMPLE_COUNT");
    
    // Should sleep for some time before de-asserting run. Can be a race condition if we don't sleep
    
    // Figure out the prescale and data width to calculate BER
    // prescale = RegReadRegister(baseNode + "PRESCALE");
    //    regDataWidth = RegReadRegister(baseNode + "RX_DATA_WIDTH");
    //    regDataWidthInt = (int)regDataWidth;
    //std::map<int, int>::iterator it = busWidthMap.find(regDataWidthInt);
    // should check if int is at the end
    //  int actualDataWidth = it->second;
    //    actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
    
    // de-assert RUN (aka go back to WAIT)
    //  assertNode(baseNode + "RUN", STOP_RUN);
    RegWriteRegister(baseNode + "RUN", STOP_RUN);
        
    // calculate BER
    //    BER = errorCount/(pow(2,(1+prescale))*sampleCount*(float)actualDataWidth);
    BER = errorCount/((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
    actualsample0=((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
    
    // If BER is lower than precision we need to check with a higher prescale to ensure that
    // that is believable. pg 231 https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty-transceivers.pdf
    if((BER < PRECISION) && (prescale != maxPrescale)) {
      prescale+=PRESCALE_STEP;
      if(prescale > maxPrescale) {
	    prescale = maxPrescale;
	//	printf("max prescale %d reached\n", maxPrescale);
      }
      assertNode(baseNode + "PRESCALE", prescale);
      // useless but just to be paranoid
      loop = true;
    } else {
      if (errorCount==0) //if scan found no errors default to BER floor
      {
        errorCount=1;
        BER = errorCount/((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
        
      }
        actualsample0=((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
        errorCount0=errorCount;
//      printf("Stopping single scan because: ");
//      if(!(BER < PRECISION)) {
//	printf("NOT BER < PRECISION\n");
//      }
//      if(!(prescale != maxPrescale)) {
//	printf("NOT prescale != maxPrescale\n");
//      }
      loop = false;
    }
  }

  // ==================================================
  // If we have dfm, we need to do calculate the BER a second time and add it to the first
  double const firstBER = BER;
  float errorCount1=0;
  float actualsample1=0;
  // You have to set this to zero because if you don't have DFE, you would skip to 'return' and BER would be double counted
  BER = 0;

  // Re-zero prescale
  prescale = 0;
  assertNode(baseNode + "PRESCALE", 0x0);

  if(dfe == rxlpmen) {
    //printf("Alright dfe = rxlpmen: %u = %u. Calculating again\n", dfe, rxlpmen);
    // whatever the UT sign was, change it 
    if(1 == (RegReadRegister(baseNode + "UT_SIGN"))) {
      RegWriteRegister(baseNode + "UT_SIGN", 0);
    } else {
      RegWriteRegister(baseNode + "UT_SIGN", 1);
    }
    
    loop = true;
    
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
      errorCount = RegReadRegister(baseNode + "ERROR_COUNT");
      sampleCount = RegReadRegister(baseNode + "SAMPLE_COUNT");
      
      // Should sleep for some time before de-asserting run. Can be a race condition if we don't sleep
      
      // Figure out the prescale and data width to calculate BER
      // prescale = RegReadRegister(baseNode + "PRESCALE");
      //    regDataWidth = RegReadRegister(baseNode + "RX_DATA_WIDTH");
      //    regDataWidthInt = (int)regDataWidth;
      //std::map<int, int>::iterator it = busWidthMap.find(regDataWidthInt);
      // should check if int is at the end
      //  int actualDataWidth = it->second;
      //    actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
      
      // de-assert RUN (aka go back to WAIT)
      //  assertNode(baseNode + "RUN", STOP_RUN);
      RegWriteRegister(baseNode + "RUN", STOP_RUN);
      
      // calculate BER
      //    BER = errorCount/(pow(2,(1+prescale))*sampleCount*(float)actualDataWidth);
      BER = errorCount/((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
      actualsample1=((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
      
      // If BER is lower than precision we need to check with a higher prescale to ensure that
      // that is believable. pg 231 https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty-transceivers.pdf
      if((BER < PRECISION) && (prescale != maxPrescale)) {
	     prescale+=PRESCALE_STEP;
        	if(prescale > maxPrescale) {
        	  prescale = maxPrescale;
        	  //	printf("max prescale %d reached\n", maxPrescale);
        	}
        	assertNode(baseNode + "PRESCALE", prescale);
        	// useless but just to be paranoid
        	loop = true;
              } else {
                if (errorCount==0) //if scan found no errors default to BER floor
                {
                  errorCount=1;
                  BER = errorCount/((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
                  
                }
                actualsample1=((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
                errorCount1=errorCount;
        	//      printf("Stopping single scan because: ");
        	//      if(!(BER < PRECISION)) {
        	//	printf("NOT BER < PRECISION\n");
        	//      }
        	//      if(!(prescale != maxPrescale)) {
        	//	printf("NOT prescale != maxPrescale\n");
        	//      }
        	loop = false;
      }
    }
  }
  singleScanOut.BER=BER+firstBER;
  singleScanOut.sample0=(unsigned long int)actualsample0;
  singleScanOut.error0=(unsigned long int)errorCount0;
  singleScanOut.sample1=(unsigned long int)actualsample1;
  singleScanOut.error1=(unsigned long int)errorCount1;

  return singleScanOut;

}

// ==================================================

#define MAXUI 0.5
#define MINUI -0.5
 
std::vector<eyescanCoords> ApolloSM::EyeScan(std::string baseNode, std::string lpmNode, double horzIncrement, int vertIncrement, uint32_t maxPrescale) {
  //clock for timing
  time_t start, end; // used to time execution
  time(&start);      // recording start time
  
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

  syslog(LOG_INFO, "appending\n");
  // check for a '.' at the end of baseNode and add it if it isn't there 
  if(baseNode.compare(baseNode.size()-1,1,".") != 0) {
    baseNode.append(".");
  }

  // Figure out RXOUT_DIV to set max phase
  uint32_t rxoutDiv = RegReadRegister(baseNode + "RXOUT_DIV");
  // should check if int is at the end
  int maxPhase = rxoutDivMap.find(rxoutDiv)->second;

  printf("The max phase is: %d\n", maxPhase);

  double phaseMultiplier = maxPhase/MAXUI;

  // =========================
  double min_BER=100.;
  // Set offsets and perform eyescan
  for(int voltage = minVoltage; voltage <= maxVoltage; voltage+=vertIncrement) {

    // https://www.xilinx.com/support/documentation/user_guides/ug476_7Series_Transceivers.pdf#page=300 go to ES_VERT_OFFSET description
    // For bit 7 (8th bit) of ES_VERT_OFFSET
    uint32_t POSITIVE = 0;
    uint32_t NEGATIVE = 1;

    printf("Voltage= %d\n", voltage);
    syslog(LOG_INFO, "%d\n", voltage);
 
    if(voltage < 0) {
      SetEyeScanVoltage(baseNode, (uint8_t)(-1*voltage), NEGATIVE); 
    } else {
      SetEyeScanVoltage(baseNode, voltage, POSITIVE);
    }
    
    for(double phase = MINUI; phase <= MAXUI; phase+=horzIncrement) {
      
      int phaseInt;
      uint32_t sign;

      if(phase < 0) {
	phaseInt = abs(ceil(phase*phaseMultiplier));
	sign = NEGATIVE;
      } else {
	phaseInt = abs(floor(phase*phaseMultiplier));
      	sign = POSITIVE;
      }
      printf("phase is %f\n", phase);
      SetEyeScanPhase(baseNode, phaseInt, sign);
      esCoords.resize(resizeCount);
      

      // record voltage and phase coordinates
      esCoords[coordsIndex].voltage = voltage; 
      esCoords[coordsIndex].phase = phase;
      //      printf("%d %d\n", voltage, phaseInt);
      //Get BER for this point
      //esCoords[coordsIndex].BER = SingleEyeScan(baseNode, lpmNode, maxPrescale);
      SESout singleScanOut=SingleEyeScan(baseNode, lpmNode, maxPrescale);
      esCoords[coordsIndex].BER=singleScanOut.BER;
      esCoords[coordsIndex].error0=singleScanOut.error0;
      esCoords[coordsIndex].sample0=singleScanOut.sample0;
      esCoords[coordsIndex].error1=singleScanOut.error1;
      esCoords[coordsIndex].sample1=singleScanOut.sample1;
      printf("BER=%.20f\n",singleScanOut.BER);
      if (esCoords[coordsIndex].BER<min_BER)
      {
        min_BER=esCoords[coordsIndex].BER;
      }
      //sample count and error count for this point
      // uint32_t const regDataWidth = RegReadRegister(baseNode + "RX_DATA_WIDTH");
      // int const regDataWidthInt = (int)regDataWidth;
      // int const actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
      // int sampleCount = RegReadRegister(baseNode + "SAMPLE_COUNT");
      // int prescale = RegReadRegister(baseNode + "PRESCALE");
      //int es_sample_count=((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
      //esCoords[coordsIndex].sample = es_sample_count;
      //esCoords[coordsIndex].error = RegReadRegister(baseNode + "ERROR_COUNT");
      // Vert sign mask is 0x80 so we need to shift right by 7
      esCoords[coordsIndex].voltageReg = RegReadRegister(baseNode + "VERT_OFFSET_MAG") | (RegReadRegister(baseNode + "VERT_OFFSET_SIGN") << 7); 
      esCoords[coordsIndex].phaseReg = RegReadRegister(baseNode + "HORZ_OFFSET_MAG")&0x0FFF;

      //      printf("%.9f\n", esCoords[coordsIndex].BER);
      //      printf("%d\n", esCoords[coordsIndex].voltage);
      
      // going to next coordinate/scan 
      coordsIndex++;
      resizeCount++;
    }
  }
  //clock end
    // Recording end time.
    time(&end);                                                                                 //end simulation time 

    // Calculating total time taken by the program.
    double time_taken = double(end - start);
    printf("Time taken by program is %f seconds.\n",time_taken);
    printf("Min BER is %.15f\n.", min_BER);

//  // reset FPGA_ID
//  zeroFPGA_ID();
  return esCoords;
}

