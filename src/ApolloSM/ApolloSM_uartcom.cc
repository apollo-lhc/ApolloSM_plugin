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

  // To check number of chars sent
  int i = 0;

  char readChar;
  char last = 0;

  std::string recvline;

  // read until prompt character
  while(true) {
    // read if there are characters to read
    if(RegReadNode(nRD_VALID)) {
      readChar = 0xFF&RegReadNode(nRD_DATA);
      // Currently, we tell the difference between a regular '>' and the prompt character, also '>',
      // by checking if the previous character was a newline
      if((promptChar == readChar) && ('\n' == last)) {
	RegWriteNode(nRD_VALID, 1);
	break;
      }
      last = readChar;
      recvline.push_back(readChar);
      RegWriteNode(nRD_VALID, 1);
    }

    // write if there are characters to write
    // If writebuffer is not half full or full, send one char of message
    if(!RegReadNode(nWR_FIFO_FULL) && ((int)sendline.size() != i)) {
      RegWriteNode(nWR_DATA, sendline[i]);
      i++;
    }
  }

  return recvline;
}
