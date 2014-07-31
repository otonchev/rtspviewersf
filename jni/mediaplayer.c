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
#include <gst/video/video.h>
#include <gst/video/videooverlay.h>

#include "mediaplayer.h"
#include "media-player-marshal.h"
#include "rtspstreamer.h"
#include "windowrenderer.h"

#define GST_MEDIA_PLAYER_GET_PRIVATE(obj)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_MEDIA_PLAYER, GstMediaPlayerPrivate))

struct _GstMediaPlayerPrivate
{
  GMainLoop *main_loop;         /* GLib main loop */
  GMainContext *context;        /* GLib context used to run the main loop */
  GstElement *pipeline;         /* The running pipeline */
  GstState state;               /* Current pipeline state */
  GstState target_state;        /* Desired pipeline state, to be set once buffering is complete */
  gint64 duration;              /* Cached clip duration */
  gchar *user;                  /* User id for RTSP authentication */
  gchar *pass;                  /* Password for RTSP authentication */
  gboolean is_live;             /* Is media live */
  GstClockTime last_seek_time;  /* For seeking overflow prevention (throttling) */
  gint64 desired_position;      /* Position to seek to, once the pipeline is running */
  pthread_t gst_app_thread;     /* The thread running the main loop */
  GstRTSPStreamer *streamer;
  GstWindowRenderer *renderer;
};

/* Do not allow seeks to be performed closer than this distance. It is visually useless, and will probably
 * confuse some demuxers. */
#define SEEK_MIN_DELAY (500 * GST_MSECOND)

/* object properties */
enum
{
  PROP_0,
  PROP_RTSP_STREAMER,
  PROP_WINDOW_RENDERER
};

enum
{
  SIGNAL_NEW_STATUS,
  SIGNAL_ERROR,
  SIGNAL_NEW_POSITION,
  SIGNAL_LAST
};

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category

static guint gst_media_player_signals[SIGNAL_LAST] = { 0 };

static void execute_seek (GstMediaPlayer * player, gint64 desired_position);
static gboolean delayed_seek_cb (gpointer user_data);
static void gst_media_player_finalize (GObject * obj);
static void gst_media_player_get_property (GObject *object, guint property_id,
    GValue *value, GParamSpec *pspec);
static void gst_media_player_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE (GstMediaPlayer, gst_media_player, G_TYPE_OBJECT);

static void
gst_media_player_class_init (GstMediaPlayerClass * klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GstMediaPlayerPrivate));

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_media_player_finalize;
  gobject_class->get_property = gst_media_player_get_property;
  gobject_class->set_property = gst_media_player_set_property;

  g_object_class_install_property (gobject_class,
      PROP_RTSP_STREAMER, g_param_spec_object ("rtsp-streamer", "RTSPStreamer",
      "RTSP Streamer", GST_TYPE_RTSP_STREAMER, G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
      PROP_WINDOW_RENDERER, g_param_spec_object ("window-renderer",
      "WindowRenderer", "Window Renderer", GST_TYPE_WINDOW_RENDERER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gst_media_player_signals[SIGNAL_NEW_STATUS] =
      g_signal_new ("new-status", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstMediaPlayerClass, new_status), NULL, NULL,
      g_cclosure_marshal_VOID__CHAR, G_TYPE_NONE, 1, G_TYPE_POINTER);

  gst_media_player_signals[SIGNAL_ERROR] =
      g_signal_new ("error", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstMediaPlayerClass, error), NULL, NULL,
      g_cclosure_marshal_VOID__CHAR, G_TYPE_NONE, 1, G_TYPE_POINTER);

  gst_media_player_signals[SIGNAL_NEW_POSITION] =
      g_signal_new ("new-position", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstMediaPlayerClass, new_position),
      NULL, NULL, g_cclosure_user_marshal_VOID__INT_INT, G_TYPE_NONE, 2,
      G_TYPE_INT, G_TYPE_INT);
}

static void
gst_media_player_init (GstMediaPlayer * player)
{
}

