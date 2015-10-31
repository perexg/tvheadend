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


#ifndef TVH_TRANSCODING_HANDLER_H__
#define TVH_TRANSCODING_HANDLER_H__


#include "tvheadend.h"
#include "streaming.h"

#include "transcoding.h"

#include <libavcodec/avcodec.h>


/* TVHHandlerClass ************************************************************/

/* handler methods */
struct t_handler;

typedef void (*setup_meth) (struct t_handler *, streaming_start_component_t *,
                            transcoder_props_t *, int);
typedef th_pkt_t *(*handle_meth) (struct t_handler *, th_pkt_t *);
typedef int (*init_meth) (struct t_handler *);
typedef void (*finish_meth) (struct t_handler *);

/* handler class */
typedef struct t_handler_cls {
    enum AVMediaType type;
    const char *name;
    size_t size;

    setup_meth setup;
    handle_meth handle;
    init_meth init; // optional
    finish_meth finish; // optional

    SLIST_ENTRY(t_handler_cls) link;
} TVHHandlerClass;

void
handler_register_all(void);


/* TVHHandler *****************************************************************/

typedef struct t_handler {
    TVHHandlerClass *cls;
    int passthrough;
    AVCodecContext *decontext;
    AVCodecContext *encontext;
} TVHHandler;

TVHHandler *
handler_create(streaming_start_component_t *component,
               transcoder_props_t *props);

void
handler_destroy(TVHHandler *self);

th_pkt_t *
handler_handle(TVHHandler *self, th_pkt_t *packet);


#endif // TVH_TRANSCODING_HANDLER_H__
