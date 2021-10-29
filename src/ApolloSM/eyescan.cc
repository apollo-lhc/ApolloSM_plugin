#include <ApolloSM/ApolloSM.hh>
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
#define PRECISION 0.00000001 // 10^-9
#define PRESCALE_STEP 3


std::vector<std::vector<std::string> > const static REQUIRED_TABLE_ELEMENTS ={ \
  {},									\
  {"ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "RX_EYESCAN_VS.CODE","RX_EYESCAN_VS.SIGN","RX_EYESCAN_VS.UT",	\
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","ES_PRESCALE",			\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4",					\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4",					\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4",					\
   "ES_CONTROL_STATUS"							\
  },									\
  {"ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "ES_VERT_OFFSET.MAG","ES_VERT_OFFSET.SIGN","ES_VERT_OFFSET.UT",	\
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","PMA_RSV2","ES_PRESCALE",		\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4",					\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4",					\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4",					\
   "ES_CONTROL_STATUS"							\
  },									\
  {"ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "RX_EYESCAN_VS.CODE","RX_EYESCAN_VS.SIGN","RX_EYESCAN_VS.UT",	\
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","ES_PRESCALE",			\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4","ES_SDATA_MASK5",			\
   "ES_SDATA_MASK6","ES_SDATA_MASK7","ES_SDATA_MASK8","ES_SDATA_MASK9",	\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4","ES_QUALIFIER5",			\
   "ES_QUALIFIER6","ES_QUALIFIER7","ES_QUALIFIER8","ES_QUALIFIER9",	\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4","ES_QUAL_MASK5",			\
   "ES_QUAL_MASK6","ES_QUAL_MASK7","ES_QUAL_MASK8","ES_QUAL_MASK9",	\
   "ES_CONTROL_STATUS"							\
  },									\
  {"ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "ES_VERT_OFFSET.MAG","ES_VERT_OFFSET.SIGN","ES_VERT_OFFSET.UT",	\
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","ES_PRESCALE",			\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4",					\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4",					\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4",					\
   "ES_CONTROL_STATUS"							\
  }									\
};

//Zero means an ivalid value
uint8_t const static INTERNAL_DATA_WIDITH[10][3] = {{0,0,0},	\
						    {0,0,0},	\
						    {16,0,0},	\
						    {20,0,0},	\
						    {16,32,0},	\
						    {20,40,0},	\
						    {0,32,64},	\
						    {0,40,80},	\
						    {0,0,64},	\
						    {0,0,80},	\
}