static void
gst_media_player_get_property (GObject *object, guint property_id,
    GValue *value, GParamSpec *pspec)
{
  GstMediaPlayer *player = GST_MEDIA_PLAYER (object);
  GstMediaPlayerPrivate *priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  switch (property_id)
  {
    case PROP_RTSP_STREAMER:
      g_value_set_object (value, priv->streamer);
      break;
    case PROP_WINDOW_RENDERER:
      g_value_set_object (value, priv->renderer);
      break;
  }
}

static void
gst_media_player_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
  GstMediaPlayer *player = GST_MEDIA_PLAYER (object);
  GstMediaPlayerPrivate *priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  switch (property_id)
  {
    case PROP_RTSP_STREAMER:
      priv->streamer = g_value_get_object (value);
      break;
    case PROP_WINDOW_RENDERER:
      priv->renderer = g_value_get_object (value);
      break;
  }
}

GstMediaPlayer*
gst_media_player_new (GstRTSPStreamer * streamer, GstWindowRenderer * renderer)
{
  GstMediaPlayer *player;

  player = g_object_new (GST_TYPE_MEDIA_PLAYER, "rtsp-streamer", streamer,
      "window-renderer", renderer, NULL);

  return player;
}

/* Perform seek, if we are not too close to the previous seek. Otherwise,
 * schedule the seek for some time in the future. */
static void
execute_seek (GstMediaPlayer * player, gint64 desired_position)
{
  gint64 diff;
  GstMediaPlayerPrivate *priv;

  GST_DEBUG ("new seek");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (desired_position == GST_CLOCK_TIME_NONE)
    return;

  diff = gst_util_get_timestamp () - priv->last_seek_time;

  if (GST_CLOCK_TIME_IS_VALID (priv->last_seek_time) && diff < SEEK_MIN_DELAY) {
    /* The previous seek was too close, delay this one */
    GSource *timeout_source;

    if (priv->desired_position == GST_CLOCK_TIME_NONE) {
      /* There was no previous seek scheduled. Setup a timer for some time in
       * the future */
      timeout_source =
          g_timeout_source_new ((SEEK_MIN_DELAY - diff) / GST_MSECOND);
      g_source_set_callback (timeout_source, (GSourceFunc)delayed_seek_cb,
          player, NULL);
      g_source_attach (timeout_source, priv->context);
      g_source_unref (timeout_source);
    }
    /* Update the desired seek position. If multiple petitions are received
     * before it is time to perform a seek, only the last one is remembered. */
    priv->desired_position = desired_position;
    GST_DEBUG ("Throttling seek to %" GST_TIME_FORMAT
        ", will be in %" GST_TIME_FORMAT, GST_TIME_ARGS (desired_position),
        GST_TIME_ARGS (SEEK_MIN_DELAY - diff));
  } else {
    /* Perform the seek now */
    GST_DEBUG ("Seeking to %" GST_TIME_FORMAT,
        GST_TIME_ARGS (desired_position));
    priv->last_seek_time = gst_util_get_timestamp ();
    gst_element_seek_simple (priv->pipeline, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, desired_position);
    priv->desired_position = GST_CLOCK_TIME_NONE;
  }
}

/* Delayed seek callback. This gets called by the timer setup in the above
 * function. */
static gboolean
delayed_seek_cb (gpointer user_data)
{
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  GST_DEBUG ("Doing delayed seek to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (priv->desired_position));

  execute_seek (player, priv->desired_position);
  return FALSE;
}

static void
error_cb (GstBus *bus, GstMessage *msg, gpointer userdata)
{
  GError *err;
  gchar *debug_info;
  gchar *message_string;
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)userdata;

  GST_DEBUG ("error in pipeline");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  gst_message_parse_error (msg, &err, &debug_info);
  message_string = g_strdup_printf ("Error received from element %s: %s",
      GST_OBJECT_NAME (msg->src), err->message);
  g_clear_error (&err);
  g_free (debug_info);

  g_signal_emit (player, gst_media_player_signals[SIGNAL_ERROR], 0,
      message_string, NULL);

  g_free (message_string);
  priv->target_state = GST_STATE_NULL;
  gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  g_signal_emit (player, gst_media_player_signals[SIGNAL_NEW_STATUS], 0,
      "STOPPED", NULL);
}

