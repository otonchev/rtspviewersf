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
 * GstRTSPViewer: Interface for creating the RTSP streaming pipeline
 */
#include "rtspstreamer.h"

G_DEFINE_INTERFACE (GstRTSPStreamer, gst_rtsp_streamer, G_TYPE_OBJECT);

GstElement *
gst_rtsp_streamer_create_pipeline (GstRTSPStreamer * streamer,
    GMainContext * context, GError ** error)
{
  return
      GST_RTSP_STREMAER_GET_INTERFACE (streamer)->create_pipeline (streamer,
          context, error);
}

void
gst_rtsp_streamer_set_uri (GstRTSPStreamer * streamer, const gchar * uri,
    const gchar * user, const gchar * pass)
{
  GST_RTSP_STREMAER_GET_INTERFACE (streamer)->set_uri (streamer, uri, user, pass);
}

static void
gst_rtsp_streamer_default_init (GstRTSPStreamerInterface * streamer)
{
}
