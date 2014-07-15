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
#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#ifndef __GST_RTSP_WINDOW_VIEWER_H__
#define __GST_RTSP_WINDOW_VIEWER_H__

typedef struct _GstRTSPWindowViewer GstRTSPWindowViewer; /* dummy object */
typedef struct _GstRTSPWindowViewerInterface GstRTSPWindowViewerInterface;

#define GST_TYPE_RTSP_WINDOW_VIEWER    (gst_rtsp_window_viewer_get_type ())
#define GST_RTSP_WINDOW_VIEWER(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTSP_WINDOW_VIEWER, GstRTSPWindowViewer))
#define GST_IS_RTSP_WINDOW_VIEWER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTSP_WINDOW_VIEWER))
#define GST_RTSP_WINDOW_VIEWER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_RTSP_WINDOW_VIEWER, GstRTSPWindowViewerInterface))

struct _GstRTSPWindowViewerInterface {
  GTypeInterface iface;

  /*< signals >*/
  void (*size_changed) (GstRTSPWindowViewer * viewer, gint width, gint height);

  /*< methods >*/
  void (*set_window) (GstRTSPWindowViewer * viewer,
      ANativeWindow * native_window);
  void (*release_window) (GstRTSPWindowViewer * viewer);
};

extern GQuark gst_rtsp_window_viewer_error_quark (void);

GType         gst_rtsp_window_viewer_get_type   (void);

void gst_rtsp_window_viewer_set_window (GstRTSPWindowViewer * viewer,
    ANativeWindow * native_window);
void gst_rtsp_window_viewer_release_window (GstRTSPWindowViewer * viewer);

#endif /* __GST_RTSP_WINDOW_VIEWER_H__ */
