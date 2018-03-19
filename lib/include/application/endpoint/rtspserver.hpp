#ifndef LIBWEBSTREAMER_APPLICATION_ENDPOINT_RTSPSERVER_HPP
#define LIBWEBSTREAMER_APPLICATION_ENDPOINT_RTSPSERVER_HPP

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <framework/Pipeline.hpp>
#include <utils/pipejoint.hpp>

namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {
            class RtspServer : public libwebstreamer::framework::Endpoint
            {
            public:
                explicit RtspServer(const std::string &id, const std::string &type,
                                    const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner);
                ~RtspServer()
                {
                }
                void initialize(const std::string &path);
                virtual bool add_to_pipeline();
                virtual bool remove_from_pipeline();

                std::vector<libwebstreamer::PipeJoint> &joints()
                {
                    return joints_;
                }

                static GstPadProbeReturn cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer rtspclient);

            private:
                std::string path_;
                std::vector<libwebstreamer::PipeJoint> joints_;
            };

            GstRTSPServer *get_rtsp_server();
        }
    }
}

#endif