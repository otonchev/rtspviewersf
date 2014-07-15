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
#include <gst/video/video.h>
#include <gst/video/videooverlay.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "rtspviewer.h"
#include "rtspstreamer.h"
#include "rtspwindowviewer.h"
#include "media-player-marshal.h"

#define GST_RTSP_VIEWER_GET_PRIVATE(obj)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_RTSP_VIEWER, GstRTSPViewerPrivate))

struct _GstRTSPViewerPrivate
{
  GstElement *pipeline;
  ANativeWindow *native_window;
};

static void gst_rtsp_viewer_finalize (GObject * obj);
static void gst_rtsp_viewer_streamer_interface_init (GstRTSPStreamerInterface *
    iface);
static GstElement * gst_rtsp_viewer_create_pipeline (GstRTSPStreamer * iface,
    GMainContext * context, GError ** error);
static void gst_rtsp_viewer_window_viewer_interface_init (GstRTSPWindowViewerInterface *
    iface);
static void gst_rtsp_viewer_set_window (GstRTSPWindowViewer * viewer,
    ANativeWindow * native_window);
static void gst_rtsp_viewer_release_window (GstRTSPWindowViewer * viewer);

G_DEFINE_TYPE_WITH_CODE (GstRTSPViewer, gst_rtsp_viewer, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_RTSP_STREAMER,
        gst_rtsp_viewer_streamer_interface_init)
    G_IMPLEMENT_INTERFACE (GST_TYPE_RTSP_WINDOW_VIEWER,
        gst_rtsp_viewer_window_viewer_interface_init));

static void
gst_rtsp_viewer_class_init (GstRTSPViewerClass * klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GstRTSPViewerPrivate));

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_rtsp_viewer_finalize;
}

static void
gst_rtsp_viewer_init (GstRTSPViewer * self)
{
}

static void
gst_rtsp_viewer_finalize (GObject * obj)
{
  GstRTSPViewer *viewer;
  GstRTSPViewerPrivate *priv;

  viewer = GST_RTSP_VIEWER (obj);
  priv = GST_RTSP_VIEWER_GET_PRIVATE (viewer);

  gst_rtsp_viewer_release_window (GST_RTSP_WINDOW_VIEWER (viewer));

  if (priv->pipeline != NULL) {
    gst_object_unref (priv->pipeline);
    priv->pipeline = NULL;
  }

  G_OBJECT_CLASS (gst_rtsp_viewer_parent_class)->finalize (obj);
}

static void
gst_rtsp_viewer_streamer_interface_init (GstRTSPStreamerInterface * iface)
{
  iface->create_pipeline = gst_rtsp_viewer_create_pipeline;
}

static void
gst_rtsp_viewer_window_viewer_interface_init (GstRTSPWindowViewerInterface *
    iface)
{
  iface->set_window = gst_rtsp_viewer_set_window;
  iface->release_window = gst_rtsp_viewer_release_window;
}

/* Retrieve the video sink's Caps and tell the application about the media size */
static void
check_media_size (GstRTSPViewer *viewer)
{
  GstElement *video_sink;
  GstPad *video_sink_pad;
  GstCaps *caps;
  int width;
  int height;
  GstVideoInfo vinfo;
  GstRTSPViewerPrivate *priv;

  priv = GST_RTSP_VIEWER_GET_PRIVATE (viewer);

  /* Retrieve the Caps at the entrance of the video sink */
  g_object_get (priv->pipeline, "video-sink", &video_sink, NULL);
  video_sink_pad = gst_element_get_static_pad (video_sink, "sink");
  caps = gst_pad_get_current_caps (video_sink_pad);

  if (gst_video_info_from_caps (&vinfo, caps)) {
	int par_n, par_d;

	width = vinfo.width;
	height = vinfo.height;
	par_n = vinfo.par_n;
	par_d = vinfo.par_d;

    width = width * par_n / par_d;

	GST_DEBUG ("Media size is %dx%d, notifying application", width, height);

    g_signal_emit_by_name (viewer, "size-changed", 0, width, height, NULL);
  }

  gst_caps_unref(caps);
  gst_object_unref (video_sink_pad);
  gst_object_unref(video_sink);
}

static void
state_changed_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  GstState old_state;
  GstState new_state;
  GstState pending_state;
  GstRTSPViewerPrivate *priv;
  GstRTSPViewer *viewer = (GstRTSPViewer *)user_data;

  GST_DEBUG ("state changed");

  priv = GST_RTSP_VIEWER_GET_PRIVATE (viewer);

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  /* Only pay attention to messages coming from the pipeline, not its children */
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (priv->pipeline)) {
    /* The Ready to Paused state change is particularly interesting: */
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      /* By now the sink already knows the media size */
      check_media_size (viewer);
    }
  }
}

static GstElement *
gst_rtsp_viewer_create_pipeline (GstRTSPStreamer * streamer, GMainContext * context, GError ** error)
{
  GstElement * pipeline;
  GstBus *bus;
  GSource *bus_source;
  GstRTSPViewerPrivate *priv;

  priv = GST_RTSP_VIEWER_GET_PRIVATE (streamer);

  priv->pipeline = gst_parse_launch("playbin", error);

  bus = gst_element_get_bus (priv->pipeline);
  bus_source = gst_bus_create_watch (bus);
  g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func, NULL, NULL);
  g_source_attach (bus_source, context);
  g_source_unref (bus_source);
  g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)state_changed_cb, streamer);
  gst_object_unref (bus);

  return priv->pipeline;
}

static void
gst_rtsp_viewer_set_window (GstRTSPWindowViewer * viewer,
    ANativeWindow * native_window)
{
  GstRTSPViewerPrivate *priv;

  priv = GST_RTSP_VIEWER_GET_PRIVATE (viewer);

  if (priv->native_window != NULL) {
    ANativeWindow_release (priv->native_window);

    if (priv->native_window == native_window) {
      if (priv->pipeline) {
        gst_video_overlay_expose (GST_VIDEO_OVERLAY (priv->pipeline));
      }
      return;
    } else {
      gst_rtsp_viewer_release_window (viewer);
    }
  }

  priv->native_window = native_window;
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (priv->pipeline), (guintptr)priv->native_window);
}

static void
gst_rtsp_viewer_release_window (GstRTSPWindowViewer * viewer)
{
  GstRTSPViewerPrivate *priv;

  priv = GST_RTSP_VIEWER_GET_PRIVATE (viewer);

  if (priv->pipeline != NULL) {
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (priv->pipeline), (guintptr)NULL);
    gst_element_set_state (priv->pipeline, GST_STATE_READY);
  }

  if (priv->native_window != NULL) {
    ANativeWindow_release (priv->native_window);
    priv->native_window = NULL;
  }
}