#define assertNode(node, correctVal) do{				\
    SM->RegWriteRegister(node, correctVal);				\
    if(correctVal != SM->RegReadRegister(node)) {			\
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

eyescan::eyescan(ApolloSM*SM, 
		 std::string const & _DRPBaseNode, 
		 std::string const & _lpmNode, 
		 int _nBinsX, int _nBinsY, int _maxPrescale):es_state(UNINIT){
  //Set the node that tells us the LPM / DFE mode enabled
  lpmNode=_lpmNode;
  if(!SM->myMatchRegex(lpmNode).size()){
    throwException("Bad LPMEN node "+lpmNOde+"\n");
  }

 
  DRPBaseNode=_DRPBaseNode;
  // check for a '.' at the end of baseNode and add it if it isn't there 
  if(DRPBaseNode.compare(DRPBaseNode.size()-1,1,".") != 0) {
    DRPBaseNode.append(".");
  }
  
  //Determine the transceiver type
  if( (SM->myMatchRegex(DRPBaseNode+"TYPE_7S_GTX")).size()){
    xcvrType=SERDES_t::GTX_7S;
  }else if( (SM->myMatchRegex(DRPBaseNode+"TYPE_7S_GTH")).size()){
    xcvrType=SERDES_t::GTH_7S;
  }else if( (SM->myMatchRegex(DRPBaseNode+"TYPE_USP_GTH")).size()){
    xcvrType=SERDES_t::GTH_SUP;
  }else if( (SM->myMatchRegex(DRPBaseNode+"TYPE_USP_GTY")).size()){
    xcvrType=SERDES_t::GTY_USP;
  }else{
    throwException("No transceiver type found for node "+DRPBaseNode+".\n");
  }

  //check that all needed addresses exist
  for(auto itCheck= REQUIRED_TABLE_ELEMENTS[xcvrType].begin();
      itCheck != REQUIRED_TABLE_ELEMENTS[xcvrType].end();
      itCheck++){
    if(SM->myMatchRegex(DRPBaseNode+(*itCheck)).size() == 0){
      throwException("Missing "+DRPBaseNode+"."+(*itCheck)+".\n");
    }
  }

  //Determine the prescale
  maxPrescale = _maxPrescale;
  if(maxPrescale > MAX_PRESCALE_VALUE){
    throwException("Prescale larger than max prescale.\n");
  }

  //Determin the number of Xbins
  maxXBins = 64*asdfasdfasdf;
  nBinsX=_nBinsX;
  if(nBinsX > MAX_X_BINS){
    throwException("nBinsX is outside of range.\n");
  }

  //Determin the number of Ybins
  nBinsY=_nBinsY;
  if(nBinsY > MAX_Y_BINS){
    throwException("nBinsY is outside of range.\n");
  }
  
  
  
  // Check ES_EYE_SCAN_EN 
  if(SM->Read(DRPBaseNode+"ES_EYE_SCAN_EN") != 1){
    throwException("ES_EYE_SCAN_END is not '1'.\n Enabling it requires a PMA reset.\n");
  }
  if( (xcvr_type == SERDES_T::GTX_7S) && (SM->Read(DRPBaseNode+"PMA_RSV2")&0x20 != 0x20)){
    throwException("PMA_RSV2 bit 5 must be '1' for GTX_7S SERDES. Enabling it requires a PMA reset.\n");
  }


  // Set ES_ERRDET_EN to 1 so that we have stastical mode, not waveform mode.
  SM->Write(DRPBaseNode+"ES_ERRDET_EN",1);


  // ** ES_PRESCALE set prescale
  SM->Write(DRPBaseNode + "ES_PRESCALE",maxPrescale);

  
  //Get the values of RX_INT_DATAWIDTH and RX_DATA_WIDTH
  rxDataWidth = SM->Read(DRPBaseNode+"RX_DATA_WIDTH");
  if(rxDataWidth >= 10){
    throwException("RX_DATA_WIDTH is larger than 9\n");
  }
  rxIntDataWidth = SM->Read(DRPBaseNode+"RX_INT_DATAWIDTH");
  if(rxIntDataWidth >= 3){
    throwException("RX_INT_DATAWIDTH is larger than 2\n");
  }
  uint8_t internalDataWidth = INTERNAL_DATA_WIDTH[rxDataWidth][rxIntDataWidth];
  if(internalDataWidth == 0){
    throwException("RX_DATA_WIDTH("+std::to_string(rxDataWidth)+
		   ") and RX_INT_DATAWIDTH("+std::to_string(rxIntDataWidth)+
		   ") is an invalid combination\n.");
  }

  //Set ES bit masks
  //If in the future we want to only perform eyescans on specific special bit patterns,
  //Change Qalifier and QualMask appropriately
  int qualSize = 160 ? (xvvrType == GTY_USP) : 80;
  {
    uint16_t es_Qualifier,es_QualMask,es_SDataMask = {0,0,0};
    //fill all of these multi-reg values    
    for(int iBit = 0; iBit <  qualSize; ibit++){
      //Move these down one bit
      es_SDataMask >>= 0x1;
      es_Qualifier >>= 0x1;
      es_QualMask  >>= 0x1;

      //Set the MSB of ex_X to a '1' if we want a 1 in this bit postion
      if(!(
	   (iBit < (qualSize/2)) &&
	   (iBit >= (qualSize/2 - internalDataWidth))){
	   //This is not a bit we care about, so mark it with a '1'
	   esDataMask |= 0x8000;
	}
      }

      es_QualMask  |= 0x8000;
      //es_Qualifier we want to be '0', so we don't have to do anytying.

      if(iBit&0xF == 0xF){
	//we've filled 16 bits, let'w write it. 
	SM->Write(DRPBaseNode+"ES_SDATA_MASK"+std::to_string(iBit&0xF0),es_SDataMask);
	SM->Write(DRPBaseNode+"ES_QUALIFIER" +std::to_string(iBit&0xF0),es_SDataMask);
	SM->Write(DRPBaseNode+"ES_QUAL_MASK" +std::to_string(iBit&0xF0),es_SDataMask);
	//Shifts will remove old values
      }
    }
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
    break;
  case SCAN_START:
    assertNode(DRPBaseNode + "ES_PRESCALE", 0x3);
    scan_pixel(SM);
  case SCAN_PIXEL:
    if (END == SM->RegReadRegister(DRPBaseNode + "ES_CONTROL_STATUS")){
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
  ///this all needs to be replaced by an integer method

  uint32_t rxoutDiv = SM->RegReadRegister(DRPBaseNode + "RXOUT_DIV");
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
  
  uint32_t errorCount = SM->RegReadRegister(DRPBaseNode + "ERROR_COUNT");
  uint32_t sampleCount = SM->RegReadRegister(DRPBaseNode + "SAMPLE_COUNT");
  uint32_t actualsample0;
  uint32_t errorCount0;
  uint32_t const regDataWidth = SM->RegReadRegister(DRPBaseNode + "RX_DATA_WIDTH");
  int const regDataWidthInt = (int)regDataWidth;
  int const actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
  //printf("EndPixelLPM before stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(DRPBaseNode + "CTRL_STATUS"));
  //printf("EndPixelLPM before stop_run RUN=%u\n",SM->RegReadRegister(DRPBaseNode + "RUN"));
  SM->RegWriteRegister(DRPBaseNode + "RUN",STOP_RUN);
  //printf("EndPixelLPM after stop_run RUN=%u\n",SM->RegReadRegister(DRPBaseNode + "RUN"));
  //printf("EndPixelLPM after stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(DRPBaseNode + "CTRL_STATUS"));
  //printf("============================================\n");
  int while_count = 0;
  while(SM->RegReadRegister(DRPBaseNode+"RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;

  // calculate BER
  double BER = errorCount/((1 << (1+cur_prescale))*sampleCount*(float)actualDataWidth);
  if((BER < PRECISION) && (cur_prescale != Max_prescale)) {
    cur_prescale+=PRESCALE_STEP;
      
    if(cur_prescale > Max_prescale) {
      cur_prescale = Max_prescale;
         
    }
      
    scan_pixel(SM);
    return SCAN_PIXEL;
  } else {
    if (errorCount==0) //if scan found no errors default to BER floor
      {
        errorCount=1;
        BER = errorCount/((1 << (1+cur_prescale))*sampleCount*(float)actualDataWidth);
      }
    actualsample0=((1 << (1+cur_prescale))*sampleCount*(float)actualDataWidth);
    errorCount0=errorCount;
    (*it).BER=BER;
    (*it).sample0=actualsample0;
    (*it).error0=errorCount0;
    (*it).sample1=0;
    (*it).error1=0;
    (*it).voltageReg = SM->RegReadRegister(DRPBaseNode + "VERT_OFFSET_MAG") | (SM->RegReadRegister(DRPBaseNode + "VERT_OFFSET_SIGN") << 7); 
    (*it).phaseReg = SM->RegReadRegister(DRPBaseNode + "HORZ_OFFSET_MAG")&0x0FFF;
    it++;
    if (it==Coords_vect.end())
      {
        return SCAN_DONE;
      } else {
      cur_prescale=0;
      scan_pixel(SM);
      return SCAN_PIXEL;
    }
  }
}

eyescan::ES_state_t eyescan::EndPixelDFE(ApolloSM*SM){
  // read error and sample count
  uint32_t const regDataWidth = SM->RegReadRegister(DRPBaseNode + "RX_DATA_WIDTH");
  int const regDataWidthInt = (int)regDataWidth;
  int const actualDataWidth = busWidthMap.find(regDataWidthInt)->second;
  uint32_t errorCount = SM->RegReadRegister(DRPBaseNode + "ERROR_COUNT");
  uint32_t sampleCount = SM->RegReadRegister(DRPBaseNode + "SAMPLE_COUNT");
  //printf("EndPixelLPM before stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(DRPBaseNode + "CTRL_STATUS"));
  //printf("EndPixelLPM before stop_run RUN=%u\n",SM->RegReadRegister(DRPBaseNode + "RUN"));
  SM->RegWriteRegister(DRPBaseNode + "RUN",STOP_RUN);
  //printf("EndPixelLPM after stop_run RUN=%u\n",SM->RegReadRegister(DRPBaseNode + "RUN"));
  //printf("EndPixelLPM after stop_run CTRL_STATUS=%u\n",SM->RegReadRegister(DRPBaseNode + "CTRL_STATUS"));
  //printf("============================================\n");
  int while_count = 0;
  while(SM->RegReadRegister(DRPBaseNode+"RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;
  uint32_t actualsample =((1 << (1+cur_prescale))*sampleCount*actualDataWidth);
  // calculate BER
  double BER = errorCount/((1 << (1+cur_prescale))*sampleCount*(double)actualDataWidth);
  if((BER < PRECISION) && (cur_prescale != Max_prescale)) {
    cur_prescale+=PRESCALE_STEP;
      
    if(cur_prescale > Max_prescale) {
      cur_prescale = Max_prescale;
    }
    assertNode(DRPBaseNode + "PRESCALE", cur_prescale);
    scan_pixel(SM);
    return SCAN_PIXEL;
  } else {
    if (errorCount==0){
      errorCount=1;
      BER = errorCount/((1 << (1+cur_prescale))*sampleCount*(float)actualDataWidth);
    }
    switch (dfe_state){
    case FIRST:
      firstBER=BER;
      (*it).sample0=actualsample;
      (*it).error0=errorCount;
      if(1 == (SM->RegReadRegister(DRPBaseNode + "UT_SIGN"))) {
	SM->RegWriteRegister(DRPBaseNode + "UT_SIGN", 0);
      } else {
	SM->RegWriteRegister(DRPBaseNode + "UT_SIGN", 1);
      }
      BER=0;
      cur_prescale=0;
      dfe_state=SECOND;
      scan_pixel(SM);
      return SCAN_PIXEL;
    case SECOND:
	    
      BER=firstBER+BER;
	    
      (*it).sample1=actualsample;
      (*it).error1=errorCount;
      (*it).BER=BER;
      (*it).voltageReg = SM->RegReadRegister(DRPBaseNode + "VERT_OFFSET_MAG") | (SM->RegReadRegister(DRPBaseNode + "VERT_OFFSET_SIGN") << 7); 
      (*it).phaseReg = SM->RegReadRegister(DRPBaseNode + "HORZ_OFFSET_MAG")&0x0FFF;
      firstBER=0;
      it++;
      if (it==Coords_vect.end())
	{
	      
	  return SCAN_DONE;
	} else {
	cur_prescale=0;
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

void eyescan::SetEyeScanPhase(ApolloSM*SM, std::string DRPBaseNode, /*uint16_t*/ int horzOffset, uint32_t sign) {
  // write the hex
  SM->RegWriteRegister(DRPBaseNode + "HORZ_OFFSET_MAG", horzOffset);
  SM->RegWriteRegister(DRPBaseNode + "PHASE_UNIFICATION", sign); 
  //RegWriteRegister(DRPBaseNode + "HORZ_OFFSET_MAG", (horzOffset + 4096)&0x0FFF);
}

void eyescan::SetEyeScanVoltage(ApolloSM*SM, std::string DRPBaseNode, uint8_t vertOffset, uint32_t sign) {
  // write the hex
  //Take into account RX_EYESCAN_VS_RANGE.... 
  SM->RegWriteRegister(DRPBaseNode + "VERT_OFFSET_MAG", vertOffset);
  SM->RegWriteRegister(DRPBaseNode + "VERT_OFFSET_SIGN", sign);
  //SM->RegWriteRegister(DRPBaseNode + "VERT_OFFSET_SIGN", sign);
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
  
  
  //SET VOLTAGE
  // https://www.xilinx.com/support/documentation/user_guides/ug476_7Series_Transceivers.pdf#page=300 go to ES_VERT_OFFSET description
  // For bit 7 (8th bit) of ES_VERT_OFFSET
  
  uint32_t POSITIVE = 0;
  uint32_t NEGATIVE = 1;
  int32_t voltInt = (*it).voltageInt;
  int32_t phaseInt = (*it).phaseInt;
  assertNode(DRPBaseNode + "PRESCALE", cur_prescale);

  //printf("Voltage= %f\n", volt);
  syslog(LOG_INFO, "%d\n", voltInt);
  //printf("We think volt is %d\n",voltInt);
  if(voltInt < 0) {
    SetEyeScanVoltage(SM, DRPBaseNode, (uint8_t)(-1*voltInt), NEGATIVE); 
  } else {
    SetEyeScanVoltage(SM, DRPBaseNode, voltInt, POSITIVE);
  }
  double transceivvolt = SM->RegReadRegister(DRPBaseNode+"VERT_OFFSET_MAG");
  if(SM->RegReadRegister(DRPBaseNode+"VERT_OFFSET_SIGN")==1){
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

  SetEyeScanPhase(SM, DRPBaseNode, abs(phaseInt), sign);
  //printf("We think phase is %f\n",phase);
  //printf("We think phaseInt is %d\n",phaseInt);
  int32_t transceivphase = SM->RegReadRegister(DRPBaseNode + "HORZ_OFFSET_MAG");
  if(SM->RegReadRegister(DRPBaseNode + "PHASE_UNIFICATION")==1){
    transceivphase=transceivphase*-1.;
  }
  
  //printf("Transceiver phase is %d\n",transceivphase);
  // confirm we are in WAIT, if not, stop scan
  //  confirmNode(DRPBaseNode + "CTRL_STATUS", WAIT);
  //printf("==========================================\n");
  //printf("ctrl_status before stop run write is %u\n",SM->RegReadRegister(DRPBaseNode+"CTRL_STATUS"));
  //printf("RUN before stop run write is %u\n",SM->RegReadRegister(DRPBaseNode+"RUN"));
  SM->RegWriteRegister(DRPBaseNode + "RUN", STOP_RUN);
  int while_count = 0;
  while(SM->RegReadRegister(DRPBaseNode+"RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;

  //printf("RUN after stop run write is %u\n",SM->RegReadRegister(DRPBaseNode+"RUN"));
  //printf("ctrl_status after stop run write is %u\n",SM->RegReadRegister(DRPBaseNode+"CTRL_STATUS"));
  // assert RUN
  //  assertNode(DRPBaseNode + "RUN", RUN);
  SM->RegWriteRegister(DRPBaseNode + "RUN", RUN);  
  while(SM->RegReadRegister(DRPBaseNode+"RUN")!=RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }
  while_count=0;
  //printf("RUN after run write is %u\n",SM->RegReadRegister(DRPBaseNode+"RUN"));
  //printf("ctrl_status after run write is %u\n",SM->RegReadRegister(DRPBaseNode+"CTRL_STATUS"));
}
