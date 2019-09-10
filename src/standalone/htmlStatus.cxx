#include <stdio.h>
#include <ApolloSM/ApolloSM.hh>
#include <vector>
#include <string>

//TCLAP parser
#include <tclap/CmdLine.h>

int main(int argc, char** argv) {

  
  std::string file;
  size_t verbosity;
  std::string type;
  std::string connection_file;

  try {
    TCLAP::CmdLine cmd("Apollo XVC.",
		       ' ',
		       "XVC");
    TCLAP::ValueArg<std::string> conn_file("c", //one char flag
					  "connection_file", // full flag name
					  "connection file", //description
					  true, //required
					  std::string(""), //Default
					  "string", //type
					  cmd);


    TCLAP::ValueArg<std::string> filename("f", //one char flag
					  "file", // full flag name
					  "HTML table file", //description
					  true, //required
					  std::string("index.html"), //Default
					  "string", //type
					  cmd);

    TCLAP::ValueArg<size_t> level("l", //one char flag
				  "verbosity", //full flag name
				  "level sets verbosity for the HTML table, 1-9", //description
				  false, //Not Required
				  1, //Default
				  "size_t", //type
				  cmd);

    TCLAP::ValueArg<std::string> bare("t", //one char flag
				      "type", //full flag name
				      "Option to set HTML as bare", //description
				      false, //Not required 
				      std::string("HTML"), //Default
				      "string", //type
				      cmd);
    
    //Parse the command line arguments
    cmd.parse(argc, argv);
    file = filename.getValue();
    verbosity = level.getValue();
    type = bare.getValue();
    connection_file = conn_file.getValue();
    fprintf(stderr, "running: verbosity=%d filename=\"%s\"\n", verbosity, file.c_str());
    

  }catch (TCLAP::ArgException &e) {
    fprintf(stderr, "Failed to Parse Command Line, running default: verbosity=1 filename=\"index.html\"\n");
    return -1;
  }


  //Create ApolloSM class
  ApolloSM * SM = NULL;
  SM = new ApolloSM();
  std::vector<std::string> arg;
  arg.push_back(connection_file);
  SM->Connect(arg);

  std::string strOut;
  //Generate HTML Status
  strOut = SM->GenerateHTMLStatus(file, verbosity, type);

  //Close ApolloSM and END
  if(NULL != SM) {delete SM;}
  return 0;
}
