#include <ApolloSM/uioLabelFinder.hh>
#include <string>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>

int main(int argc, char ** argv){
  std::string label;
  uint32_t address;
  uint32_t data = 1;
  char* UIO_DEBUG = getenv("UIO_DEBUG");

  switch (argc){
  case 4:
    //Get data
    data = strtoul(argv[3],NULL,0);
    //Get address
    address = strtoul(argv[2],NULL,0);
    //set UIO label
    label.assign(argv[1]);
    break;
  default:
    printf("Usage: %s uio_label addr data\n",argv[0]);
    return 1;
    break;
  }

  //Find UIO for label
  int uio = label2uio(argv[1]);
  if(uio < 0){
    // try the old version
    if (NULL != UIO_DEBUG) {
      printf("simple UIO finder failed, trying legacy\n");
    }
    uio = label2uio_old(argv[1]);
    if (uio < 0) {
      // at this point, old version has failed.
      fprintf(stderr,"%s not found\n",argv[1]);
      return 1;
    }
  }
  else{
    printf("UIO: %d\n",uio);
  }
  char UIOFilename[] = "/dev/uioXXXXXXXXXX ";
  snprintf(UIOFilename,strlen(UIOFilename),
	   "/dev/uio%d",uio);

  //Open UIO
  int fdUIO = open(UIOFilename,O_RDWR);
  if(fdUIO < 0){
    fprintf(stderr,"Error opening %s\n",label.c_str());
    return 1;
  }

  uint32_t * ptr = (uint32_t *) mmap(NULL,sizeof(uint32_t)*(address+1),
				   PROT_READ|PROT_WRITE, MAP_SHARED,
				   fdUIO,0x0);
  
  if(MAP_FAILED == ptr){
    fprintf(stderr,"Failed to mmap\n");
    return 1;
  }

  ptr[address] = data;
  return 0;
}
