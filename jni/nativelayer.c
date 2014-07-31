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
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/video/video.h>
#include <pthread.h>

#include "mediaplayer.h"
#include "rtspstreamer.h"
#include "rtspviewer.h"

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category

/*
 * These macros provide a way to store the native pointer to CustomData, which
 * might be 32 or 64 bits, into a jlong, which is always 64 bits, without
 * warnings.
 */
#if GLIB_SIZEOF_VOID_P == 8
# define J_TO_NATIVEP(jdata) (CustomData *)jdata
# define NATIVEP_TO_J(ndata) (jlong)ndata
#else
# define J_TO_NATIVEP(jdata) (CustomData *)(jint)jdata
# define NATIVEP_TO_J(ndata) (jlong)(jint)ndata
#endif

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
  jobject app;                  /* Application instance, used to call its methods.
                                 * A global reference is kept. */
  GstMediaPlayer *player;       /* GstMediaPlayer instance, this is the pipeline */
} CustomData;

/* These global variables cache values which are not changing during
 * execution */
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jmethodID set_error_method_id;
static jmethodID set_state_method_id;
static jmethodID set_current_position_method_id;
static jmethodID on_media_size_changed_method_id;

/*
 * Private methods
 */

/* Register this thread with the VM */
static JNIEnv *
attach_current_thread (void)
{
  JNIEnv *env;
  JavaVMAttachArgs args;

  GST_DEBUG ("Attaching thread %p", g_thread_self ());
  args.version = JNI_VERSION_1_4;
  args.name = NULL;
  args.group = NULL;

  if ((*java_vm)->AttachCurrentThread (java_vm, &env, &args) < 0) {
    GST_ERROR ("Failed to attach current thread");
    return NULL;
  }

  return env;
}

/* Unregister this thread from the VM */
static void
detach_current_thread (void *env)
{
  GST_DEBUG ("Detaching thread %p", g_thread_self ());
  (*java_vm)->DetachCurrentThread (java_vm);
}

/* Retrieve the JNI environment for this thread */
static JNIEnv *
get_jni_env (void)
{
  JNIEnv *env;

  if ((env = pthread_getspecific (current_jni_env)) == NULL) {
    env = attach_current_thread ();
    pthread_setspecific (current_jni_env, env);
  }

  return env;
}

/*
 * Callbacks
 */
static void
new_status (GstMediaPlayer * player, gchar * state, gpointer user_data)
{
  CustomData *data = (CustomData *) user_data;
  JNIEnv *env = get_jni_env ();
  jstring jmessage;

  GST_DEBUG ("Setting state to: %s", state);

  jmessage = (*env)->NewStringUTF (env, state);

  (*env)->CallVoidMethod (env, data->app, set_state_method_id,
      NATIVEP_TO_J (data), jmessage);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
  }

  (*env)->DeleteLocalRef (env, jmessage);
}

static void
error (GstMediaPlayer * player, gchar * error, gpointer user_data)
{
  CustomData *data = (CustomData *) user_data;
  JNIEnv *env = get_jni_env ();
  jstring jmessage;

  GST_DEBUG ("Setting error to: %s", error);

  jmessage = (*env)->NewStringUTF (env, error);

  (*env)->CallVoidMethod (env, data->app, set_error_method_id,
      NATIVEP_TO_J (data), jmessage);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
  }

  (*env)->DeleteLocalRef (env, jmessage);
}

static void
size_changed (GstMediaPlayer * player, gint width, gint height,
    gpointer user_data)
{
  CustomData *data = (CustomData *) user_data;
  JNIEnv *env = get_jni_env ();

  (*env)->CallVoidMethod (env, data->app, on_media_size_changed_method_id,
      NATIVEP_TO_J (data), (jint) width, (jint) height);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
  }
}

