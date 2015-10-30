/**
 *  Transcoding
 *  Copyright (C) 2015 Tvheadend
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


#include "handler.h"


/* SubtitleHandler ************************************************************/

static void
Subtitle_setup(TVHHandler *self, streaming_start_component_t *component,
               transcoder_props_t *props, int same_type)
{
    self->passthrough = same_type;
}


static th_pkt_t *
Subtitle_handle(TVHHandler *self, th_pkt_t *packet)
{
    return packet;
}


TVHHandlerClass SubtitleHandler = {
    .type = AVMEDIA_TYPE_SUBTITLE,
    .name = "Subtitle",
    .size = sizeof(TVHHandler),
    .setup = (setup_meth)Subtitle_setup,
    .handle = (handle_meth)Subtitle_handle,
};
