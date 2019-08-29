#include <ApolloSM/ApolloSM.hh>
#include "ApolloSM/svfplayer.hh"
//#include <../../butool-ipbus-herlpers/include/IPBusRegHelper/IPBusRegHelper.hh>
#include <stdio.h>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
 
/*DEBUGGING*/
#define DEBUG
#ifndef DEBUG
int counter = 0;
int lines = 32;
#endif

//Defining variables for AXI
uint32_t tms32, tdi32, length32, tdo32;
int tmsval, tdival, indx;

void SVFPlayer::tck() {
  //write tms & tdi, then update length
  tms32 ^= (-tmsval ^ tms32) & (1UL << indx);
  tdi32 ^= (-tdival ^ tdi32) & (1UL << indx);
  length32 = indx + 1;

  //if tms and tdi full
  if(indx == 31) {
    
    //assign registers
    RegWriteNode(*nLength, length32);
    RegWriteNode(*nTMS, tms32);
    RegWriteNode(*nTDI, tdi32);
    RegWriteNode(*nGO, 1UL);

    //wait for read
    while(RegReadNode(*nGO)) {}
    
    //reset local registers
    length32 = 0UL;
    tms32 = 0UL;
    tdi32 = 0UL;
    //reset indx
    indx = 0;
  } else {indx++;}

  //Debugging
#ifndef DEBUG
  if (counter == lines) {}
  else {
    counter++;
    //print tdi
    fprintf(stderr, "_tdi_");
    for (int run = 0; run < 32; ++run) {
      if (tdi32 >> run & 0x1) fprintf(stderr, "1");
      else fprintf(stderr, "0");
    }
    fprintf(stderr, "\n");
    //print tms
    fprintf(stderr, "_tms_");
    for (int run = 0; run < 32; ++run) {
      if (tms32 >> run & 0x1) fprintf(stderr, "1");
      else fprintf(stderr, "0");
    }
    fprintf(stderr, "\n");
  }
#endif
}

//Empty definitions,
static int io_tdo() {return -1;}
void SVFPlayer::pulse_sck() {}
void SVFPlayer::set_trst(int v) {if ((v * 0)==1){fprintf(stderr,"null");} }
int SVFPlayer::set_frequency(int v) {return (v * 0);}

int SVFPlayer::setup(std::string const & XVCReg) {

  //Setting nodes
  nTDI = &GetNode(XVCReg+".TDI_VECTOR");
  nTDO = &GetNode(XVCReg+".TDO_VECTOR");
  nTMS = &GetNode(XVCReg+".TMS_VECTOR");
  nLength = &GetNode(XVCReg+".LENGTH");
  nGO = &GetNode(XVCReg+".GO");
  
  //Setting up AXI
  tms32 = 0UL;
  tdi32 = 0UL;
  length32 = 0UL;
  tdo32 = 0UL;
  tmsval = 0;
  tdival = 0;
  indx = 0;
  return 0;
}

int SVFPlayer::shutdown() {

  
  //assign registers
  RegWriteNode(*nLength, length32);
  RegWriteNode(*nTMS, tms32);
  RegWriteNode(*nTDI, tdi32);
  RegWriteNode(*nGO, 1);
  
  //wait for read
  while(RegReadNode(*nGO)) {}
  
  //reset local registers
  length32 = 0UL;
  tms32 = 0UL;
  tdi32 = 0UL;
  tmsval = 0;
  tdival =0;
  indx = 0;
  return 0;
}

//runs a few times
void SVFPlayer::udelay(long usecs, int tms, long num_tck) {
  if (num_tck > 0) {
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    tmsval = !! tms;
    while (num_tck > 0) {
      tck();
      num_tck--;
    }
    gettimeofday(&tv2, NULL);
    if (tv2.tv_sec > tv1.tv_sec) {
      usecs -= (1000000 - tv1.tv_usec) + (tv2.tv_usec - tv1.tv_sec - 1) * 1000000;
      tv1.tv_usec = 0;
    }
    usecs -= tv2.tv_usec;
  }
  if (usecs > 0) {usleep(usecs);}
}

//Runs for reading file
int SVFPlayer::getbyte() {
  return fgetc(f);
}

//Main function for setting tms, tdi, and tck
int SVFPlayer::pulse_tck(int tms, int tdi, int tdo, int rmask, int sync) {
  if( ((rmask + sync) * 0) == 1) {fprintf(stderr, "null");}
  //set tms val
  tmsval = !! tms;
  //set tdi val
  if (tdi >= 0) {
    bitcount_tdi++;
    tdival = !! tdi;
  }
  //pulse tck
  tck();
  //read tdo
  int line_tdo = io_tdo();
  int rc = line_tdo >= 0 ? line_tdo : 0;
  //interpret tdo
  if (tdo >= 0 && line_tdo >= 0) {
    bitcount_tdo++;
    if (tdo != line_tdo) {rc = -1;}
  }
  //END
  return rc;
}

int SVFPlayer::play(std::string const & svfFile , std::string const & XVCReg) {
  
  //Giving credit to original creator
  fprintf(stderr, "\nxsvftool-gpio, part of Lib(X)SVF (http://www.clifford.at/libxsvf/).\n");
  fprintf(stderr, "Copyright (C) 2009  RIEGL Research ForschungsGmbH\n");
  fprintf(stderr, "Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>\n");
  fprintf(stderr, "Lib(X)SVF is free software licensed under the ISC license.\n");  
  fprintf(stderr, "Modified for use in Apollo platform by Michael Kremer, kremerme@bu.edu\n\n"); //Mike

  //open SVF file
  f = fopen(svfFile.c_str(),"rb"); //swith to take path in
  if (f == NULL) {fprintf(stderr, "failed to open path\n");}
  else {fprintf(stderr, "playing %s\n", svfFile.c_str());}

  //set Tap State
  tap_state = LIBXSVF_TAP_INIT;

  //Run setup
  if (setup(XVCReg) < 0) {
    fprintf(stderr, "Setup of JTAG interface failed.\n");
    return -1;
  } else {fprintf(stderr, "JTAG setup succesful\n");}

  //Run svf player
  int rc = svf_reader();
  tap_walk(LIBXSVF_TAP_RESET); //Reset tap
  
  //Run shutdown
  if (shutdown() < 0) {
    fprintf(stderr, "Shutdown of JTAG interface failed.\n");
    return -1;
  } else {
    fprintf(stderr, "JTAG shtdown succesful.\n");
#ifndef DEBUG
    fprintf(stderr, "Ran %d significant tdi bits.\n", bitcount_tdi);
    fprintf(stderr, "Recieved %d significant tdo bits.\n", bitcount_tdo);
#endif
  }
  return rc;
}

SVFPlayer::SVFPlayer(uhal::HwInterface * const * _hw): f(NULL), nTDI(NULL), nTDO(NULL), nTMS(NULL), nLength(NULL), nGO(NULL) {
  SetHWInterface(_hw);  
}
