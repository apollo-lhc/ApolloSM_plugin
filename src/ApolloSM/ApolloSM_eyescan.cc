#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/eyescan_class.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <BUTool/ToolException.hh>
#include <BUTool/helpers/register_helper.hh>
#include <IPBusIO/IPBusIO.hh>
#include <vector>
#include <stdlib.h>
//#include <math.h> // pow
#include <map>
#include <syslog.h>
#include <time.h>

#define WAIT 0x1
#define END 0x5
#define RUN 0x1
#define STOP_RUN 0x0
#define PRECISION 0.00000001 // 10^-9

#define PRESCALE_STEP 3

#define assertNode(node, correctVal) do{ \
  SM->RegWriteRegister(node, correctVal); \
  if(correctVal != SM->RegReadRegister(node)) { \
    throwException("Unable to set " + node + " correctly to: " + std::to_string(correctVal));\
  } \
} while(0)

#define confirmNode(node, correctVal) do{ \
  if(correctVal != SM->RegReadRegister(node)) { \
    throwException(node + " is not set correctly to: " + std::to_string(correctVal)); \
  } \
} while(0)


// Does not need to be an ApolloSM function, only assertNode and confirmNode (below) will use this
void eyescan::throwException(std::string message) {
  BUException::EYESCAN_ERROR e;
  e.Append(message);
  throw e;
}

// // assert to the node the correct value. Must be an ApolloSM function to use RegWriteRegister and RegReadRegister
// void eyescan::assertNode(ApolloSM*SM, std::string node, uint32_t correctVal) {
//   SM->RegWriteRegister(node, correctVal);
//   // Might be able to just put confirmNode here
//   if(correctVal != SM->RegReadRegister(node)) {
//     throwException("Unable to set " + node + " correctly to: " + std::to_string(correctVal));
//   }
// }

// // confirm that the node value is correct. Must be an ApolloSM function to use RegReadRegister 
// void eyescan::confirmNode(ApolloSM*SM, std::string node, uint32_t correctVal) {
//   if(correctVal != SM->RegReadRegister(node)) {
//     throwException(node + " is not set correctly to: " + std::to_string(correctVal));
//   }
// }


eyescan::eyescan(ApolloSM*SM, std::string baseNode_set, std::string lpmNode_set, int nBinsX, int nBinsY, int max_prescale){
  ES_state_t es_state=UNINIT;
  std::vector<eyescanCoords> scan_output;
  uint32_t Max_prescale= max_prescale;
  std::string baseNode=baseNode_set;
  std::string lpmNode=lpmNode_set;
 
  syslog(LOG_INFO, "appending\n");
  // check for a '.' at the end of baseNode and add it if it isn't there 
  if(baseNode.compare(baseNode.size()-1,1,".") != 0) {
    baseNode.append(".");
  }
  //check that all needed addresses exist
  SM->myMatchRegex(baseNode+lpmNode);
  SM->myMatchRegex(baseNode+"HORZ_OFFSET_MAG");
  SM->myMatchRegex(baseNode+"PHASE_UNIFICATION");
  SM->myMatchRegex(baseNode+"VERT_OFFSET_MAG");
  SM->myMatchRegex(baseNode+"VERT_OFFSET_SIGN");
  SM->myMatchRegex(baseNode+"RX_DATA_WIDTH");
  SM->myMatchRegex(baseNode+"PRESCALE");
  SM->myMatchRegex(baseNode+"RUN");
  SM->myMatchRegex(baseNode+"CTRL_STATUS");
  SM->myMatchRegex(baseNode+"ERROR_COUNT");
  SM->myMatchRegex(baseNode+"SAMPLE_COUNT");
  SM->myMatchRegex(baseNode+"UT_SIGN");



  //Figure out which transister type we're scanning
  typedef enum {gtx, gty, gth, unknown} transist;
  
  transist t;
  
  std::vector<std::string> test_gtx = SM->myMatchRegex(baseNode+"TYPE_7_GTX");
  std::vector<std::string> test_gty = SM->myMatchRegex(baseNode+"TYPE_USP_GTY");
  std::vector<std::string> test_gth = SM->myMatchRegex(baseNode+"TYPE_USP_GTH");
  
  if(test_gtx.size()!=0){
    t=gtx;
    printf("Transistor is GTX.\n");
  } else if(test_gty.size()!=0){
    t=gty;
    printf("Transistor is GTY.\n");
  } else if(test_gth.size()!=0){
    t=gth;
    printf("Transistor is GTH.\n");
  } else{
    t=unknown;
      throwException("No transistor type found.\n");
  }
  // ** ES_EYE_SCAN_EN assert 1
  assertNode(baseNode + "EYE_SCAN_EN", ES_EYE_SCAN_EN);

  // ** ES_ERRDET_EN assert 1
  assertNode(baseNode + "ERRDET_EN", ES_ERRDET_EN);

  // ** ES_PRESCALE set prescale
  //  uint32_t prescale32 = std::stoi(prescale); 
  assertNode(baseNode + "PRESCALE", Max_prescale);

  // *** PMA_CFG confirm all 0s
  if(t == gtx) {
    confirmNode(baseNode + "PMA_CFG", PMA_CFG);
  }
  
  // ** PMA_RSV2[5] assert 1 
  if(t == gtx) {
    assertNode(baseNode + "PMA_RSV2", PMA_RSV2);
  }
  
  int qualSize = 0;
  if(t == gtx) {
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
  if(t == gtx) {
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
  if(t == gtx) {
    confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH_GTX);
  } else if (t == gth) {
    confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH_GTH);
  } else {
    confirmNode(baseNode + "RX_DATA_WIDTH", RX_DATA_WIDTH_GTY);
  }  

  // ** RX_INT_DATAWIDTH confirm
  // 7 series is 1
  // usp is either 0 or 1 but the default (when powered up) seems to be 0. 
  // https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty-transceivers.pdf pg 317 gives only a little more info
  // look for "internal data width"
  if(t == gtx) {
    confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH_GTX);
  } else if(t == gth) {
    confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH_GTH);
  } else {
    confirmNode(baseNode + "RX_INT_DATAWIDTH", RX_INT_DATAWIDTH_GTY);
  }

  //make voltage vector
  double volt_step=254./nBinsY;
  //double volt_array[nBinsY+1];
  double volt;
  std::vector<double> volt_vect;
  if (nBinsY==1)
  {
    volt_vect[0]=0.;
  } else{
    for (double i = -254.; i <= 254; i=i+volt_step)
    {
      volt_vect.push_back(i);
    }
  }
  
  //make phase array
  double phase_step=1./nBinsX;
  //double phase_array[nBinsX+1];
  double phase;
  std::vector<double> phase_vect;
  if (nBinsX==1)
  {
    phase_vect[0]=0.;
  } else{
    for (double i = -.5; i <= .5; i=i+phase_step)
    {
      phase_vect.push_back(i);
    }
  }

  
  std::vector<eyescan::Coords> Coords_vect;
  for (unsigned int i = 0; i < volt_vect.size(); ++i)
  {
    for ( unsigned int j = 0; j < phase_vect.size(); ++j)
    {
      Coords_vect[i].voltage=volt_vect[i];
      Coords_vect[j].phase=phase_vect[j];
    }
  }
  volt = Coords_vect[0].voltage;
  phase=Coords_vect[0].phase;
  scan_pixel(SM, lpmNode, phase, volt, Max_prescale);
  es_state=BUSY;
}

