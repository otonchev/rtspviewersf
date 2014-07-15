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
 * GstRTSPViewer: GstRTSPStreamer and GstRTSPWindowViewer creating a RTSP
 * pipeline which displays the video content on the screen.
 */
#include "rtspviewer.h"
#include "rtspstreamer.h"

static void gst_rtsp_viewer_streamer_interface_init (GstRTSPStreamerInterface * iface);
static GstElement * gst_rtsp_viewer_create_pipeline (GstRTSPStreamer * iface, GError ** error);

G_DEFINE_TYPE_WITH_CODE (GstRTSPViewer, gst_rtsp_viewer, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_RTSP_STREAMER,
        gst_rtsp_viewer_streamer_interface_init));

static void
gst_rtsp_viewer_class_init (GstRTSPViewerClass * klass)
{
}

static void
gst_rtsp_viewer_init (GstRTSPViewer * self)
{
}

static void
gst_rtsp_viewer_streamer_interface_init (GstRTSPStreamerInterface * iface)
{
  iface->create_pipeline = gst_rtsp_viewer_create_pipeline;
}

static GstElement *
gst_rtsp_viewer_create_pipeline (GstRTSPStreamer * streamer, GError ** error)
{
  GstElement * pipeline;

  pipeline = gst_parse_launch("playbin", error);
  return pipeline;
}
