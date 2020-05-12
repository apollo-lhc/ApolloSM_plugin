#include <standalone/EyeScanLink.hh>
//#include <standalone/FPGA.hh>
#include <stdlib.h> // atof, atoi
#include <string>
#include <vector>
#include <boost/program_options.hpp>
//#include <sstream>
#include <ApolloSM/ApolloSM.hh>
#include <syslog.h>
//#include <standalone/daemon.hh> // checkNode
#include <map>
#include <math.h>
#include <ApolloSM/eyescan.hh>

#define RUNPYTHON "python /var/www/lighttpd/eyescan.py"

// This chart was found in the ultrascale plus (USP) data sheet
// https://www.xilinx.com/support/documentation/user_guides/ug578-ultrascale-gty pg 231
// The 7 series datasheet doesn't have one. We should find it
// Both the 7 series and USP are 32 bit bus width. The corresponding map is as follows
std::map<uint32_t, double> static const precisionMap =
  {
    {1 , pow(10, -6)},
    {4 , pow(10, -7)},
    {7 , pow(10, -8)},
    {11, pow(10, -9)},
    {14, pow(10, -10)},
    {17, pow(10, -11)},
    {21, pow(10, -12)},
    {24, pow(10, -13)},
    {27, pow(10, -14)},
    {31, pow(10, -15)}
  };

// ==================================================

#define MAXALLOWEDPRESCALE 31

EyeScanLink::EyeScanLink(std::string linkName, boost::program_options::parsed_options PO) {

  // initialize variables to defaults
  this->name        = linkName;
  this->phase       = 0.02;
  this->voltage     = 2;
  this->outfile     = linkName + ".txt";
  this->png         = linkName + ".png";
  this->maxPrescale = 3;
    
  // Total number of options received from config file
  int numOptions = PO.options.size();

  // Look for all options related to this eye scan link (ie. PHASE, VOLTAGE, OUTFILE, MAXPRESCALE, etc.)
  for(int i = 0; i < numOptions; i++) {
    std::string nextOption = PO.options[i].string_key;

    if(0 == nextOption.compare(linkName + ".PHASE"))            // ex: C2C1_PHY.PHASE
      {							        
	// found value for phase			        
	this->phase = atof(PO.options[i].value[0].c_str());		        
      }							        
    else if(0 == nextOption.compare(linkName + ".VOLTAGE"))     // ex: C2C1_PHY.VOLTAGE
      {							        
	// found value for voltage			        
	this->voltage = atoi(PO.options[i].value[0].c_str());		        
      }							        
    else if(0 == nextOption.compare(linkName + ".OUTFILE"))     // ex: C2C1_PHY.OUTFILE
      {
	// found data file name
	this->outfile = PO.options[i].value[0].c_str();
      } // the python code currently dictates the name of the png so I won't attempt to get it from the config file. This can be changed 
    else if(0 == nextOption.compare(linkName + ".MAXPRESCALE")) // ex: C2C1_PHY.PRESCALE
      {
	// found max prescale 
	this->maxPrescale = atoi(PO.options[i].value[0].c_str());
	// Don't go pass the max allowed prescale
	if(this->maxPrescale > MAXALLOWEDPRESCALE) {
	  syslog(LOG_INFO, "Specified max prescale is greater than 31, the max allowed prescale. Using 31\n");
	  this->maxPrescale = MAXALLOWEDPRESCALE;
	}
      }
  }
}

EyeScanLink::~EyeScanLink() {
}

void EyeScanLink::printInfo() {
  // log all relevant information about this link
  syslog(LOG_INFO, "Found EyeScanLink: %s with info:\n", (this->name).c_str());
  syslog(LOG_INFO, "   phase: %.3f\n"       , this->phase);
  syslog(LOG_INFO, "   voltage: %d\n"       , this->voltage);
  syslog(LOG_INFO, "   data file name: %s\n", (this->outfile).c_str());
  syslog(LOG_INFO, "   max prescale: %d\n"  , this->maxPrescale);
  syslog(LOG_INFO, "\n\n");
}

// Set up all attributes for an eye scan
void EyeScanLink::enableEyeScan(ApolloSM * SM) {
  // 1 is the prescale. EnableEyeScan doesn't actually need this argument anymore since
  // the prescales are now dynamically adjusted up to a max
  SM->EnableEyeScan(this->name, 1);
}

// // Set horizontal and vertical 
// void setOffsets() {
// 
// }

void EyeScanLink::scan(ApolloSM * SM) {
  // empty the vector
  (this->esCoords).clear();
  // perform scan  
  this->esCoords = SM->EyeScan(this->name, this->phase, this->voltage, this->maxPrescale);

}

