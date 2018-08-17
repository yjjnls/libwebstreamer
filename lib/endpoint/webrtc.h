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

#ifndef _LIBWEBSTREAMER_ENDPOINT_WEBRTC_H_
#define _LIBWEBSTREAMER_ENDPOINT_WEBRTC_H_


#include <framework/app.h>
#include <utils/pipejoint.h>

class WebRTC : public IEndpoint
{
 public:
    WebRTC(IApp *app, const std::string &name);
    ~WebRTC();

    virtual bool initialize(Promise *promise);
    virtual void terminate();

    void set_remote_description(Promise *promise);
    void set_remote_candidate(Promise *promise);

    GstElement *pipeline() { return pipeline_; }
    std::string &launch() { return launch_; }

    GstElement *video_output_pipejoint() { return video_output_joint_.upstream_joint; }
    GstElement *video_input_pipejoint() { return video_input_joint_.downstream_joint; }
    GstElement *audio_output_pipejoint() { return audio_output_joint_.upstream_joint; }
    GstElement *audio_input_pipejoint() { return audio_input_joint_.downstream_joint; }

 private:
    static void on_ice_candidate(GstElement *webrtc G_GNUC_UNUSED,
                                 guint mlineindex,
                                 gchar *candidate,
                                 gpointer user_data G_GNUC_UNUSED);
    static void on_sdp_created(GstPromise *promise, gpointer user_data);
    static void on_negotiation_needed(GstElement *element, gpointer user_data);
    static void on_webrtc_pad_added(GstElement *webrtc, GstPad *new_pad, gpointer user_data);

    static GstPadProbeReturn on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);



    bool add_to_pipeline();

    GstElement *pipeline_;
    GstElement *bin_;
    GstElement *webrtc_;

    PipeJoint video_input_joint_;
    PipeJoint video_output_joint_;
    PipeJoint audio_input_joint_;
    PipeJoint audio_output_joint_;
    std::string role_;
    std::string launch_;
};
#endif
