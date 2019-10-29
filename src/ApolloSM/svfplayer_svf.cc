#include "ApolloSM/svfplayer.hh"
#include <sys/time.h>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static int realloc_maxsize[LIBXSVF_MEM_NUM];
static void *h_realloc(void *ptr, int size, enum libxsvf_mem which) {
  if (size > realloc_maxsize[which]) {realloc_maxsize[which] = size;}
  return realloc(ptr, size);
}   

int SVFPlayer::read_command(char **buffer_p, int *len_p)
{
  char *buffer = *buffer_p;
  int braket_mode = 0;
  int len = *len_p;
  int p = 0;

  while (1)
    {
      if (len < p+10) {
	len = len < 64 ? 96 : len*2;
 	buffer =(char *) h_realloc(buffer, len, LIBXSVF_MEM_SVF_COMMANDBUF);
	*buffer_p = buffer;
	*len_p = len;
	if (!buffer) {
	  fprintf(stderr, "Allocating memory failed.\n");
	  return -1;
	}
      }
      buffer[p] = 0;

      int ch = getbyte();
      if (ch < 0) {
      handle_eof:
	if (p == 0)
	  return 0;
	fprintf(stderr, "Unexpected EOF.\n");
	return -1;
      }
      if (ch <= ' ') {
      insert_eol:
	if (!braket_mode && p > 0 && buffer[p-1] != ' ')
	  buffer[p++] = ' ';
	continue;
      }
      if (ch == '!') {
      skip_to_eol:
	while (1) {
	  ch = getbyte();
	  if (ch < 0)
	    goto handle_eof;
	  if (ch < ' ' && ch != '\t')
	    goto insert_eol;
	}
      }
      if (ch == '/' && p > 0 && buffer[p-1] == '/') {
	p--;
	goto skip_to_eol;
      }
      if (ch == ';')
	break;
      if (ch == '(') {
	if (!braket_mode && p > 0 && buffer[p-1] != ' ')
	  buffer[p++] = ' ';
	braket_mode++;
      }
      if (ch >= 'a' && ch <= 'z')
	buffer[p++] = ch - ('a' - 'A');
      else
	buffer[p++] = ch;
      if (ch == ')') {
	braket_mode--;
	if (!braket_mode)
	  buffer[p++] = ' ';
      }
    }
  return 1;
}

int strtokencmp(const char *str1, const char *str2)
{
  int i = 0;
  while (1) {
    if ((str1[i] == ' ' || str1[i] == 0) && (str2[i] == ' ' || str2[i] == 0))
      return 0;
    if (str1[i] < str2[i])
      return -1;
    if (str1[i] > str2[i])
      return +1;
    i++;
  }
}

int strtokenskip(const char *str1)
{
  int i = 0;
  while (str1[i] != 0 && str1[i] != ' ') i++;
  while (str1[i] == ' ') i++;
  return i;
}


int SVFPlayer::token2tapstate(const char *str1)
{
#define X(_t) if (!strtokencmp(str1, #_t)) return LIBXSVF_TAP_ ## _t;
  X(RESET)
    X(IDLE)
    X(DRSELECT)
    X(DRCAPTURE)
    X(DRSHIFT)
    X(DREXIT1)
    X(DRPAUSE)
    X(DREXIT2)
    X(DRUPDATE)
    X(IRSELECT)
    X(IRCAPTURE)
    X(IRSHIFT)
    X(IREXIT1)
    X(IRPAUSE)
    X(IREXIT2)
    X(IRUPDATE)
#undef X
    return -1;
}

struct bitdata_s {
  int len, alloced_len;
  int alloced_bytes;
  unsigned char *tdi_data;
  unsigned char *tdi_mask;
  unsigned char *tdo_data;
  unsigned char *tdo_mask;
  unsigned char *ret_mask;
  int has_tdo_data;
};

void SVFPlayer::bitdata_free(struct bitdata_s *bd, int offset)
{
  h_realloc(bd->tdi_data, 0, (libxsvf_mem)(offset+0));
  h_realloc(bd->tdi_mask, 0, (libxsvf_mem)(offset+1));
  h_realloc(bd->tdo_data, 0, (libxsvf_mem)(offset+2));
  h_realloc(bd->tdo_mask, 0, (libxsvf_mem)(offset+3));
  h_realloc(bd->ret_mask, 0, (libxsvf_mem)(offset+4));

  bd->tdi_data = NULL;
  bd->tdi_mask = NULL;
  bd->tdo_data = NULL;
  bd->tdo_mask = NULL;
  bd->ret_mask = NULL;
}

