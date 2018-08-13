/*
 * Copyright 2018 KEDACOM Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBWEBSTREAMER_APPLICATION_MULTIPOINTS_H_
#define _LIBWEBSTREAMER_APPLICATION_MULTIPOINTS_H_

#include <framework/app.h>
#include <mutex>  // NOLINT

// #define USE_AUTO_SINK 1
struct sink_link
{
    GstElement *joint;
    GstPad *request_pad;
    void *pipeline;
    gboolean video_probe_invoke_control;
    gboolean audio_probe_invoke_control;
    bool is_output;

    sink_link(GstPad *pad, GstElement *joint_element, void *pipe, bool output = true)
        : joint(joint_element)
        , request_pad(pad)
        , pipeline(pipe)
        , video_probe_invoke_control(FALSE)
        , audio_probe_invoke_control(FALSE)
        , is_output(output)
    {
    }
};
class WebStreamer;
class MultiPoints : public IApp
{
 public:
    APP(MultiPoints);

    MultiPoints(const std::string &name, WebStreamer *ws);
    ~MultiPoints();
    void link_stream_output_joint(GstElement *upstream_joint);
    void remove_stream_output_joint(GstElement *upstream_joint);
    void link_stream_input_joint(GstElement *downstream_joint);
    void remove_stream_input_joint(GstElement *downstream_joint);

    virtual void On(Promise *promise);
    virtual bool Initialize(Promise *promise);
    virtual bool Destroy(Promise *promise);

 protected:
    void Startup(Promise *promise);
    void Stop(Promise *promise);
    void add_member(Promise *promise);
    void set_speaker(Promise *promise);
    void remove_member(Promise *promise);
    void set_remote_description(Promise *promise);
    void set_remote_candidate(Promise *promise);

    bool on_add_endpoint(IEndpoint *endpoint);
    virtual bool MessageHandler(GstMessage *msg);

 private:
    static GstPadProbeReturn
    on_request_pad_remove_video_probe(GstPad *pad,
                                      GstPadProbeInfo *probe_info,
                                      gpointer data);

    static GstPadProbeReturn
    on_request_pad_remove_audio_probe(GstPad *pad,
                                      GstPadProbeInfo *probe_info,
                                      gpointer data);

    static GstPadProbeReturn on_monitor_data(GstPad *pad,
                                             GstPadProbeInfo *info,
                                             gpointer user_data);
    std::list<IEndpoint *>::iterator find_member(const std::string &name);


    GstElement *video_tee_;
    GstElement *audio_tee_;
    GstElement *video_selector_;
    GstElement *audio_selector_;
    GstElement *default_video_src_;
    GstElement *default_audio_src_;
    std::list<WebRTC *> members_;
    IEndpoint *speaker_;

    std::list<sink_link *> tee_sinks_;  // all the request pad of tee,
                                        // release when removing from
                                        // pipeline
    std::list<sink_link *> selector_sinks_;
    static std::mutex tee_mutex_;
    static std::mutex selector_mutex_;

#ifdef USE_AUTO_SINK
    GstElement *fake_video_decodec_;
    GstElement *fake_audio_decodec_;
    GstElement *fake_audio_convert_;
    GstElement *fake_audio_resample_;
#endif
};

#endif
