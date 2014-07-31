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
 * GstWindowRenderer: Interface for rendering the video data on the screen.
 */
#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#ifndef __GST_WINDOW_RENDERER_H__
#define __GST_WINDOW_RENDERER_H__

typedef struct _GstWindowRenderer GstWindowRenderer; /* dummy object */
typedef struct _GstWindowRendererInterface GstWindowRendererInterface;

#define GST_TYPE_WINDOW_RENDERER    (gst_window_renderer_get_type ())
#define GST_WINDOW_RENDERER(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_WINDOW_RENDERER, GstWindowRenderer))
#define GST_IS_WINDOW_RENDERER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_WINDOW_RENDERER))
#define GST_WINDOW_RENDERER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_WINDOW_RENDERER, GstWindowRendererInterface))

struct _GstWindowRendererInterface {
  GTypeInterface iface;

  /*< signals >*/
  void (*size_changed) (GstWindowRenderer * renderer, gint width, gint height);

  /*< methods >*/
  void (*set_window) (GstWindowRenderer * renderer,
      ANativeWindow * native_window);
  void (*release_window) (GstWindowRenderer * renderer);
};

extern GQuark gst_window_renderer_error_quark (void);

GType         gst_window_renderer_get_type   (void);

void gst_window_renderer_set_window (GstWindowRenderer * renderer,
    ANativeWindow * native_window);
void gst_window_renderer_release_window (GstWindowRenderer * renderer);

#endif /* __GST_WINDOW_RENDERER_H__ */
