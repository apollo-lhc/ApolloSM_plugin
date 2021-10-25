#ifndef __CSVCLASS_HH__
#define __CSVCLASS_HH__
//#include <ApolloSM/eyescan_class.hh>
#include <vector>
#include "rapidcsv.h"
#include <fstream>
#include <boost/tuple/tuple.hpp>
#include <ApolloSM/eyescan_class.hh>
#define MAXUI 0.5
#define MINUI -0.5

class CSV{

	public:
		std::string SW_version;
		std::string gt_type;
		std::string time_start;
		std::string time_end;
		std::string scan_name;
		std::string link_settings;
		std::string reset_rx;
		int open_area;
		int horz_open;
		double horz_percent;
		int vert_open;
		double vert_percent;
		std::string dwell;
		double dwell_ber;
		int dwell_time;
		int horz_inc;
		std::string horz_range;
		int vert_inc;
		std::string vert_range;
		std::string misc_info;
		int dimension;
		int maxPhase;

		std::vector<int> phaseint;
		std::vector<int> volt;
		std::vector<std::string> volt_string;
		std::vector<std::vector<double>> BER;
		std::vector<std::string> phase_string;


	public:
                CSV();
		~CSV();
		void read(std::string filename);
		void write(std::string filename);
                void fill(std::vector<eyescan::eyescanCoords> Coords, std::string filename);
		std::vector<boost::tuple<double, double, double>> plot_out();

	private:
		//CSV();
		bool isNumber(const std::string& str);

};

#endif