// This is more like a typedef than a namespace
// namespace plt = matplotlibcpp;

void EyeScanLink::plot() {
  double precision = pow(10, -6); // Default prescale is 3 so we round down to 1
  // The max prescale gives our precision.
  // Not every prescale has a precision. Only every 3rd or 4th do.
  // If we can't find a precision, we decrement the prescale to continue
  // searching. We decrement instead of increment because we are 
  // conservative about the precision. We decrement by at most i = 3
  // because, if you look at the map, any prescale not on there
  // is guaranteed to have a prescale with a defined precision 
  // at most 3 steps lower (or higher but we're decrementing).
  for(uint32_t i = 0; i <= 3; i++) {
    // log something
    syslog(LOG_INFO, "precision search\n");
    // does this even work?
    if(precisionMap.find((this->maxPrescale)-i) != precisionMap.end()) {
      precision = precisionMap.find((this->maxPrescale)-i)->second;
      syslog(LOG_INFO, "precision found\n");
      break;
    }
    //       log something
    syslog(LOG_INFO, "precision not found\n");
  }

  // precision is the lowest value BER that we could have possibly
  // with 99.5% confidence calculated from a scan. Any lower, zero for
  // example, should be regarded as this lowest value 
  for(size_t i = 0; i < esCoords.size(); i++) {
    if(esCoords[i].BER < precision) {
      esCoords[i].BER = precision;
    }
  }

  // Since we're not using an external python plotting script
  // we don't really need to write the data to file.
  // We can keep this code for debugging
  FILE * dataFile = fopen((this->outfile).c_str(), "w");
  //  printf("\n\n\n\n\nThe size of esCoords is: %d\n", (int)esCoords.size());
  
  // write data/BERs to file
  for(size_t i = 0; i < esCoords.size(); i++) {
    fprintf(dataFile, "%.9f ", esCoords[i].phase);
    fprintf(dataFile, "%d "  , esCoords[i].voltage);
    fprintf(dataFile, "%f "  , esCoords[i].BER);
    fprintf(dataFile, "%x "  , esCoords[i].voltageReg & 0xFF);
    fprintf(dataFile, "%x\n" , esCoords[i].phaseReg & 0xFFF);
  }
  
  fclose(dataFile);

  // run the python script like a terminal command
  //  std::string pythonCommand = 
  syslog(LOG_INFO, "running command: %s\n", (std::string(RUNPYTHON) + std::string(" ") + this->outfile).c_str());
  system((std::string(RUNPYTHON) + std::string(" ") + this->outfile).c_str());
  // move the png to /var/www/lighttpd for uploading
  // does C++ block until python has finished outputting the png?
  syslog(LOG_INFO, "running command: %s\n", (std::string("cp ") + this->png + std::string(" /var/www/lighttpd")).c_str());;
  system((std::string("cp ") + this->png + std::string(" /var/www/lighttpd")).c_str());

//  // Since we don't have python's np.unique, this is the best we can do for now
//  // We are making a vector of the voltages and phases where each phase
//  // and voltage appears only once (unlike in esCoords). I use double since
//  // matplotlib takes doubles. Not sure if it takes other types
//  std::vector<double> voltages;
//  for(double voltage = -127; voltage <= 127; voltage+=(double)(this->voltage)) {
//    voltages.push_back(voltage)
//  }
//  std::vector<int> phases;
//  for(double phase = -0.5; phase <= 0.5; phase+=(this->phase)) {
//    phases.push_back(phase);
//  }
//  
//  // This is the equivalent of python's np.meshgrid
//  std::vector<std::vector<double> > phaseMesh;
//  for(size_t i = 0; i < voltage.size(); i++) {
//    phaseMesh.push_back(phases);
//  }
//  std::vector<std::vector<double> > voltageMesh;
//  for(size_t i = 0; i < phase.size(); i++) {
//    voltageMesh.push_back(voltages);
//  }
//  
//  plt::backend("Agg");
//  // Set the size of output image to 1200x780 pixels
//  plt::figure_size(1200, 780);
//  // Plot line from given x and y data. Color is selected automatically.
//  plt::plot(x, y);
//  // Plot a red dashed line from given x and y data.
//  plt::plot(x, w,"r--");
//  // Plot a line whose name will show up as "log(x)" in the legend.
//  plt::named_plot("log(x)", x, z);
//  // Set x-axis to interval [0,1000000]
//  plt::xlim(0, 1000*1000);
//  // Add graph title
//  plt::title("Sample figure");
//  // Enable legend.
//  plt::legend();
//  // Save the image (file format is determined by the extension)
//  plt::save("./basic.png");
}
