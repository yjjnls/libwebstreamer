#include <application/pipeline/livestream.hpp>
#include <application/endpoint/rtspclient.hpp>
#include <application/endpoint/rtspserver.hpp>
#include <application/endpoint/webrtc.hpp>
#include <utils/pipejoint.hpp>
#include <mutex>

namespace libwebstreamer
{
    namespace application
    {
        namespace pipeline
        {
            using namespace libwebstreamer;
            using namespace libwebstreamer::framework;

            static std::mutex joint_mutex;
            LiveStream::LiveStream(const std::string &id)
                : Pipeline(id)
                , video_tee_pad_(NULL)
                , fake_video_queue_(NULL)
                , fake_video_sink_(NULL)
                , audio_tee_pad_(NULL)
                , fake_audio_queue_(NULL)
                , fake_audio_sink_(NULL)
            {
                video_tee_ = gst_element_factory_make("tee", "video_tee");
                audio_tee_ = gst_element_factory_make("tee", "audio_tee");
                g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), video_tee_));
                g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), audio_tee_));
                USE_OWN_SAME_DEBUG();
            }
            LiveStream::~LiveStream()
            {
                gst_element_set_state(pipeline(), GST_STATE_NULL);
                if (!sinks_.empty())
                {
                    for (auto info : sinks_)
                    {
                        GstElement *upstream_joint = info->upstream_joint;
                        LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

                        //remove pipeline dynamicly
                        g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->pipeline()), upstream_joint));
                        gst_element_unlink(pipeline->video_tee_, upstream_joint);

                        gst_element_release_request_pad(pipeline->video_tee_, info->tee_pad);
                        gst_object_unref(info->tee_pad);
                        delete info;
                    }
                }
            }

            void LiveStream::add_pipe_joint(GstElement *upstream_joint)
            {
                joint_mutex.lock();
                gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
                if (g_str_equal(media_type, "video"))
                {
                    GST_DEBUG("[livestream] add_pipe_joint: video");
                    GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(video_tee_), "src_%u");
                    GstPad *pad = gst_element_request_pad(video_tee_, templ, NULL, NULL);
                    sink_link *info = new sink_link(pad, upstream_joint, this);

                    g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), upstream_joint));
                    gst_element_sync_state_with_parent(upstream_joint);

                    GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");
                    GstPadLinkReturn ret = gst_pad_link(pad, sinkpad);
                    g_warn_if_fail(ret == GST_PAD_LINK_OK);
                    gst_object_unref(sinkpad);

                    sinks_.push_back(info);
                }
                else if (g_str_equal(media_type, "audio"))
                {
                    GST_DEBUG("[livestream] add_pipe_joint: audio");
                    GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(audio_tee_), "src_%u");
                    GstPad *pad = gst_element_request_pad(audio_tee_, templ, NULL, NULL);
                    sink_link *info = new sink_link(pad, upstream_joint, this);

                    g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), upstream_joint));
                    gst_element_sync_state_with_parent(upstream_joint);

                    GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");
                    GstPadLinkReturn ret = gst_pad_link(pad, sinkpad);
                    g_warn_if_fail(ret == GST_PAD_LINK_OK);
                    gst_object_unref(sinkpad);

                    sinks_.push_back(info);
                }
                joint_mutex.unlock();
            }
            GstPadProbeReturn LiveStream::on_tee_pad_remove_video_probe(GstPad *teepad, GstPadProbeInfo *probe_info, gpointer data)
            {
                sink_link *info = static_cast<sink_link *>(data);
                if (!g_atomic_int_compare_and_exchange(&info->video_probe_invoke_control, TRUE, FALSE))
                {
                    return GST_PAD_PROBE_OK;
                }

                GstElement *upstream_joint = info->upstream_joint;
                LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

                //remove pipeline dynamicaly
                GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");
                gst_pad_unlink(info->tee_pad, sinkpad);
                gst_object_unref(sinkpad);
                gst_element_set_state(upstream_joint, GST_STATE_NULL);
                g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->pipeline()), upstream_joint));

                gst_element_release_request_pad(pipeline->video_tee_, info->tee_pad);
                gst_object_unref(info->tee_pad);
                delete static_cast<sink_link *>(data);
                GST_DEBUG("[livestream] remove video joint from tee pad");
                return GST_PAD_PROBE_REMOVE;
            }
            GstPadProbeReturn LiveStream::on_tee_pad_remove_audio_probe(GstPad *pad, GstPadProbeInfo *probe_info, gpointer data)
            {
                sink_link *info = static_cast<sink_link *>(data);
                if (!g_atomic_int_compare_and_exchange(&info->audio_probe_invoke_control, TRUE, FALSE))
                {
                    return GST_PAD_PROBE_OK;
                }

                GstElement *upstream_joint = info->upstream_joint;
                LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

                //remove pipeline dynamicaly
                GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");
                gst_pad_unlink(info->tee_pad, sinkpad);
                gst_object_unref(sinkpad);
                gst_element_set_state(upstream_joint, GST_STATE_NULL);
                g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->pipeline()), upstream_joint));

                gst_element_release_request_pad(pipeline->audio_tee_, info->tee_pad);
                gst_object_unref(info->tee_pad);
                delete static_cast<sink_link *>(data);
                GST_DEBUG("[livestream] remove audio joint from tee pad");
                return GST_PAD_PROBE_REMOVE;
            }
            void LiveStream::remove_pipe_joint(GstElement *upstream_joint)
            {
                joint_mutex.lock();
                gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
                if (g_str_equal(media_type, "video"))
                {
                    auto it = sinks_.begin();
                    for (; it != sinks_.end(); ++it)
                    {
                        if ((*it)->upstream_joint == upstream_joint)
                        {
                            break;
                        }
                    }
                    if (it == sinks_.end())
                    {
                        g_warn_if_reached();
                        // TODO...
                        return;
                    }
                    (*it)->video_probe_invoke_control = TRUE;
                    gst_pad_add_probe((*it)->tee_pad, GST_PAD_PROBE_TYPE_IDLE, on_tee_pad_remove_video_probe, *it, NULL);
                    sinks_.erase(it);
                    GST_DEBUG("[livestream] remove video joint completed");
                }
                else if (g_str_equal(media_type, "audio"))
                {
                    auto it = sinks_.begin();
                    for (; it != sinks_.end(); ++it)
                    {
                        if ((*it)->upstream_joint == upstream_joint)
                        {
                            break;
                        }
                    }
                    if (it == sinks_.end())
                    {
                        g_warn_if_reached();
                        // TODO...
                        return;
                    }
                    (*it)->audio_probe_invoke_control = TRUE;
                    gst_pad_add_probe((*it)->tee_pad, GST_PAD_PROBE_TYPE_IDLE, on_tee_pad_remove_audio_probe, *it, NULL);
                    sinks_.erase(it);
                    GST_DEBUG("[livestream] remove video joint completed");
                }
                joint_mutex.unlock();
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
                            fake_video_queue_ = gst_element_factory_make("queue", "fake_video_queue");

#ifdef USE_AUTO_SINK
                            fake_video_decodec_ = gst_element_factory_make("avdec_h264", "fake_video_decodec");
                            fake_video_sink_ = gst_element_factory_make("autovideosink", "fake_video_sink");
                            g_object_set(fake_video_sink_, "sync", FALSE, NULL);
                            gst_bin_add_many(GST_BIN(pipeline()), fake_video_decodec_, fake_video_queue_, fake_video_sink_, NULL);
                            gst_element_link_many(fake_video_queue_, fake_video_decodec_, fake_video_sink_, NULL);
#else
                            fake_video_sink_ = gst_element_factory_make("fakesink", "fake_video_sink");
                            g_object_set(fake_video_sink_, "sync", FALSE, NULL);
                            gst_bin_add_many(GST_BIN(pipeline()), fake_video_queue_, fake_video_sink_, NULL);
                            gst_element_link_many(fake_video_queue_, fake_video_sink_, NULL);
#endif

                            GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(video_tee_), "src_%u");
                            video_tee_pad_ = gst_element_request_pad(video_tee_, templ, NULL, NULL);
                            GstPad *sinkpad = gst_element_get_static_pad(fake_video_queue_, "sink");
                            g_warn_if_fail(gst_pad_link(video_tee_pad_, sinkpad) == GST_PAD_LINK_OK);
                            gst_object_unref(sinkpad);

                            //monitor data probe
                            // GstPad *pad = gst_element_get_static_pad(fake_video_queue_, "src");
                            // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, this, NULL);
                            // gst_object_unref(pad);
                        }
                        if (!audio_encoding().empty())
                        {
                            GstElement *audio_depay = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline()), "audio-depay");
                            g_warn_if_fail(audio_depay);

                            g_warn_if_fail(gst_element_link(audio_depay, audio_tee_));
                            fake_audio_queue_ = gst_element_factory_make("queue", "fake_audio_queue");

