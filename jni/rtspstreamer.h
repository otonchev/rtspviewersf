/*
 * Copyright (C) 2014 Ognyan Tonchev <otonchev at gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * GstRTSPStreamer: Interface for creating the RTSP streaming pipeline
 */
#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#ifndef __GST_RTSP_STREAMER_H__
#define __GST_RTSP_STREAMER_H__

typedef struct _GstRTSPStreamer GstRTSPStreamer; /* dummy object */
typedef struct _GstRTSPStreamerInterface GstRTSPStreamerInterface;

#define GST_TYPE_RTSP_STREAMER         (gst_rtsp_streamer_get_type ())
#define GST_RTSP_STREAMER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTSP_STREAMER, GstRTSPStreamer))
#define GST_IS_RTSP_STREAMER(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTSP_STREAMER))
#define GST_RTSP_STREMAER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_RTSP_STREAMER, GstRTSPStreamerInterface))

struct _GstRTSPStreamerInterface {
  GTypeInterface iface;

  GstElement * (*create_pipeline) (GstRTSPStreamer * streamer, GMainContext * context, GError ** error);
  void (*set_uri) (GstRTSPStreamer * streamer, const gchar * uri, const gchar * user, const gchar * pass);
};

extern GQuark gst_rtsp_streamer_error_quark (void);

GType         gst_rtsp_streamer_get_type   (void);

GstElement * gst_rtsp_streamer_create_pipeline (GstRTSPStreamer * streamer, GMainContext * context, GError ** error);
void gst_rtsp_streamer_set_uri (GstRTSPStreamer * streamer, const gchar * uri, const gchar * user, const gchar * pass);

#endif /* __GST_RTSP_STREAMER_H__ */
