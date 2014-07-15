#ifndef _GST_RTSP_VIEWER_H_
#define _GST_RTSP_VIEWER_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GST_TYPE_RTSP_VIEWER (gst_rtsp_viewer_get_type ())
#define GST_RTSP_VIEWER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GST_TYPE_RTSP_VIEWER, GstRTSPViewer))
#define GST_RTSP_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GST_RTSP_VIEWER_TYPE, GstRTSPViewerClass))
#define GST_IS_RTSP_VIEWER(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GST_RTSP_VIEWER_TYPE))
#define GST_IS_RTSP_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_RTSP_VIEWER_TYPE))
#define GST_RTSP_VIEWER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_RTSP_VIEWER_TYPE, GstRTSPViewerClass))

typedef struct _GstRTSPViewer GstRTSPViewer;
typedef struct _GstRTSPViewerClass GstRTSPViewerClass;

struct _GstRTSPViewer {
  GObject parent;

  /*< protected >*/

  /*< private >*/
};

struct _GstRTSPViewerClass {
  GObjectClass parent_class;

  /*< private >*/
};

GType gst_rtsp_viewer_get_type (void);

G_END_DECLS

#endif /* _GST_RTSP_VIEWER_H_ */
