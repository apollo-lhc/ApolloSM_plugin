#include <ApolloSM/svfplayer.hh>
//#include <stdio.h>
//#include <string>
#include <sys/time.h> //get time of day
#include <unistd.h> //usleep
//#include <string.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <errno.h>
//#include <stdint.h>
#include <sys/mman.h> //memmap
//#include <sys/types.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>  //for fd consts
#include <stdexcept> //runtime_error
#include <ApolloSM/uioLabelFinder.hh> 

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
    
    jtag_reg->length_offset = length32;
    jtag_reg->tms_offset    = tms32;
    jtag_reg->tdi_offset    = tdi32;
    jtag_reg->ctrl_offset   = 1;

    //wait for read
    while(jtag_reg->ctrl_offset) {}
    
    //reset local registers
    length32 = 0UL;
    tms32 = 0UL;
    tdi32 = 0UL;
    //reset indx
    indx = 0;
  } else {indx++;}

}

//Empty definitions,
static int io_tdo() {return -1;}
void SVFPlayer::pulse_sck() {}
void SVFPlayer::set_trst(int v) {if ((v * 0)==1){fprintf(stderr,"null");} }
int SVFPlayer::set_frequency(int v) {return (v * 0);}

int SVFPlayer::setup() {

  
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
  jtag_reg->length_offset = length32;
  jtag_reg->tms_offset    = tms32;
  jtag_reg->tdi_offset    = tdi32;
  jtag_reg->ctrl_offset   = 1;
    
  //wait for read
  while(jtag_reg->ctrl_offset) {}
  
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
  return fgetc(svfFile);
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

int SVFPlayer::play(std::string const & svfFileName , std::string const & XVCLabel) {
  
  //Giving credit to original creator
  //fprintf(stderr, "\nxsvftool-gpio, part of Lib(X)SVF (http://www.clifford.at/libxsvf/).\n");
  //fprintf(stderr, "Copyright (C) 2009  RIEGL Research ForschungsGmbH\n");
  //fprintf(stderr, "Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>\n");
  //fprintf(stderr, "Lib(X)SVF is free software licensed under the ISC license.\n");  
  //fprintf(stderr, "Modified for use in Apollo platform by Michael Kremer, kremerme@bu.edu\n\n"); //Mike

  //open SVF file
  svfFile = fopen(svfFileName.c_str(),"rb"); //swith to take path in
  if (NULL == svfFile ) {
    throw std::runtime_error("failed to open svf file");    
  }
  
  //set Tap State
  tap_state = LIBXSVF_TAP_INIT;

  int nUIO = label2uio(XVCLabel);

  size_t const uioFileNameLength = 1024;
  char * uioFileName = new char[uioFileNameLength+1];
  memset(uioFileName,0x0,uioFileNameLength+1);
  snprintf(uioFileName,uioFileNameLength,"/dev/uio%d",nUIO);
  printf("Found UIO labeled %s @ %s\n",XVCLabel.c_str(),uioFileName);
  //Run setup
  fdUIO = open(uioFileName,O_RDWR);
  delete [] uioFileName;
  if (fdUIO < 0) {
    throw std::runtime_error("Failed to open UIO device");    
  }
  
  jtag_reg = (sXVC volatile*) mmap(NULL,sizeof(sXVC),
				   PROT_READ|PROT_WRITE, MAP_SHARED,
				   fdUIO, 0x0);
  if(MAP_FAILED == jtag_reg){
    throw std::runtime_error("mem map failed");
  }


  
  //Run svf player
  printf("Reading svf file...\n");
  int rc = svf_reader();
  tap_walk(LIBXSVF_TAP_RESET); //Reset tap
  
  //Run shutdown
  if (shutdown() < 0) {
    throw std::runtime_error("Shutdown of JTAG interface failed.");
  }
  return rc;
}

SVFPlayer::SVFPlayer() {
  jtag_reg = NULL;
  svfFile = NULL;
  
}
