#include <IPBusIO/IPBusIO.hh>
#include "libxsvf.hh"
//maybe
#include <IPBusIO/IPBusConnection.hh>
#include <IPBusStatus/IPBusStatus.hh>
#include <BUException/ExceptionBase.hh>
#include <ApolloSM/ApolloSM.hh>
#include <stdio.h>

class SVFPlayer : public IPBusIO {
public:
  SVFPlayer(uhal::HwInterface * const * _hw);  
  int play(std::string const & svfFile , std::string const & XVCReg);
private:

  SVFPlayer();

  /* Defined in svfplayer.cc */
  int setup(std::string const & XVCReg);
  int shutdown();
  void udelay(long usecs, int tms, long num_tck);
  int getbyte();
  int sync();
  int pulse_tck(int tms, int tdi, int tdo, int rmask, int sync);
  void pulse_sck();
  void set_trst(int v);
  int set_frequency(int v);

  /* defined in svfplayer_svf.cc */
  int read_command(char **buffer_p, int *len_p);
  int token2tapstate(const char *str1);
  void bitdata_free(struct bitdata_s *bd, int offset);
  const char * bitdata_parse(const char *p, struct bitdata_s *bd, int offset);
  int getbit(unsigned char *data, int n);
  int bitdata_play(struct bitdata_s *bd, enum libxsvf_tap_state estate);
  int svf();

  /* defined in svfplayer_tap.cc */
  void tap_transition(int v);
  int tap_walk(enum libxsvf_tap_state s);

  /* internal variables */
  enum libxsvf_tap_state tap_state;
  FILE *f;
  int verbose;
  int clockcount;
  int bitcount_tdi;
  int bitcount_tdo;
  int retval_i;
  int retval[256];

  /* nodes for AXI connections */
  uhal::Node const * nTDI;
  uhal::Node const * nTDO;
  uhal::Node const * nTMS;
  uhal::Node const * nLength;
  uhal::Node const * nGO;
};
