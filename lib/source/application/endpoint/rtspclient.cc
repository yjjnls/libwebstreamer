#include <application/endpoint/rtspclient.hpp>
#include <gst/gst.h>

namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {
            using namespace libwebstreamer;

            RtspClient::RtspClient(const std::string &id, const std::string &type,
                                   const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner)
                : libwebstreamer::framework::Endpoint(id, type, pipeline_owner)
            {
            }

            void RtspClient::initialize(const std::string &url)
            {
                rtspsrc_ = gst_element_factory_make("rtspsrc", "rtspsrc");

                // configuration::PortRange port_range;
                // webstreamer::configuration::query_rtspsrc_port_range(port_range);
                // g_object_set(G_OBJECT(rtspsrc), "location", url.c_str(),
                //              "port-range", webstreamer::configuration::to_string(port_range), NULL);
                g_object_set(G_OBJECT(rtspsrc_), "location", url.c_str(), NULL);
            }
            GstPadProbeReturn RtspClient::on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer rtspclient)
            {
                static int count = 0;
                RtspClient *rtsp_client = static_cast<RtspClient *>(rtspclient);
                auto pipeline = rtsp_client->pipeline_owner().lock();

                // printf(".%d", GST_STATE(pipeline->pipeline()));
                printf(".");
                return GST_PAD_PROBE_OK;
            }
            void RtspClient::on_rtspsrc_pad_added(GstElement *src, GstPad *src_pad, gpointer rtspclient)
            {
                RtspClient *rtsp_client = static_cast<RtspClient *>(rtspclient);
                GstCaps *caps = gst_pad_query_caps(src_pad, NULL);
                GstStructure *stru = gst_caps_get_structure(caps, 0);
                const GValue *media_type = gst_structure_get_value(stru, "media");

                auto pipeline = rtsp_client->pipeline_owner().lock();

                if (g_str_equal(g_value_get_string(media_type), "video"))
                {
                    if (!pipeline->video_encoding().empty())
                    {
                        GstPad *sink_pad = gst_element_get_static_pad(GST_ELEMENT_CAST(rtsp_client->rtpdepay_video_), "sink");
                        GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad);
                        g_warn_if_fail(ret == GST_PAD_LINK_OK);
                        gst_object_unref(sink_pad);
                        // gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, rtspclient, NULL);
                    }
                }
                else if (g_str_equal(g_value_get_string(media_type), "audio"))
                {
                    if (!pipeline->audio_encoding().empty())
                    {
                        GstPad *sink_pad = gst_element_get_static_pad(GST_ELEMENT_CAST(rtsp_client->rtpaudiodepay_), "sink");
                        GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad);
                        g_warn_if_fail(ret == GST_PAD_LINK_OK);
                        gst_object_unref(sink_pad);
                        gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, rtspclient, NULL);
                    }
                }
                else
                {
                    g_warn_if_reached();
                }
            }
            // void RtspClient::on_rtp_time_out(GstElement *rtpbin, guint session, guint ssrc, gpointer user_data)
            // {
            //     printf("----------on_rtp_time_out---------\n");
            // }
            // void RtspClient::on_get_new_rtpbin(GstElement *rtspsrc, GstElement *manager, gpointer user_data)
            // {
            //     printf("---------------on_get_new_rtpbin---------------\n");
            //     g_signal_connect(manager, "on-timeout", (GCallback)on_rtp_time_out, user_data);
            // }
            gboolean RtspClient::on_rtspsrc_select_stream(GstElement *src, guint stream_id, GstCaps *stream_caps, gpointer rtspclient)
            {
                RtspClient *rtsp_client = static_cast<RtspClient *>(rtspclient);
                GstStructure *stru = gst_caps_get_structure(stream_caps, 0);
                std::string media_type(gst_structure_get_string(stru, "media"));
                if (media_type == "video")
                {
                    if (!rtsp_client->pipeline_owner().lock()->video_encoding().empty())
                        return TRUE;
                }
                if (media_type == "audio")
                {
                    if (!rtsp_client->pipeline_owner().lock()->audio_encoding().empty())
                        return TRUE;
                }
                if (gst_structure_has_field(stru, "a-recvonly"))
                {
                }
                return FALSE;
            }
            bool RtspClient::add_to_pipeline()
            {
                auto pipeline = pipeline_owner().lock();
                static int added = 0;
                if (added++ == 0)
                {
                    gst_bin_add(GST_BIN(pipeline_owner().lock()->pipeline()), rtspsrc_);
                    g_signal_connect(rtspsrc_, "pad-added", (GCallback)on_rtspsrc_pad_added, this);
                    g_signal_connect(rtspsrc_, "select-stream", (GCallback)on_rtspsrc_select_stream, this);
                }

                if (!pipeline->video_encoding().empty())
                {
                    VideoEncodingType video_codec = get_video_encoding_type(pipeline->video_encoding());
                    switch (video_codec)
                    {
                        case VideoEncodingType::H264:
                            rtpdepay_video_ = gst_element_factory_make("rtph264depay", "depay");
                            parse_video_ = gst_element_factory_make("h264parse", "parse");
                            break;
                        case VideoEncodingType::H265:
                            rtpdepay_video_ = gst_element_factory_make("rtph265depay", "depay");
                            parse_video_ = gst_element_factory_make("h265parse", "parse");
                            break;
                        default:
                            g_message("Invalid Video Codec!");
                            return false;
                    }
                    g_warn_if_fail(rtpdepay_video_ && parse_video_);

                    g_warn_if_fail(pipeline_owner().lock() != NULL);
                    g_warn_if_fail(pipeline_owner().lock()->pipeline() != NULL);
                    gst_bin_add_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtpdepay_video_, parse_video_, NULL);
                    g_warn_if_fail(gst_element_link(rtpdepay_video_, parse_video_));

                    // g_signal_connect(rtspsrc_, "new-manager", (GCallback)on_get_new_rtpbin, this);
                }
                if (!pipeline->audio_encoding().empty())
                {
                    AudioEncodingType audio_codec = get_audio_encoding_type(pipeline->audio_encoding());
                    switch (audio_codec)
                    {
                        case AudioEncodingType::PCMA:
                            rtpaudiodepay_ = gst_element_factory_make("rtppcmadepay", "audio-depay");
                            break;
                        case AudioEncodingType::PCMU:
                            rtpaudiodepay_ = gst_element_factory_make("rtppcmudepay", "audio-depay");
                            break;
                        case AudioEncodingType::OPUS:
                            rtpaudiodepay_ = gst_element_factory_make("rtpopusdepay", "audio-depay");
                            break;
                        default:
                            g_message("Invalid Audio Codec!");
                            return false;
                    }

                    g_warn_if_fail(rtpaudiodepay_);
                    gst_bin_add_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtpaudiodepay_, NULL);
                }

                return true;
            }

            bool RtspClient::remove_from_pipeline()
            {
                gst_bin_remove_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtspsrc_, rtpdepay_video_, parse_video_, NULL);
                gst_bin_remove_many(GST_BIN(pipeline_owner().lock()->pipeline()), rtpaudiodepay_, NULL);
                return true;
            }
        }
    }
}