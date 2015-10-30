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


#include "service.h"

#include "transcoder.h"


/* TVHStream ******************************************************************/

static void
stream_invalidate(TVHStream *self)
{
    tvhinfo("transcode", "stream: %d:%s ==> invalidating",
            self->index, streaming_component_type2txt(self->type));
    self->index = 0;
}


static th_pkt_t *
stream_handle(TVHStream *self, th_pkt_t *pkt)
{
    th_pkt_t *transcoded = handler_handle(self->handler, pkt);
    if (!transcoded) {
        stream_invalidate(self);
    }
    return transcoded;
}


static void
stream_stop(TVHStream *self)
{
    stream_invalidate(self);
    if (self->handler) {
        handler_destroy(self->handler);
    }
}


static TVHStream *
stream_create(TVHTranscoder *transcoder, streaming_start_component_t *component)
{
    TVHHandler *handler = NULL;
    TVHStream *self = NULL;

    handler = handler_create(component, &transcoder->t_props);
    if (handler) {
        self = calloc(1, sizeof(TVHStream));
        if (!self) {
            handler_destroy(handler);
        }
        else {
            self->index = component->ssc_index;
            self->type = component->ssc_type;
            self->handler = handler;
        }
    }
    return self;
}


static void
stream_destroy(TVHStream *self)
{
    if (self) {
        stream_stop(self);
        free(self);
        self = NULL;
    }
}


/* TVHTranscoder **************************************************************/

static inline int
transcoder_shortid(TVHTranscoder *self)
{
    return self->id & 0xffff;
}


static int
transcoder_stream_count(TVHTranscoder *self, streaming_start_t *src) {
    int video = 0, audio = 0, subtitle = 0;
    int i;
    streaming_start_component_t *ssc = NULL;

    for (i = 0; i < src->ss_num_components; i++) {
        ssc = &src->ss_components[i];
        if (ssc->ssc_disabled)
            continue;

        if (SCT_ISVIDEO(ssc->ssc_type)) {
            if (self->t_props.tp_vcodec[0] == '\0')
                video = 0;
            else if (!strcmp(self->t_props.tp_vcodec, "copy"))
                video++;
            else
                video = 1;
        }
        else if (SCT_ISAUDIO(ssc->ssc_type)) {
            if (self->t_props.tp_acodec[0] == '\0')
                audio = 0;
            else if (!strcmp(self->t_props.tp_acodec, "copy"))
                audio++;
            else
                audio = 1;

        }
        else if (SCT_ISSUBTITLE(ssc->ssc_type)) {
            if (self->t_props.tp_scodec[0] == '\0')
                subtitle = 0;
            else if (!strcmp(self->t_props.tp_scodec, "copy"))
                subtitle++;
            else
                subtitle = 1;
        }
    }

    tvhtrace("transcode", "%04X: transcoder_stream_count=%d",
             transcoder_shortid(self), (video + audio + subtitle));

    return (video + audio + subtitle);
}


static void
transcoder_deliver(TVHTranscoder *self, th_pkt_t *pkt)
{
    streaming_message_t *msg = NULL;

    if (pkt) { // can be called with a null pkt
        msg = streaming_msg_create_pkt(pkt); // we assume this never fails?
        streaming_target_deliver2(self->output, msg);
        pkt_ref_dec(pkt);
    }
}

static void
transcoder_handle(TVHTranscoder *self, th_pkt_t *pkt)
{
    TVHStream *stream = NULL;

    SLIST_FOREACH(stream, &self->streams, link) {
        if (pkt->pkt_componentindex == stream->index) {
            if (pkt->pkt_payload) {
                transcoder_deliver(self, stream_handle(stream, pkt));
                break;
            }
            else {
                transcoder_deliver(self, pkt); // will decref pkt
                return;
            }
        }
    }
    pkt_ref_dec(pkt);
}


static streaming_start_t *
transcoder_start(TVHTranscoder *self, streaming_start_t *src)
{
    int n = transcoder_stream_count(self, src);
    streaming_start_t *start = NULL;
    TVHStream *stream = NULL;
    int i, j;

    start = calloc(1, (sizeof(streaming_start_t) +
                       sizeof(streaming_start_component_t) * n));
    if (start) {
        start->ss_refcount       = 1;
        start->ss_num_components = n;
        start->ss_pcr_pid        = src->ss_pcr_pid;
        start->ss_pmt_pid        = src->ss_pmt_pid;
        service_source_info_copy(&start->ss_si, &src->ss_si);

        for (i = j = 0; i < src->ss_num_components && j < n; i++) {
            streaming_start_component_t *ssc_src = &src->ss_components[i];
            streaming_start_component_t *ssc = &start->ss_components[j];

            if (ssc_src->ssc_disabled)
                continue;

            *ssc = *ssc_src;

            if(!(stream = stream_create(self, ssc))) {
                tvhinfo("transcode", "%04X: %d:%s ==> Filtered",
                        transcoder_shortid(self), ssc->ssc_index,
                        streaming_component_type2txt(ssc->ssc_type));
            }
            else {
                SLIST_INSERT_HEAD(&self->streams, stream, link);
                j++;
            }
        }
    }

    return start;
}


static void
transcoder_stop(TVHTranscoder *self)
{
    TVHStream *stream = NULL;

    while (!SLIST_EMPTY(&self->streams)) {
        stream = SLIST_FIRST(&self->streams);
        SLIST_REMOVE_HEAD(&self->streams, link);
        stream_destroy(stream);
    }
}


static void
transcoder_stream(void *opaque, streaming_message_t *msg)
{
    TVHTranscoder *self = opaque;
    streaming_start_t *start = NULL;

    switch (msg->sm_type) {
        case SMT_PACKET:
            transcoder_handle(self, msg->sm_data);
            msg->sm_data = NULL;
            streaming_msg_free(msg);
            break;

        case SMT_START:
            start = transcoder_start(self, msg->sm_data);
            streaming_start_unref(msg->sm_data);
            msg->sm_data = start;
            streaming_target_deliver2(self->output, msg);
            break;

        case SMT_STOP:
            transcoder_stop(self);
            /* Fallthrough */

        default:
            streaming_target_deliver2(self->output, msg);
            break;
    }
}


/* public */

streaming_target_t *
transcoder_create(streaming_target_t *output)
{
    static uint32_t id = 0;

    TVHTranscoder *self = calloc(1, sizeof(TVHTranscoder));
    if (self) {
        self->id = ++id;
        if (!self->id) {
            self->id = ++id;
        }
        SLIST_INIT(&self->streams);
        self->output = output;
        streaming_target_init((streaming_target_t *)self,
                              transcoder_stream, self, 0);
    }
    return (streaming_target_t *)self;
}


void
transcoder_destroy(streaming_target_t *st)
{
    TVHTranscoder *self = (TVHTranscoder *)st;

    if (self) {
        transcoder_stop(self);
        free(self);
        self = NULL;
    }
}