#ifdef USE_AUTO_SINK
                            fake_audio_decodec_ = gst_element_factory_make("alawdec", "fake_audio_decodec");
                            fake_audio_sink_ = gst_element_factory_make("autoaudiosink", "fake_audio_sink");
                            fake_audio_convert_ = gst_element_factory_make("audioconvert", "fake_audio_convert");
                            fake_audio_resample_ = gst_element_factory_make("audioresample", "fake_audio_resample");
                            g_object_set(fake_audio_sink_, "sync", FALSE, NULL);
                            gst_bin_add_many(GST_BIN(pipeline()), fake_audio_decodec_, fake_audio_queue_, fake_audio_sink_, fake_audio_convert_, fake_audio_resample_, NULL);
                            gst_element_link_many(fake_audio_queue_, fake_audio_decodec_, fake_audio_convert_, fake_audio_resample_, fake_audio_sink_, NULL);
#else
                            fake_audio_sink_ = gst_element_factory_make("fakesink", "fake_audio_sink");
                            g_object_set(fake_audio_sink_, "sync", FALSE, NULL);
                            gst_bin_add_many(GST_BIN(pipeline()), fake_audio_queue_, fake_audio_sink_, NULL);
                            gst_element_link_many(fake_audio_queue_, fake_audio_sink_, NULL);
