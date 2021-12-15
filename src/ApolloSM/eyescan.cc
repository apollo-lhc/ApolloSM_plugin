
#include <ApolloSM/eyescan_class.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <BUTool/ToolException.hh>
#include <BUTool/helpers/register_helper.hh>
#include <IPBusIO/IPBusIO.hh>
#include <vector>
#include <queue>
#include <stdlib.h>
//#include <math.h> // pow
#include <map>
#include <syslog.h>
#include <time.h>

#define WAIT 0x1
#define END 0x5
#define RUN 0x1
#define STOP_RUN 0x0
#define PRECISION 0.00000001  //PK: 10^-8 not 10^-9
#define PRESCALE_STEP 1  //PK: default from BU was 3
#define MIN_ERROR_COUNT 3  //PK: default from Rui's MATLAB script

#define assertNode(node, correctVal) do{				\
    SM->RegWriteRegister(node, correctVal);                            \
    if(std::abs(correctVal) != std::abs(SM->RegReadRegister(node))) {			\
      throwException("Unable to set " + node + " correctly to: " + std::to_string(correctVal));	\
    }									\
  } while(0)

#define confirmNode(node, correctVal) do{				\
    if(correctVal != SM->RegReadRegister(node)) {			\
      throwException(node + " is not set correctly to: " + std::to_string(correctVal)); \
    }									\
  } while(0)


// Does not need to be an ApolloSM function, only assertNode and confirmNode (below) will use this
void eyescan::throwException(std::string message) {
  BUException::EYESCAN_ERROR e;
  e.Append(message);
  throw e;
}

