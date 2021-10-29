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


std::vector<std::vector<std::string> > const static REQUIRED_TABLE_ELEMENTS ={ \
  {},									\
  {"ES_HORZ_OFFSET.REG","ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION", \
   "RX_EYESCAN_VS.CODE","RX_EYESCAN_VS.REG","RX_EYESCAN_VS.SIGN","RX_EYESCAN_VS.UT", \
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","ES_PRESCALE",			\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4",					\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4",					\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4",					\
   "ES_CONTROL_STATUS","RX_EYESCAN_VS_RANGE",				\
   "ES_ERROR_COUNT","ES_SAMPLE_COUNT",					\
   "ES_CONTROL.RUN","ES_CONTROL.ARM"					\
  },									\
  {"ES_HORZ_OFFSET.REG","ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "ES_VERT_OFFSET.REG","ES_VERT_OFFSET.MAG","ES_VERT_OFFSET.SIGN","ES_VERT_OFFSET.UT",	\
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","PMA_RSV2","ES_PRESCALE",		\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4",					\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4",					\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4",					\
   "ES_CONTROL_STATUS",							\
   "ES_ERROR_COUNT","ES_SAMPLE_COUNT",					\
   "ES_CONTROL.RUN","ES_CONTROL.ARM"					\
  },									\
  {"ES_HORZ_OFFSET.REG","ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "RX_EYESCAN_VS.CODE","RX_EYESCAN_VS.REG","RX_EYESCAN_VS.SIGN","RX_EYESCAN_VS.UT",	\
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
   "ES_CONTROL_STATUS","RX_EYESCAN_VS_RANGE",				\
   "ES_ERROR_COUNT","ES_SAMPLE_COUNT",					\
   "ES_CONTROL.RUN","ES_CONTROL.ARM"					\
  },									\
  {"ES_HORZ_OFFSET.REG","ES_HORZ_OFFSET.OFFSET","ES_HORZ_OFFSET.PHASE_UNIFICATION",		\
   "ES_VERT_OFFSET.REG","ES_VERT_OFFSET.MAG","ES_VERT_OFFSET.SIGN","ES_VERT_OFFSET.UT",	\
   "ES_EYE_SCAN_EN","ES_ERRDET_EN","ES_PRESCALE",			\
   "RX_DATA_WIDTH","RX_INT_DATAWIDTH",					\
   "ES_SDATA_MASK0","ES_SDATA_MASK1","ES_SDATA_MASK2",			\
   "ES_SDATA_MASK3","ES_SDATA_MASK4",					\
   "ES_QUALIFIER0","ES_QUALIFIER1","ES_QUALIFIER2",			\
   "ES_QUALIFIER3","ES_QUALIFIER4",					\
   "ES_QUAL_MASK0","ES_QUAL_MASK1","ES_QUAL_MASK2",			\
   "ES_QUAL_MASK3","ES_QUAL_MASK4",					\
   "ES_CONTROL_STATUS",							\
   "ES_ERROR_COUNT","ES_SAMPLE_COUNT",					\
   "ES_CONTROL.RUN","ES_CONTROL.ARM"					\
  }									\
};


// RXOUT_DIV hex value (DRP encoding) vs max horizontal offset
// https://www.xilinx.com/support/documentation/application_notes/xapp1198-eye-scan.pdf pgs 8 and 9
#define MAX_HORIZONTAL_VALUE_MAG_SIZE 5
uint16_t const static MAX_HORIZONTAL_VALUE_MAG[MAX_HORIZONTAL_VALUE_MAG_SIZE] = {32,64,128,256,512};

#define GTH_USP_DELTA_V_SIZE 4
double const static GTH_USP_DELTA_V[GTH_USP_DELTA_V_SIZE] = {1.5,1.8,2.2,2.8}; //mv/count
#define GTY_USP_DELTA_V_SIZE 4
double const static GTY_USP_DELTA_V[GTY_USP_DELTA_V_SIZE] = {1.6,2.0,2.4,3.3}; //mv/count

// identifies bus data width
// read hex value (DRP encoding) vs bus width (attribute encoding)
#define BUS_WIDTH_SIZE 10
uint8_t const static BUS_WIDTH[BUS_WIDTH_SIZE] = {0,
						  0,
						  16,
						  20,
						  32,
						  40,
						  64,
						  80,
						  128,
						  160};

//Zero means an ivalid value
uint8_t const static INTERNAL_DATA_WIDITH[BUS_WIDTH_SIZE][3] = {{0,0,0},	\
								{0,0,0}, \
								{16,0,0}, \
								{20,0,0}, \
								{16,32,0}, \
								{20,40,0}, \
								{0,32,64}, \
								{0,40,80}, \
								{0,0,64}, \
								{0,0,80}, \
};

