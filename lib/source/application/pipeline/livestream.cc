#include <application/pipeline/livestream.hpp>
#include <application/endpoint/rtspclient.hpp>
#include <application/endpoint/rtspserver.hpp>
#include <application/endpoint/webrtc.hpp>
#include <utils/pipejoint.hpp>

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

                gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
                if (g_str_equal(media_type, "video"))
                {
                    printf("---------video: %d--------\n", GST_STATE(pipeline()));
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
                    printf("===============\n");
                }
                else if (g_str_equal(media_type, "audio"))
                {
                    // // printf("---------audio--------\n");
                    GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(audio_tee_), "src_%u");
                    GstPad *pad = gst_element_request_pad(audio_tee_, templ, NULL, NULL);
                    sink_link *info = new sink_link(pad, upstream_joint, this);
                    info->audio_probe_invoke_control = TRUE;
                    sinks_.push_back(info);
                    // g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), upstream_joint));
                    // g_warn_if_fail(gst_element_link(audio_tee_, upstream_joint));
                }
                if (GST_STATE(pipeline()) != GST_STATE_PLAYING)
                {
                    printf("\n\n\n\t\tpipeline  state not playing\n\n\n");
                    GstStateChangeReturn ret = gst_element_set_state(pipeline(), GST_STATE_PLAYING);
                }
            }
            GstPadProbeReturn LiveStream::on_tee_pad_remove_video_probe(GstPad *teepad, GstPadProbeInfo *probe_info, gpointer data)
            {
                printf("-------on_tee_pad_remove_video_probe------\n");
                sink_link *info = static_cast<sink_link *>(data);
                if (!g_atomic_int_compare_and_exchange(&info->video_probe_invoke_control, TRUE, FALSE))
                {
                    return GST_PAD_PROBE_OK;
                }

                GstElement *upstream_joint = info->upstream_joint;
                LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

                //remove pipeline dynamicly
                // printf("======-1========\n");
                GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");
                gst_pad_unlink(info->tee_pad, sinkpad);
                gst_object_unref(sinkpad);
                // printf("======0========\n");
                gst_element_set_state(upstream_joint, GST_STATE_NULL);
                g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->pipeline()), upstream_joint));

                // printf("======1========\n");
                gst_element_release_request_pad(pipeline->video_tee_, info->tee_pad);
                // printf("======2========\n");
                gst_object_unref(info->tee_pad);
                // printf("======3========\n");
                delete static_cast<sink_link *>(data);
                printf("---------remove upstream_joint--------\n");
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

                //remove pipeline dynamicly
                gst_element_unlink(pipeline->audio_tee_, upstream_joint);
                gst_element_set_state(upstream_joint, GST_STATE_NULL);
                g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->pipeline()), upstream_joint));

                gst_element_release_request_pad(pipeline->audio_tee_, info->tee_pad);
                gst_object_unref(info->tee_pad);
                delete static_cast<sink_link *>(data);
                return GST_PAD_PROBE_REMOVE;
            }
            void LiveStream::remove_pipe_joint(GstElement *upstream_joint)
            {
                // printf("---------remove_pipe_joint--------\n");
                // g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline()), upstream_joint));

                gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
                // printf("---------remove_pipe_joint2: %s--------\n", media_type);
                if (g_str_equal(media_type, "video"))
                {
                    printf("---------video release: %d--------\n", sinks_.size());
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
                    printf("~~~~~~~~~~~~~\n");
                    sinks_.erase(it);
                    // gst_element_unlink(video_tee_, upstream_joint);
                }
                else if (g_str_equal(media_type, "audio"))
                {
                    // // printf("---------audio release--------\n");
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
                    // gst_element_unlink(audio_tee_, upstream_joint);
                }
                // printf("---------remove_pipe_joint3--------\n");
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
                            // fake_video_queue_ = gst_element_factory_make("queue", "fake_video_queue");
#ifdef USE_AUTO_SINK
                            // fake_video_decodec_ = gst_element_factory_make("avdec_h264", "fake_decodec");
                            // fake_video_sink_ = gst_element_factory_make("autovideosink", "fake_video_sink");
                            // gst_bin_add_many(GST_BIN(pipeline()), fake_video_decodec_, fake_video_queue_, fake_video_sink_, NULL);
                            // gst_element_link_many(fake_video_queue_, fake_video_decodec_, fake_video_sink_, NULL);
#else
                            fake_video_sink_ = gst_element_factory_make("fakesink", "fake_video_sink");
                            g_object_set(fake_video_sink_, "sync", TRUE, NULL);
                            gst_bin_add_many(GST_BIN(pipeline()), fake_video_queue_, fake_video_sink_, NULL);
                            gst_element_link_many(fake_video_queue_, fake_video_sink_, NULL);
#endif
                            // GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(video_tee_), "src_%u");
                            // video_tee_pad_ = gst_element_request_pad(video_tee_, templ, NULL, NULL);
                            // GstPad *sinkpad = gst_element_get_static_pad(fake_video_queue_, "sink");
                            // g_warn_if_fail(gst_pad_link(video_tee_pad_, sinkpad) == GST_PAD_LINK_OK);
                            // gst_object_unref(sinkpad);
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
#ifndef USE_AUTO_SINK
                            g_warn_if_fail(gst_element_link(rtppcmadepay, audio_tee_));
#else
                            GstElement *decodec = gst_element_factory_make("alawdec", "alawdec");
                            GstElement *sink = gst_element_factory_make("autoaudiosink", "autoaudiosink");
                            gst_bin_add_many(GST_BIN(pipeline()), decodec, sink, NULL);
                            g_warn_if_fail(gst_element_link(rtppcmadepay, decodec));
                            g_warn_if_fail(gst_element_link(decodec, sink));
#endif
#endif
                        }
                        // gst_element_set_state(pipeline(), GST_STATE_PLAYING);
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
            GstPadProbeReturn LiveStream::cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
            {
                printf("\n~`````\n");
                static int count = 0;
                auto pipeline = static_cast<LiveStream *>(user_data);
                gst_element_set_base_time(pipeline->pipe, gst_element_get_base_time(pipeline->pipeline()));
                return GST_PAD_PROBE_REMOVE;
            }
            void LiveStream::add_test_sink(const std::string &name)
            {
                if (!video_encoding_.empty())
                {
                    if (name != "1")
                    {
                        // GstElement *queue = gst_element_factory_make("queue", NULL);
                        GstElement *sink = gst_element_factory_make("autovideosink", NULL);
                        GstElement *decode = gst_element_factory_make("avdec_h264", NULL);

                        static std::string media_type = "video";
                        std::string pipejoint_name = std::string("rtspserver_video_endpoint_joint") + name;
                        PipeJoint video_pipejoint = make_pipe_joint(media_type, pipejoint_name);
                        pipe = gst_pipeline_new("test");
                        gst_bin_add_many(GST_BIN(pipe), video_pipejoint.downstream_joint, decode, sink, NULL);

                        g_object_set_data(G_OBJECT(video_pipejoint.upstream_joint), "media-type", "video");
                        add_pipe_joint(video_pipejoint.upstream_joint);

                        // gst_element_sync_state_with_parent(video_pipejoint.downstream_joint);
                        // gst_element_sync_state_with_parent(decode);
                        // gst_element_sync_state_with_parent(sink);
                        g_warn_if_fail(gst_element_link(video_pipejoint.downstream_joint, decode));
                        g_warn_if_fail(gst_element_link(decode, sink));

                        GstPad *pad = gst_element_get_static_pad(video_pipejoint.downstream_joint, "src");
                        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, this->cb_have_data, this, NULL);

                        gst_element_set_state(pipe, GST_STATE_PLAYING);
                        // gst_element_set_base_time(pipe, gst_element_get_base_time(pipeline()));

                        // GstClock *pipeline_clock = gst_pipeline_get_clock(GST_PIPELINE(pipeline()));
                        // gst_pipeline_use_clock(GST_PIPELINE(pipe), pipeline_clock);
                        // GstClock *sys_clock = gst_system_clock_obtain();
                        // gst_pipeline_use_clock(GST_PIPELINE(pipeline()), sys_clock);
                        // gst_pipeline_use_clock(GST_PIPELINE(pipe), sys_clock);
                        // g_object_unref(sys_clock);
                        // gst_element_set_base_time(pipeline(), 0);

                        printf("------pipeline base time: %d ------\n", gst_element_get_base_time(pipeline()));
                        printf("------new pipeline base time: %d ------\n", gst_element_get_base_time(pipe));


                        // g_object_set_data(G_OBJECT(queue), "media-type", "video");
                        // add_pipe_joint(queue);

                        // gst_bin_add_many(GST_BIN(pipeline()), decode, sink, NULL);
                        // gst_element_sync_state_with_parent(decode);
                        // gst_element_sync_state_with_parent(sink);
                        // g_warn_if_fail(gst_element_link(queue, decode));
                        // g_warn_if_fail(gst_element_link(decode, sink));
                    }
                    else
                    {
                        GstElement *queue = gst_element_factory_make("queue", NULL);
                        GstElement *sink = gst_element_factory_make("filesink", NULL);
                        g_object_set(G_OBJECT(sink), "location", "test.mp4", NULL);

                        g_object_set_data(G_OBJECT(queue), "media-type", "video");
                        add_pipe_joint(queue);

                        gst_bin_add_many(GST_BIN(pipeline()), sink, NULL);
                        gst_element_sync_state_with_parent(sink);
                        g_warn_if_fail(gst_element_link(queue, sink));
                    }
                }

                if (!audio_encoding_.empty())
                {
                    fake_audio_sink_ = gst_element_factory_make("fakesink", "audio_sink");
                    gst_bin_add_many(GST_BIN(pipeline()), fake_audio_sink_, NULL);
                    g_warn_if_fail(gst_element_link(audio_tee_, fake_audio_sink_));
                }
            }

            void LiveStream::remove_fake_sink()
            {
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