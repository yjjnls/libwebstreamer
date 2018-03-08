#ifndef LIBWEBSTREAMER_APPLICATION_ENDPOINT_RTSPCLIENT_HPP
#define LIBWEBSTREAMER_APPLICATION_ENDPOINT_RTSPCLIENT_HPP

#include <gst/gst.h>
#include <framework/Pipeline.hpp>
//#define ENABLE_AUDIO_CODEC
namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {

            class RtspClient : public libwebstreamer::framework::Endpoint
            {
            public:
                explicit RtspClient(const std::string &id, const std::string &type,
                                    const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner);
                ~RtspClient()
                {
                }
                void initialize(const std::string &url);
                virtual bool add_to_pipeline();
                virtual bool remove_from_pipeline();

            private:
                static void on_rtspsrc_pad_added(GstElement *src, GstPad *src_pad, gpointer depay);
                static gboolean on_rtspsrc_select_stream(GstElement* src, guint stream_id, GstCaps* stream_caps, gpointer rtspclient);
                static void on_rtp_time_out(GstElement *rtpbin, guint session, guint ssrc, gpointer user_data);
                static void on_get_new_rtpbin(GstElement *rtspsrc, GstElement *manager, gpointer user_data);

                GstElement *rtspsrc_;
                GstElement *rtpdepay_video_;
                GstElement *parse_video_;
                GstElement *rtpaudiodepay_;
#ifdef ENABLE_AUDIO_CODEC
                GstElement *alawdec;
#endif
            };
        }
    }
}

#endif