static void
new_position (GstMediaPlayer * player, gint position, gint duration,
    gpointer user_data)
{
  CustomData *data = (CustomData *) user_data;
  JNIEnv *env = get_jni_env ();

  (*env)->CallVoidMethod (env, data->app, set_current_position_method_id,
      NATIVEP_TO_J (data), position, duration);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
  }
}

/*
 * Java Bindings
 */

/* Instruct the native code to create its internal data structure and
 * pipeline */
static jlong
gst_native_player_create (JNIEnv * env, jobject thiz)
{
  GstMediaPlayer *player;
  GObject *viewer;
  CustomData *data;
  jlong result;

  data = g_new0 (CustomData, 1);
  GST_DEBUG ("Created CustomData at %p", data);

  viewer = g_object_new (GST_TYPE_RTSP_VIEWER, NULL);

  player = gst_media_player_new (GST_RTSP_STREAMER (viewer), GST_WINDOW_RENDERER (viewer));

  g_signal_connect (viewer, "size-changed", (GCallback) size_changed,
      data);

  g_signal_connect (G_OBJECT (player), "new-status", (GCallback) new_status,
      data);
  g_signal_connect (G_OBJECT (player), "error", (GCallback) error, data);
  g_signal_connect (G_OBJECT (player), "new-position", (GCallback) new_position,
      data);

  if (!gst_media_player_setup_thread (player, NULL)) {
    GST_ERROR ("Could not configure player");
  }
  GST_DEBUG ("Created GstMediaPlayer at %p", player);

  data->player = player;
  data->app = (*env)->NewGlobalRef (env, thiz);
  GST_DEBUG ("Created GlobalRef for app object at %p", data->app);

  GST_DEBUG_CATEGORY_INIT (debug_category, "nativelayer", 0, "Native layer");
  gst_debug_set_threshold_for_name ("nativelayer", GST_LEVEL_DEBUG);

  return NATIVEP_TO_J (data);
}

/* Free resources */
static void
gst_native_player_finalize (JNIEnv * env, jobject thiz, jlong datap)
{
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  GST_DEBUG ("Finalizing...");
  g_object_unref (data->player);
  data->player = NULL;
  (*env)->DeleteGlobalRef (env, data->app);
  data->app = NULL;
  GST_DEBUG ("Freeing CustomData at %p", data);
  g_free (data);

  GST_DEBUG ("Done finalizing");
}

/* Set pipelines's URI */
void
gst_native_set_uri (JNIEnv * env, jobject thiz, jlong datap, jstring uri,
    jstring user, jstring pass)
{
  const jbyte *char_uri;
  const jbyte *char_user = NULL;
  const jbyte *char_pass = NULL;
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  char_uri = (*env)->GetStringUTFChars (env, uri, NULL);
  if (user != NULL)
    char_user = (*env)->GetStringUTFChars (env, user, NULL);
  if (pass != NULL)
    char_pass = (*env)->GetStringUTFChars (env, pass, NULL);

  GST_DEBUG ("Setting URI to %s for player %p", char_uri, data);
  gst_media_player_set_uri (data->player, char_uri, char_user, char_pass);

  (*env)->ReleaseStringUTFChars (env, uri, char_uri);
  if (char_user != NULL)
    (*env)->ReleaseStringUTFChars (env, user, char_user);
  if (char_pass != NULL)
    (*env)->ReleaseStringUTFChars (env, pass, char_pass);
}

/* Set pipeline to PLAYING state */
static void
gst_native_play (JNIEnv * env, jobject thiz, jlong datap)
{
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  GST_DEBUG ("Setting state to PLAYING");
  gst_media_player_set_state (data->player, GST_STATE_PLAYING);
}

/* Set pipeline to PAUSED state */
static void
gst_native_pause (JNIEnv * env, jobject thiz, jlong datap)
{
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  GST_DEBUG ("Setting state to PAUSED");
  gst_media_player_set_state (data->player, GST_STATE_PAUSED);
}