static void
eos_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  GST_DEBUG ("eos");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  priv->target_state = GST_STATE_PAUSED;
  priv->is_live = (gst_element_set_state (priv->pipeline, GST_STATE_PAUSED) ==
      GST_STATE_CHANGE_NO_PREROLL);
  execute_seek (player, 0);
}

static void
state_changed_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  GstState old_state;
  GstState new_state;
  GstState pending_state;
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  GST_DEBUG ("state changed");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  /* Only pay attention to messages coming from the pipeline, not its
   * children */
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (priv->pipeline)) {
    priv->state = new_state;
    gchar *message =
        g_strdup_printf ("%s", gst_element_state_get_name (new_state));

    g_signal_emit (player, gst_media_player_signals[SIGNAL_NEW_STATUS], 0,
        message, NULL);

    g_free (message);

    /* The Ready to Paused state change is particularly interesting: */
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
       /* If there was a scheduled seek, perform it now that we have moved to
        * the Paused state */
      if (GST_CLOCK_TIME_IS_VALID (priv->desired_position))
        execute_seek (player, priv->desired_position);
    }
  }
}

/* Called when the duration of the media changes. Just mark it as unknown, so we
 * re-query it in the next UI refresh. */
static void
duration_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  GST_DEBUG ("new duration");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  priv->duration = GST_CLOCK_TIME_NONE;
}

/* Called when buffering messages are received. We inform the UI about the
 * current buffering level and keep the pipeline paused until 100% buffering is
 * reached. At that point, set the desired state. */
static void
buffering_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  gint percent;
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (priv->is_live)
    return;

  GST_DEBUG ("buffering");

  gst_message_parse_buffering (msg, &percent);
  if (percent < 100 && priv->target_state >= GST_STATE_PAUSED) {
    gchar * message_string = g_strdup_printf ("Buffering %d%%", percent);
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);

    g_signal_emit (player, gst_media_player_signals[SIGNAL_NEW_STATUS], 0,
        message_string, NULL);

    g_free (message_string);
  } else if (priv->target_state >= GST_STATE_PLAYING) {
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  } else if (priv->target_state >= GST_STATE_PAUSED) {
    g_signal_emit (player, gst_media_player_signals[SIGNAL_NEW_STATUS], 0,
        "Buffering complete", NULL);
  }
}

/* Called when the clock is lost */
static void
clock_lost_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  GST_DEBUG ("clock lost");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (priv->target_state >= GST_STATE_PLAYING) {
    gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
    gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  }
}

/* If we have pipeline and it is running, query the current position and clip
 * duration and inform the application */
static gboolean
check_position (gpointer user_data)
{
  GstFormat fmt = GST_FORMAT_TIME;
  gint64 current = -1;
  gint64 position;
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  GST_LOG ("updating position");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  /* We do not want to update anything unless we have a working pipeline in the
   * PAUSED or PLAYING state */
  if (!priv || !priv->pipeline || priv->state < GST_STATE_PAUSED)
    return TRUE;

  /* If we didn't know it yet, query the stream duration */
  if (!GST_CLOCK_TIME_IS_VALID (priv->duration)) {
    if (!gst_element_query_duration (priv->pipeline, fmt, &priv->duration)) {
      GST_WARNING ("Could not query current duration (normal for still "
          "pictures)");
      priv->duration = 0;
    }
  }

  if (!gst_element_query_position (priv->pipeline, fmt, &position)) {
    GST_WARNING ("Could not query current position (normal for still "
        "pictures)");
    position = 0;
  }

  /* Java expects these values in milliseconds, and GStreamer provides
   * nanoseconds */
  g_signal_emit (player, gst_media_player_signals[SIGNAL_NEW_POSITION], 0,
      position / GST_MSECOND, priv->duration / GST_MSECOND, NULL);

  return TRUE;
}

static void *
thread_function (void *user_data)
{
  GstMediaPlayerPrivate *priv;
  GstMediaPlayer *player = (GstMediaPlayer *)user_data;

  GST_DEBUG ("Main loop thread");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  g_main_context_push_thread_default (priv->context);

  g_main_loop_run (priv->main_loop);

  GST_DEBUG ("Exited main loop");

  /* Free resources */
  g_main_context_pop_thread_default (priv->context);

  return NULL;
}

