
// #include <string>
// #include "rapidcsv.h"
// #include <fstream>
// #include <vector>
#include "csv_class.hh"


CSV::CSV(){
  //maxPhase = input_maxPhase;
}
CSV::~CSV(){}

void CSV::read(std::string filename){
	rapidcsv::Document doc (filename, rapidcsv::LabelParams(-1,0));
	SW_version = doc.GetRow<std::string>("SW Version")[0];
	gt_type = doc.GetRow<std::string>("GT Type")[0];
	time_start = doc.GetRow<std::string>("Date and Time Started")[0];
	time_end = doc.GetRow<std::string>("Date and Time Ended")[0];
	scan_name = doc.GetRow<std::string>("Scan Name")[0];
	link_settings = doc.GetRow<std::string>("Link Settings")[0];
	reset_rx = doc.GetRow<std::string>("Reset RX After Applying Settings")[0];
	open_area = std::stoi(doc.GetRow<std::string>("Open Area")[0]);
	horz_open = std::stoi(doc.GetRow<std::string>("Horizontal Opening")[0]);
	horz_percent = std::stod(doc.GetRow<std::string>("Horizontal Percentage")[0]);
	vert_open = std::stoi(doc.GetRow<std::string>("Vertical Opening")[0]);
	vert_percent = std::stod(doc.GetRow<std::string>("Vertical Percentage")[0]);
	dwell = doc.GetRow<std::string>("Dwell")[0];
	dwell_ber = std::stod(doc.GetRow<std::string>("Dwell BER")[0]);
	dwell_time = std::stoi(doc.GetRow<std::string>("Dwell Time")[0]);
	horz_inc = std::stoi(doc.GetRow<std::string>("Horizontal Increment")[0]);
	horz_range = doc.GetRow<std::string>("Horizontal Range")[0];
	vert_inc = std::stoi(doc.GetRow<std::string>("Vertical Increment")[0]);
	vert_range = doc.GetRow<std::string>("Vertical Range")[0];
	misc_info = doc.GetRow<std::string>("Misc Info")[0];
	phaseint = doc.GetRow<int>("2d statistical");
	phase_string = doc.GetRow<std::string>("2d statistical");

	std::vector<std::string> col = doc.GetColumn<std::string>(-1);

	for (std::vector<std::string>::iterator i = col.begin(); i != col.end(); ++i)
	{
		if (isNumber((*i)))
		{
			volt.push_back(std::stoi(*i));
			volt_string.push_back((*i));
		}
	}

	for (std::vector<std::string>::iterator i = volt_string.begin(); i != volt_string.end(); ++i)
	{
		std::vector<double> ber_vect = doc.GetRow<double>(*i);
		BER.push_back(ber_vect);
	}

}

