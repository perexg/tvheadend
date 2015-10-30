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


/* AudioHandler ***************************************************************/

typedef struct {
    TVHHandler handler;
    int8_t channels;
    int32_t bitrate;
} TVHAudioHandler;


static void
Audio_setup(TVHAudioHandler *self, streaming_start_component_t *component,
            transcoder_props_t *props, int same_type)
{
    int passthrough = (same_type && props->tp_abitrate < 16);
    if (!passthrough) {
        self->channels = props->tp_channels;
        self->bitrate = props->tp_abitrate * 1000;
    }
    ((TVHHandler *)self)->passthrough = passthrough;
}


static th_pkt_t *
Audio_handle(TVHAudioHandler *self, th_pkt_t *packet)
{
    return packet;
}


TVHHandlerClass AudioHandler = {
    .type = AVMEDIA_TYPE_AUDIO,
    .name = "Audio",
    .size = sizeof(TVHAudioHandler),
    .setup = (setup_meth)Audio_setup,
    .handle = (handle_meth)Audio_handle,
};
