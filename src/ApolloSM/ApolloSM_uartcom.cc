#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/ApolloSM_Exceptions.hh>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/select.h>
#include <termios.h>
#include <algorithm>


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

static void SetupTermIOS(int fd){
  struct termios term_opts;  

  tcgetattr(fd,&term_opts); //get existing options
  cfsetispeed(&term_opts,B115200); //set baudrate
  cfsetospeed(&term_opts,B115200); //set baudrate
  term_opts.c_cflag |= CLOCAL;  //Do not change owner of port
  term_opts.c_cflag |= CREAD;   // enable receiver

  //Set the data size to 8
  term_opts.c_cflag &= ~CSIZE;
  term_opts.c_cflag |= CS8;

  term_opts.c_iflag &= ~(INLCR | IGNCR | ICRNL);

  //set parity
  term_opts.c_cflag &= ~PARENB;
  term_opts.c_cflag &= ~CSTOPB;

  //disable hardware flow control
  term_opts.c_cflag &= ~CRTSCTS;
  term_opts.c_iflag &= ~(IXON | IXOFF | IXANY);

  //set raw mode
  term_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  term_opts.c_oflag &= ~OPOST;


  tcsetattr(fd,TCSANOW,&term_opts); 

}



// The function where all the talking to and reading from command module happens
void ApolloSM::UART_Terminal(std::string const & ttyDev) {  
  // ttyDev descriptor
  int fd = open(ttyDev.c_str(), O_RDWR);
  if(-1 == fd){
    BUException::IO_ERROR e;
    e.Append("Unable to open device " + ttyDev + "\n");
    throw e;    
  }

  //Setup the termios structures
  SetupTermIOS(fd);

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


  //maxfdp1 is the max fd plus 1
  int maxfdp1 = std::max(fd,std::max(STDIN_FILENO,STDIN_FILENO));
  maxfdp1++;
  fd_set readSet;
  fd_set writeSet; 
  FD_ZERO(&readSet); // Zero out
  FD_ZERO(&writeSet); // Zero out

  //Set read mask
  FD_SET(fd, &readSet);
  FD_SET(STDIN_FILENO, &readSet);

  FD_SET(fd, &writeSet);
  FD_SET(STDOUT_FILENO, &writeSet);


  while(interactiveLoop) {
    
    fd_set rSetCopy = readSet;
    fd_set wSetCopy = writeSet;
    int ret_psel = pselect(maxfdp1,&rSetCopy,&wSetCopy,NULL,NULL,&(sa.sa_mask));
    
    if(ret_psel > 0){
      if(FD_ISSET(fd,&rSetCopy) && FD_ISSET(STDOUT_FILENO,&wSetCopy)){
      	char outChar;
	int ret = read(fd,&outChar,1);
	if(1 == ret){
	  printf("%c",outChar);
	  fflush(stdout);
	}
      }else if(FD_ISSET(STDIN_FILENO,&rSetCopy) && FD_ISSET(fd,&wSetCopy)){
	char userInput;
	int ret;
	ret = read(STDIN_FILENO,&userInput,1);
	if(1 == ret){	
	  if(29 == userInput){
	    interactiveLoop = false;
	    continue;	
	  }else if (13 == userInput){
	    write(fd,&userInput,1);
	  }else if (10 == userInput){	
	    write(fd,&userInput,1);
	  }else if (127 == userInput){
	    char const BS = 8;
	    write(fd,&BS,1);
	    printf("%c ",8); //Draw backspace by backspace, space, (backspace from remote echo)
	  }else{
	    write(fd,&userInput,1);
	  }
	}
      }
    }else{
      interactiveLoop = false;
      continue;
    }  
  }
  printf("Closing command module comm...\n");

  // Restore old action of pressing Ctrl-C before returning (which is to kill the program)
  sigaction(SIGINT, &oldsa, NULL);

  // Leave curses mode
  endwin();
  printf("\n");
  fflush(stderr);
  close(fd);
  return;
}

