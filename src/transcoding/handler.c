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


#include "libav.h"

#include "handler.h"


extern TVHHandlerClass VideoHandler;
extern TVHHandlerClass AudioHandler;
extern TVHHandlerClass SubtitleHandler;


/* TVHHandlerClass ************************************************************/

SLIST_HEAD(TVHHandlers, t_handler_cls);
static struct TVHHandlers handlers;


static TVHHandlerClass *
handler_find_cls(enum AVMediaType type)
{
    TVHHandlerClass *cls = NULL;

    SLIST_FOREACH(cls, &handlers, link) {
        if (cls->type == type) {
            break;
        }
    }
    return cls;
}


static void
handler_register(TVHHandlerClass *cls)
{
    static size_t min_size = sizeof(TVHHandler);

    if (!cls->name || cls->size < min_size || !cls->setup || !cls->handle) {
        tvherror("transcode", "incomplete/wrong definition for %s handler class",
                 cls->name ? cls->name : "<unknown>");
        return;
    }
    tvhinfo("transcode", "registering %s handler", cls->name);
    SLIST_INSERT_HEAD(&handlers, cls, link);
}


/* public */

void
handler_register_all()
{
    handler_register(&VideoHandler);
    handler_register(&AudioHandler);
    handler_register(&SubtitleHandler);
}


/* TVHHandler *****************************************************************/

static AVCodecContext *
handler_alloc_context3(const AVCodec *codec)
{
    AVCodecContext *avctx = avcodec_alloc_context3(codec);
    if (avctx) {
        avctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    }
    return avctx;
}


static enum AVMediaType
handler_media_type(streaming_component_type_t type)
{
    if (SCT_ISVIDEO(type)) {
        return AVMEDIA_TYPE_VIDEO;
    }
    if (SCT_ISAUDIO(type)) {
        return AVMEDIA_TYPE_AUDIO;
    }
    if (SCT_ISSUBTITLE(type)) {
        return AVMEDIA_TYPE_SUBTITLE;
    }
    return AVMEDIA_TYPE_UNKNOWN;
}


static TVHHandler *
handler_new(enum AVMediaType type)
{
    const char *type_name = av_get_media_type_string(type);
    TVHHandlerClass *cls = NULL;
    TVHHandler *self = NULL;

    cls = handler_find_cls(type);
    if (!cls) {
        tvherror("transcode", "failed to find handler class for %s type",
                 type_name ? type_name : "<unknown>");
    }
    else {
        self = calloc(1, cls->size);
        if (self) {
            self->cls = cls;
        }
    }

    return self;
}


static const char *
handler_encodec_name(streaming_component_type_t type, transcoder_props_t *props)
{
    if (SCT_ISVIDEO(type)) {
        return props->tp_vcodec;
    }
    if (SCT_ISAUDIO(type)) {
        return props->tp_acodec;
    }
    if (SCT_ISSUBTITLE(type)) {
        return props->tp_scodec;
    }
    return NULL;
}


static int
handler_init(TVHHandler *self, streaming_start_component_t *component,
             transcoder_props_t *props)
{
    const char *encodec_name = handler_encodec_name(component->ssc_type, props);
    enum AVCodecID decodec_id =
        streaming_component_type2codec_id(component->ssc_type);
    streaming_component_type_t out_type = SCT_UNKNOWN;
    AVCodec *decodec = NULL, *encodec = NULL;
    self->passthrough = 0;

    if (!encodec_name) {
        return -1;
    }

    if (!strcmp(encodec_name, "copy") || decodec_id == AV_CODEC_ID_NONE) {
        self->passthrough = 1;
        goto finish;
    }

    decodec = avcodec_find_decoder(decodec_id);
    encodec = avcodec_find_encoder_by_name(encodec_name);
    if (!decodec || !encodec) {
        self->passthrough = 1;
        goto finish;
    }
    if (decodec->type != encodec->type) { // is this even possible?
        return -1;
    }

    out_type = codec_id2streaming_component_type(encodec->id);
    self->cls->setup(self, component, props, (out_type == component->ssc_type));
    if (self->passthrough) {
        goto finish;
    }

    self->decontext = handler_alloc_context3(decodec);
    self->encontext = handler_alloc_context3(encodec);
    if (!self->decontext || !self->encontext) {
        return -1;
    }

    if (self->cls->init && self->cls->init(self)) {
        return -1;
    }

finish:
    if (self->passthrough) {
        pktbuf_ref_inc(component->ssc_gh);
    }
    else {
        component->ssc_gh = NULL;
        component->ssc_type = out_type;
    }
    return 0;
}


/* public */

TVHHandler *
handler_create(streaming_start_component_t *component,
               transcoder_props_t *props)
{
    TVHHandler *self = NULL;
    enum AVMediaType type = handler_media_type(component->ssc_type);

    if (type != AVMEDIA_TYPE_UNKNOWN) {
        self = handler_new(type);
        if (self && handler_init(self, component, props)) {
            tvherror("transcode", "failed to init %s handler",
                     self->cls->name);
            handler_destroy(self);
        }
    }

    return self;
}

void
handler_destroy(TVHHandler *self)
{
    if (self) {
        if (self->cls->finish) {
            self->cls->finish(self);
        }
        if (self->encontext) {
            avcodec_free_context(&self->encontext);
        }
        if (self->decontext) {
            avcodec_free_context(&self->decontext);
        }
        free(self);
        self = NULL;
    }
}


th_pkt_t *
handler_handle(TVHHandler *self, th_pkt_t *packet)
{
    if (self->passthrough) {
        return packet;
    }
    return self->cls->handle(self, packet);
}