#endif

                            GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(audio_tee_), "src_%u");
                            audio_tee_pad_ = gst_element_request_pad(audio_tee_, templ, NULL, NULL);
                            GstPad *sinkpad = gst_element_get_static_pad(fake_audio_queue_, "sink");
                            g_warn_if_fail(gst_pad_link(audio_tee_pad_, sinkpad) == GST_PAD_LINK_OK);
                            gst_object_unref(sinkpad);

                            // //monitor data probe
                            // GstPad *pad = gst_element_get_static_pad(audio_depay, "sink");
                            // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, this, NULL);
                            // gst_object_unref(pad);
                            // g_debug("\n---audio---\n");
                        }
                        gst_element_set_state(pipeline(), GST_STATE_PLAYING);
                    }
                    break;
                    case EndpointType::RTSP_SERVER:
                        if (!endpoint->add_to_pipeline())
                            return false;
                        break;
                    case EndpointType::TEST_SINK:
                        if (!endpoint->add_to_pipeline())
                            return false;
                        break;
                    case EndpointType::WEBRTC:
                        // static_cast<webstreamer::pipeline::endpoint::WebRTC *>(&endpoint)->add_to_pipeline(*this);
                        // static_cast<webstreamer::pipeline::endpoint::WebRTC *>(&endpoint)->disable_signalling_incoming_feedback();
                        break;
                    default:
                        g_warn_if_reached();
                        break;
                }

                return true;
            }

            bool LiveStream::on_remove_endpoint(const std::shared_ptr<Endpoint> endpoint)
            {
                switch (get_endpoint_type(endpoint->type()))
                {
                    case EndpointType::RTSP_CLIENT:
                    {
                        endpoint->remove_from_pipeline();
                        if (!video_encoding().empty() && video_tee_pad_ != NULL)
                        {
                            gst_element_release_request_pad(video_tee_, video_tee_pad_);
                            gst_object_unref(video_tee_pad_);
                        }
                        if (!audio_encoding().empty() && audio_tee_pad_ != NULL)
                        {
                            gst_element_release_request_pad(audio_tee_, audio_tee_pad_);
                            gst_object_unref(audio_tee_pad_);
                        }
                    }
                    break;
                    case EndpointType::RTSP_SERVER:
                        endpoint->remove_from_pipeline();
                        break;
                    case EndpointType::TEST_SINK:
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
            GstPadProbeReturn LiveStream::on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
            {
                static int count = 0;
                auto pipeline = static_cast<LiveStream *>(user_data);
                printf("+%d", GST_STATE(pipeline->pipeline()));
                return GST_PAD_PROBE_OK;
            }

            bool LiveStream::MessageHandler(GstMessage *msg)
            {
                // g_debug("\n [livestream message] %s: %s\n",GST_OBJECT_NAME(msg->src),GST_MESSAGE_TYPE_NAME(msg));
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