int hex(char ch)
{
  if (ch >= 'A' && ch <= 'Z')
    return (ch - 'A') + 10;
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  return 0;
}


const char * SVFPlayer::bitdata_parse(const char *p, struct bitdata_s *bd, int offset)
{
  int i, j;
  bd->len = 0;
  bd->has_tdo_data = 0;
  while (*p >= '0' && *p <= '9') {
    bd->len = bd->len * 10 + (*p - '0');
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  if (bd->len != bd->alloced_len) {
    bitdata_free(bd, offset);
    bd->alloced_len = bd->len;
    bd->alloced_bytes = (bd->len+7) / 8;
  }
  while (*p)
    {
      int memnum = 0;
      unsigned char **dp = NULL;
      if (!strtokencmp(p, "TDI")) {
	p += strtokenskip(p);
	dp = &bd->tdi_data;
	memnum = 0;
      }
      if (!strtokencmp(p, "TDO")) {
	p += strtokenskip(p);
	dp = &bd->tdo_data;
	bd->has_tdo_data = 1;
	memnum = 1;
      }
      if (!strtokencmp(p, "SMASK")) {
	p += strtokenskip(p);
	dp = &bd->tdi_mask;
	memnum = 2;
      }
      if (!strtokencmp(p, "MASK")) {
	p += strtokenskip(p);
	dp = &bd->tdo_mask;
	memnum = 3;
      }
      if (!strtokencmp(p, "RMASK")) {
	p += strtokenskip(p);
	dp = &bd->ret_mask;
	memnum = 4;
      }
      if (!dp)
	return NULL;
      if (*dp == NULL) {
	*dp = (unsigned char *) h_realloc(*dp, bd->alloced_bytes, (libxsvf_mem)(offset+memnum));
      }
      if (*dp == NULL) {
	fprintf(stderr, "Allocating memory failed.\n");
	return NULL;
      }

      unsigned char *d = *dp;
      for (i=0; i<bd->alloced_bytes; i++)
	d[i] = 0;

      if (*p != '(')
	return NULL;
      p++;

      int hexdigits = 0;
      for (i=0; (p[i] >= 'A' && p[i] <= 'F') || (p[i] >= '0' && p[i] <= '9'); i++)
	hexdigits++;

      i = bd->alloced_bytes*2 - hexdigits;
      for (j=0; j<hexdigits; j++, i++, p++) {
	if ((i&0x1) == 0x0) {
	  d[i>>1] |= hex(*p) << 4;
	} else {
	  d[i>>1] |= hex(*p);
	}

      }

      if (*p != ')')
	return NULL;
      p++;
      while (*p == ' ') {
	p++;
      }
    }
  return p;
}

int SVFPlayer::getbit(unsigned char *data, int n)
{
  //  return (data[n/8] & (1 << (7 - n%8))) ? 1 : 0;
  return (data[(n>>3)] & (1 << (7 - (n&0x7)))) ? 1 : 0;
}

int SVFPlayer::bitdata_play(struct bitdata_s *bd, enum libxsvf_tap_state estate)
{
  int left_padding = (8 - (bd->len & 0x7)) & 0x7;
  int tdo_error = 0;
  int tms = 0;

  if(bd->len > 10000){
    updateCount=80;
    totalBitCount=bd->len;
    printf("Programming FPGA.\n[");
    for(size_t i = 0; i < totalBitCount/(totalBitCount/updateCount);i++){
      printf("=");
    }
    printf("]\n[");
    fflush(stdout);
    currentBitCount=0;
    updateBitCount = totalBitCount/updateCount; 
  }else{
    currentBitCount = 0;
    updateBitCount  = 0;
  }

  for (int i=bd->len+left_padding-1; i >= left_padding; i--) {
    if (i == left_padding && tap_state != estate) {
      tap_state = (libxsvf_tap_state)((int)tap_state + 1);
      tms = 1;
    }
    int tdi = -1;
    if (bd->tdi_data) {
      if (!bd->tdi_mask || getbit(bd->tdi_mask, i))
	tdi = getbit(bd->tdi_data, i);
    }
    currentBitCount++;
    int tdo = -1;
    if (bd->tdo_data && bd->has_tdo_data && (!bd->tdo_mask || getbit(bd->tdo_mask, i)))
      tdo = getbit(bd->tdo_data, i);
    int rmask = bd->ret_mask && getbit(bd->ret_mask, i);
    if(pulse_tck(tms, tdi, tdo, rmask, 0) < 0)
      tdo_error = 1;
    
    //updates
    if((updateBitCount != 0) && (currentBitCount > updateBitCount)){
      printf(".");
      fflush(stdout);
      //      updateBitCount += totalBitCount/updateCount; 
      currentBitCount=0;
    }
  }
  if(updateBitCount != 0){
    printf(".]\n");
    fflush(stdout);
  }

  // if (tms)
    //  report_tapstate();

  if (!tdo_error)
    return 0;

  fprintf(stderr, "TDO mismatch.\n");
  return -1;
}

int SVFPlayer::svf_reader()
{ 
  char *command_buffer = NULL;
  int command_buffer_len = 0;
  int rc, i;

  struct bitdata_s bd_hdr = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};
  struct bitdata_s bd_hir = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};
  struct bitdata_s bd_tdr = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};
  struct bitdata_s bd_tir = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};
  struct bitdata_s bd_sdr = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};
  struct bitdata_s bd_sir = { 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};

  int state_endir  = LIBXSVF_TAP_IDLE;
  int state_enddr  = LIBXSVF_TAP_IDLE;
  int state_run    = LIBXSVF_TAP_IDLE;
  int state_endrun = LIBXSVF_TAP_IDLE;

  while (1)
    {
      rc = read_command(&command_buffer, &command_buffer_len);

      if (rc <= 0)
	break;

      const char *p = command_buffer;

      if (!strtokencmp(p, "ENDIR")) {
	p += strtokenskip(p);
	state_endir = token2tapstate(p);
	if (state_endir < 0)
	  goto syntax_error;
	p += strtokenskip(p);
	goto eol_check;
      }

      if (!strtokencmp(p, "ENDDR")) {
	p += strtokenskip(p);
	state_enddr = token2tapstate(p);
	if (state_endir < 0)
	  goto syntax_error;
	p += strtokenskip(p);
	goto eol_check;
      }

      if (!strtokencmp(p, "FREQUENCY")) {
	unsigned long number = 0;
	int got_decimal_point = 0;
	int decimal_digits = 0;
	int exp = 0;
	p += strtokenskip(p);
	if (*p < '0' || *p > '9')
	  goto syntax_error;
	while ((*p >= '0' && *p <= '9') || (*p == '.')) {
	  if (*p == '.') {
	    got_decimal_point = 1;
	  } else {
	    if (got_decimal_point)
	      decimal_digits++;
	    number = number*10 + (*p - '0');
	  }
	  p++;
	}
	if(*p == 'E' || *p == 'e') {
	  p++;
	  if (*p == '+')
	    p++;
	  while (*p >= '0' && *p <= '9') {
	    exp = exp*10 + (*p - '0');
	    p++;
	  }
	  exp -= decimal_digits;
	  if (exp < 0)
	    goto syntax_error;
	  for(i=0; i<exp; i++)
	    number *= 10;
	}
	while (*p == ' ') {
	  p++;
	}
	p += strtokenskip(p);
	if(set_frequency(number) < 0) {
	  fprintf(stderr, "FREQUENCY command failed!\n");
	  goto error;
	}
	goto eol_check;
      }

      if (!strtokencmp(p, "HDR")) {
	p += strtokenskip(p);
	p = bitdata_parse(p, &bd_hdr, LIBXSVF_MEM_SVF_HDR_TDI_DATA);
	if (!p)
	  goto syntax_error;
	goto eol_check;
      }

      if (!strtokencmp(p, "HIR")) {
	p += strtokenskip(p);
	p = bitdata_parse(p, &bd_hir, LIBXSVF_MEM_SVF_HIR_TDI_DATA);
	if (!p)
	  goto syntax_error;
	goto eol_check;
      }

      if (!strtokencmp(p, "PIO") || !strtokencmp(p, "PIOMAP")) {
	goto unsupported_error;
      }

      if (!strtokencmp(p, "RUNTEST")) {
	p += strtokenskip(p);
	int tck_count = -1;
	int sck_count = -1;
	int min_time = -1;
	int max_time = -1;
	while (*p) {
	  int got_maximum = 0;
	  if (!strtokencmp(p, "MAXIMUM")) {
	    p += strtokenskip(p);
	    got_maximum = 1;
	  }
	  int got_endstate = 0;
	  if (!strtokencmp(p, "ENDSTATE")) {
	    p += strtokenskip(p);
	    got_endstate = 1;
	  }
	  int st = token2tapstate(p);
	  if (st >= 0) {
	    p += strtokenskip(p);
	    if (got_endstate)
	      state_endrun = st;
	    else
	      state_run = st;
	    continue;
	  }
	  if (*p < '0' || *p > '9')
	    goto syntax_error;
	  int number = 0;
	  int exp = 0, expsign = 1;
	  int number_e6, exp_e6;
	  while (*p >= '0' && *p <= '9') {
	    number = number*10 + (*p - '0');
	    p++;
	  }
	  if(*p == '.')
	    {
	      p++;
	      while (*p >= '0' && *p <= '9')
		p++;
	      // FIXME: accept fractional part
	    }
	  if(*p == 'E' || *p == 'e') {
	    p++;
	    if(*p == '-') {
	      expsign = -1;
	      p++;
	    }
	    if(*p == '+') {
	      expsign = 1;
	      p++;
	    }
	    while (*p >= '0' && *p <= '9') {
	      exp = exp*10 + (*p - '0');
	      p++;
	    }
	    exp = exp * expsign;
	    number_e6 = number;
	    exp_e6 = exp + 6;
	    while (exp < 0) {
	      number /= 10;
	      exp++;
	    }
	    while (exp > 0) {
	      number *= 10;
	      exp--;
	    }
	    while (exp_e6 < 0) {
	      number_e6 /= 10;
	      exp_e6++;
	    }
	    while (exp_e6 > 0) {
	      number_e6 *= 10;
	      exp_e6--;
	    }
	  } else {
	    number_e6 = number * 1000000;
	  }
	  while (*p == ' ') {
	    p++;
	  }
	  if (!strtokencmp(p, "SEC")) {
	    p += strtokenskip(p);
	    if (got_maximum)
	      max_time = number_e6;
	    else
	      min_time = number_e6;
	    continue;
	  }
	  if (!strtokencmp(p, "TCK")) {
	    p += strtokenskip(p);
	    tck_count = number;
	    continue;
	  }
	  if (!strtokencmp(p, "SCK")) {
	    p += strtokenskip(p);
	    sck_count = number;
	    continue;
	  }
	  goto syntax_error;
	}
	if(tap_walk((libxsvf_tap_state)state_run) < 0)
	  goto error;
	if (max_time >= 0) {
	  fprintf(stderr, "WARNING: Maximum time in SVF RUNTEST command is ingored.\n");
	}
	if (sck_count >= 0) {
	  for (i=0; i < sck_count; i++) {
	    pulse_sck();
	  }
	}
	if (min_time >= 0 || tck_count >= 0) {
	  //	  udelay(min_time >= 0 ? min_time : 0, 0, tck_count >= 0 ? tck_count : 0);
	}
	if(tap_walk((libxsvf_tap_state)state_endrun) < 0)
	  goto error;
	goto eol_check;
      }

      if (!strtokencmp(p, "SDR")) {
	p += strtokenskip(p);
	p = bitdata_parse(p, &bd_sdr, LIBXSVF_MEM_SVF_SDR_TDI_DATA);
	if (!p)
	  goto syntax_error;
	if(tap_walk(LIBXSVF_TAP_DRSHIFT) < 0)
	  goto error;
	if (bitdata_play(&bd_hdr, bd_sdr.len+bd_tdr.len > 0 ? LIBXSVF_TAP_DRSHIFT : (libxsvf_tap_state)state_enddr) < 0)
	  goto error;
	if (bitdata_play(&bd_sdr, bd_tdr.len > 0 ? LIBXSVF_TAP_DRSHIFT : (libxsvf_tap_state)state_enddr) < 0)
	  goto error;
	if (bitdata_play(&bd_tdr, (libxsvf_tap_state)state_enddr) < 0)
	  goto error;
	if(tap_walk((libxsvf_tap_state)state_enddr) < 0)
	  goto error;
	goto eol_check;
      }

      if (!strtokencmp(p, "SIR")) {
	p += strtokenskip(p);
	p = bitdata_parse(p, &bd_sir, LIBXSVF_MEM_SVF_SIR_TDI_DATA);
	if (!p)
	  goto syntax_error;
	if(tap_walk(LIBXSVF_TAP_IRSHIFT) < 0)
	  goto error;
	if (bitdata_play(&bd_hir, bd_sir.len+bd_tir.len > 0 ? LIBXSVF_TAP_IRSHIFT : (libxsvf_tap_state)state_endir) < 0)
	  goto error;
	if (bitdata_play(&bd_sir, bd_tir.len > 0 ? LIBXSVF_TAP_IRSHIFT : (libxsvf_tap_state)state_endir) < 0)
	  goto error;
	if (bitdata_play(&bd_tir,(libxsvf_tap_state)state_endir) < 0)
	  goto error;
	if(tap_walk((libxsvf_tap_state)state_endir) < 0)
	  goto error;
	goto eol_check;
      }

      if (!strtokencmp(p, "STATE")) {
	p += strtokenskip(p);
	while (*p) {
	  int st = token2tapstate(p);
	  if (st < 0)
	    goto syntax_error;
	  if(tap_walk((libxsvf_tap_state)st) < 0)
	    goto error;
	  p += strtokenskip(p);
	}
	goto eol_check;
      }

      if (!strtokencmp(p, "TDR")) {
	p += strtokenskip(p);
	p = bitdata_parse(p, &bd_tdr, LIBXSVF_MEM_SVF_TDR_TDI_DATA);
	if (!p)
	  goto syntax_error;
	goto eol_check;
      }

      if (!strtokencmp(p, "TIR")) {
	p += strtokenskip(p);
	p = bitdata_parse(p, &bd_tir, LIBXSVF_MEM_SVF_TIR_TDI_DATA);
	if (!p)
	  goto syntax_error;
	goto eol_check;
      }

      if (!strtokencmp(p, "TRST")) {
	p += strtokenskip(p);
	if (!strtokencmp(p, "ON")) {
	  p += strtokenskip(p);
	  set_trst(1);
	  goto eol_check;
	}
	if (!strtokencmp(p, "OFF")) {
	  p += strtokenskip(p);
	  set_trst(0);
	  goto eol_check;
	}
	if (!strtokencmp(p, "Z")) {
	  p += strtokenskip(p);
	  set_trst(-1);
	  goto eol_check;
	}
	if (!strtokencmp(p, "ABSENT")) {
	  p += strtokenskip(p);
	  set_trst(-2);
	  goto eol_check;
	}
	goto syntax_error;
      }

    eol_check:
      while (*p == ' ')
	p++;
      if (*p == 0)
	continue;

    syntax_error:
      fprintf(stderr, "SVF Syntax Error:");
      if (0) {
      unsupported_error:
	fprintf(stderr, "Error in SVF input: unsupported command:");
      }
    error:
      rc = -1;
      break;
    }

  bitdata_free(&bd_hdr, LIBXSVF_MEM_SVF_HDR_TDI_DATA);
  bitdata_free(&bd_hir, LIBXSVF_MEM_SVF_HIR_TDI_DATA);
  bitdata_free(&bd_tdr, LIBXSVF_MEM_SVF_TDR_TDI_DATA);
  bitdata_free(&bd_tir, LIBXSVF_MEM_SVF_TIR_TDI_DATA);
  bitdata_free(&bd_sdr, LIBXSVF_MEM_SVF_SDR_TDI_DATA);
  bitdata_free(&bd_sir, LIBXSVF_MEM_SVF_SIR_TDI_DATA);

  h_realloc(command_buffer, 0, LIBXSVF_MEM_SVF_COMMANDBUF);
  rc = 0;
  return rc;
}

