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


#ifndef TVH_TRANSCODING_TRANSCODER_H__
#define TVH_TRANSCODING_TRANSCODER_H__


#include "transcoding.h"
#include "handler.h"


/* TVHStream ******************************************************************/

typedef struct t_stream {
    int index;
    streaming_component_type_t type;
    TVHHandler *handler;

    SLIST_ENTRY(t_stream) link;
} TVHStream;


/* TVHTranscoder **************************************************************/

SLIST_HEAD(TVHStreams, t_stream);

typedef struct t_transcoder {
    streaming_target_t input;
    streaming_target_t *output;
    uint32_t id;
    transcoder_props_t t_props;

    struct TVHStreams streams;
} TVHTranscoder;


#endif // TVH_TRANSCODING_TRANSCODER_H__
