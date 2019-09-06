#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <vector>
#include <string>

//TCLAP parser
#include <tclap/CmdLine.h>

int main(int, char**) {

  //Create ApolloSM class & connect
  ApolloSM * SM = NULL;
  SM = new ApolloSM();
  std::vector<std::string> arg;
  arg.push_back("connections.xml");
  SM->Connect(arg);

  SM->GenerateHTMLStatus(1,"index.html");
  
  //Close ApolloSM class and END
  if(NULL != SM) {
    delete SM;
  }
  return 0;
}

/*
TCLAP::CmdLine cmd("Apollo XVC.",
		       ' ',
		       "XVC");
   
    //connection file
    TCLAP::ValueArg<std::string> connFile("c",              //one char flag
					  "connection",      // full flag name
					  "xml connection file",//description
					  true,            //required
					  std::string(""),  //Default is empty
					  "string",         // type
					  cmd);
    // XVC name base
    TCLAP::ValueArg<std::string> xvcPreFix("v",              //one char flag
					       "xvc",      // full flag name
					       "xvc prefix",//description
					       true,            //required
					       std::string(""),  //Default is empty
					       "string",         // type
					       cmd);

    // port number
    TCLAP::ValueArg<int> xvcPort("p",              //one char flag
				 "port",      // full flag name
				 "xvc port number",//description
				 true,            //required
				 -1,  //Default is empty
				 "int",         // type
				 cmd);
*/