eyescan::eyescan(ApolloSM*SM, std::string baseNode_set, std::string lpmNode_set, int nBinsX_set, int nBinsY_set, int max_prescale):es_state(UNINIT){
 
  Max_prescale= max_prescale;
  baseNode=baseNode_set;
  lpmNode=lpmNode_set;
  nBinsX=nBinsX_set;
  nBinsY=nBinsY_set;
 
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
  SM->myMatchRegex(baseNode+"TYPE_7_GTX");
  //Figure out which transceiver type we're scanning
  typedef enum {gtx, gty, gth, unknown} transceiv;
  
  transceiv t;
  
  std::vector<std::string> test_gtx = SM->myMatchRegex(baseNode+"TYPE_7_GTX");
  std::vector<std::string> test_gty = SM->myMatchRegex(baseNode+"TYPE_USP_GTY");
  std::vector<std::string> test_gth = SM->myMatchRegex(baseNode+"TYPE_USP_GTH");
 
  if(test_gtx.size()!=0){
    t=gtx;
    
  } else if(test_gty.size()!=0){
    t=gty;
    
  } else if(test_gth.size()!=0){
    t=gth;
    
  } else{
    t=unknown;
    throwException("No transceiver type found for node "+baseNode+".\n");
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
  rxlpmen = SM->RegReadRegister(lpmNode);
  if(rxlpmen==DFE){
    dfe_state=FIRST;
  }else{
    dfe_state=LPM_MODE;
  }
  es_state=SCAN_INIT;
  
}

eyescan::~eyescan() {};
eyescan::ES_state_t eyescan::check(){  //checks es_state
  return es_state;
}
void eyescan::update(ApolloSM*SM){

  switch (es_state){
  case SCAN_INIT:
    initialize(SM);
    es_state= SCAN_READY;
    break;
  case SCAN_READY://make reset a func; make this a READY state
    printf("READY\n");
    break;
  case SCAN_START:
    printf("START\n");
    cur_prescale = SM->RegReadRegister(baseNode + "PRESCALE"); 
    assertNode(baseNode + "PRESCALE", cur_prescale); 
    scan_pixel(SM);
  case SCAN_PIXEL:
    //printf("PIXEL\n");
    if (END == SM->RegReadRegister(baseNode + "CTRL_STATUS")){
      if (rxlpmen==DFE){
	es_state = EndPixelDFE(SM);
          
      } else {
	es_state = EndPixelLPM(SM);
      }
    } else {
      break;
    }
      
    break;
  case SCAN_DONE:
    printf("DONE/n");
    break;
  default :
    break;
  }
}

void eyescan::start(){
  if(es_state==SCAN_READY){
    es_state=SCAN_START;
  } else {
    throwException("Scan can't start as it is not in the ready state.");
  }
}

void eyescan::initialize(ApolloSM*SM){
  uint32_t rxoutDiv = SM->RegReadRegister(baseNode + "RXOUT_DIV");
  int maxPhase = rxoutDivMap.find(rxoutDiv)->second;
  double phaseMultiplier = maxPhase/MAXUI;
  //make Coords vector
  double volt_step=254./nBinsY;
  //double volt;
  std::vector<double> volt_vect;
  if (nBinsY==1){
    volt_vect.push_back(0.);
  } else{
    for (double i = -127.; i <= 127.; i=i+volt_step){
      volt_vect.push_back(i);
    }
  }
  
  //make phase array
  double phase_step=1./nBinsX;
  //double phase;
  std::vector<double> phase_vect;
  if (nBinsX==1)
    {
      phase_vect.push_back(0.);
    } else{
    for (double i = -.5; i <= .5; i=i+phase_step)
      {
	phase_vect.push_back(i);
      }
  }

  for (unsigned int i = 0; i < volt_vect.size(); ++i)
    {
      //std::vector<eyescan::Coords> Coords_col;
      for ( unsigned int j = 0; j < phase_vect.size(); ++j)
	{
	  eyescan::eyescanCoords pixel;
	  pixel.voltage=volt_vect[i];
	  pixel.phase=phase_vect[j];
	  if(volt_vect[i]<0){
	    pixel.voltageInt=ceil(volt_vect[i]);
	  } else {
	    pixel.voltageInt=floor(volt_vect[i]);
	  }
	  if(phase_vect[i]<0){
	    pixel.phaseInt=ceil(phase_vect[j]*phaseMultiplier);
	  } else {
	    pixel.phaseInt=floor(phase_vect[j]*phaseMultiplier);
	  }
	  //printf("Initialize phaseInt is %d\n", pixel.phaseInt);
	  Coords_vect.push_back(pixel);
	}
    }
  it=Coords_vect.begin();
  
  es_state=SCAN_READY;
}

void eyescan::reset(){
  for (std::vector<eyescanCoords>::iterator i = Coords_vect.begin(); i != Coords_vect.end(); ++i)
    {
      (*it).sample0=0;
      (*it).error0=0;
      (*it).sample1=0;
      (*it).error1=0;
      (*it).voltageReg=0;
      (*it).phaseReg=0;
    }
  es_state=SCAN_READY;
}

eyescan::ES_state_t eyescan::EndPixelLPM(ApolloSM*SM){
  // read error and sample count
  
  uint32_t errorCount = SM->RegReadRegister(baseNode + "ERROR_COUNT");
  uint32_t sampleCount = SM->RegReadRegister(baseNode + "SAMPLE_COUNT");
  uint32_t actualsample0;
  uint32_t errorCount0;
  uint32_t const regDataWidth = SM->RegReadRegister(baseNode + "RX_DATA_WIDTH");
  int const regDataWidthInt = (int)regDataWidth;
  int const actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
  //printf("EndPixelLPM before stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(baseNode + "CTRL_STATUS"));
  //printf("EndPixelLPM before stop_run RUN=%u\n",SM->RegReadRegister(baseNode + "RUN"));
  SM->RegWriteRegister(baseNode + "RUN",STOP_RUN);
  //printf("EndPixelLPM after stop_run RUN=%u\n",SM->RegReadRegister(baseNode + "RUN"));
  //printf("EndPixelLPM after stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(baseNode + "CTRL_STATUS"));
  //printf("============================================\n");
  int while_count = 0;
  while(SM->RegReadRegister(baseNode+"RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;
 
  // PK: calculate BER (neighboring prescales)
  // LPM
  double BER = errorCount / ((1 << (1 + cur_prescale)) * sampleCount * (float)actualDataWidth);

  if (((BER < PRECISION) && (cur_prescale != Max_prescale)) || ((errorCount > 10 * MIN_ERROR_COUNT) && (cur_prescale != 0))) {
	  if (cur_prescale < Max_prescale && errorCount < 10 * MIN_ERROR_COUNT) {
		  cur_prescale = std::min(cur_prescale + PRESCALE_STEP, Max_prescale);
		  if (errorCount < MIN_ERROR_COUNT) {
			  assertNode(baseNode + "PRESCALE", cur_prescale);
			  scan_pixel(SM);
			  return SCAN_PIXEL;

		  }
	  }
	  else if ((cur_prescale > 0 && errorCount > 10 * MIN_ERROR_COUNT)) {
		  if (errorCount > 1000 * MIN_ERROR_COUNT) {
			  cur_prescale = std::max((int)(cur_prescale - 2 * PRESCALE_STEP), 0);
		  }
		  else {
			  cur_prescale = std::max((int)(cur_prescale - PRESCALE_STEP), 0);
		  }
		  assertNode(baseNode + "PRESCALE", std::max((int)cur_prescale, 0));
		  scan_pixel(SM);
		  return SCAN_PIXEL;



	  }
	  else {
		  scan_pixel(SM);
		  return SCAN_PIXEL;

	  }
	  scan_pixel(SM);
	  return SCAN_PIXEL;
  }
  else {
	  if (errorCount == 0) //if scan found no errors default to BER floor
	  {
		  errorCount = 1;
		  BER = errorCount / ((1 << (1 + cur_prescale)) * sampleCount * (float)actualDataWidth);
	  }
	  actualsample0 = ((1 << (1 + cur_prescale)) * sampleCount * (float)actualDataWidth);
	  errorCount0 = errorCount;
	  (*it).BER = BER;
	  (*it).sample0 = actualsample0;
	  (*it).error0 = errorCount0;
	  (*it).sample1 = 0;					
	  (*it).error1 = 0;
	  (*it).voltageReg = SM->RegReadRegister(baseNode + "VERT_OFFSET_MAG") | (SM->RegReadRegister(baseNode + "VERT_OFFSET_SIGN") << 7);
	  (*it).phaseReg = SM->RegReadRegister(baseNode + "HORZ_OFFSET_MAG") & 0x0FFF;
	  it++;
	  if (it == Coords_vect.end())
	  {
		  return SCAN_DONE;
	  }
	  else {
		  scan_pixel(SM); 
		  return SCAN_PIXEL;
	  }
  }
   

}

eyescan::ES_state_t eyescan::EndPixelDFE(ApolloSM*SM){
  // read error and sample count
  uint32_t const regDataWidth = SM->RegReadRegister(baseNode + "RX_DATA_WIDTH");
  int const regDataWidthInt = (int)regDataWidth;
  int const actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
  uint32_t errorCount = SM->RegReadRegister(baseNode + "ERROR_COUNT");
  uint32_t sampleCount = SM->RegReadRegister(baseNode + "SAMPLE_COUNT");
  //printf("EndPixelLPM before stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(baseNode + "CTRL_STATUS"));
  //printf("EndPixelLPM before stop_run RUN=%u\n",SM->RegReadRegister(baseNode + "RUN"));
  SM->RegWriteRegister(baseNode + "RUN",STOP_RUN);
  //printf("EndPixelLPM after stop_run RUN=%u\n",SM->RegReadRegister(baseNode + "RUN"));
  //printf("EndPixelLPM after stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(baseNode + "CTRL_STATUS"));
  //printf("============================================\n");
  int while_count = 0;
  while(SM->RegReadRegister(baseNode+"RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;
  uint32_t actualsample =((1 << (1+cur_prescale))*sampleCount*actualDataWidth);
  
   
  // PK: calculate BER (neighboring prescales)
  // DFE: Yes 
  
  double BER = errorCount / ((1 << (1 + cur_prescale)) * sampleCount * (float)actualDataWidth);
  if(((BER < PRECISION) && (cur_prescale != Max_prescale)) || ((errorCount > 10*MIN_ERROR_COUNT) && (cur_prescale != 0))) {
	  if (cur_prescale < Max_prescale && errorCount < 10 * MIN_ERROR_COUNT)  {
		  cur_prescale = std::min(cur_prescale + PRESCALE_STEP, Max_prescale);
		  if (errorCount < MIN_ERROR_COUNT) {
			  assertNode(baseNode + "PRESCALE", cur_prescale);
			  scan_pixel(SM);
			  return SCAN_PIXEL;

		  }
	  }
	  else if ((cur_prescale > 0 && errorCount > 10*MIN_ERROR_COUNT)) {
		  if (errorCount > 1000 * MIN_ERROR_COUNT) {
			  cur_prescale = std::max((int)(cur_prescale - 2 * PRESCALE_STEP), 0);
		  }
		  else {
			  cur_prescale = std::max((int)(cur_prescale - PRESCALE_STEP), 0);
		  }
		  assertNode(baseNode + "PRESCALE", std::max((int)cur_prescale, 0));
		  scan_pixel(SM);
		  return SCAN_PIXEL;



	  }
	  else {
          scan_pixel(SM);
		  return SCAN_PIXEL;

	  }
          scan_pixel(SM);
          return SCAN_PIXEL;
  }
  else {
          
	  if (errorCount == 0) {
                  
		  errorCount = 1;
		  BER = errorCount / ((1 << (1 + cur_prescale)) * sampleCount * (float)actualDataWidth);
	  }
	  switch (dfe_state) {
	  case FIRST:
		  firstBER = BER;
		  (*it).sample0 = actualsample;
		  (*it).error0 = errorCount;
		  if (1 == (SM->RegReadRegister(baseNode + "UT_SIGN"))) {
			  SM->RegWriteRegister(baseNode + "UT_SIGN", 0);
		  }
		  else {
			  SM->RegWriteRegister(baseNode + "UT_SIGN", 1);
		  }
		  BER = 0;
		  dfe_state = SECOND;
		  scan_pixel(SM); 
		  return SCAN_PIXEL;
	  case SECOND:

		  BER = firstBER + BER;

		  (*it).sample1 = actualsample;
		  (*it).error1 = errorCount;
		  (*it).BER = BER;
		  (*it).voltageReg = SM->RegReadRegister(baseNode + "VERT_OFFSET_MAG") | (SM->RegReadRegister(baseNode + "VERT_OFFSET_SIGN") << 7);
                  (*it).phaseReg = SM->RegReadRegister(baseNode + "HORZ_OFFSET_MAG") & 0x0FFF;
		  firstBER = 0;
		  it++;
		  if (it == Coords_vect.end())
		  {

			  return SCAN_DONE;
		  }
		  else {
			  dfe_state = FIRST;
			  scan_pixel(SM); 
			  return SCAN_PIXEL;
		  }
	  case LPM_MODE:
		  throwException("DFE mode not LPM");
		  return UNINIT;
	  default:
		  throwException("Not DFE or LPM mode?");
		  return UNINIT;

	  }
  }
     
  
}

void eyescan::SetEyeScanPhase(ApolloSM*SM, std::string baseNode, /*uint16_t*/ int horzOffset, uint32_t sign) {
  // write the hex
  SM->RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", horzOffset);
  SM->RegWriteRegister(baseNode + "PHASE_UNIFICATION", sign); 
  //RegWriteRegister(baseNode + "HORZ_OFFSET_MAG", (horzOffset + 4096)&0x0FFF);
}

void eyescan::SetEyeScanVoltage(ApolloSM*SM, std::string baseNode, uint8_t vertOffset, uint32_t sign) {
  // write the hex
  SM->RegWriteRegister(baseNode + "VERT_OFFSET_MAG", vertOffset);
  SM->RegWriteRegister(baseNode + "VERT_OFFSET_SIGN", sign);
  //SM->RegWriteRegister(baseNode + "VERT_OFFSET_SIGN", sign);
}

std::vector<eyescan::eyescanCoords> const& eyescan::dataout(){
  return Coords_vect;
}



void eyescan::fileDump(std::string outputFile){

  const std::vector<eyescan::eyescanCoords> esCoords=dataout();
  
  FILE * dataFile = fopen(outputFile.c_str(), "w");
  if(dataFile!= NULL){
    printf("\n\n\n\n\nThe size of esCoords is: %d\n", (int)esCoords.size());
    
    for(int i = 0; i < (int)esCoords.size(); i++) {
      fprintf(dataFile, "%.9f ", esCoords[i].phase);
      fprintf(dataFile, "%d ", esCoords[i].voltageInt);
      fprintf(dataFile, "%.20f ", esCoords[i].BER);
      fprintf(dataFile, "%u ", esCoords[i].sample0);
      fprintf(dataFile, "%u ", esCoords[i].error0);
      fprintf(dataFile, "%u ", esCoords[i].sample1);
      fprintf(dataFile, "%u ", esCoords[i].error1);
      fprintf(dataFile, "0x%03x ", esCoords[i].voltageReg & 0xFF);
      fprintf(dataFile, "0x%03x\n", esCoords[i].phaseReg & 0xFFF);
    }
    fclose(dataFile);
  } else {
    throwException("Unable to open file"+outputFile);
    //printf("Unable to open file %s\n", outputFile.c_str());
  }
}


void eyescan::scan_pixel(ApolloSM*SM){
  es_state = SCAN_PIXEL;
  //std::cout << __LINE__ << std::endl;  
  
  //SET VOLTAGE
  // https://www.xilinx.com/support/documentation/user_guides/ug476_7Series_Transceivers.pdf#page=300 go to ES_VERT_OFFSET description
  // For bit 7 (8th bit) of ES_VERT_OFFSET
  
  uint32_t POSITIVE = 0;
  uint32_t NEGATIVE = 1;
  int32_t voltInt = (*it).voltageInt;
  int32_t phaseInt = (*it).phaseInt;
  //printf("scan_pixel: prescale: %d \n", cur_prescale);
  assertNode(baseNode + "PRESCALE", cur_prescale);
  
  //printf("Voltage= %f\n", volt);
  syslog(LOG_INFO, "%d\n", voltInt);
  //printf("We think volt is %d\n",voltInt);
  if(voltInt < 0) {
    SetEyeScanVoltage(SM, baseNode, (uint8_t)(-1*voltInt), NEGATIVE); 
  } else {
    SetEyeScanVoltage(SM, baseNode, voltInt, POSITIVE);
  }
  double transceivvolt = SM->RegReadRegister(baseNode+"VERT_OFFSET_MAG");
  if(SM->RegReadRegister(baseNode+"VERT_OFFSET_SIGN")==1){
    transceivvolt=transceivvolt*-1;
  }
  //printf("Volt actually is %f\n",transistvolt);
  //SET PHASE
  //int phaseInt;
  uint32_t sign;

  if(phaseInt < 0) {
    //phaseInt = abs(ceil(phase*phaseMultiplier));
    sign = NEGATIVE;
  } else {
    //phaseInt = abs(floor(phase*phaseMultiplier));
    sign = POSITIVE;
  }

  SetEyeScanPhase(SM, baseNode, abs(phaseInt), sign);
  //printf("We think phase is %f\n",phase);
  //printf("We think phaseInt is %d\n",phaseInt);
  int32_t transceivphase = SM->RegReadRegister(baseNode + "HORZ_OFFSET_MAG");
  if(SM->RegReadRegister(baseNode + "PHASE_UNIFICATION")==1){
    transceivphase=transceivphase*-1.;
  }
  
  //printf("Transceiver phase is %d\n",transceivphase);
  // confirm we are in WAIT, if not, stop scan
  //  confirmNode(baseNode + "CTRL_STATUS", WAIT);
  //printf("==========================================\n");
  //printf("ctrl_status before stop run write is %u\n",SM->RegReadRegister(baseNode+"CTRL_STATUS"));
  //printf("RUN before stop run write is %u\n",SM->RegReadRegister(baseNode+"RUN"));
  SM->RegWriteRegister(baseNode + "RUN", STOP_RUN);
  int while_count = 0;
  while(SM->RegReadRegister(baseNode+"RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0; 
  //printf("RUN after stop run write is %u\n",SM->RegReadRegister(baseNode+"RUN"));
  //printf("ctrl_status after stop run write is %u\n",SM->RegReadRegister(baseNode+"CTRL_STATUS"));
  // assert RUN
  //  assertNode(baseNode + "RUN", RUN);
  SM->RegWriteRegister(baseNode + "RUN", RUN);  
  while(SM->RegReadRegister(baseNode+"RUN")!=RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;
  //printf("RUN after run write is %u\n",SM->RegReadRegister(baseNode+"RUN"));
  //printf("ctrl_status after run write is %u\n",SM->RegReadRegister(baseNode+"CTRL_STATUS"));
}
