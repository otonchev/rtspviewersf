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
#ifndef __GST_MEDIA_PLAYER_H__
#define __GST_MEDIA_PLAYER_H__

#include <glib-object.h>
#include <gst/gst.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "rtspstreamer.h"
#include "windowrenderer.h"

G_BEGIN_DECLS

typedef struct _GstMediaPlayer GstMediaPlayer;
typedef struct _GstMediaPlayerClass GstMediaPlayerClass;
typedef struct _GstMediaPlayerPrivate GstMediaPlayerPrivate;

/*
 * Type macros.
 */
#define GST_TYPE_MEDIA_PLAYER                (gst_media_player_get_type ())
#define GST_IS_MEDIA_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MEDIA_PLAYER))
#define GST_IS_MEDIA_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MEDIA_PLAYER))
#define GST_MEDIA_PLAYER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MEDIA_PLAYER, GstMediaPlayerClass))
#define GST_MEDIA_PLAYER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MEDIA_PLAYER, GstMediaPlayer))
#define GST_MEDIA_PLAYER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MEDIA_PLAYER, GstMediaPlayerClass))
#define GST_MEDIA_PLAYER_CAST(obj)           ((GstMediaPlayer*)(obj))
#define GST_MEDIA_PLAYER_CLASS_CAST(klass)   ((GstMediaPlayerClass*)(klass))

struct _GstMediaPlayer {
  GObject parent_instance;

  /*< public >*/

  /*< private >*/
  GstMediaPlayerPrivate *priv;
};

struct _GstMediaPlayerClass {
  GObjectClass parent_class;

  /* signals */
  void (*new_status) (GstMediaPlayer * player, gchar * state);
  void (*error) (GstMediaPlayer * player, gchar * error);
  void (*gst_initialized) (GstMediaPlayer * player);
  void (*size_changed) (GstMediaPlayer * player, gint width, gint height);
  void (*new_position) (GstMediaPlayer * player, gint position, gint duration);

  /*< public >*/

  /*< private >*/
};

GType gst_media_player_get_type (void);

GstMediaPlayer *gst_media_player_new(GstRTSPStreamer * streamer, GstWindowRenderer * renderer);
gboolean gst_media_player_setup_thread (GstMediaPlayer *player, GError ** error);
gboolean gst_media_player_set_state (GstMediaPlayer * player, GstState state);
gboolean gst_media_player_set_position (GstMediaPlayer * player, gint64 position);
void gst_media_player_set_uri (GstMediaPlayer * player, const gchar * url, const gchar * user, const gchar * pass);
void gst_media_player_set_native_window (GstMediaPlayer * player, ANativeWindow * native_window);
void gst_media_player_release_native_window (GstMediaPlayer * player);

G_END_DECLS

#endif /* __GST_MEDIA_PLAYER_H__ */