/**
 * gst_media_player_setup_thread:
 * @player: a #GstMediaPlayer
 * @error: #GError or NULL
 *
 * Sets up the pipeline and starts the main loop thread.
 */
gboolean
gst_media_player_setup_thread (GstMediaPlayer *player, GError ** error)
{
  GstMediaPlayerPrivate *priv;
  GstBus *bus;
  GSource *bus_source;
  GSource *timeout_source;

  g_return_val_if_fail (GST_IS_MEDIA_PLAYER (player), FALSE);

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  GST_DEBUG ("Creating main context... (GstMediaPlayer: %p)", player);
  priv->context = g_main_context_new ();

  priv->pipeline = gst_rtsp_streamer_create_pipeline (priv->streamer,
      priv->context, error);
  if (priv->pipeline == NULL) {
    return FALSE;
  }

  /* Set the pipeline to READY, so it can already accept a window handle, if we
   * have one */
  priv->target_state = GST_STATE_READY;
  gst_element_set_state (priv->pipeline, GST_STATE_READY);

  /* Instruct the bus to emit signals for each received message, and connect to
   * the interesting signals */
  bus = gst_element_get_bus (priv->pipeline);
  bus_source = gst_bus_create_watch (bus);
  g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,
      NULL, NULL);
  g_source_attach (bus_source, priv->context);
  g_source_unref (bus_source);
  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb,
      player);
  g_signal_connect (G_OBJECT (bus), "message::eos", (GCallback)eos_cb, player);
  g_signal_connect (G_OBJECT (bus), "message::state-changed",
      (GCallback)state_changed_cb, player);
  g_signal_connect (G_OBJECT (bus), "message::duration", (GCallback)duration_cb,
      player);
  g_signal_connect (G_OBJECT (bus), "message::buffering",
      (GCallback)buffering_cb, player);
  g_signal_connect (G_OBJECT (bus), "message::clock-lost",
      (GCallback)clock_lost_cb, player);
  gst_object_unref (bus);

  /* Create a GLib Main Loop */
  GST_DEBUG ("Creating main loop... (GstMediaPlayer: %p)", player);
  priv->main_loop = g_main_loop_new (priv->context, FALSE);

  /* Register a function that GLib will call 4 times per second */
  timeout_source = g_timeout_source_new (250);
  g_source_set_callback (timeout_source, (GSourceFunc)check_position, player,
      NULL);
  g_source_attach (timeout_source, priv->context);
  g_source_unref (timeout_source);

  priv->desired_position = GST_CLOCK_TIME_NONE;
  priv->last_seek_time = GST_CLOCK_TIME_NONE;

  GST_DEBUG_CATEGORY_INIT (debug_category, "mediaplayer", 0, "Media Player");
  gst_debug_set_threshold_for_name ("mediaplayer", GST_LEVEL_DEBUG);

  pthread_create (&priv->gst_app_thread, NULL, &thread_function, player);

  return TRUE;
}

static void
gst_media_player_finalize (GObject * obj)
{
  GstMediaPlayer *player;
  GstMediaPlayerPrivate *priv;

  player = GST_MEDIA_PLAYER (obj);
  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (priv->main_loop != NULL) {
    GST_DEBUG ("Quitting main loop...");
    g_main_loop_quit (priv->main_loop);
  }

  if (priv->gst_app_thread != 0) {
    GST_DEBUG ("Waiting for thread to finish...");
    pthread_join (priv->gst_app_thread, NULL);
    priv->gst_app_thread = 0;
  }

  if (priv->main_loop != NULL) {
    g_main_loop_unref (priv->main_loop);
    priv->main_loop = NULL;
  }

  if (priv->context != NULL) {
    g_main_context_unref (priv->context);
    priv->context = NULL;
  }

  priv->target_state = GST_STATE_NULL;

  if (priv->pipeline != NULL) {
    GST_DEBUG ("Stopping pipeline");
    gst_element_set_state (priv->pipeline, GST_STATE_NULL);
    gst_object_unref (priv->pipeline);
    priv->pipeline = NULL;
  }

  if (priv->user != NULL) {
    g_free (priv->user);
    priv->user = NULL;
  }
  if (priv->pass != NULL) {
    g_free (priv->pass);
    priv->pass = NULL;
  }

  if (priv->renderer != NULL) {
    g_object_unref (priv->renderer);
    priv->renderer = NULL;
  }

  if (priv->streamer != NULL) {
    g_object_unref (priv->streamer);
    priv->streamer = NULL;
  }

  GST_DEBUG ("Done with cleanup");
  G_OBJECT_CLASS (gst_media_player_parent_class)->finalize (obj);
}

