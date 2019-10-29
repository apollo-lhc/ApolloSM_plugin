#ifndef __SVF_PLAYER_HH__
#define __SVF_PLAYER_HH__
#include <ApolloSM/svplayer_consts.hh>
#include <string>
#include <stdint.h>

class SVFPlayer {
public:
  SVFPlayer();  
  int play(std::string const & svfFile , std::string const & XVCLabel);
private:
  typedef struct  {
    uint32_t length_offset;
    uint32_t tms_offset;
    uint32_t tdi_offset;
    uint32_t tdo_offset;
    uint32_t ctrl_offset;
  } sXVC;

  /* Defined in svfplayer.cc */
  int  setup();
  int  shutdown();
  void udelay(long usecs, int tms, long num_tck);
  int  getbyte();
  int  sync();
  int  pulse_tck(int tms, int tdi, int tdo, int rmask, int sync);
  void pulse_sck();
  void set_trst(int v);
  int  set_frequency(int v);
  void tck();
  
  /* defined in svfplayer_svf.cc */
  int  read_command(char **buffer_p, int *len_p);
  int  token2tapstate(const char *str1);
  void bitdata_free(struct bitdata_s *bd, int offset);
  const char * bitdata_parse(const char *p, struct bitdata_s *bd, int offset);
  int  getbit(unsigned char *data, int n);
  int  bitdata_play(struct bitdata_s *bd, enum libxsvf_tap_state estate);
  int  svf_reader();

  /* defined in svfplayer_tap.cc */
  void tap_transition(int v);
  int  tap_walk(enum libxsvf_tap_state s);

  /* internal variables */
  enum libxsvf_tap_state tap_state;
  FILE * svfFile;

  int fdUIO;  
  sXVC volatile * jtag_reg;
  
  size_t updateCount;
  size_t totalBitCount;
  size_t currentBitCount;
  size_t updateBitCount;

  int clockcount;
  int bitcount_tdi;
  int bitcount_tdo;
  int retval_i;
  int retval[256];

};

#endif
