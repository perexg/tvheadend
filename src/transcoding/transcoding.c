/**
 *  Transcoding
 *  Copyright (C) 2013 John Törnblom
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

#include "libav.h"

#include "transcoding.h"
#include "transcoder.h"


#define WORKING_ENCODER(x) \
  ((x) == AV_CODEC_ID_H264 || (x) == AV_CODEC_ID_MPEG2VIDEO || \
   (x) == AV_CODEC_ID_VP8  || /* (x) == AV_CODEC_ID_VP9 || */ \
   (x) == AV_CODEC_ID_HEVC || (x) == AV_CODEC_ID_AAC || \
   (x) == AV_CODEC_ID_MP2  || (x) == AV_CODEC_ID_VORBIS)


/**
 *
 */
htsmsg_t *
transcoder_get_capabilities(int experimental)
{
  AVCodec *p = NULL;
  streaming_component_type_t sct;
  htsmsg_t *array = htsmsg_create_list(), *m;
  char buf[128];

  while ((p = av_codec_next(p))) {

    if (!libav_is_encoder(p))
      continue;

    if (!WORKING_ENCODER(p->id))
      continue;

    if ((p->capabilities & CODEC_CAP_EXPERIMENTAL) && !experimental)
      continue;

    sct = codec_id2streaming_component_type(p->id);
    if (sct == SCT_NONE || sct == SCT_UNKNOWN)
      continue;

    m = htsmsg_create_map();
    htsmsg_add_s32(m, "type", sct);
    htsmsg_add_u32(m, "id", p->id);
    htsmsg_add_str(m, "name", p->name);
    snprintf(buf, sizeof(buf), "%s%s",
             p->long_name ?: "",
             (p->capabilities & CODEC_CAP_EXPERIMENTAL) ?
               " (Experimental)" : "");
    if (buf[0] != '\0')
      htsmsg_add_str(m, "long_name", buf);
    htsmsg_add_msg(array, NULL, m);
  }
  return array;
}


/**
 *
 */
void
transcoder_set_properties(streaming_target_t *st, transcoder_props_t *props)
{
    TVHTranscoder *t = (TVHTranscoder *)st;
    transcoder_props_t *tp = &t->t_props;

    tp->tp_deinterlace = props->tp_deinterlace;
    tp->tp_resolution  = props->tp_resolution;
    tp->tp_channels    = props->tp_channels;
    tp->tp_vbitrate    = props->tp_vbitrate;
    tp->tp_abitrate    = props->tp_abitrate;
    memcpy(tp->tp_language, props->tp_language, 4);
    strncpy(tp->tp_vcodec, props->tp_vcodec, sizeof(tp->tp_vcodec)-1);
    strncpy(tp->tp_acodec, props->tp_acodec, sizeof(tp->tp_acodec)-1);
    strncpy(tp->tp_scodec, props->tp_scodec, sizeof(tp->tp_scodec)-1);
}


/*
 *
 */
void
transcoding_init(void)
{
    handler_register_all();
}
