#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#ifndef __GST_RTSP_STREAMER_H__
#define __GST_RTSP_STREAMER_H__

typedef struct _GstRTSPStreamer GstRTSPStreamer; /* dummy object */
typedef struct _GstRTSPStreamerInterface GstRTSPStreamerInterface;

#define GST_TYPE_RTSP_STREAMER         (gst_rtsp_streamer_get_type ())
#define GST_RTSP_STREAMER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTSP_STREAMER, GstRTSPStreamer))
#define GST_IS_RTSP_STREAMER(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTSP_STREAMER))
#define GST_RTSP_STREMAER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_RTSP_STREAMER, GstRTSPStreamerInterface))

struct _GstRTSPStreamerInterface {
  GTypeInterface iface;

  GstElement * (*create_pipeline) (GstRTSPStreamer * streamer, GError ** error);
};

extern GQuark gst_rtsp_streamer_error_quark (void);

GType         gst_rtsp_streamer_get_type   (void);

GstElement * gst_rtsp_streamer_create_pipeline (GstRTSPStreamer * streamer, GError ** error);

#endif /* __GST_RTSP_STREAMER_H__ */