/**
 * gst_media_player_set_state:
 * @player: a #GstMediaPlayer
 * @state: a #GstState
 *
 * Sets a new state.
 */
gboolean
gst_media_player_set_state (GstMediaPlayer * player, GstState state)
{
  GstMediaPlayerPrivate *priv;

  g_return_val_if_fail (GST_IS_MEDIA_PLAYER (player), FALSE);

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  GST_DEBUG ("Setting state to %d", state);
  priv->duration = GST_CLOCK_TIME_NONE;
  priv->target_state = state;
  priv->is_live = (gst_element_set_state (priv->pipeline, state) ==
      GST_STATE_CHANGE_NO_PREROLL);

  return TRUE;
}

/**
 * gst_media_player_set_position:
 * @player: a #GstMediaPlayer
 *
 * Sets a new position in the media.
 */
gboolean
gst_media_player_set_position (GstMediaPlayer * player, gint64 position)
{
  GstMediaPlayerPrivate *priv;

  g_return_val_if_fail (GST_IS_MEDIA_PLAYER (player), FALSE);

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (priv->state >= GST_STATE_PAUSED) {
    execute_seek (player, position);
  } else {
    GST_DEBUG ("Scheduling seek to %" GST_TIME_FORMAT " for later",
        GST_TIME_ARGS (position));
    priv->desired_position = position;
  }

  return TRUE;
}

/**
 * gst_media_player_set_uri:
 * @player: a #GstMediaPlayer
 * @uri: uri
 * @user: user id for the RTSP authentication
 * @pass: password for the RTSP authentication
 *
 * Sets the source Uri.
 */
void
gst_media_player_set_uri (GstMediaPlayer * player, const gchar * uri,
    const gchar * user, const gchar * pass)
{
  GstMediaPlayerPrivate *priv;

  g_return_if_fail (GST_IS_MEDIA_PLAYER (player));

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  g_return_if_fail (priv->pipeline != NULL);
  g_return_if_fail (priv->streamer != NULL);

  if (priv->target_state >= GST_STATE_READY)
    gst_element_set_state (priv->pipeline, GST_STATE_READY);

  gst_rtsp_streamer_set_uri (priv->streamer, uri, user, pass);

  priv->is_live = (gst_element_set_state (priv->pipeline, priv->target_state) ==
      GST_STATE_CHANGE_NO_PREROLL);
}

/**
 * gst_media_player_release_native_window:
 * @player: a #GstMediaPlayer
 * @native_window: a #ANativeWindow
 *
 * Adds a new android window. Can be released with
 * gst_media_player_release_native_window ().
 */
void
gst_media_player_set_native_window (GstMediaPlayer * player,
    ANativeWindow * native_window)
{
  GstMediaPlayerPrivate *priv;

  g_return_if_fail (GST_IS_MEDIA_PLAYER (player));

  GST_DEBUG ("Setting new native window %p", native_window);

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (priv->renderer == NULL)
    return;

  gst_window_renderer_set_window (priv->renderer, native_window);
}

/**
 * gst_media_player_release_native_window:
 * @player: a #GstMediaPlayer
 *
 * Releases android window previously set with
 * gst_media_player_set_native_window ().
 */
void
gst_media_player_release_native_window (GstMediaPlayer * player)
{
  GstMediaPlayerPrivate *priv;

  g_return_if_fail (GST_IS_MEDIA_PLAYER (player));

  GST_DEBUG ("Releasing native window");

  priv = GST_MEDIA_PLAYER_GET_PRIVATE (player);

  if (priv->renderer == NULL)
    return;

  gst_window_renderer_release_window (priv->renderer);
}
