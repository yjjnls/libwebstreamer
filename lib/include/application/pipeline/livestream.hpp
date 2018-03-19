#ifndef LIBWEBSTREAMER_APPLICATION_PIPELINE_LIVESTREAM_HPP
#define LIBWEBSTREAMER_APPLICATION_PIPELINE_LIVESTREAM_HPP

#include <framework/pipeline.hpp>

// #define USE_AUTO_SINK 1
namespace libwebstreamer
{
    namespace application
    {
        namespace pipeline
        {
            struct sink_link
            {
                GstPad *tee_pad;
                GstElement *upstream_joint;
                void *pipeline;
                gboolean video_probe_invoke_control;
                gboolean audio_probe_invoke_control;

                sink_link(GstPad *pad, GstElement *joint, void *pipe)
                    : upstream_joint(joint)
                    , pipeline(pipe)
                    , tee_pad(pad)
                    , video_probe_invoke_control(FALSE)
                    , audio_probe_invoke_control(FALSE)
                {
                }
            };
            class LiveStream : public libwebstreamer::framework::Pipeline
            {
            public:
                explicit LiveStream(const std::string &id);
                ~LiveStream();
                virtual void add_pipe_joint(GstElement *upstream_joint);
                virtual void remove_pipe_joint(GstElement *upstream_joint);

                virtual void add_test_sink(const std::string &name);
                virtual void remove_fake_sink();

            protected:
                virtual bool on_add_endpoint(const std::shared_ptr<libwebstreamer::framework::Endpoint> endpoint);
                virtual bool on_remove_endpoint(const std::shared_ptr<libwebstreamer::framework::Endpoint> endpoint);
                virtual bool MessageHandler(GstMessage *msg);

                static GstPadProbeReturn on_tee_pad_add_video_probe(GstPad *pad, GstPadProbeInfo *probe_info, gpointer data);
                static GstPadProbeReturn on_tee_pad_add_audio_probe(GstPad *pad, GstPadProbeInfo *probe_info, gpointer data);
                static GstPadProbeReturn on_tee_pad_remove_video_probe(GstPad *pad, GstPadProbeInfo *probe_info, gpointer data);
                static GstPadProbeReturn on_tee_pad_remove_audio_probe(GstPad *pad, GstPadProbeInfo *probe_info, gpointer data);

            private:
                GstElement *video_tee_;
                GstElement *audio_tee_;

                std::list<sink_link *> sinks_;//all the request pad of tee, release when removing from pipeline

                GstPad *video_tee_pad_;
                GstElement *fake_video_queue_;
                GstElement *fake_video_sink_;
                GstPad *audio_tee_pad_;
                GstElement *fake_audio_queue_;
                GstElement *fake_audio_sink_;
#ifdef USE_AUTO_SINK
                GstElement *fake_video_decodec_;
                GstElement *fake_audio_decodec_;
#endif
            };
        }
    }
}

#endif