eyescan::~eyescan() {};

eyescan::ES_state_t eyescan::check(){  //checks es_state
  return es_state;
}
void eyescan::update(ApolloSM*SM){
  ES_state_t s = check();
  switch (s){
    case UNINIT:
      break;
    case BUSY:
      break;

    case WAITING_PIXEL:
      volt = Coords_vect[0].voltage;
      phase = Coords_vect[0].phase;
      scan_pixel(SM, lpmNode, phase, volt, Max_prescale);
      es_state=BUSY;
    case DONE:
      break;

    default :
      break;

  }

}

void eyescan::SetEyeScanPhase(ApolloSM*SM, std::string baseNode, /*uint16_t*/ int horzOffset, uint32_t sign) {
  // write the hex
  SM->RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", horzOffset);
  //printf("Set phase stop 1 \n");
  SM->RegWriteRegister(baseNode + "PHASE_UNIFICATION", sign); 
  //RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", (horzOffset + 4096)&0x0FFF);
 }

void eyescan::SetEyeScanVoltage(ApolloSM*SM, std::string baseNode, uint8_t vertOffset, uint32_t sign) {
  // write the hex
  SM->RegWriteRegister(baseNode + "VERT_OFFSET_MAG", vertOffset);
  SM->RegWriteRegister(baseNode + "VERT_OFFSET_SIGN", sign);
}

std::vector<eyescan::eyescanCoords> eyescan::dataout(){
  return scan_output;
}

