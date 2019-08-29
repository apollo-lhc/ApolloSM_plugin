#include <ApolloSM/ApolloSM.hh>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>

//Definitions
#define STDIN 0

// For Ctrl-C handling
//---------------------------------------------------------------------------
bool volatile interactiveLoop;

void static signal_handler(int const signum) {
  if(SIGINT == signum) {
    printf("\n");
    interactiveLoop = false;
  }
  return;
}

//---------------------------------------------------------------------------
bool SetNonBlocking(int &fd, bool value) {
  // Get the previous flags
  int currentFlags = fcntl(fd, F_GETFL, 0);
  if(currentFlags < 0) {return false;}

  // Make the socket non-blocking
  if(value) {currentFlags |= O_NONBLOCK;}
  else {currentFlags &= ~O_NONBLOCK;}

  int currentFlags2 = fcntl(fd, F_SETFL, currentFlags);
  if(currentFlags2 < 0) {return false;}

  return(true);
}



// The function where all the talking to and reading from command module happens
void ApolloSM::UART_Terminal(std::string baseNode) {
  baseNode.append(".UART.");

  //get nodes for read/write
  uhal::Node const & nRD_FIFO_FULL = GetNode(baseNode+"RD_FIFO_FULL");
  uhal::Node const & nRD_VALID     = GetNode(baseNode+"RD_VALID");
  uhal::Node const & nRD_DATA      = GetNode(baseNode+"RD_DATA");
  uhal::Node const & nWR_DATA      = GetNode(baseNode+"WR_DATA");;

  // To catch Ctrl-C and break out of talking through SOL
  struct sigaction sa;
  
  // To restore old handling of Ctrl-C
  struct sigaction oldsa;

  // Instantiate sigaction struct member with signal handler function
  sa.sa_handler = signal_handler;
  // Catch SIGINT and pass to function signal_handler within sigaction struct sa
  sigaction(SIGINT, &sa, &oldsa);
  interactiveLoop = true;

  printf("Opening command module comm...\n");
  printf("Press Ctrl-] to close\n");


  // Enter curses mode
  initscr();
  cbreak();
  noecho();
  timeout(0);

  try{
    while(interactiveLoop) {
    
      if(RegReadNode(nRD_FIFO_FULL)) {printf("Buffer full\n");} 

      // read data from UART (keep this FIFO empty)
      while(RegReadNode(nRD_VALID)) { 
	char outChar = RegReadNode(nRD_DATA);
	printf("%c",outChar);
	if(10 == outChar){
	  printf("%c",13);
	}
	RegWriteNode(nRD_VALID,1);
      }
      fflush(stdout);

      //Read from user
      int userInput = getch();
      if(ERR != userInput){
	if(29 == userInput){
	  interactiveLoop = false;
	  continue;	
	}else if (10 == userInput){	
	  RegWriteNode(nWR_DATA,13);
	  RegWriteNode(nWR_DATA,10);
	}else if (127 == userInput){
	  RegWriteNode(nWR_DATA,8); //bs
	  printf("%c ",8); //Draw backspace by backspace, space, (backspace from remote echo)
	}else{
	  RegWriteNode(nWR_DATA,0xFF&userInput);
	}
      }
    }
  }catch (std::exception &e){}

  printf("Closing command module comm...\n");

  // Restore old action of pressing Ctrl-C before returning (which is to kill the program)
  sigaction(SIGINT, &oldsa, NULL);

  // Leave curses mode
  endwin();
  printf("\n");
  fflush(stdout);

  return;
}

std::string ApolloSM::UART_CMD(std::string baseNode, std::string sendline, char const promptChar) {
  baseNode.append(".UART.");
  
  //get nodes for read/write
  uhal::Node const & nWR_FIFO_FULL = GetNode(baseNode+"WR_FIFO_FULL");
  uhal::Node const & nRD_DATA      = GetNode(baseNode+"RD_DATA");
  uhal::Node const & nWR_DATA      = GetNode(baseNode+"WR_DATA");
  uhal::Node const & nRD_VALID     = GetNode(baseNode+"RD_VALID");

  char readChar;
  char last = 0;

  std::string recvline;

  //Press enter
  RegWriteNode(nWR_DATA,13);
  RegWriteNode(nWR_DATA,10);
  usleep(10000);

  //remove any '\n's
  size_t pos = std::string::npos;
  while((pos = sendline.find('\n')) != std::string::npos){
    sendline.erase(sendline.begin()+pos);
  }
  //remote any '\r's
  while((pos = sendline.find('\r')) != std::string::npos){
    sendline.erase(sendline.begin()+pos);
  }

  //make sure the buffer is clear
  while(RegReadNode(nRD_VALID)){
    RegWriteNode(nRD_VALID,1);
  }

  //Write the command
  for(size_t i = 0; i < sendline.size();i++){
    while(RegReadNode(nWR_FIFO_FULL)){} //Wait for FIFO to be not full
    RegWriteNode(nWR_DATA,sendline[i]);
    
    //wait for character to be echoed back
    while(!RegReadNode(nRD_VALID)){
    }
    if(sendline[i] != char(RegReadNode(nRD_DATA))){
      printf("Error: mismatched character %c %c\n",sendline[i],char(RegReadNode(nRD_DATA)));
    }
    RegWriteNode(nRD_VALID,1); //ack this char    
  }
  
  //Press enter
  RegWriteNode(nWR_DATA,13);
  RegWriteNode(nWR_DATA,10);


  // read until prompt character
  int readTries = 1000;
  while(readTries) {
    readTries--;
    // read if there are characters to read
    if(RegReadNode(nRD_VALID)) {
      readChar = 0xFF&RegReadNode(nRD_DATA);
      RegWriteNode(nRD_VALID, 1);
      readTries = 1000;
      // Currently, we tell the difference between a regular '>' and the prompt character, also '>',
      // by checking if the previous character was a newline
      if((promptChar == readChar) && (('\n' == last) || ('\r' == last))) {
	break;
      }
      last = readChar;
      recvline.push_back(readChar);
    }
  }

  return recvline;
}
