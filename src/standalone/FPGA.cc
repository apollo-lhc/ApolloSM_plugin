#include <standalone/FPGA.hh>
#include <string>
#include <boost/program_options.hpp>
#include <sstream>
#include <syslog.h>
#include <ApolloSM/ApolloSM.hh>

FPGA::FPGA(std::string nameArg, std::string cmArg, boost::program_options::parsed_options PO) {
  // initialize variables
  this->name    = nameArg;
  this->cm      = cmArg;
  this->program = false;
  this->svfFile = "";
  this->xvc     = "";
  this->c2c     = "";
  this->done    = "";
  this->init    = "";
  this->axi     = "";
  this->axilite = "";
  
  // Total number of options received from config file
  int numOptions = PO.options.size();

  // Look for all options related to this FPGA (ie. svf file, xvc label, c2c, etc.)
  for(int i = 0; i < numOptions; i++) {
    std::string nextOption = PO.options[i].string_key;
    std::string value      = PO.options[i].value[0].c_str();
    
    if(0 == nextOption.compare(cmArg + "." + nameArg + ".PROGRAM"))       // ex: CM1.KINTEX.PROGRAM
      {
	// found value for program
	std::istringstream(PO.options[i].value[0]) >> std::boolalpha >> (this->program);
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".SVFFILE"))  // ex: CM1.KINTEX.SVFFILE
      {
	// found value for svf file
	this->svfFile = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".XVCLABEL")) // ex: CM1.KINTEX.XVCLABEL
      {
	// found value for xvc label
	this->xvc = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".C2C"))      // ex: CM1.KINTEX.C2C
      {
	// found value for c2c base node
	this->c2c = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".DONEBIT"))  // ex: CM1.KINTEX.DONEBIT
      {
	// found value for done bit
	this->done = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".INITBIT"))  // ex: CM1.KINTEX.INITBIT
      {
	// found value for initialization bit
	this->init = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".AXI"))      // ex: CM1.KINTEX.AXI
      {
	// found value for axi unblock bit
	this->axi = value;
      }
    else if(0 == nextOption.compare(cmArg + "." + nameArg + ".AXILITE"))  // ex: CM1.KINTEX.AXILITE
      {
	// found value for axilite unblock bit
	this->axilite = value;
      }
  }
}

FPGA::~FPGA() {
}

void FPGA::printInfo() {
  syslog(LOG_INFO, "in fpga print info\n");



  syslog(LOG_INFO, "   cm     : %s\n", (this->cm).c_str());
  std::stringstream ss1;
  std::string str1;
  ss1 << (this->program);
  ss1 >> str1;
  syslog(LOG_INFO, "   program: %s\n", str1.c_str());
  syslog(LOG_INFO, "   svfFile: %s\n", (this->svfFile).c_str());
  syslog(LOG_INFO, "   xvc    : %s\n", (this->xvc).c_str());
  syslog(LOG_INFO, "   c2c    : %s\n", (this->c2c).c_str());
  syslog(LOG_INFO, "   done   : %s\n", (this->done).c_str());
  syslog(LOG_INFO, "   init   : %s\n", (this->init).c_str());
  syslog(LOG_INFO, "   axi    : %s\n", (this->axi).c_str());
  syslog(LOG_INFO, "   axilite: %s\n", (this->axilite).c_str()); 
}

// Checks register/node values
bool checkNode(ApolloSM const * const SM, std::string const node, uint32_t const correctVal) {
  bool const GOOD = true;
  bool const BAD  = false;

  uint32_t readVal;
  if(correctVal != (readVal = SM->RegReadRegister(node))) {
    syslog(LOG_ERR, "%s is, incorrectly, %d\n", node.c_str(), readVal);
    return BAD;
  } 
  return GOOD;
}

