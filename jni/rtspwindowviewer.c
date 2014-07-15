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
 * GstRTSPWindowViewer: Interface for rendering the video data on the
 * screen.
 */
#include "rtspwindowviewer.h"
#include "media-player-marshal.h"

G_DEFINE_INTERFACE (GstRTSPWindowViewer, gst_rtsp_window_viewer, G_TYPE_OBJECT);

enum
{
  SIGNAL_SIZE_CHANGED,
  SIGNAL_LAST
};

static guint gst_rtsp_window_viewer_signals[SIGNAL_LAST] = { 0 };

void
gst_rtsp_window_viewer_set_window (GstRTSPWindowViewer * viewer,
    ANativeWindow * native_window)
{
  GST_RTSP_WINDOW_VIEWER_GET_INTERFACE (viewer)->set_window (viewer,
      native_window);
}

void
gst_rtsp_window_viewer_release_window (GstRTSPWindowViewer * viewer)
{
  GST_RTSP_WINDOW_VIEWER_GET_INTERFACE (viewer)->release_window (viewer);
}

static void
gst_rtsp_window_viewer_default_init (GstRTSPWindowViewerInterface * viewer)
{
  gst_rtsp_window_viewer_signals[SIGNAL_SIZE_CHANGED] =
      g_signal_new ("size-changed", G_TYPE_FROM_INTERFACE (viewer), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstRTSPWindowViewerInterface, size_changed), NULL, NULL,
      g_cclosure_user_marshal_VOID__INT_INT, G_TYPE_NONE, 1, G_TYPE_INT, G_TYPE_INT);
}
