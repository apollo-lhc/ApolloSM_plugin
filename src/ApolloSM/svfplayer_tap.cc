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


void SVFPlayer::tap_transition(int v)
{
  pulse_tck(v, -1, -1, 0, 0);
}

int SVFPlayer::tap_walk(enum libxsvf_tap_state s)
{
  int i, j;
  for (i=0; s != tap_state; i++)
    {
      switch (tap_state)
	{
	  /* Special States */
	case LIBXSVF_TAP_INIT:
	  for (j = 0; j < 6; j++)
	    tap_transition(1);
	  tap_state = LIBXSVF_TAP_RESET;
	  break;
	case LIBXSVF_TAP_RESET:
	  tap_transition(0);
	  tap_state = LIBXSVF_TAP_IDLE;
	  break;
	case LIBXSVF_TAP_IDLE:
	  tap_transition(1);
	  tap_state = LIBXSVF_TAP_DRSELECT;
	  break;

	  /* DR States */
	case LIBXSVF_TAP_DRSELECT:
	  if (s >= LIBXSVF_TAP_IRSELECT || s == LIBXSVF_TAP_RESET) {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_IRSELECT;
	  } else {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_DRCAPTURE;
	  }
	  break;
	case LIBXSVF_TAP_DRCAPTURE:
	  if (s == LIBXSVF_TAP_DRSHIFT) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_DRSHIFT;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_DREXIT1;
	  }
	  break;
	case LIBXSVF_TAP_DRSHIFT:
	  tap_transition(1);
	  tap_state = LIBXSVF_TAP_DREXIT1;
	  break;
	case LIBXSVF_TAP_DREXIT1:
	  if (s == LIBXSVF_TAP_DRPAUSE) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_DRPAUSE;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_DRUPDATE;
	  }
	  break;
	case LIBXSVF_TAP_DRPAUSE:
	  tap_transition(1);
	  tap_state = LIBXSVF_TAP_DREXIT2;
	  break;
	case LIBXSVF_TAP_DREXIT2:
	  if (s == LIBXSVF_TAP_DRSHIFT) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_DRSHIFT;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_DRUPDATE;
	  }
	  break;
	case LIBXSVF_TAP_DRUPDATE:
	  if (s == LIBXSVF_TAP_IDLE) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_IDLE;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_DRSELECT;
	  }
	  break;

	  /* IR States */
	case LIBXSVF_TAP_IRSELECT:
	  if (s == LIBXSVF_TAP_RESET) {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_RESET;
	  } else {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_IRCAPTURE;
	  }
	  break;
	case LIBXSVF_TAP_IRCAPTURE:
	  if (s == LIBXSVF_TAP_IRSHIFT) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_IRSHIFT;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_IREXIT1;
	  }
	  break;
	case LIBXSVF_TAP_IRSHIFT:
	  tap_transition(1);
	  tap_state = LIBXSVF_TAP_IREXIT1;
	  break;
	case LIBXSVF_TAP_IREXIT1:
	  if (s == LIBXSVF_TAP_IRPAUSE) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_IRPAUSE;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_IRUPDATE;
	  }
	  break;
	case LIBXSVF_TAP_IRPAUSE:
	  tap_transition(1);
	  tap_state = LIBXSVF_TAP_IREXIT2;
	  break;
	case LIBXSVF_TAP_IREXIT2:
	  if (s == LIBXSVF_TAP_IRSHIFT) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_IRSHIFT;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_IRUPDATE;
	  }
	  break;
	case LIBXSVF_TAP_IRUPDATE:
	  if (s == LIBXSVF_TAP_IDLE) {
	    tap_transition(0);
	    tap_state = LIBXSVF_TAP_IDLE;
	  } else {
	    tap_transition(1);
	    tap_state = LIBXSVF_TAP_DRSELECT;
	  }
	  break;

	default:
	  fprintf(stderr, "Illegal tap state. (%d)\n",tap_state);
	  return -1;
	}
      if (i>10) {
	fprintf(stderr, "Loop in tap walker.\n"); 
	return -1;
      }
    }

  return 0;
}
