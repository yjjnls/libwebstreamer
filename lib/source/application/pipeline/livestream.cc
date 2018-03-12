#include <application/pipeline/livestream.hpp>
#include <application/endpoint/rtspclient.hpp>
#include <application/endpoint/rtspserver.hpp>
#include <application/endpoint/webrtc.hpp>

namespace libwebstreamer
{
    namespace application
    {
        namespace pipeline
        {
            using namespace libwebstreamer;
            using namespace libwebstreamer::framework;

            LiveStream::LiveStream(const std::string &id)
                : Pipeline(id)
            {
                video_tee_ = gst_element_factory_make("tee", "video-tee");
                audio_tee_ = gst_element_factory_make("tee", "audio-tee");
                g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), video_tee_));
                g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), audio_tee_));
            }

            // //debug
            // LiveStream::~LiveStream()
            // {
            //     gst_element_set_state(pipeline_, GST_STATE_NULL);
            //     for (auto it : endpoints_)
            //     {
            //         //make every endpoint do clean work
            //         on_remove_endpoint(it);
            //     }
            // }

            void LiveStream::add_pipe_joint(GstElement *upstream_joint)
            {
                g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), upstream_joint));

                gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
                if (g_str_equal(media_type, "video"))
                {
                    // printf("---------video--------\n");
                    g_warn_if_fail(gst_element_link(video_tee_, upstream_joint));
                }
                else if (g_str_equal(media_type, "audio"))
                {
                    // printf("---------audio--------\n");
                    g_warn_if_fail(gst_element_link(audio_tee_, upstream_joint));
                }
                GstStateChangeReturn ret = gst_element_set_state(pipeline(), GST_STATE_PLAYING);
            }
            void LiveStream::remove_pipe_joint(GstElement *upstream_joint)
            {
                g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline()), upstream_joint));

                gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
                if (g_str_equal(media_type, "video"))
                {
                    // printf("---------video release--------\n");
                    gst_element_unlink(video_tee_, upstream_joint);
                }
                else if (g_str_equal(media_type, "audio"))
                {
                    // printf("---------audio release--------\n");
                    gst_element_unlink(audio_tee_, upstream_joint);
                }
            }

            bool LiveStream::on_add_endpoint(const std::shared_ptr<Endpoint> endpoint)
            {
                switch (get_endpoint_type(endpoint->type()))
                {
                    case EndpointType::RTSP_CLIENT:
                    {
                        if (!endpoint->add_to_pipeline())
                            return false;
                        if (!video_encoding().empty())
                        {
                            GstElement *parse = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline()), "parse");
                            g_warn_if_fail(parse);
                            g_warn_if_fail(gst_element_link(parse, video_tee_));
                            // GstElement *decodec = gst_element_factory_make("avdec_h264", "avdec_h264");
                            // GstElement *sink = gst_element_factory_make("autovideosink", "autovideosink");
                            // gst_bin_add_many(GST_BIN(pipeline()), decodec, sink, NULL);
                            // g_warn_if_fail(gst_element_link(parse, decodec));
                            // g_warn_if_fail(gst_element_link(decodec, sink));
                        }
                        if (!audio_encoding().empty())
                        {
#ifdef ENABLE_AUDIO_CODEC
                            GstElement *alawdec = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline()), "alawdec");
                            g_warn_if_fail(alawdec);
                            g_warn_if_fail(gst_element_link(alawdec, audio_tee_));
#else
                            GstElement *rtppcmadepay = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline()), "audio-depay");
                            g_warn_if_fail(rtppcmadepay);
                            g_warn_if_fail(gst_element_link(rtppcmadepay, audio_tee_));
                            // GstElement *decodec = gst_element_factory_make("alawdec", "alawdec");
                            // GstElement *sink = gst_element_factory_make("autoaudiosink", "autoaudiosink");
                            // gst_bin_add_many(GST_BIN(pipeline()), decodec, sink, NULL);
                            // g_warn_if_fail(gst_element_link(rtppcmadepay, decodec));
                            // g_warn_if_fail(gst_element_link(decodec, sink));
#endif
                        }
                    }
                    break;
                    case EndpointType::RTSP_SERVER:
                        if (!endpoint->add_to_pipeline())
                            return false;
                        break;
                    case EndpointType::WEBRTC:
                        // static_cast<webstreamer::pipeline::endpoint::WebRTC *>(&endpoint)->add_to_pipeline(*this);
                        // static_cast<webstreamer::pipeline::endpoint::WebRTC *>(&endpoint)->disable_signalling_incoming_feedback();
                        break;
                    default:
                        break;
                }
                // gst_element_set_state(pipeline(), GST_STATE_PLAYING);
                return true;
            }

            bool LiveStream::on_remove_endpoint(const std::shared_ptr<Endpoint> endpoint)
            {
                switch (get_endpoint_type(endpoint->type()))
                {
                    case EndpointType::RTSP_CLIENT:
                        endpoint->remove_from_pipeline();
                        break;
                    case EndpointType::RTSP_SERVER:
                        endpoint->remove_from_pipeline();
                        break;
                    case EndpointType::WEBRTC:
                        // static_cast<webstreamer::pipeline::endpoint::WebRTC *>(&endpoint)->remove_from_pipeline(*this);
                        break;
                    default:
                        // todo...
                        g_warn_if_reached();
                        return false;
                }
                return true;
            }
            bool LiveStream::MessageHandler(GstMessage *msg)
            {
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
                {
                    GError *err = NULL;
                    gchar *dbg_info = NULL;

                    gst_message_parse_error(msg, &err, &dbg_info);
                    g_critical("errors occured in pipeline: livestream!\n---------------------------\nERROR from element %s: %s\nDebugging info: %s\n---------------------------\n",
                               GST_OBJECT_NAME(msg->src), err->message, (dbg_info) ? dbg_info : "none");

                    g_error_free(err);
                    g_free(dbg_info);
                }
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_WARNING)
                {
                    GError *warn = NULL;
                    gchar *dbg_info = NULL;

                    gst_message_parse_warning(msg, &warn, &dbg_info);
                    g_warning("warnning occured in pipeline: livestream!\n---------------------------\nERROR from element %s: %s\nDebugging info: %s\n---------------------------\n",
                              GST_OBJECT_NAME(msg->src), warn->message, (dbg_info) ? dbg_info : "none");

                    g_error_free(warn);
                    g_free(dbg_info);
                }
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_INFO)
                {
                    GError *warn = NULL;
                    gchar *dbg_info = NULL;

                    gst_message_parse_info(msg, &warn, &dbg_info);
                    g_message("info message in pipeline: livestream!\n---------------------------\nERROR from element %s: %s\nDebugging info: %s\n---------------------------\n",
                              GST_OBJECT_NAME(msg->src), warn->message, (dbg_info) ? dbg_info : "none");

                    g_error_free(warn);
                    g_free(dbg_info);
                }
                return true;
            }
        }
    }
}