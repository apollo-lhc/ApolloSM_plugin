#ifndef __UIO_LABEL_FINDER_HH__
#define __UIO_LABEL_FINDER_HH__
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <string.h>
#include <boost/filesystem.hpp>

using namespace boost::filesystem;

static size_t ReadFileToBuffer(std::string const & fileName,char * buffer,size_t bufferSize){
  //open the file
  FILE * inFile = fopen(fileName.c_str(),"r");
  if(NULL == inFile){
    //failed to opn file.
    return 0;
  }

  //read file  
  if(fgets(buffer,bufferSize,inFile) == NULL){
    fclose(inFile);
    return 0;
  }
  fclose(inFile);
  return 1;
}

uint64_t SearchDeviceTree(std::string const & dvtPath,std::string const & name){
  uint64_t address = 0;
  FILE *labelfile=0;
  char label[128];
  // traverse through the device-tree   
  for (directory_iterator x(dvtPath); x!=directory_iterator(); ++x){
    if (!is_directory(x->path()) ||
	!exists(x->path()/"label")) {
      continue;
    }
    labelfile = fopen((x->path().native()+"/label").c_str(),"r");
    fgets(label,128,labelfile);
    fclose(labelfile);

    if(!strcmp(label, name.c_str())){
      //Get endpoint AXI address from path           
      // looks something like LABEL@DEADBEEFXX       
      std::string stringAddr=x->path().filename().native();

      //Check if we find the @          
      size_t addrStart = stringAddr.find("@");
      if(addrStart == std::string::npos){
	fprintf(stderr,"directory name %s has incorrect format. Missing \"@\" ",x->path().filename().native().c_str() );
	break; //expect the name to be in x@xxxxxxxx format for example myReg@0x41200000    
      }
      //Convert the found string into binary         
      if(addrStart+1 > stringAddr.size()){
	fprintf(stderr,"directory name %s has incorrect format. Missing size ", x->path().filename().native().c_str() );
	break; //expect the name to be in x@xxxxxxxx format for example myReg@0x41200000    

      }
      stringAddr = stringAddr.substr(addrStart+1);

      //Get the names's address from the path (in hex)            
      address = std::strtoull(stringAddr.c_str() , 0, 16);
      break;
    }
  }
  return address;
}



// A function that takes a uio label and returns the uio number
int label2uio(std::string ilabel)
{
  size_t const bufferSize = 1024;
  char * buffer = new char[bufferSize];
  memset(buffer,0x0,bufferSize);

  bool foundValidMatch = false;
  uint64_t dtEntryAddr=0, uioEntryAddr=0;
  // search through the file system to see if there is a uio that matches the name
  std::string const uiopath = "/sys/class/uio/";
  std::string       dvtpath = "/proc/device-tree/";

  //Search through all amba paths
  for (directory_iterator itDVTPath(dvtpath); 
       itDVTPath!=directory_iterator();
       ++itDVTPath){
    //Check that this is a path with amba in its name
    if ((!is_directory(itDVTPath->path())) || 
	(itDVTPath->path().string().find("amba")==std::string::npos) ) {
      continue;
    }else{
      dtEntryAddr=SearchDeviceTree(itDVTPath->path().string(),ilabel);
      if(dtEntryAddr != 0){
	//we found the correct entry
	break;
      }
    }
  }

  //check if we found anything  
  //Check if we found a device with the correct name
  if(dtEntryAddr==0) {
    std::cout<<"Cannot find a device that matches label "<<(ilabel).c_str()<<" device not opened!" << std::endl;
    return -1;
  }


  // Traverse through the /sys/class/uio directory
  for (directory_iterator itDir(uiopath); itDir!=directory_iterator(); ++itDir){
    //same kind of search as above, just looking for a uio dir with maps/map0/addr and maps/map0/size
    if (!is_directory(itDir->path())) {
      continue;
    }
    if (!exists(itDir->path()/"maps/map0/addr")) {
      continue;
    }
    if (!exists(itDir->path()/"maps/map0/size")) {
      continue;
    }

    //process address of UIO entry
    if(!ReadFileToBuffer((itDir->path()/"maps/map0/addr").native(),buffer,bufferSize)){
      //bad read
      continue;
    }
    uioEntryAddr = std::strtoul(buffer, 0, 16);

    // see if the UIO address matches the device tree address
    if (dtEntryAddr == uioEntryAddr){
      if(!ReadFileToBuffer((itDir->path().native()+"/maps/map0/size"),buffer,bufferSize)){
	//bad read
	continue;
      }
      
      //the size was in number of bytes, convert into number of uint32
      //size_t size=std::strtoul( buffer, 0, 16)/4;  
      strcpy(buffer,itDir->path().filename().native().c_str());
      foundValidMatch = true;
      break;
    }
  }

  //Did we find a 
  if (!foundValidMatch){
    return -1;
  }
  
  char* endptr;
  int uionumber = strtol(buffer+3,&endptr,10);
  if (uionumber <0){
    return uionumber;
  }
  return uionumber;
}
#endif
