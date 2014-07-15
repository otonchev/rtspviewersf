#include "rtspstreamer.h"

G_DEFINE_INTERFACE (GstRTSPStreamer, gst_rtsp_streamer, G_TYPE_OBJECT);

GstElement *
gst_rtsp_streamer_create_pipeline (GstRTSPStreamer * streamer, GError ** error)
{
  return
      GST_RTSP_STREMAER_GET_INTERFACE (streamer)->create_pipeline (streamer, error);
}

static void
gst_rtsp_streamer_default_init (GstRTSPStreamerInterface * streamer)
{
}