eyescan::eyescanCoords eyescan::scan_pixel(ApolloSM*SM, std::string lpmNode, float phase, float volt, uint32_t prescale){
  es_state = BUSY;
	eyescanCoords singleScanOut;
  double BER;
  float errorCount;
  float sampleCount;
  float errorCount0;
  float actualsample0;
  uint32_t maxPrescale=prescale;

  uint32_t const regDataWidth = SM->RegReadRegister(baseNode + "RX_DATA_WIDTH");
  int const regDataWidthInt = (int)regDataWidth;
  int const actualDataWidth = SM->busWidthMap.find(regDataWidthInt)->second;
  
  // Check if we're DFE or LPM
  uint32_t const dfe = 0;
  uint32_t const lpm = 1;

  uint32_t const rxlpmen = SM->RegReadRegister(lpmNode);

  //SET VOLTAGE
  // https://www.xilinx.com/support/documentation/user_guides/ug476_7Series_Transceivers.pdf#page=300 go to ES_VERT_OFFSET description
  // For bit 7 (8th bit) of ES_VERT_OFFSET
  uint32_t POSITIVE = 0;
  uint32_t NEGATIVE = 1;

  printf("Voltage= %f\n", volt);
  syslog(LOG_INFO, "%f\n", volt);

if(volt < 0) {
  SetEyeScanVoltage(SM, baseNode, (uint8_t)(-1*volt), NEGATIVE); 
  } else {
    SetEyeScanVoltage(SM, baseNode, volt, POSITIVE);
  }

//SET PHASE
int phaseInt;
uint32_t sign;

if(phase < 0) {
	phaseInt = abs(ceil(phase));
	sign = NEGATIVE;
} else {
	phaseInt = abs(floor(phase));
  sign = POSITIVE;
  }
printf("phase is %f\n", phase);
SetEyeScanPhase(SM, baseNode, phaseInt, sign);

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
    SM->RegWriteRegister(baseNode + "RUN", STOP_RUN);
    
    // assert RUN
    //  assertNode(baseNode + "RUN", RUN);
    SM->RegWriteRegister(baseNode + "RUN", RUN);  
    
    // poll END
    int count = 0;
    // one second max
    while(1000000 > count) {
      if(END == SM->RegReadRegister(baseNode + "CTRL_STATUS")) {
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
    errorCount = SM->RegReadRegister(baseNode + "ERROR_COUNT");
    sampleCount = SM->RegReadRegister(baseNode + "SAMPLE_COUNT");
    
    // Should sleep for some time before de-asserting run. Can be a race condition if we don't sleep
    
    // de-assert RUN (aka go back to WAIT)
    //  assertNode(baseNode + "RUN", STOP_RUN);
    SM->RegWriteRegister(baseNode + "RUN", STOP_RUN);
        
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
    if(1 == (SM->RegReadRegister(baseNode + "UT_SIGN"))) {
      SM->RegWriteRegister(baseNode + "UT_SIGN", 0);
    } else {
      SM->RegWriteRegister(baseNode + "UT_SIGN", 1);
    }
    
    loop = true;
    
    while(loop) {
      //confirm we are in WAIT, if not, stop scan
      //confirmNode(baseNode + "CTRL_STATUS", WAIT);
      SM->RegWriteRegister(baseNode + "RUN", STOP_RUN);
      
      // assert RUN
      //  assertNode(baseNode + "RUN", RUN);
      SM->RegWriteRegister(baseNode + "RUN", RUN);  
      
      // poll END
      int count = 0;
      // one second max
      while(1000000 > count) {
	if(END == SM->RegReadRegister(baseNode + "CTRL_STATUS")) {
	  // Scan has ended
	  break;
	}
	// sleep 1 microsecond
	usleep(1000);
	count++;
	if(1000000 == count) {
	  throwException("BER sequence did not reach end in one second\n");
	}
      }	  
      
      // read error and sample count
      errorCount = SM->RegReadRegister(baseNode + "ERROR_COUNT");
      sampleCount = SM->RegReadRegister(baseNode + "SAMPLE_COUNT");
      
      //Should sleep for some time before de-asserting run. Can be a race condition if we don't sleep
      
      // de-assert RUN (aka go back to WAIT)
      // assertNode(baseNode + "RUN", STOP_RUN);
      SM->RegWriteRegister(baseNode + "RUN", STOP_RUN);
      
      // calculate BER
      BER = errorCount/((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
      actualsample1=((1 << (1+prescale))*sampleCount*(float)actualDataWidth);
      
      // If BER is lower than precision we need to check with a higher prescale to ensure that
      // that is believable. pg 231 https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty-transceivers.pdf
      if((BER < PRECISION) && (prescale != maxPrescale)) {
	prescale+=PRESCALE_STEP;
	if(prescale > maxPrescale) {
	  prescale = maxPrescale;
	  //printf("max prescale %d reached\n", maxPrescale);
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
	loop = false;
      }
    }
  }
  singleScanOut.BER=BER+firstBER;
  singleScanOut.sample0=(unsigned long int)actualsample0;
  singleScanOut.error0=(unsigned long int)errorCount0;
  singleScanOut.sample1=(unsigned long int)actualsample1;
  singleScanOut.error1=(unsigned long int)errorCount1;
  singleScanOut.voltageReg = SM->RegReadRegister(baseNode + "VERT_OFFSET_MAG") | (SM->RegReadRegister(baseNode + "VERT_OFFSET_SIGN") << 7); 
  singleScanOut.phaseReg = SM->RegReadRegister(baseNode + "HORZ_OFFSET_MAG")&0x0FFF;
  scan_output.push_back(singleScanOut);
  Coords_vect.erase(Coords_vect.begin());
  if (Coords_vect.size()==0)
  {
    es_state=DONE;
  } else{
    es_state=WAITING_PIXEL;
  }

  return singleScanOut;
}