// Does not need to be an ApolloSM function, only assertNode and confirmNode (below) will use this
void eyescan::throwException(std::string message) {
  BUException::EYESCAN_ERROR e;
  e.Append(message);
  throw e;
}

eyescan::eyescan(ApolloSM*SM, 
		 std::string const & _DRPBaseNode, 
		 std::string const & _lpmNode, 
		 int _binXIncr,  //positive value
		 int _binYIncr, int _maxPrescale):es_state(UNINIT){
  //Set the node that tells us the LPM / DFE mode enabled
  lpmNode=_lpmNode;
  if(!SM->myMatchRegex(lpmNode).size()){
    throwException("Bad LPMEN node "+lpmNode+"\n");
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
    xcvrType=SERDES_t::GTH_USP;
  }else if( (SM->myMatchRegex(DRPBaseNode+"TYPE_USP_GTY")).size()){
    xcvrType=SERDES_t::GTY_USP;
  }else{
    throwException("No transceiver type found for node "+DRPBaseNode+".\n");
  }

  //check that all needed addresses exist
  for(auto itCheck= REQUIRED_TABLE_ELEMENTS[int(xcvrType)].begin();
      itCheck != REQUIRED_TABLE_ELEMENTS[int(xcvrType)].end();
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

  
  //Determin parameters for the horizontal axis
  rxOutDiv = SM->RegReadRegister(DRPBaseNode + "RXOUT_DIV");
  if(rxOutDiv >= MAX_HORIZONTAL_VALUE_MAG_SIZE){
    throwException("RXOUT_DIV is outside of range.\n");
  }
  maxXBinMag = MAX_HORIZONTAL_VALUE_MAG[rxOutDiv];
  binXIncr=_binXIncr;
  if(binXIncr > maxXBinMag){
    throwException("XIncr is too large\n");
  }else if (binXIncr <=0){
    throwException("XIncr is too small\n");
  }
  //Compute the maximal horizontal x bin magnitude used for this scan
  binXBoundary = (maxXBinMag/binXIncr) * binXIncr;
  binXCount    = 2*(maxXBinMag/binXIncr) + 1;
  

  //Determin the parameters for the vertical axis
  
  if(xcvrType == SERDES_t::GTX_7S || xcvrType == SERDES_t::GTH_7S){
    binYdV = -1;
  }else{
    uint8_t indexV = SM->RegReadRegister(DRPBaseNode + "RX_EYESCAN_VS_RANGE");
    if(indexV >= 4){
      throwException("RX_EYESCAN_VX_RANGE invalid");
    }
    if (xcvrType == SERDES_t::GTH_USP){
      binYdV = GTH_USP_DELTA_V[indexV];
    }else if (xcvrType == SERDES_t::GTY_USP){
      binYdV = GTY_USP_DELTA_V[indexV];
    }else{
      binYdV = -1;
    }

  maxYBinMag = MAX_Y_BIN_MAG;
  binYIncr=_binYIncr;
  if(binYIncr > maxYBinMag){
    throwException("YIncr is too large\n");
  //Compute the maximal horizontal x bin magnitude used for this scan
  if(binYIncr == 0){
    binYBoundary = 0;
    binYCount    = 1;
  }else{
    binYBoundary = (maxYBinMag/binYIncr) * binYIncr;
    binYCount    = 2*(maxYBinMag/binYIncr) + 1;
  }

  
  
  // Check ES_EYE_SCAN_EN 
  if(SM->RegReadRegister(DRPBaseNode+"ES_EYE_SCAN_EN") != 1){
    throwException("ES_EYE_SCAN_END is not '1'.\n Enabling it requires a PMA reset.\n");
  }
  if( (xcvr_type == SERDES_T::GTX_7S) && (SM->RegReadRegister(DRPBaseNode+"PMA_RSV2")&0x20 != 0x20)){
    throwException("PMA_RSV2 bit 5 must be '1' for GTX_7S SERDES. Enabling it requires a PMA reset.\n");
  }


  // Set ES_ERRDET_EN to 1 so that we have stastical mode, not waveform mode.
  SM->RegWriteRegister(DRPBaseNode+"ES_ERRDET_EN",1);


  // ** ES_PRESCALE set prescale
  SM->RegWriteRegister(DRPBaseNode + "ES_PRESCALE",maxPrescale);

  
  //Get the values of RX_INT_DATAWIDTH and RX_DATA_WIDTH
  rxDataWidth = SM->RegReadRegister(DRPBaseNode+"RX_DATA_WIDTH");
  if(rxDataWidth >= BUS_WIDTH_SIZE){
    throwException("RX_DATA_WIDTH is larger than 9\n");
  }
  rxIntDataWidth = SM->RegReadRegister(DRPBaseNode+"RX_INT_DATAWIDTH");
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
	SM->RegWriteRegister(DRPBaseNode+"ES_SDATA_MASK"+std::to_string(iBit&0xF0),es_SDataMask);
	SM->RegWriteRegister(DRPBaseNode+"ES_QUALIFIER" +std::to_string(iBit&0xF0),es_SDataMask);
	SM->RegWriteRegister(DRPBaseNode+"ES_QUAL_MASK" +std::to_string(iBit&0xF0),es_SDataMask);
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

  linkSpeedGbps = 10; //default to 10Gbps
  
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
    SM->RegWriteRegister(DRPBaseNode + "ES_PRESCALE", 0x3);
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
  for (int16_t iHorz   = -binXBoundary; iHorz <= binXBoundary; iHorz+=binXIncr){
    for (int16_t iVert = -binYBoundary; iVert <= binYBoundary; iVert+=binYIncr){
      eyescan::eyescanCoords pixel;
      //Set this pixel up to be scanned.      

      //=====================================================
      // Floating point versions
      //verticle value
      if(binYdV < 0){
	//This is in units of counts, not mV
	pixel.voltage=iVert;
	pixel.voltageReal = false;
      }else{
	pixel.voltage=iVert*binYdV;
	pixel.voltageReal = true;
      }
      //horizontal value
      pixel.phase = iHorz*(MAX_UIO_MAG/maxXBinMag);

      //=====================================================
      //Register write versions
      //ES_VERT_OFFSET like value
      if(iVert >= 0){
	//set magnitude
	pixel.vertWriteVal = iVert & 0x007F;
	//Set UT sign and offset sign
	pixel.vertWriteVal &= 0xFE7F;	
      }else{
	//set magnitude
	pixel.vertWriteVal = (-1*iVert) & 0x007F;
	//Set UT sign and offset sign
	pixel.vertWriteVal |= 0x0180;	
      }
      
      //ES_HORZ_OFFSET like value
      pixel.horzWriteVal = iHorz&0x7FF;
      if(xcvrType == SERDES_T::GTY_USP){
	// Phase unification bit in GTY depends on link speed
	if( linkSpeedGbps > 10){
	  pixel.horzWriteVal |= 0x0800;
	}else{
	  pixel.horzWriteVal &= 0xF7FF;
	}
      }else{
	//all other transceivers
	//Set phase unification bit to '1' for all negative numbers
	if(iHorz < 0){	  
	  pixel.horzWriteVal |= 0x0800;
	}
      }
      Coords_vect.push_back(pixel);
    }
  }
  it=Coords_vect.begin();  
  es_state=SCAN_READY;
}

void eyescan::reset(){
  for (std::vector<eyescanCoords>::iterator i = Coords_vect.begin(); i != Coords_vect.end(); ++i)
    {
      (*it).reset();
    }
  es_state=SCAN_READY;
}

eyescan::ES_state_t eyescan::EndPixelLPM(ApolloSM*SM){
  // read error and sample count
  
  uint32_t errorRawCount  = SM->RegReadRegister(DRPBaseNode + "ES_ERROR_COUNT");
  uint32_t sampleRawCount = SM->RegReadRegister(DRPBaseNode + "ES_SAMPLE_COUNT");

  uint64_t sampleCount;
  SM->RegWriteRegister(DRPBaseNode + "ES_CONTROL.RUN",STOP_RUN);

  //Make sure we get out of the run state
  int while_count = 0;
  while(SM->RegReadRegister(DRPBaseNode+"ES_CONTROL.RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }


  //Update the sample count and error counts
  sampleCount = (1 << (1+(*it).prescale)) * sampleRawCount*BUS_WIDTH[rxDataWidth];
  (*it).error0 += errorRawCount;
  (*it).sample0 += sampleCount;


  // calculate BER
  (*it).BER = (*it).error0 / double((*it).sample0);
  if((BER < PRECISION) && (cur_prescale != Max_prescale)) {
    (*it).prescale += PRESCALE_STEP; 
    if((*it).prescale > Max_prescale) {
      (*it).prescale = Max_prescale;         
    }      
    //keep scanning pixel to get BER
    scan_pixel(SM);
    return SCAN_PIXEL;
  } else {
    if (errorCount==0){ //if scan found no errors default to BER floor
      BER = 1.0/double((*it).sample0);
    }else{
      (*it).BER=BER;
    }
    (*it).sample1=0;
    (*it).error1=0;
    //move to the next pixel
    it++;
    if (it==Coords_vect.end()){
      return SCAN_DONE;
    } else {
      scan_pixel(SM);
      return SCAN_PIXEL;
    }
  }
}

eyescan::ES_state_t eyescan::EndPixelDFE(ApolloSM*SM){
  
  uint32_t errorRawCount  = SM->RegReadRegister(DRPBaseNode + "ES_ERROR_COUNT");
  uint32_t sampleRawCount = SM->RegReadRegister(DRPBaseNode + "ES_SAMPLE_COUNT");

  uint64_t sampleCount;
  SM->RegWriteRegister(DRPBaseNode + "ES_CONTROL.RUN",STOP_RUN);

  //Make sure we get out of the run state
  int while_count = 0;
  while(SM->RegReadRegister(DRPBaseNode+"ES_CONTROL.RUN")!=STOP_RUN){
    usleep(100);
    if(while_count==10000){
      throwException("Base Node state machine stuck.");
      break;
    }
    while_count++;
  }


  sampleCount = (1 << (1+(*it).prescale)) * sampleRawCount*BUS_WIDTH[rxDataWidth];
  if(dfe_state == FIRST){

    (*it).error0 += errorRawCount;
    (*it).sample0 += sampleCount;

    //keep scanning pixel to get BER
    //go to other side of DFE scan next time
    dfe_state=SECOND;
    scan_pixel(SM);
    return SCAN_PIXEL;

  }else if (dfe_state == SECOND){
    (*it).error1 += errorRawCount;
    (*it).sample1 += sampleCount;

    //Compute total BER
    (*it).BER = ((*it).error0 / double((*it).sample0) +
		 (*it).error1 / double((*it).sample1));
    if((BER < PRECISION) && (cur_prescale != Max_prescale)) {
      (*it).prescale += PRESCALE_STEP; 
      if((*it).prescale > Max_prescale) {
	(*it).prescale = Max_prescale;         
      }            
      //keep scanning pixel to get BER
      //go to other side of DFE scan next time
      dfe_state=FIRST;
      scan_pixel(SM);
      return SCAN_PIXEL;
    } else {
      if (errorCount==0){ //if scan found no errors default to BER floor
	(*it).BER = 0.5/double((*it).sample0) + 0.5/double((*it).sample1);
      }else{
	(*it).BER=BER;
      }
      //move to the next pixel
      it++;
      //go to other side of DFE scan next time
      dfe_state=FIRST;
      if (it==Coords_vect.end()){
	return SCAN_DONE;
      } else {
	scan_pixel(SM);
	return SCAN_PIXEL;
      }
    }
  }else{
    throwException("not in DFE mode");
  }
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
      fprintf(dataFile, "%0.9f ", esCoords[i].phase);
      if(esCoords[i].voltage_real){
	fprintf(dataFile, "%3.1f ", esCoords[i].voltage);
      }else{
	fprintf(dataFile, "%d ", esCoords[i].voltage);
      }
      fprintf(dataFile, "%0.20f ", esCoords[i].BER);
      fprintf(dataFile, "%u ", esCoords[i].sample0);
      fprintf(dataFile, "%u ", esCoords[i].error0);
      fprintf(dataFile, "%u ", esCoords[i].sample1);
      fprintf(dataFile, "%u ", esCoords[i].error1);
      fprintf(dataFile, "%u\n", esCoords[i].prescale);
    }
    fclose(dataFile);
  } else {
    throwException("Unable to open file"+outputFile);
    //printf("Unable to open file %s\n", outputFile.c_str());
  }
}


void eyescan::scan_pixel(ApolloSM*SM){
  es_state = SCAN_PIXEL;

  //prescale
  SM->RegWriteRegister(DRPBaseNode + "ES_PRESCALE",(*it).prescale);
  
  //horizontal
  SM->RegWriteRegister(DRPBaseNode + "ES_HORZ_OFFSET",(*it).horzWriteVal);

  //Vertical
  if(xcvrType == SERDES_t::GTH_USP || xcvrType == SERDES_t::GTY_USP){
    if(dfe_state == FIRST){
      SM->RegWriteRegister(DRPBaseNode + "RX_EYESCAN_VS.REG",(*it).vertWriteVal & 0xFEFF);
    }else if (dfe_state == SECOND){
      SM->RegWriteRegister(DRPBaseNode + "RX_EYESCAN_VS.REG",(*it).vertWriteVal | 0x0100);
    }else{
      SM->RegWriteRegister(DRPBaseNode + "RX_EYESCAN_VS.REG",(*it).vertWriteVal);
    }    
  }else if(xcvrType == SERDES_t::GTX_7S  || xcvrType == SERDES_t::GTH_7S ){
    if(dfe_state == FIRST){
      SM->RegWriteRegister(DRPBaseNode + "ES_VERT_OFFSET.REG",(*it).vertWriteVal & 0xFEFF);
    }else if (dfe_state == SECOND){
      SM->RegWriteRegister(DRPBaseNode + "ES_VERT_OFFSET.REG",(*it).vertWriteVal | 0x0100);
    }else{
      SM->RegWriteRegister(DRPBaseNode + "ES_VERT_OFFSET.REG",(*it).vertWriteVal);
    }    
  }

  
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
}
