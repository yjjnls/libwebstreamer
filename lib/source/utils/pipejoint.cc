#include <utils/pipejoint.hpp>

#include "gstutility/gst_inter_sink.h"
#include "gstutility/gst_inter_src.h"

namespace libwebstreamer
{
    PipeJoint make_pipe_joint(const std::string &media_type, const std::string &name)
    {
        std::string name_ = name;
        if (name_.empty())
        {
            static int id = 0;
            name_ = std::to_string(id++);
        }
        //-----------------------------
        GstElement *inter_source = (GstElement *)g_object_new(GST_TYPE_INTER_SRC, "name", "inter-source", NULL);
        GstElement *inter_sink = (GstElement *)g_object_new(GST_TYPE_INTER_SINK, "name", "inter-sink", NULL);

        g_weak_ref_set(&GST_INTER_SRC(inter_source)->sink_sinkpad, GST_INTER_SINK(inter_sink)->sinkpad);
        g_weak_ref_set(&GST_INTER_SINK(inter_sink)->src_srcpad, GST_INTER_SRC(inter_source)->internal_srcpad);
        //----------source bin-----------------
        GstElement *source_bin = gst_bin_new((name_ + "_source_bin").c_str());
        GstElement *source_bin_queue = gst_element_factory_make("queue", "source-bin-output-queue");
        g_object_set(source_bin_queue, "max-size-buffers", 3, "max-size-bytes", 0,
                     "max-size-time", G_GUINT64_CONSTANT(0), NULL);
        gst_bin_add_many(GST_BIN(source_bin), inter_source, source_bin_queue, NULL);
        gst_element_link(inter_source, source_bin_queue);

        GstPad *src_pad = gst_element_get_static_pad(source_bin_queue, "src");
        GstPad *source_bin_pad = gst_ghost_pad_new("src", src_pad);
        gst_object_unref(src_pad);
        gst_pad_set_active(source_bin_pad, TRUE);
        gst_element_add_pad(source_bin, source_bin_pad);

        //------------sink bin----------------
        GstElement *sink_bin = gst_bin_new((name_ + "_sink_bin").c_str());
        GstElement *sink_bin_queue = gst_element_factory_make("queue", "sink-bin-input-queue");
        g_object_set(sink_bin_queue, "max-size-buffers", 3, "max-size-bytes", 0,
                     "max-size-time", G_GUINT64_CONSTANT(0), NULL);
        gst_bin_add_many(GST_BIN(sink_bin), sink_bin_queue, inter_sink, NULL);
        gst_element_sync_state_with_parent(sink_bin_queue);
        gst_element_sync_state_with_parent(inter_sink);
        gst_element_link(sink_bin_queue, inter_sink);

        GstPad *sink_pad = gst_element_get_static_pad(sink_bin_queue, "sink");
        GstPad *sink_bin_pad = gst_ghost_pad_new("sink", sink_pad);
        gst_object_unref(sink_pad);
        gst_pad_set_active(sink_bin_pad, TRUE);
        gst_element_add_pad(sink_bin, sink_bin_pad);
        g_object_set_data(G_OBJECT(sink_bin), "media-type", (gchar *)media_type.c_str());

        ///-----------------
        PipeJoint pipejoint;
        pipejoint.upstream_joint = sink_bin;
        pipejoint.downstream_joint = source_bin;
        return pipejoint;
    }

    void destroy_pipe_joint(PipeJoint pipejoint)
    {
        gst_object_unref(pipejoint.upstream_joint);
        gst_object_unref(pipejoint.downstream_joint);
    }
}