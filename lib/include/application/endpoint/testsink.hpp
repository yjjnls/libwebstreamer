#ifndef LIBWEBSTREAMER_APPLICATION_ENDPOINT_TESTSINK_HPP
#define LIBWEBSTREAMER_APPLICATION_ENDPOINT_TESTSINK_HPP

#include <gst/gst.h>
#include <framework/Pipeline.hpp>
#include <utils/pipejoint.hpp>

namespace libwebstreamer
{
    namespace application
    {
        namespace endpoint
        {

            class TestSink : public libwebstreamer::framework::Endpoint
            {
            public:
                explicit TestSink(const std::string &id, const std::string &type,
                                    const std::shared_ptr<libwebstreamer::framework::Pipeline> pipeline_owner);
                ~TestSink()
                {
                }
                // void initialize(const std::string &url);
                virtual bool add_to_pipeline();
                virtual bool remove_from_pipeline();

            private:
                static GstPadProbeReturn on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
            
                GstElement *video_decodec_;
                GstElement *video_sink_;
                GstElement *audio_decodec_;
                GstElement *audio_convert_;
                GstElement *audio_resample_;
                GstElement *audio_sink_;
                libwebstreamer::PipeJoint video_joint_;
                libwebstreamer::PipeJoint audio_joint_;
                GstElement *pipeline_;
            };
        }
    }
}

#endif