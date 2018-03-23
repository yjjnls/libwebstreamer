#include <application/endpoint/testsink.hpp>
#include <gst/gst.h>

namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {
            using namespace libwebstreamer;

            TestSink::TestSink(const std::string &id, const std::string &type,
                               const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner)
                : libwebstreamer::framework::Endpoint(id, type, pipeline_owner)
            {
            }
            bool TestSink::add_to_pipeline()
            {
                pipeline_ = gst_pipeline_new(id().c_str());
                if (!pipeline_owner().lock()->video_encoding().empty())
                {
                    video_sink_ = gst_element_factory_make("autovideosink", NULL);
                    g_object_set(video_sink_, "sync", FALSE, NULL);
                    video_decodec_ = gst_element_factory_make("avdec_h264", NULL);

                    static std::string media_type = "video";
                    std::string pipejoint_name = std::string("testsink_video_endpoint_joint_") + id();
                    video_joint_ = make_pipe_joint(media_type, pipejoint_name);

                    g_object_set_data(G_OBJECT(video_joint_.upstream_joint), "media-type", "video");
                    pipeline_owner().lock()->add_pipe_joint(video_joint_.upstream_joint);

                    gst_bin_add_many(GST_BIN(pipeline_), video_joint_.downstream_joint, video_decodec_, video_sink_, NULL);
                    g_warn_if_fail(gst_element_link(video_joint_.downstream_joint, video_decodec_));
                    g_warn_if_fail(gst_element_link(video_decodec_, video_sink_));

                    // GstPad *pad = gst_element_get_static_pad(video_joint_.downstream_joint, "src");
                    // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, this->on_monitor_data, this, NULL);
                    // gst_object_unref (pad);
                }
                if (!pipeline_owner().lock()->audio_encoding().empty())
                {
                    audio_sink_ = gst_element_factory_make("autoaudiosink", NULL);
                    g_object_set(audio_sink_, "sync", FALSE, NULL);
                    audio_decodec_ = gst_element_factory_make("alawdec", NULL);
                    audio_convert_ = gst_element_factory_make("audioconvert", NULL);
                    audio_resample_ = gst_element_factory_make("audioresample", NULL);

                    static std::string media_type = "video";
                    std::string pipejoint_name = std::string("testsink_audio_endpoint_joint_") + id();
                    audio_joint_ = make_pipe_joint(media_type, pipejoint_name);

                    g_object_set_data(G_OBJECT(audio_joint_.upstream_joint), "media-type", "audio");
                    pipeline_owner().lock()->add_pipe_joint(audio_joint_.upstream_joint);

                    gst_bin_add_many(GST_BIN(pipeline_), audio_joint_.downstream_joint, audio_decodec_, audio_convert_, audio_resample_, audio_sink_, NULL);
                    g_warn_if_fail(gst_element_link(audio_joint_.downstream_joint, audio_decodec_));
                    g_warn_if_fail(gst_element_link_many(audio_decodec_, audio_convert_, audio_resample_, audio_sink_, NULL));
                }

                gst_element_set_state(pipeline_, GST_STATE_PLAYING);
                // gst_element_set_base_time(pipeline_, gst_element_get_base_time(pipeline_owner().lock()->pipeline()));
                return true;
            }
            bool TestSink::remove_from_pipeline()
            {
                if (!pipeline_owner().lock()->video_encoding().empty())
                {
                    pipeline_owner().lock()->remove_pipe_joint(video_joint_.upstream_joint);
                    gst_bin_remove_many(GST_BIN(pipeline_), video_joint_.downstream_joint, video_decodec_, video_sink_, NULL);
                    gst_element_set_state(video_joint_.downstream_joint, GST_STATE_NULL);
                    gst_element_set_state(video_decodec_, GST_STATE_NULL);
                    gst_element_set_state(video_sink_, GST_STATE_NULL);
                    video_decodec_ = NULL;
                    video_sink_ = NULL;
                }

                if (!pipeline_owner().lock()->audio_encoding().empty())
                {
                    pipeline_owner().lock()->remove_pipe_joint(audio_joint_.upstream_joint);
                    gst_bin_remove_many(GST_BIN(pipeline_), audio_joint_.downstream_joint, audio_decodec_, audio_convert_, audio_resample_, audio_sink_, NULL);
                    gst_element_set_state(audio_joint_.downstream_joint, GST_STATE_NULL);
                    gst_element_set_state(audio_decodec_, GST_STATE_NULL);
                    gst_element_set_state(audio_convert_, GST_STATE_NULL);
                    gst_element_set_state(audio_resample_, GST_STATE_NULL);
                    gst_element_set_state(audio_sink_, GST_STATE_NULL);
                    audio_decodec_ = NULL;
                    audio_sink_ = NULL;
                }


                return true;
            }
        }
    }
}