/* Set pipeline to READY state */
static void
gst_native_ready (JNIEnv * env, jobject thiz, jlong datap)
{
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  GST_DEBUG ("Setting state to READY");
  gst_media_player_set_state (data->player, GST_STATE_READY);
}

/* Instruct the pipeline to seek to a different position */
void
gst_native_set_position (JNIEnv * env, jobject thiz, jlong datap,
    int milliseconds)
{
  gint64 desired_position;
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  desired_position = (gint64) (milliseconds * GST_MSECOND);
  gst_media_player_set_position (data->player, desired_position);
}

/* Native layer initializer: retrieve method and field IDs */
static jboolean
gst_native_layer_init (JNIEnv * env, jobject obj)
{
  guint i;

  set_state_method_id =
      (*env)->GetMethodID (env, obj, "nativeStateChanged",
      "(JLjava/lang/String;)V");
  set_error_method_id =
      (*env)->GetMethodID (env, obj, "nativeErrorOccured",
      "(JLjava/lang/String;)V");
  set_current_position_method_id =
      (*env)->GetMethodID (env, obj, "nativePositionUpdated", "(JII)V");
  on_media_size_changed_method_id =
      (*env)->GetMethodID (env, obj, "nativeMediaSizeChanged", "(JII)V");

  if (!set_state_method_id || !set_error_method_id ||
      !on_media_size_changed_method_id || !set_current_position_method_id) {
    /* We emit this message through the Android log instead of the GStreamer log
     * because the later has not been initialized yet.
     */
    __android_log_print (ANDROID_LOG_ERROR, "nativelayer", "The calling class "
        "does not implement all necessary interface methods");
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

static void
gst_native_surface_init (JNIEnv * env, jobject thiz, jlong datap,
    jobject surface)
{
  ANativeWindow *new_native_window;
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  new_native_window = ANativeWindow_fromSurface (env, surface);

  GST_DEBUG ("Received surface %p (native window %p) %p", surface,
      new_native_window, data);

  gst_media_player_set_native_window (data->player, new_native_window);
}

static void
gst_native_surface_finalize (JNIEnv * env, jobject thiz, jlong datap)
{
  CustomData *data;

  data = J_TO_NATIVEP (datap);
  if (!data)
    return;

  GST_DEBUG ("Releasing Native Window %p", data);

  gst_media_player_release_native_window (data->player);
}

/* List of implemented native methods */
static JNINativeMethod native_methods[] = {
  {"nativePlayerCreate", "()J", (void *) gst_native_player_create},
  {"nativePlayerFinalize", "(J)V", (void *) gst_native_player_finalize},
  {"nativeSetUri", "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        (void *) gst_native_set_uri},
  {"nativePlay", "(J)V", (void *) gst_native_play},
  {"nativePause", "(J)V", (void *) gst_native_pause},
  {"nativeReady", "(J)V", (void *) gst_native_ready},
  {"nativeSetPosition", "(JI)V", (void *) gst_native_set_position},
  {"nativeSurfaceInit", "(JLjava/lang/Object;)V",
        (void *) gst_native_surface_init},
  {"nativeSurfaceFinalize", "(J)V", (void *) gst_native_surface_finalize},
  {"nativeLayerInit", "()Z", (void *) gst_native_layer_init}
};

/* Library initializer */
jint
JNI_OnLoad (JavaVM * vm, void *reserved)
{
  JNIEnv *env = NULL;

  java_vm = vm;

  if ((*vm)->GetEnv (vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) {
    __android_log_print (ANDROID_LOG_ERROR, "nativelayer",
        "Could not retrieve JNIEnv");
    return 0;
  }
  jclass klass = (*env)->FindClass (env,
      "com/gst_sdk_tutorials/rtspviewersf/RTSPViewerSF");
  (*env)->RegisterNatives (env, klass, native_methods,
      G_N_ELEMENTS (native_methods));

  pthread_key_create (&current_jni_env, detach_current_thread);

  return JNI_VERSION_1_4;
}
