#include <application/endpoint/rtspserver.hpp>
// #define USE_TEST_SOURCE 1
namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {

            static GstRTSPServer *rtsp_server = NULL;
            static int session_count = 0;

            GstRTSPServer *get_rtsp_server()
            {
                if (rtsp_server == NULL)
                    rtsp_server = gst_rtsp_server_new();

                return rtsp_server;
            }

            RtspServer::RtspServer(const std::string &id, const std::string &type,
                                   const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner)
                : libwebstreamer::framework::Endpoint(id, type, pipeline_owner)
                , pipeline_(NULL)
            {
            }

            void RtspServer::initialize(const std::string &path)
            {
                path_ = path;
            }
            static gboolean timeout(GstRTSPServer *server)
            {
                GstRTSPSessionPool *pool;

                pool = gst_rtsp_server_get_session_pool(server);

                gst_rtsp_session_pool_cleanup(pool);
                g_object_unref(pool);

                return TRUE;
            }
            // static GstElement *pipe = NULL;
            void RtspServer::on_media_new_state(GstRTSPMedia *media, gint state, gpointer user_data)
            {
                printf("~~~~~~~~state: %d~~~~~~~~~\n", state);
                if (state == GST_STATE_NULL)
                {
                    printf("\n~~~~~~~~    Rtsp Server: No client!   ~~~~~~~~\n");
                    typedef libwebstreamer::framework::Pipeline Pipeline;
                    std::shared_ptr<Pipeline> pipeline = static_cast<RtspServer *>(user_data)->pipeline_owner().lock();

                    static_cast<RtspServer *>(user_data)->remove_from_pipeline();

                    static_cast<RtspServer *>(user_data)->pipeline_ = NULL;
                    // pipe = NULL;
                }
                if (state == GST_STATE_PLAYING)
                {
                    typedef libwebstreamer::framework::Pipeline Pipeline;
                    std::shared_ptr<Pipeline> pipeline = static_cast<RtspServer *>(user_data)->pipeline_owner().lock();

                    auto base_time = gst_element_get_base_time(pipeline->pipeline());
                    printf("~~~~~~~======  %d   =====\n", base_time);
                    gst_element_set_base_time(static_cast<RtspServer *>(user_data)->pipeline_, base_time);
                    // gst_element_set_base_time(pipe, gst_element_get_base_time(pipeline->pipeline()));

                    printf("\n\n\n\n\nplaying %d\n\n\n\n\n", GST_STATE(static_cast<RtspServer *>(user_data)->pipeline_));
                }
            }
            GstPadProbeReturn RtspServer::cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
            {
                static int count = 0;

                auto pipeline = static_cast<RtspServer *>(user_data)->pipeline_owner().lock();
                printf("-%d\n", GST_STATE(pipeline->pipeline()));
                return GST_PAD_PROBE_OK;
            }


            void RtspServer::on_rtsp_media_constructed(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data)
            {
                using namespace libwebstreamer;
                typedef libwebstreamer::framework::Pipeline Pipeline;
                std::shared_ptr<Pipeline> pipeline = static_cast<RtspServer *>(user_data)->pipeline_owner().lock();
                GstElement *rtsp_server_media_bin = gst_rtsp_media_get_element(media);

                g_signal_connect(media, "new-state", (GCallback)on_media_new_state, user_data);
                g_warn_if_fail(!static_cast<RtspServer *>(user_data)->pipeline_);

                static_cast<RtspServer *>(user_data)->pipeline_ = gst_pipeline_new("rtsp-server-test");
                // GstClock *sys_clock = gst_system_clock_obtain();
                // gst_pipeline_use_clock(GST_PIPELINE(static_cast<RtspServer *>(user_data)->pipeline_), sys_clock);
                // g_object_unref(sys_clock);
                gst_rtsp_media_take_pipeline(media, GST_PIPELINE_CAST(static_cast<RtspServer *>(user_data)->pipeline_));

                // gst_rtsp_media_take_pipeline(media, GST_PIPELINE_CAST(pipeline->pipeline()));

                // pipe = gst_pipeline_new("rtsp-server-test");
                // gst_rtsp_media_take_pipeline(media, GST_PIPELINE(pipe));

                // static std::string name2("2");
                //                 pipeline->add_test_sink(name2);
                if (!pipeline->video_encoding().empty())
                {
                    printf("--------video-------\n");
                    static std::string media_type = "video";
                    std::string pipejoint_name = std::string("rtspserver_video_endpoint_joint") + std::to_string(session_count);
                    PipeJoint video_pipejoint = make_pipe_joint(media_type, pipejoint_name);

                    pipeline->add_pipe_joint(video_pipejoint.upstream_joint);

                    g_warn_if_fail(gst_bin_add(GST_BIN(rtsp_server_media_bin), video_pipejoint.downstream_joint));

                    GstElement *video_pay = gst_bin_get_by_name_recurse_up(GST_BIN(rtsp_server_media_bin), "pay0");
                    g_warn_if_fail(gst_element_link(video_pipejoint.downstream_joint, video_pay));



                    video_pipejoint.rtsp_server_media_bin = rtsp_server_media_bin;
                    static_cast<RtspServer *>(user_data)->joints().push_back(video_pipejoint);

                    GstPad *pad = gst_element_get_static_pad(video_pay, "src");
                    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, static_cast<RtspServer *>(user_data)->cb_have_data, user_data, NULL);
                }

                if (!pipeline->audio_encoding().empty())
                {
                    printf("--------audio-------\n");
                    static std::string media_type = "audio";
                    std::string pipejoint_name = std::string("rtspserver_audio_endpoint_joint") + std::to_string(session_count);
                    PipeJoint audio_pipejoint = make_pipe_joint(media_type, pipejoint_name);
                    g_warn_if_fail(gst_bin_add(GST_BIN(rtsp_server_media_bin), audio_pipejoint.downstream_joint));

                    GstElement *audio_pay = gst_bin_get_by_name_recurse_up(GST_BIN(rtsp_server_media_bin), "pay1");
                    g_warn_if_fail(gst_element_link(audio_pipejoint.downstream_joint, audio_pay));

                    pipeline->add_pipe_joint(audio_pipejoint.upstream_joint);

                    audio_pipejoint.rtsp_server_media_bin = rtsp_server_media_bin;
                    static_cast<RtspServer *>(user_data)->joints().push_back(audio_pipejoint);
                }
                session_count++;
            }

            bool RtspServer::add_to_pipeline()
            {
                GstRTSPMountPoints *mount_points = gst_rtsp_server_get_mount_points(rtsp_server);

                GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
                /* if you want multiple clients to see the same video, set the shared property to TRUE */
                gst_rtsp_media_factory_set_shared(factory, TRUE);
#ifdef USE_TEST_SOURCE
                gst_rtsp_media_factory_set_launch(factory,
                                                  "( "
                                                  "videotestsrc ! video/x-raw,width=352,height=288,framerate=15/1 ! "
                                                  "x264enc ! rtph264pay name=pay0 pt=96 "
                                                  "audiotestsrc ! audio/x-raw,rate=8000 ! "
                                                  "alawenc ! rtppcmapay name=pay1 pt=97 "
                                                  ")");
#else
                std::string video_encoding = pipeline_owner().lock()->video_encoding();
                std::string audio_encoding = pipeline_owner().lock()->audio_encoding();
                std::string launch = "( ";
                if (!video_encoding.empty())
                    launch += "rtp" + video_encoding + "pay pt=96 name=pay0";
                if (!audio_encoding.empty())
                    launch += "  rtp" + audio_encoding + "pay pt=97 name=pay1";
                launch += " )";

                gst_rtsp_media_factory_set_launch(factory, launch.c_str());
                g_signal_connect(factory, "media-constructed", (GCallback)on_rtsp_media_constructed, (gpointer)(this));
#endif

                gst_rtsp_mount_points_add_factory(mount_points, path_.c_str(), factory);
                g_object_unref(mount_points);
                /* do session cleanup every 2 seconds */
                g_timeout_add_seconds(2, (GSourceFunc)timeout, rtsp_server);

                static std::string name1("1");
                static std::string name2("2");
                static std::string name3("3");
                static int k = 0;
                // pipeline_owner().lock()->add_test_sink(name1);
                // pipeline_owner().lock()->add_test_sink(name2);
                // pipeline_owner().lock()->add_test_sink(name3);

                return true;
            }


            bool RtspServer::remove_from_pipeline()
            {
                for (auto &it : joints_)
                {
                    g_warn_if_fail(gst_bin_remove(GST_BIN(it.rtsp_server_media_bin), it.downstream_joint));
                    pipeline_owner().lock()->remove_pipe_joint(it.upstream_joint);
                }
                joints_.clear();
                return true;
            }
        }
    }
}