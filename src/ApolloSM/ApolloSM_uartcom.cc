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
void ApolloSM::UART_Terminal() {
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

  char writeByte;

  int n;

  // Enter curses mode
  initscr();
  cbreak();
  noecho();
  
  //Set STDIN to non-blocking
  int commandfd = STDIN;
  SetNonBlocking(commandfd, true);

  while(interactiveLoop) {
    
    if(RegReadRegister("CM.CM1.UART.RD_FIFO_FULL")) {printf("Buffer full\n");} 

    // read data from UART (keep this FIFO empty)
    while(RegReadRegister("CM.CM1.UART.RD_VALID")) { 
      printf("%c", RegReadRegister("CM.CM1.UART.RD_DATA"));
      fflush(stdout);
      RegWriteRegister("CM.CM1.UART.RD_VALID", 1);
    }

    //Read from user
    n = read(commandfd, &writeByte, sizeof(writeByte));
    if(0 < n) {
      // Group separator (user wants to break out of interactive mode)
      if(29 == writeByte) {
	interactiveLoop = false;
	continue;
      }

      if('\n' == writeByte) {
	RegWriteRegister("CM.CM1.UART.WR_DATA", '\r'); 
      } else {
	RegWriteRegister("CM.CM1.UART.WR_DATA", writeByte); //hw[writeAddr] = writeByte;
      }
    } else {
      //Check errno for EWOULDBLOCK (everything OK) and everythign else (BAD end loop)
    }
  }

  printf("Closing command module comm...\n");

  // Restore old action of pressing Ctrl-C before returning (which is to kill the program)
  sigaction(SIGINT, &oldsa, NULL);

  // Leave curses mode
  endwin();
  printf("\n");
  fflush(stdout);

  return;
}

std::string ApolloSM::UART_CMD(std::string sendline) {
  // To check number of chars sent
  int i = 0;

  char readChar;
  char last = 0;

  std::string recvline;

  // read until prompt character
  while(true) {
    // read if there are characters to read
    //Mike
    if(RegReadRegister("CM.CM1.UART.RD_DATA")) { //if(RD_AVAILABLE & hw[readAddr]) {
      readChar = 0xFF&RegReadRegister("CM.CM1.UART.RD_DATA"); //readChar = 0xFF&hw[readAddr]; 
      // Currently, we tell the difference between a regular '>' and the prompt character, also '>',
      // by checking if the previous character was a newline
      if(('>' == readChar) && ('\n' == last)) {
	RegWriteRegister("CM.CM1.UART.RD_ACK", 1); //hw[readAddr] = ACK;
	break;
      }
      last = readChar;
      recvline.push_back(readChar);
      RegWriteRegister("CM.CM1.UART.RD_ACK", 1); //hw[readAddr] = ACK;
    }

    //Mike
    // write if there are characters to write
    // If writebuffer is not half full or full, send one char of message
    if(!RegReadRegister("CM.CM1.UART.WR_FIFO_FULL") && ((int)sendline.size() != i)) { //if(!(WR_FULL_FULL & hw[writeAddr]) && ((int)sendline.size() != i)) {
      RegWriteRegister("CM.CM1.UART.WR_DATA", sendline[i]); //hw[writeAddr] = sendline[i];
      i++;
    }
  }

  return recvline;
}
