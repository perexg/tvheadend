/*
 *  IPTV - UDP/RTP handler
 *
 *  Copyright (C) 2013 Adam Sutton
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvheadend.h"
#include "iptv_private.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#if defined(PLATFORM_LINUX)
#include <linux/netdevice.h>
#elif defined(PLATFORM_FREEBSD)
#  include <netdb.h>
#  include <net/if.h>
#  ifndef IPV6_ADD_MEMBERSHIP
#    define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP
#    define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP
#  endif
#endif


/*
 * Connect UDP/RTP
 */
static int
iptv_udp_start ( iptv_mux_t *im, const url_t *url )
{
  char name[256];
  udp_connection_t *conn;

  im->mm_display_name((mpegts_mux_t*)im, name, sizeof(name));

  conn = udp_bind("iptv", name, url->host, url->port,
                  im->mm_iptv_interface, IPTV_PKT_SIZE);
  if (conn == UDP_FATAL_ERROR)
    return SM_CODE_TUNING_FAILED;
  if (conn == NULL)
    return -1;

  /* Done */
  im->mm_iptv_fd         = conn->fd;
  im->mm_iptv_connection = conn;
  iptv_input_mux_started(im);
  return 0;
}

static ssize_t
iptv_udp_read ( iptv_mux_t *im, size_t *off )
{
  /* UDP/RTP should not have TS packets straddling datagrams, I think! */
  im->mm_iptv_buffer.sb_ptr = 0;

  /* Read */
  return sbuf_read(&im->mm_iptv_buffer, im->mm_iptv_fd);
}

static ssize_t
iptv_rtp_read ( iptv_mux_t *im, size_t *off )
{
  ssize_t len, hlen;

  /* Raw packet */
  len = iptv_udp_read(im, NULL);
  if (len < 0)
    return -1;

  /* Strip RTP header */
  if (len < 12)
    return 0; // ignore
  if ((im->mm_iptv_buffer.sb_data[0] & 0xC0) != 0x80)
    return 0;
  if ((im->mm_iptv_buffer.sb_data[1] & 0x7F) != 33)
    return 0;
  hlen = ((im->mm_iptv_buffer.sb_data[0] & 0xf) * 4) + 12;
  if (im->mm_iptv_buffer.sb_data[0] & 0x10) {
    if (len < hlen+4)
      return 0;
    hlen += (im->mm_iptv_buffer.sb_data[hlen+2] << 8) 
          | (im->mm_iptv_buffer.sb_data[hlen+3] * 4);
    hlen += 4;
  }
  if (len < hlen || ((len - hlen) % 188) != 0)
    return 0;

  /* Cut */
  sbuf_cut(&im->mm_iptv_buffer, hlen);
  // TODO: would be nice to avoid this extra copy, it was possible until I
  //       changed the API!

  return len;
}

/*
 * Initialise UDP handler
 */

void
iptv_udp_init ( void )
{
  static iptv_handler_t ih[] = {
    {
      .scheme = "udp",
      .start  = iptv_udp_start,
      .read   = iptv_udp_read,
    },
    {
      .scheme = "rtp",
      .start  = iptv_udp_start,
      .read   = iptv_rtp_read,
    }
  };
  iptv_handler_register(ih, 2);
}
