#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/select.h>

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
/*
char ApolloSM::readByte() {
  //make sure the buffer is clear  
  bool notTimedOut = 1;
  while(notTimedOut) {
    // make copy of readSet everytime pselect is used because we don't want contents of readSet changed
    fd_set readSetCopy = readSet;
    int returnVal;
    // Pselect returns 0 for time out, -1 for error, or number of file descriptors ready to be read
    if(0 == (returnVal = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL)))  {
      // timed out
      notTimedOut = 0;
      continue;
    } else if(-1 == returnVal) {
      BUException::IO_ERROR e;
      e.Append("read error: error from pselect while clearing buffer for " + file + "\n");
      throw e;
    } else {
      // Do nothing with the character read
      if(read(fd, &readChar, sizeof(readChar)) < 0) {
	BUException::IO_ERROR e;
	e.Append("read error: error reading from " + file + "\n");
	throw e;
      }
    }
  }
  
}

void ApolloSM::writeByte() {
  
}
*/
std::string ApolloSM::UART_CMD(std::string i, std::string, char const) {
  // 1 for CM1, 2 for CM2, 3 for ESM
  std::string file = "/dev/ttyUL" + i;
  
  // file descriptor
  int fd = open(file.c_str(), O_RDWR);

  printf("The file descriptor is: %d\n", fd);

  // ==================================================
  // For pselect
  // The biggest file descriptor number plus one (hence p1)
  // There is only one file descriptor number so fd + 1
  int maxfdp1 = fd + 1;

  // Set of all file descriptors to be read/write from, has only one element: fd
  // These two sets are basically the same thing
  fd_set readSet;
  fd_set writeSet; 
  FD_ZERO(&readSet); // Zero out
  FD_ZERO(&writeSet); // Zero out
  // Put fd in read/writeSet
  FD_SET(fd, &readSet);
  FD_SET(fd, &writeSet);

  // timeout for reading
  struct timespec t;
  t.tv_sec = 0; // seconds
  t.tv_nsec = 100000000; // 100 milliseconds
  // ==================================================
  
  char readChar;
  //  char last = 0;

  std::string recvline;

  //Press enter
  char enter = 0xd;
  //  enter[0] = '\r';
  //  enter[0] = '\n';
  //std::size_t enterLen = 1;
  //  write(fd, enter, enterLen);
  write(fd, &enter,1);
  write(fd, &enter,1);
  usleep(10000);

  printf("First enter sent\n");

  // ==================================================
  //make sure the buffer is clear  
  bool readNotTimedOut = 1;
  while(readNotTimedOut) {
    //    printf("%ld %ld\n", t.tv_sec, t.tv_nsec);
    // make copy of readSet everytime pselect is used because we don't want contents of readSet changed
    fd_set readSetCopy = readSet;
    int returnVal;
    // Pselect returns 0 for time out, -1 for error, or number of file descriptors ready to be read
    if(0 == (returnVal = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL)))  {
      // timed out
      readNotTimedOut = 0;
      continue;
    } else if(-1 == returnVal) {
      BUException::IO_ERROR e;
      e.Append("read error: error from pselect while clearing buffer for " + file + "\n");
      throw e;
    } else {
      // Do nothing with the character read
      if(read(fd, &readChar, sizeof(readChar)) < 0) {
	BUException::IO_ERROR e;
	e.Append("read error: error reading from " + file + "\n");
	throw e;
      }
      printf("%c", readChar);
      fflush(stdout);
    }
  }
  
  printf("Buffer cleared\n");

