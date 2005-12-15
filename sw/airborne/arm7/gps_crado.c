
#include "gps_crado.h"

uint32_t gps_itow;
int32_t gps_alt;
uint16_t gps_gspeed;
int16_t gps_climb;
int16_t gps_course;
int32_t gps_utm_east, gps_utm_north;
uint8_t gps_utm_zone;
uint8_t gps_mode;
volatile uint8_t gps_msg_received;
uint8_t gps_pos_available;

#define UBX_MAX_PAYLOAD 255
static uint8_t  ubx_msg_buf[UBX_MAX_PAYLOAD];

#define UNINIT        0
#define GOT_SYNC1     1
#define GOT_SYNC2     2
#define GOT_CLASS     3
#define GOT_ID        4
#define GOT_LEN1      5
#define GOT_LEN2      6
#define GOT_PAYLOAD   7
#define GOT_CHECKSUM1 8

static uint8_t  ubx_status;
static uint16_t ubx_len;
static uint8_t  ubx_msg_idx;
static uint8_t ck_a, ck_b, ubx_id, ubx_class;

struct svinfo gps_svinfos[NB_CHANNELS];
uint8_t gps_nb_channels;
uint8_t gps_nb_ovrn;

#define Min(x,y) (x < y ? x : y)

void parse_gps_msg( void ) {
  if (ubx_class == UBX_NAV_ID) {
    if (ubx_id == UBX_NAV_POSUTM_ID) {
      gps_utm_east = UBX_NAV_POSUTM_EAST(ubx_msg_buf);
      gps_utm_north = UBX_NAV_POSUTM_NORTH(ubx_msg_buf);
      gps_alt = UBX_NAV_POSUTM_ALT(ubx_msg_buf);
      gps_utm_zone = UBX_NAV_POSUTM_ZONE(ubx_msg_buf);
    } else if (ubx_id == UBX_NAV_STATUS_ID) {
      gps_mode = UBX_NAV_STATUS_GPSfix(ubx_msg_buf);
    } else if (ubx_id == UBX_NAV_VELNED_ID) {
      gps_gspeed = UBX_NAV_VELNED_GSpeed(ubx_msg_buf);
      gps_climb = - UBX_NAV_VELNED_VEL_D(ubx_msg_buf);
      gps_course = UBX_NAV_VELNED_Heading(ubx_msg_buf) / 10000;
      gps_itow = UBX_NAV_VELNED_ITOW(ubx_msg_buf);
      
      gps_pos_available = True; /* The 3 UBX messages are sent in one rafale */
    } else if (ubx_id == UBX_NAV_SVINFO_ID) {
      gps_nb_channels = UBX_NAV_SVINFO_NCH(ubx_msg_buf);
      uint8_t i;
      for(i = 0; i < Min(gps_nb_channels, NB_CHANNELS); i++) {
	//	memcpy(&(gps_svinfos[i]), (ubx_msg_buf+9+12*i), 7);
	gps_svinfos[i].svid = UBX_NAV_SVINFO_SVID(ubx_msg_buf, i);
	gps_svinfos[i].flags = UBX_NAV_SVINFO_Flags(ubx_msg_buf, i);
	gps_svinfos[i].qi = UBX_NAV_SVINFO_QI(ubx_msg_buf, i);
	gps_svinfos[i].cno = UBX_NAV_SVINFO_CNO(ubx_msg_buf, i);
	gps_svinfos[i].elev = UBX_NAV_SVINFO_Elev(ubx_msg_buf, i);
	gps_svinfos[i].azim = UBX_NAV_SVINFO_Azim(ubx_msg_buf, i);
      }
    }
  }
}



void parse_ubx( uint8_t c ) {
  if (ubx_status < GOT_PAYLOAD) {
    ck_a += c;
    ck_b += ck_a;
  }
  switch (ubx_status) {
  case UNINIT:
    if (c == UBX_SYNC1)
      ubx_status++;
    break;
  case GOT_SYNC1:
    if (c != UBX_SYNC2)
      goto error;
    ck_a = 0;
    ck_b = 0;
    ubx_status++;
    break;
  case GOT_SYNC2:
    if (gps_msg_received) {
      /* Previous message has not yet been parsed: discard this one */
      gps_nb_ovrn++;
      goto error;
    }
    ubx_class = c;
    ubx_status++;
    break;
  case GOT_CLASS:
    ubx_id = c;
    ubx_status++;
    break;    
  case GOT_ID:
    ubx_len = c;
    ubx_status++;
    break;
  case GOT_LEN1:
    ubx_len |= (c<<8);
    if (ubx_len > UBX_MAX_PAYLOAD)
      goto error;
    ubx_msg_idx = 0;
    ubx_status++;
    break;
  case GOT_LEN2:
    ubx_msg_buf[ubx_msg_idx] = c;
    ubx_msg_idx++;
    if (ubx_msg_idx >= ubx_len) {
      ubx_status++;
    }
    break;
  case GOT_PAYLOAD:
    if (c != ck_a)
      goto error;
    ubx_status++;
    break;
  case GOT_CHECKSUM1:
    if (c != ck_b)
      goto error;
    gps_msg_received = True;
    goto restart;
    break;
  }
  return;
 error:  
 restart:
  ubx_status = UNINIT;
  return;
}