// Bring-up CM FPGAs
// It would be nice to incorporate this into bringUp. We would have to figure out how to break out of the try block without return
int bringupCMFPGAs(ApolloSM const * const SM, FPGA const myFPGA) {
  syslog(LOG_INFO, "in bringupfpga\n");


  int const success =  0;
  int const fail    = -1;
  int const nofile  = -2;

  try {
    // ==============================    
    syslog(LOG_INFO, 
	   "Programming: %s FPGA associated with: %s using XVC label: %s and svf file: %s and checking clock locks at: %s\n", 
	   myFPGA.name.c_str(), 
	   myFPGA.cm.c_str(), 
	   myFPGA.xvc.c_str(), 
	   myFPGA.svfFile.c_str(), 
	   myFPGA.c2c.c_str());
    // Check CM is actually powered up and "good". 
    std::string CM_CTRL = "CM." + myFPGA.cm + ".CTRL.";
    if(!checkNode(SM, CM_CTRL + "PWR_GOOD"   , 1)) {return fail;}
    if(!checkNode(SM, CM_CTRL + "IOS_ENABLED", 1)) {return fail;}
    //if(!checkNode(SM, CM_CTRL + "STATE"      , 4)) {return fail;}
    if(!checkNode(SM, CM_CTRL + "STATE"      , 3)) {return fail;}
    // Check that svf file exists
    FILE * f = fopen(myFPGA.svfFile.c_str(), "rb");
    if(NULL == f) {return nofile;}
    fclose(f);
    // program
    SM->svfplayer(myFPGA.svfFile, myFPGA.xvc);
    // Check CM.CM*.C2C clocks are locked
    if(!checkNode(SM, myFPGA.c2c + ".CPLL_LOCK"      , 1)) {return fail;}
    if(!checkNode(SM, myFPGA.c2c + ".PHY_GT_PLL_LOCK", 1)) {return fail;}
    syslog(LOG_INFO, "Successfully programmed %s FPGA\n", myFPGA.name.c_str());
    
    if(myFPGA.init.compare("")) {
      // non empty initialize bit, so we initialize
      syslog(LOG_INFO, "Initializing %s fpga with %s\n", myFPGA.name.c_str(), myFPGA.init.c_str());
      // Get FPGA out of error state
      SM->RegWriteRegister(myFPGA.init, 1);
      usleep(1000000);
      SM->RegWriteRegister(myFPGA.init, 0);
      usleep(5000000);
      // Check that phy lane is up, link is good, and that there are no errors
      if(!checkNode(SM, myFPGA.c2c + ".MB_ERROR"    , 0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".CONFIG_ERROR", 0)) {return fail;}
      //if(!checkNode(SM, myFPGA.c2c + ".LINK_ERROR",   0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".LINK_ERROR"  , 1)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".PHY_HARD_ERR", 0)) {return fail;}
      //if(!checkNode(SM, myFPGA.c2c + ".PHY_SOFT_ERR", 0)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".PHY_MMCM_LOL", 0)) {return fail;} 
      if(!checkNode(SM, myFPGA.c2c + ".PHY_LANE_UP" , 1)) {return fail;}
      if(!checkNode(SM, myFPGA.c2c + ".LINK_GOOD"   , 1)) {return fail;}
      syslog(LOG_INFO, "Initialized %s fpga with %s. Lanes up, links good, and no errors.\n", myFPGA.name.c_str(), myFPGA.c2c.c_str());
    }
     
    // unblock axi
    if(myFPGA.axi.compare("")) {
      SM->RegWriteAction(myFPGA.axi.c_str());      
      syslog(LOG_INFO, "%s: %s unblocked\n", myFPGA.name.c_str(), myFPGA.axi.c_str());    
    }
    if(myFPGA.axilite.compare("")) {
      SM->RegWriteAction(myFPGA.axilite.c_str());
      syslog(LOG_INFO, "%s: %s unblocked\n", myFPGA.name.c_str(), myFPGA.axilite.c_str());
    }
  } catch(BUException::exBase const & e) {
    syslog(LOG_ERR, "Caught BUException: %s\n   Info: %s\n", e.what(), e.Description());
    return fail;
  } catch (std::exception const & e) {
    syslog(LOG_ERR, "Caught std::exception: %s\n", e.what());
    return fail;
  }
  
  return success;
}

void FPGA::bringUp(ApolloSM const * const SM) {
  syslog(LOG_INFO, "in fpga::bringup\n");


  if(this->program) {
    syslog(LOG_INFO, "%s has program = true. Attempting to program...\n", (this->name).c_str());
    int const success =  0;
    int const fail    = -1;
    int const nofile  = -2;
    
    //	    int const programmingFailed     = 0;
    //	    int const programmingSuccessful = 1;
    // assert 0 to done bit
    //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
    switch(bringupCMFPGAs(SM, this)) {
    case success:
      syslog(LOG_INFO, "Bringing up %s FPGA of %s succeeded. Setting %s to 1\n",(this->name).c_str(), (this->cm).c_str(), (this->done).c_str());
      // write 1 to done bit
      //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingSuccessful);
      break;
    case fail:
      // assert 0 to done bit (paranoid)
      syslog(LOG_ERR, "Bringing up %s FPGA of %s failed. Setting %s to 0\n", (this->name).c_str(), (this->name).c_str(), (this->done).c_str());
      //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
      break;
    case nofile:
      // assert 0 to done bit (paranoid)
      syslog(LOG_ERR, "svf file %s does not exist for %s FPGA. Setting %s to 0\n", (this->svfFile).c_str(), (this->name).c_str(), (this->done).c_str());
      //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
      break;
    }
  } else {
    syslog(LOG_INFO, "%s FPGA will not be programmed because it has program = false\n", (this->name).c_str());
  }
  
//  syslog(LOG_INFO, "%s has program = true. Attempting to program..\n", (this->FPGAs[f]).name.c_str());
//	int const success =  0;
//	int const fail    = -1;
//	int const nofile  = -2;
//	
//	// int const programmingFailed     = 0;
//	// int const programmingSuccessful = 1;
//	// assert 0 to done bit
//	// SM->RegWriteRegister(FPGAs[f].done, programmingFailed);
//	switch(bringupCMFPGAs(SM, allCMs[i].FPGAs[f])) {
//	case success:
//	  syslog(LOG_INFO, "Bringing up %s: %s FPGA succeeded. Setting %s to 1\n", (this->name).c_str(), (this->FPGAs[f]).name.c_str(), (this->FPGAs[f]).done.c_str());
//	  // write 1 to done bit
//	  //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingSuccessful);
//	  break;
//	case fail:
//	  // assert 0 to done bit (paranoid)
//	  syslog(LOG_ERR, "Bringing up %s: %s FPGA failed. Setting %s to 0\n", (this->name).c_str(), (this->FPGAs[f]).name.c_str(), (this->FPGAs[f]).done.c_str());
//	  //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
//	  break;
//	case nofile:
//	  // assert 0 to done bit (paranoid)
//	  syslog(LOG_ERR, "svf file %s does not exist for %s FPGA. Setting %s to 0\n", (this->FPGAs[f]).svfFile.c_str(), (this->FPGAs[f]).name.c_str(), (this->.FPGAs[f]).done.c_str());
//	  //	SM->RegWriteRegister(allCMs[i].FPGAs[f].done, programmingFailed);
//	  break;
//	    }
//      } else {
//	syslog(LOG_INFO, "%s will not be programmed because it has program = false\n",(this->FPGAs[f]).name.c_str());
//      }
//    }

}

//std::string name    ; 
//std::string cm      ;
//std::string svfFile ;
//std::string xvc     ;
//std::string c2c     ;
//std::string done    ;
//std::string init    ;
//std::string axi     ;
//std::string axilite ;