//////  // ==================================================  
//////  // Remove trailing carriage and line feed from message
//////  //remove any '\n's
//////  size_t pos = std::string::npos;
//////  while((pos = sendline.find('\n')) != std::string::npos){
//////    sendline.erase(sendline.begin()+pos);
//////  }
//////  //remote any '\r's
//////  while((pos = sendline.find('\r')) != std::string::npos){
//////    sendline.erase(sendline.begin()+pos);
//////  }
//////
//////  // ==================================================  
//////  //Write the command
//////  // Write one byte and then immediately read one byte to make sure they are the same
//////  for(size_t i = 0; i < sendline.size();i++){
//////    //Wait for write buffer in file descriptor to be not full
//////    printf("%ld %ld\n", t.tv_sec, t.tv_nsec);
//////    // make copy of wroteSet everytime pselect is used because we don't want contents of writeSet changed
//////    fd_set writeSetCopy = writeSet;
//////    int returnValw;
//////    // Pselect returns 0 for time out, -1 for error, or number of file descriptors ready to be written to
//////    if(0 == (returnValw = pselect(maxfdp1, NULL, &writeSetCopy, NULL, &t, NULL)))  {
//////      // If the buffer is full for more than 5 seconds something is probably wrong
//////      BUException::IO_ERROR e;
//////      e.Append("pselect timed out writing to " + file + " in 5 seconds\n");
//////      throw e;
//////    } else if(-1 == returnValw) {
//////      BUException::IO_ERROR e;
//////      e.Append("write error: error from pselect while waiting for writer buffer in " + file + " to clear\n");
//////      throw e;
//////    } else {
//////      // Write char
//////      char writeChar[1];
//////      std::size_t writeCharLen = 1;
//////      writeChar[0] = sendline[i];
//////      if(write(fd, writeChar, writeCharLen) < 0) {
//////	BUException::IO_ERROR e;
//////	e.Append("write error: error writing to " + file + "\n");
//////	throw e;
//////      }
//////      printf("sent: %c\n", sendline[i]);
//////      printf("sent: %c\n", writeChar[0]);
//////    }
//////    
//////    printf("%ld %ld\n", t.tv_sec, t.tv_nsec);
//////
//////    //wait for character to be echoed back
//////    // make copy of readSet everytime pselect is used because we don't want contents of readSet changed
//////    fd_set readSetCopy = readSet;
//////    int returnValr;
//////    // Pselect returns 0 for time out, -1 for error, or number of file descriptors ready to be read
//////    if(0 == (returnValr = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL)))  {
//////      // If it takes longer than 5 seconds for a character to be echoed back something is probably wrong
//////      BUException::IO_ERROR e;
//////      e.Append("pselect timed out while polling " + file + " for echoed command\n");
//////      throw e;
//////    } else if(-1 == returnValr) {
//////      BUException::IO_ERROR e;
//////      e.Append("read error: error from pselect polling " + file + " for echoed command\n");
//////      throw e;
//////    } else {
//////      // make sure echoed character is same as sent character
//////      if(read(fd, &readChar, sizeof(readChar)) < 0) {
//////	BUException::IO_ERROR e;
//////	e.Append("read error: error reading echoed command from " + file + "\n");
//////	throw e;
//////      }
//////      printf("read: %d\n", readChar);
//////    }
////// 
//////    if(sendline[i] != readChar){
//////      printf("Error: mismatched character %c %c\n",sendline[i],readChar);
//////      printf("Error: mismatched character %c %d\n",sendline[i],readChar);
//////      return "Mismatched character\n";
//////    }
//////    
//////  }
//////
//////  //Press enter
//////  write(fd, enter, enterLen);
//////
//////  printf("Second enter pressed\n");
//////
//////  // read until prompt character
//////  readNotTimedOut = 1;
//////  while(readNotTimedOut) {
//////    fd_set readSetCopy = readSet;
//////    int returnVal;
//////    if(0 == (returnVal = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL))) {
//////      // timedout
//////      readNotTimedOut = 0;
//////      continue;
//////    } else if (-1 == returnVal) {
//////      BUException::IO_ERROR e;
//////      e.Append("pselect error while polling read fd from " + file + " after command was sent and echoed\n");
//////      throw e;
//////    } else {
//////      if(0 > read(fd, &readChar, sizeof(readChar))) {
//////	BUException::IO_ERROR e;
//////	e.Append("read error: error reading from " + file + " after command was sent and echoed\n");
//////	throw e;
//////      }
//////    }
//////    // Currently, we tell the difference between a regular '>' and the prompt character, also '>',
//////    // by checking if the previous character was a newline
//////    if((promptChar == readChar) && (('\n' == last) || ('\r' == last))) {
//////      break;
//////    }
//////    last = readChar;
//////    recvline.push_back(readChar);
//////  }
//////
  return recvline;
}
