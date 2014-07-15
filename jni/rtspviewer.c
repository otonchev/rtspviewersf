#include "rtspviewer.h"
#include "rtspstreamer.h"

static void gst_rtsp_viewer_streamer_interface_init (GstRTSPStreamerInterface * iface);
static GstElement * gst_rtsp_viewer_create_pipeline (GstRTSPStreamer * iface, GError ** error);

G_DEFINE_TYPE_WITH_CODE (GstRTSPViewer, gst_rtsp_viewer, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_RTSP_STREAMER,
        gst_rtsp_viewer_streamer_interface_init));

static void
gst_rtsp_viewer_class_init (GstRTSPViewerClass * klass)
{
}

static void
gst_rtsp_viewer_init (GstRTSPViewer * self)
{
}

static void
gst_rtsp_viewer_streamer_interface_init (GstRTSPStreamerInterface * iface)
{
  iface->create_pipeline = gst_rtsp_viewer_create_pipeline;
}

static GstElement *
gst_rtsp_viewer_create_pipeline (GstRTSPStreamer * streamer, GError ** error)
{
  GstElement * pipeline;

  pipeline = gst_parse_launch("playbin", error);
  return pipeline;
}
