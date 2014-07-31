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
 * GstWindowRenderer: Interface for rendering the video data on the
 * screen.
 */
#include "windowrenderer.h"
#include "media-player-marshal.h"

G_DEFINE_INTERFACE (GstWindowRenderer, gst_window_renderer, G_TYPE_OBJECT);

enum
{
  SIGNAL_SIZE_CHANGED,
  SIGNAL_LAST
};

static guint gst_window_renderer_signals[SIGNAL_LAST] = { 0 };

void
gst_window_renderer_set_window (GstWindowRenderer * renderer,
    ANativeWindow * native_window)
{
  GST_WINDOW_RENDERER_GET_INTERFACE (renderer)->set_window (renderer,
      native_window);
}

void
gst_window_renderer_release_window (GstWindowRenderer * renderer)
{
  GST_WINDOW_RENDERER_GET_INTERFACE (renderer)->release_window (renderer);
}

static void
gst_window_renderer_default_init (GstWindowRendererInterface * renderer)
{
  gst_window_renderer_signals[SIGNAL_SIZE_CHANGED] =
      g_signal_new ("size-changed", G_TYPE_FROM_INTERFACE (renderer),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWindowRendererInterface,
      size_changed), NULL, NULL, g_cclosure_user_marshal_VOID__INT_INT,
      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
}