std::string ApolloSM::UART_CMD(std::string const & ttyDev, std::string sendline, char const promptChar) {  
  // ttyDev descriptor
  int fd = open(ttyDev.c_str(), O_RDWR);
  if(-1 == fd){
    BUException::IO_ERROR e;
    e.Append("Unable to open device " + ttyDev + "\n");
    throw e;    
  }

  //Setup the termios structures
  SetupTermIOS(fd);


  // ==================================================
  // For pselect
  // The biggest ttyDev descriptor number plus one (hence p1)
  // There is only one ttyDev descriptor number so fd + 1
  int maxfdp1 = fd + 1;

  // Set of all ttyDev descriptors to be read/write from, has only one element: fd
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
  t.tv_sec = 0; // 0 seconds
  t.tv_nsec = 200000000; // 200 milliseconds
  // ==================================================
  
  char readChar;
  char last = 0;

  std::string recvline;

  //Press enter
  char enter[] = "\r";
  write(fd, enter,strlen(enter));
  write(fd, enter,strlen(enter));
  usleep(10000);


  // ==================================================
  //make sure the buffer is clear  
  bool readNotTimedOut = 1;
  while(readNotTimedOut) {
    // make copy of readSet everytime pselect is used because we don't want contents of readSet changed
    fd_set readSetCopy = readSet;
    int returnVal;
    // Pselect returns 0 for time out, -1 for error, or number of ttyDev descriptors ready to be read
    if(0 == (returnVal = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL)))  {
      // timed out
      readNotTimedOut = 0;
      continue;
    } else if(-1 == returnVal) {
      BUException::IO_ERROR e;
      e.Append("read error: error from pselect while clearing buffer for " + ttyDev + "\n");
      throw e;
    } else {
      // Do nothing with the character read
      if(read(fd, &readChar, sizeof(readChar)) < 0) {
	BUException::IO_ERROR e;
	e.Append("read error: error reading from " + ttyDev + "\n");
	throw e;
      }
    }
  }
  

  // ==================================================  
  // Remove trailing carriage and line feed from message
  //remove any '\n's
  size_t pos = std::string::npos;
  while((pos = sendline.find('\n')) != std::string::npos){
    sendline.erase(sendline.begin()+pos);
  }
  //remote any '\r's
  while((pos = sendline.find('\r')) != std::string::npos){
    sendline.erase(sendline.begin()+pos);
  }

  // ==================================================  
  //Write the command
  // Write one byte and then immediately read one byte to make sure they are the same
  for(size_t i = 0; i < sendline.size();i++){
    //Wait for write buffer in ttyDev descriptor to be not full
    // make copy of wroteSet everytime pselect is used because we don't want contents of writeSet changed
    fd_set writeSetCopy = writeSet;
    int returnValw;
    // Pselect returns 0 for time out, -1 for error, or number of ttyDev descriptors ready to be written to
    if(0 == (returnValw = pselect(maxfdp1, NULL, &writeSetCopy, NULL, &t, NULL)))  {
      // If the buffer is full for more than 5 seconds something is probably wrong
      BUException::IO_ERROR e;
      e.Append("pselect timed out writing to " + ttyDev + " in 5 seconds\n");
      throw e;
    } else if(-1 == returnValw) {
      BUException::IO_ERROR e;
      e.Append("write error: error from pselect while waiting for writer buffer in " + ttyDev + " to clear\n");
      throw e;
    } else {
      // Write char
      char writeChar[1];
      std::size_t writeCharLen = 1;
      writeChar[0] = sendline[i];
      if(write(fd, writeChar, writeCharLen) < 0) {
	BUException::IO_ERROR e;
	e.Append("write error: error writing to " + ttyDev + "\n");
	throw e;
      }

      //wait for character to be echoed back
      // make copy of readSet everytime pselect is used because we don't want contents of readSet changed
      fd_set readSetCopy = readSet;
      int returnValr;
      // Pselect returns 0 for time out, -1 for error, or number of file descriptors ready to be read
      if(0 == (returnValr = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL)))  {
	// If it takes longer than 5 seconds for a character to be echoed back something is probably wrong
	BUException::IO_ERROR e;
	e.Append("pselect timed out while polling " + ttyDev + " for echoed command\n");
	throw e;
      } else if(-1 == returnValr) {
	BUException::IO_ERROR e;
	e.Append("read error: error from pselect polling " + ttyDev + " for echoed command\n");
	throw e;
      } else {
	// make sure echoed character is same as sent character
	if(read(fd, &readChar, sizeof(readChar)) < 0) {
	  BUException::IO_ERROR e;
	  e.Append("read error: error reading echoed command from " + ttyDev + "\n");
	  throw e;
	}
      }
      
      if(sendline[i] != readChar){
	printf("Error: mismatched character %c %c\n",sendline[i],readChar);
	printf("Error: mismatched character %c %d\n",sendline[i],readChar);
	return "Mismatched character\n";
      }

    }
    

    
  }

  //Press enter
  write(fd, enter, strlen(enter));

  // read until prompt character
  readNotTimedOut = 1;
  while(readNotTimedOut) {
    fd_set readSetCopy = readSet;
    int returnVal;
    if(0 == (returnVal = pselect(maxfdp1, &readSetCopy, NULL, NULL, &t, NULL))) {
      // timedout
      readNotTimedOut = 0;
      continue;
    } else if (-1 == returnVal) {
      BUException::IO_ERROR e;
      e.Append("pselect error while polling read fd from " + ttyDev + " after command was sent and echoed\n");
      throw e;
    } else {
      if(0 > read(fd, &readChar, sizeof(readChar))) {
	BUException::IO_ERROR e;
	e.Append("read error: error reading from " + ttyDev + " after command was sent and echoed\n");
	throw e;
      }
    }
    // Currently, we tell the difference between a regular '>' and the prompt character, also '>',
    // by checking if the previous character was a newline
    if((promptChar == readChar) && (('\n' == last) || ('\r' == last))) {
      break;
    }
    last = readChar;
    if(readChar != '\r'){
      recvline.push_back(readChar);
    }
  }
  close(fd);

  return recvline;
}
