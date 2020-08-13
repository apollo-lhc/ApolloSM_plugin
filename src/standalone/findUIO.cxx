#include <ApolloSM/uioLabelFinder.hh>

#include <iostream>

int main(int argc, char ** argv){
  if(argc < 2){
    printf("Usage: %s uio_label\n",argv[0]);
    return 1;
  }
  int uio = label2uio(argv[1]);
  if(uio >= 0){
    printf("%s is at /dev/uio%d\n",argv[1],uio);
  }else{
    printf("%s not found\n",argv[1]);
  }
  return 0;
}