void CSV::write(std::string filename){
	FILE * pfile;
  pfile = fopen (filename.c_str(),"w");
  fprintf(pfile,"SW Version,%s\n",SW_version.c_str());
  fprintf(pfile,"GT Type,%s\n", gt_type.c_str());
  fprintf(pfile,"Date and Time Started,%s\n", time_start.c_str());
  fprintf(pfile,"Date and Time Ended,%s\n", time_end.c_str());
  fprintf(pfile, "Scan Name,%s\n",scan_name.c_str());
  fprintf(pfile,"Link Settings,%s\n", link_settings.c_str());
  fprintf(pfile ,"Reset RX After Applying Settings,%s\n", reset_rx.c_str());
  fprintf(pfile,"Open Area,%d\n",open_area);
  fprintf(pfile,"Horizontal Opening,%d\n",horz_open);
  fprintf(pfile,"Horizontal Percentage,%g\n",horz_percent);
  fprintf(pfile,"Vertical Opening,%d\n",vert_open);
  fprintf(pfile,"Vertical Percentage,%g\n",vert_percent);
  fprintf(pfile,"Dwell,%s\n",dwell.c_str());
  fprintf(pfile,"Dwell BER,%g\n",dwell_ber);
  fprintf(pfile,"Dwell Time,%d\n",dwell_time);
  fprintf(pfile,"Horizontal Increment,%d\n",horz_inc);
  fprintf(pfile,"Horizontal Range,%s\n",horz_range.c_str());
  fprintf(pfile,"Vertical Increment,%d\n", vert_inc);
  fprintf(pfile,"Vertical Range,%s\n",vert_range.c_str());
  fprintf(pfile,"Misc Info,%s\n",misc_info.c_str());
  fprintf(pfile,"Scan Start\n");	
  std::string fullphasestring = "2d statistical";
  for (std::vector<std::string>::iterator i = phase_string.begin(); i != phase_string.end(); ++i)
  {
  	fullphasestring= fullphasestring+","+(*i);
  }
  fprintf(pfile,"%s\n",fullphasestring.c_str());
  for (int i = 0; i < BER.size(); ++i)
  {
  	fprintf(pfile, "%s",volt_string[i].c_str());
  	//std::string BER_string = volt_string[i];
  	for (std::vector<double>::iterator j = BER[i].begin(); j != BER[i].end(); ++j)
  	{
  		fprintf(pfile, ",%g",(*j));
  		//BER_string= BER_string+","+std::to_string(*j);
  	}
  	fprintf(pfile, "\n");
  	//myfile<<std::string(BER_string)+"\n";
  }
  fprintf(pfile,"Scan End\n");

}

std::vector<boost::tuple<double, double, double>> CSV::plot_out(){
	std::vector<boost::tuple<double, double, double> > pts;

	for (int i = 0; i < volt.size(); ++i)
	{
		for (int j = 0; j < phaseint.size(); ++j)
		{
			double ui;
			double phasetoui = MAXUI/((double)maxPhase);
			ui=(double)phaseint[j]* phasetoui;

			pts.push_back(boost::make_tuple(ui,(double)volt[i],BER[i][j]));
		}
	}

	return pts;
}


  

void CSV::fill(std::vector<eyescan::eyescanCoords> Coords, std::string filename){
  std::set<int> volt_set;
  std::set<int> phase_set;
  SW_version = "eyescan";
  scan_name = filename.c_str();
  std::set<eyescanCoords> Coords_set;

  for (std::vector<eyescan::eyescanCoords>::iterator i = Coords.begin(); i != Coords.end(); ++i)
    {
      if (volt_set.find((*i).voltageInt)==volt_set.end();)
	{
	  volt_set.insert((*i).voltageInt);
	}

      if (phase_set.find((*i).voltageInt)==volt_set.end();)
	{
	  phase_set.insert((*i).phaseInt);
	}
      Coords_set.insert((*i));
    }

  for (std::set<int>::iterator i = volt_set.begin(); i != volt_set.end(); ++i)
    {
      volt.push_back((*i));
      volt_string.push_back(std::to_string(*i));

    }
  for (std::set<int>::iterator i = phase_set.begin(); i != phase_set.end(); ++i)
    {
      phaseint.push_back((*i));
      phase_string.push_back(std::to_string(*i));

    }

  for (int i = 0; i < volt.size(); ++i)
    {
      std::vector<double> ber_vect;
      for (int j = 0; j < phaseint.size(); ++j)
	{
	  int item =  phaseint.size()*i+j;
	  if (Coords[item].phaseInt == phaseint[j] && Coords[item].voltageInt == volt[i])
	    {
	      ber_vect.push_back(Coords[item].BER);
	    } else {
	    throwException("Failed to convert Coords to csv");
	  }
	  BER.push_back(ber_vect);
	}

    }
}

// }

bool CSV::isNumber(const std::string& str)
{
	if((str[0]=='+' || str[0]=='-') && str.length()>1){
		for (int i = 1; i < str.length(); ++i)
		{
			if (std::isdigit(str[i])==false) return false;
		}
		return true;
	}else{
		for (char const &c : str) {
        	if (std::isdigit(c) == 0) return false;
    	}
    	return true;
	}
}
