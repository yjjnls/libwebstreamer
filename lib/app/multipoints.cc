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

#include "multipoints.h"
#include <endpoint/rtspclient.h>
#include <endpoint/rtspservice.h>
#include <endpoint/webrtc.h>
#include <webstreamer.h>
#include <utils/typedef.h>

using json = nlohmann::json;

GST_DEBUG_CATEGORY_STATIC(my_category);
#define GST_CAT_DEFAULT my_category

std::mutex MultiPoints::tee_mutex_;
std::mutex MultiPoints::selector_mutex_;

MultiPoints::MultiPoints(const std::string &name, WebStreamer *ws)
    : IApp(name, ws)
    , video_tee_(NULL)
    , audio_tee_(NULL)
    , video_selector_(NULL)
    , audio_selector_(NULL)
    , default_video_src_(NULL)
    , default_audio_src_(NULL)
    , speaker_(NULL)
    , bin_(NULL)
{
}

MultiPoints::~MultiPoints()
{
}
bool MultiPoints::Initialize(Promise *promise)
{
    IApp::Initialize(promise);

#ifdef USE_VP8
    std::string launch = "videotestsrc ! timeoverlay valignment=3 halignment=4 time-mode=2 xpos=0 ypos=0 color=4278190080 font-desc=\"Sans 48\" draw-shadow=false draw-outline=true outline-color=4278190080 ! vp8enc ! rtpvp8pay ! rtpvp8depay name=default_video_src input-selector name=video-input-selector ! tee name=video-tee allow-not-linked=true  audiotestsrc ! alawenc ! rtppcmapay ! rtppcmadepay name=default_audio_src input-selector name=audio-input-selector ! tee name=audio-tee allow-not-linked=true";  // NOLINT
#elif USE_H264
    std::string launch = "videotestsrc ! timeoverlay valignment=3 halignment=4 time-mode=2 xpos=0 ypos=0 color=4278190080 font-desc=\"Sans 48\" draw-shadow=false draw-outline=true outline-color=4278190080 ! x264enc ! rtph264pay config-interval=-1 ! rtph264depay name=default_video_src input-selector name=video-input-selector ! tee name=video-tee allow-not-linked=true  audiotestsrc ! alawenc ! rtppcmapay ! rtppcmadepay name=default_audio_src input-selector name=audio-input-selector ! tee name=audio-tee allow-not-linked=true";  // NOLINT
#endif
    GError *error = NULL;

    bin_ = gst_parse_launch(launch.c_str(), &error);

    if (error) {
        g_printerr("Failed to parse launch: %s\n", error->message);
        g_error_free(error);
        g_clear_object(&bin_);
        bin_ = NULL;
        return false;
    }
    gst_bin_add(GST_BIN(pipeline()), bin_);
    video_tee_ = gst_bin_get_by_name(GST_BIN(bin_), "video-tee");
    g_assert_nonnull(video_tee_);
    video_selector_ = gst_bin_get_by_name(GST_BIN(bin_), "video-input-selector");
    g_assert_nonnull(video_selector_);

    // GstPad *testpad = gst_element_get_static_pad(video_selector_, "src");
    // gst_pad_add_probe(testpad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, NULL, NULL);
    // gst_object_unref(testpad);

    audio_tee_ = gst_bin_get_by_name(GST_BIN(bin_), "audio-tee");
    g_assert_nonnull(audio_tee_);
    audio_selector_ = gst_bin_get_by_name(GST_BIN(bin_), "audio-input-selector");
    g_assert_nonnull(audio_selector_);

    default_video_src_ = gst_bin_get_by_name(GST_BIN(bin_), "default_video_src");
    g_warn_if_fail(gst_element_link(default_video_src_, video_selector_));
    default_audio_src_ = gst_bin_get_by_name(GST_BIN(bin_), "default_audio_src");
    g_warn_if_fail(gst_element_link(default_audio_src_, audio_selector_));

    GST_DEBUG_CATEGORY_INIT(my_category, "webstreamer", 2, "libWebStreamer");

    return true;
}
bool MultiPoints::Destroy(Promise *promise)
{
    gst_element_set_state(pipeline(), GST_STATE_NULL);
    g_assert(selector_sinks_.empty());
    g_assert(tee_sinks_.empty());
    gst_bin_remove(GST_BIN(pipeline()), bin_);

    gst_object_unref(video_tee_);
    gst_object_unref(video_selector_);
    gst_object_unref(default_video_src_);
    gst_object_unref(audio_tee_);
    gst_object_unref(audio_selector_);
    gst_object_unref(default_audio_src_);

    IApp::Destroy(promise);
    return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void MultiPoints::On(Promise *promise)
{
    const json &j = promise->meta();
    std::string action = j["action"];
    if (action == "add_member") {
        add_member(promise);
    } else if (action == "set_speaker") {
        set_speaker(promise);
    } else if (action == "remove_member") {
        remove_member(promise);
    } else if (action == "startup") {
        Startup(promise);
    } else if (action == "stop") {
        Stop(promise);
    } else if (action == "remote_sdp") {
        set_remote_description(promise);
    } else if (action == "remote_candidate") {
        set_remote_candidate(promise);
    } else {
        GST_ERROR("[multipoints] action: %s is not supported!", action.c_str());
        promise->reject("action: " + action + " is not supported!");
    }
}
void MultiPoints::set_speaker(Promise *promise)
{
    if (speaker_ != NULL) {
        GST_ERROR("[multipoints] there's already a speaker!");
        promise->reject("there's already a speaker!");
        return;
    }
    // create endpoint
    const json &j = promise->data();
    const std::string &name = j["name"];

    auto it = find_member(name);
    if (it == members_.end()) {
        GST_ERROR("[multipoints] member: %s has not been added.", name.c_str());
        promise->reject("[multipoints] member: " + name + " has not been added.");
        return;
    }
    if (*it != speaker_) {
        WebRTC *ep = static_cast<WebRTC *>(*it);
        // video
        GstElement *video_downstream_joint = ep->video_input_pipejoint();
        GstPad *video_output_src_pad = gst_element_get_static_pad(video_downstream_joint, "src");
        GstPad *video_selector_sink_pad = gst_pad_get_peer(video_output_src_pad);
        g_assert_nonnull(video_selector_sink_pad);
        g_object_set(G_OBJECT(video_selector_), "active-pad", video_selector_sink_pad, NULL);

        gst_pad_send_event(video_output_src_pad,
                           gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM,
                                                gst_structure_new("GstForceKeyUnit",
                                                                  "all-headers",
                                                                  G_TYPE_BOOLEAN,
                                                                  TRUE,
                                                                  NULL)));
        gst_object_unref(video_output_src_pad);
        // audio
        GstElement *audio_downstream_joint = ep->audio_input_pipejoint();
        GstPad *audio_output_src_pad = gst_element_get_static_pad(audio_downstream_joint, "src");
        GstPad *audio_selector_sink_pad = gst_pad_get_peer(audio_output_src_pad);
        g_assert_nonnull(audio_selector_sink_pad);
        g_object_set(G_OBJECT(audio_selector_), "active-pad", audio_selector_sink_pad, NULL);
        gst_object_unref(audio_output_src_pad);

        speaker_ = ep;
    }
    GST_INFO("[multipoints] set speaker: %s.", name.c_str());
    promise->resolve();
    return;
}
std::list<IEndpoint *>::iterator MultiPoints::find_member(const std::string &name)
{
    return std::find_if(members_.begin(),
                        members_.end(),
                        [&name](IEndpoint *ep) {
                            return ep->name() == name;
                        });
}
void MultiPoints::set_speaker_default()
{
    // video
    // g_warn_if_fail(gst_element_link(default_video_src_, video_selector_));
    GstPad *video_output_src_pad = gst_element_get_static_pad(default_video_src_, "src");
    GstPad *video_selector_sink_pad = gst_pad_get_peer(video_output_src_pad);
    g_object_set(G_OBJECT(video_selector_), "active-pad", video_selector_sink_pad, NULL);
    gst_object_unref(video_output_src_pad);
    // audio
    // g_warn_if_fail(gst_element_link(default_audio_src_, audio_selector_));
    GstPad *audio_output_src_pad = gst_element_get_static_pad(default_audio_src_, "src");
    GstPad *audio_selector_sink_pad = gst_pad_get_peer(audio_output_src_pad);
    g_object_set(G_OBJECT(audio_selector_), "active-pad", audio_selector_sink_pad, NULL);
    gst_object_unref(audio_output_src_pad);

    speaker_ = NULL;
    // usleep(1 * 1000000);
}

void MultiPoints::add_member(Promise *promise)
{
    // get endpoint protocol
    const json &j = promise->data();
    if (j.find("protocol") == j.end()) {
        GST_ERROR("[multipoints] no protocol in member.");
        promise->reject("[multipoints] no protocol in member.");
        return;
    }
    const std::string &name = j["name"];
    const std::string protocol = j["protocol"];
    if (find_member(name) != members_.end()) {
        GST_ERROR("[multipoints] member: %s has been added.", name.c_str());
        promise->reject("[multipoints] member: " + name + " has been added.");
        return;
    }

    if (j.find("video_codec") != j.end()) {
        const std::string video_codec = j["video_codec"];
        if (video_encoding().empty())
            video_encoding() = video_codec;
    }
    if (j.find("audio_codec") != j.end()) {
        const std::string audio_codec = j["audio_codec"];
        if (audio_encoding().empty())
            audio_encoding() = audio_codec;
    }

    // create endpoint
    IEndpoint *ep = new WebRTC(this, name);

    ep->protocol() = protocol;
    bool rc = ep->initialize(promise);
    if (rc) {
        // add endpoint to pipeline and link with tee
        members_.push_back(ep);
        GST_INFO("[multipoints] add member: %s (type: %s)", name.c_str(), protocol.c_str());
        promise->resolve();
        return;
    }
    ep->terminate();
    delete ep;
    ep = NULL;
    GST_ERROR("[multipoints] add member: %s failed!", name.c_str());
    promise->reject("add member " + name + " failed!");
}
void MultiPoints::remove_member(Promise *promise)
{
    const json &j = promise->data();
    const std::string &name = j["name"];
    auto it = find_member(name);
    if (it == members_.end()) {
        GST_ERROR("[multipoints] member: %s has not been added.", name.c_str());
        promise->reject("[multipoints] member: " + name + " has not been added.");
        return;
    }
    IEndpoint *ep = *it;
    if (speaker_ == ep) {
        set_speaker_default();
        GST_INFO("[multipoints] member: %s (type: %s) stops speaking and is being removed.", ep->name().c_str(), ep->protocol().c_str());
    }
    ep->terminate();
    members_.erase(it);

    GST_INFO("[multipoints] remove member: %s (type: %s)", ep->name().c_str(), ep->protocol().c_str());

    delete ep;
    promise->resolve();
}

void MultiPoints::Startup(Promise *promise)
{
    gst_element_set_state(pipeline(), GST_STATE_PLAYING);
    promise->resolve();
}
void MultiPoints::Stop(Promise *promise)
{
    set_speaker_default();
    for (auto ep : members_) {
        ep->terminate();
        GST_DEBUG("[multipoints] remove member: %s (type: %s)",
                  ep->name().c_str(),
                  ep->protocol().c_str());
        delete ep;
    }
    members_.clear();

    promise->resolve();
}

void MultiPoints::set_remote_description(Promise *promise)
{
    const json &j = promise->data();

    const std::string &name = j["name"];
    auto it = find_member(name);
    if (it == members_.end()) {
        GST_ERROR("[multipoints] member: %s has not been added.", name.c_str());
        promise->reject("[multipoints] member: " + name + " has not been added.");
        return;
    }
    WebRTC *ep = static_cast<WebRTC *>(*it);
    ep->set_remote_description(promise);

    promise->resolve();
}
void MultiPoints::set_remote_candidate(Promise *promise)
{
    const json &j = promise->data();

    const std::string &name = j["name"];
    auto it = find_member(name);
    if (it == members_.end()) {
        GST_ERROR("[multipoints] member: %s has not been added.", name.c_str());
        promise->reject("[multipoints] member: " + name + " has not been added.");
        return;
    }
    WebRTC *ep = static_cast<WebRTC *>(*it);
    ep->set_remote_candidate(promise);

    promise->resolve();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// for other endpoint
void MultiPoints::add_pipe_joint(GstElement *upstream_joint, GstElement *downstream_joint)
{
    if (upstream_joint != NULL)
        link_stream_output_joint(upstream_joint);
    if (downstream_joint != NULL)
        link_stream_input_joint(downstream_joint);
}
void MultiPoints::remove_pipe_joint(GstElement *upstream_joint, GstElement *downstream_joint)
{
    if (upstream_joint != NULL)
        remove_stream_output_joint(upstream_joint);
    if (downstream_joint != NULL)
        remove_stream_input_joint(downstream_joint);
}

void MultiPoints::link_stream_output_joint(GstElement *upstream_joint)
{
    g_assert_nonnull(upstream_joint);
    std::lock_guard<std::mutex> lck(tee_mutex_);

    gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
    g_assert_nonnull(media_type);
    if (g_str_equal(media_type, "video")) {
        GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(video_tee_), "src_%u");
        GstPad *pad = gst_element_request_pad(video_tee_, templ, NULL, NULL);
        sink_link *info = new sink_link(pad, upstream_joint, this);

        g_warn_if_fail(gst_bin_add(GST_BIN(bin_), upstream_joint));
        gst_element_sync_state_with_parent(upstream_joint);

        GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");

        // GstPad *testpad = gst_element_get_static_pad(upstream_joint, "sink");
        // gst_pad_add_probe(testpad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, NULL, NULL);
        // gst_object_unref(testpad);

        GstPadLinkReturn ret = gst_pad_link(pad, sinkpad);
        g_warn_if_fail(ret == GST_PAD_LINK_OK);
        gst_object_unref(sinkpad);

        tee_sinks_.push_back(info);
        GST_DEBUG("[multipoints] link output video.");
    } else if (g_str_equal(media_type, "audio")) {
        GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(audio_tee_), "src_%u");
        GstPad *pad = gst_element_request_pad(audio_tee_, templ, NULL, NULL);
        sink_link *info = new sink_link(pad, upstream_joint, this);

        g_warn_if_fail(gst_bin_add(GST_BIN(bin_), upstream_joint));
        gst_element_sync_state_with_parent(upstream_joint);

        GstPad *sinkpad = gst_element_get_static_pad(upstream_joint, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, sinkpad);
        g_warn_if_fail(ret == GST_PAD_LINK_OK);
        gst_object_unref(sinkpad);

        tee_sinks_.push_back(info);
        GST_DEBUG("[multipoints] link output audio.");
        // g_print("~~~~~~~~~~link output audio~~~~~~~\n");
    }
}
void MultiPoints::remove_stream_output_joint(GstElement *upstream_joint)
{
    std::lock_guard<std::mutex> lck(tee_mutex_);


    auto it = tee_sinks_.begin();
    for (; it != tee_sinks_.end(); ++it) {
        if ((*it)->joint == upstream_joint) {
            break;
        }
    }
    if (it == tee_sinks_.end()) {
        g_warn_if_reached();
        // TODO(yuanjunjie) notify application
        return;
    }


    gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
    if (g_str_equal(media_type, "video")) {
        (*it)->video_probe_invoke_control = TRUE;
        gst_pad_add_probe((*it)->request_pad, GST_PAD_PROBE_TYPE_IDLE, on_request_pad_remove_video_probe, *it, NULL);
        GST_DEBUG("[multipoints] remove output video.");
    } else if (g_str_equal(media_type, "audio")) {
        (*it)->audio_probe_invoke_control = TRUE;
        gst_pad_add_probe((*it)->request_pad, GST_PAD_PROBE_TYPE_IDLE, on_request_pad_remove_audio_probe, *it, NULL);
        GST_DEBUG("[multipoints] remove output audio.");
    }
    tee_sinks_.erase(it);
}
void MultiPoints::link_stream_input_joint(GstElement *downstream_joint)
{
    g_assert_nonnull(downstream_joint);
    std::lock_guard<std::mutex> lck(selector_mutex_);

    gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(downstream_joint), "media-type");
    g_assert_nonnull(media_type);

    if (g_str_equal(media_type, "video")) {
        GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(video_selector_), "sink_%u");
        GstPad *pad = gst_element_request_pad(video_selector_, templ, NULL, NULL);
        sink_link *info = new sink_link(pad, downstream_joint, this, false);

        g_warn_if_fail(gst_bin_add(GST_BIN(bin_), downstream_joint));
        gst_element_sync_state_with_parent(downstream_joint);


        GstPad *srcpad = gst_element_get_static_pad(downstream_joint, "src");

        // GstPad *testpad = gst_element_get_static_pad(downstream_joint, "src");
        // gst_pad_add_probe(testpad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, NULL, NULL);
        // gst_object_unref(testpad);

        GstPadLinkReturn ret = gst_pad_link(srcpad, pad);
        g_warn_if_fail(ret == GST_PAD_LINK_OK);
        gst_object_unref(srcpad);

        selector_sinks_.push_back(info);
        GST_DEBUG("[multipoints] link input video.");
    } else if (g_str_equal(media_type, "audio")) {
        GstPadTemplate *templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(audio_selector_), "sink_%u");
        GstPad *pad = gst_element_request_pad(audio_selector_, templ, NULL, NULL);

        sink_link *info = new sink_link(pad, downstream_joint, this, false);

        g_warn_if_fail(gst_bin_add(GST_BIN(bin_), downstream_joint));
        gst_element_sync_state_with_parent(downstream_joint);

        GstPad *srcpad = gst_element_get_static_pad(downstream_joint, "src");

        // GstPad *testpad = gst_element_get_static_pad(downstream_joint, "src");
        // gst_pad_add_probe(testpad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, NULL, NULL);
        // gst_object_unref(testpad);

        GstPadLinkReturn ret = gst_pad_link(srcpad, pad);
        g_warn_if_fail(ret == GST_PAD_LINK_OK);
        gst_object_unref(srcpad);

        selector_sinks_.push_back(info);
        GST_DEBUG("[multipoints] link input audio.");
        // printf("~~~~~~~~~audio ---> selector\n");
    }
}
void MultiPoints::remove_stream_input_joint(GstElement *downstream_joint)
{
    std::lock_guard<std::mutex> lck(selector_mutex_);

    auto it = selector_sinks_.begin();
    for (; it != selector_sinks_.end(); ++it) {
        if ((*it)->joint == downstream_joint) {
            break;
        }
    }
    if (it == selector_sinks_.end()) {
        g_warn_if_reached();
        // TODO(yuanjunjie) notify application
        return;
    }

    gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(downstream_joint), "media-type");
    if (g_str_equal(media_type, "video")) {
        (*it)->video_probe_invoke_control = TRUE;
        gst_pad_add_probe((*it)->request_pad, GST_PAD_PROBE_TYPE_IDLE, on_request_pad_remove_video_probe, *it, NULL);
        GST_DEBUG("[multipoints] remove input video.");
    } else if (g_str_equal(media_type, "audio")) {
        (*it)->audio_probe_invoke_control = TRUE;
        gst_pad_add_probe((*it)->request_pad, GST_PAD_PROBE_TYPE_IDLE, on_request_pad_remove_audio_probe, *it, NULL);
        GST_DEBUG("[multipoints] remove input audio.");
    }
    selector_sinks_.erase(it);
}
GstPadProbeReturn MultiPoints::on_request_pad_remove_video_probe(GstPad *teepad, GstPadProbeInfo *probe_info, gpointer data)
{
    sink_link *info = static_cast<sink_link *>(data);
    if (!g_atomic_int_compare_and_exchange(&info->video_probe_invoke_control, TRUE, FALSE)) {
        return GST_PAD_PROBE_OK;
    }

    GstElement *joint = info->joint;
    MultiPoints *pipeline = static_cast<MultiPoints *>(info->pipeline);

    // remove pipeline dynamicaly
    if (info->is_output) {
        GstPad *sinkpad = gst_element_get_static_pad(joint, "sink");
        gst_pad_unlink(info->request_pad, sinkpad);
        gst_object_unref(sinkpad);
        gst_element_set_state(joint, GST_STATE_NULL);
        g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->bin_), joint));
        gst_element_release_request_pad(pipeline->video_tee_, info->request_pad);
        gst_object_unref(info->request_pad);
        // printf("~~~~~~~remove video tee pad\n");
    } else {
        GstPad *srckpad = gst_element_get_static_pad(joint, "src");
        gst_pad_unlink(srckpad, info->request_pad);
        gst_object_unref(srckpad);
        gst_element_set_state(joint, GST_STATE_NULL);
        g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->bin_), joint));
        gst_element_release_request_pad(pipeline->video_selector_, info->request_pad);
        gst_object_unref(info->request_pad);
        // printf("~~~~~~~remove video selector pad\n");
    }
    delete static_cast<sink_link *>(data);
    return GST_PAD_PROBE_REMOVE;
}
GstPadProbeReturn MultiPoints::on_request_pad_remove_audio_probe(GstPad *teepad, GstPadProbeInfo *probe_info, gpointer data)
{
    sink_link *info = static_cast<sink_link *>(data);
    if (!g_atomic_int_compare_and_exchange(&info->audio_probe_invoke_control, TRUE, FALSE)) {
        return GST_PAD_PROBE_OK;
    }

    GstElement *joint = info->joint;
    MultiPoints *pipeline = static_cast<MultiPoints *>(info->pipeline);

    // remove pipeline dynamicaly
    if (info->is_output) {
        GstPad *sinkpad = gst_element_get_static_pad(joint, "sink");
        gst_pad_unlink(info->request_pad, sinkpad);
        gst_object_unref(sinkpad);
        gst_element_set_state(joint, GST_STATE_NULL);
        g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->bin_), joint));
        gst_element_release_request_pad(pipeline->audio_tee_, info->request_pad);
        gst_object_unref(info->request_pad);
        // printf("~~~~~~~remove audio tee pad\n");
    } else {
        GstPad *srckpad = gst_element_get_static_pad(joint, "src");
        gst_pad_unlink(srckpad, info->request_pad);
        gst_object_unref(srckpad);
        gst_element_set_state(joint, GST_STATE_NULL);
        g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->bin_), joint));
        gst_element_release_request_pad(pipeline->audio_selector_, info->request_pad);
        gst_object_unref(info->request_pad);
        // printf("~~~~~~~remove audio selector pad\n");
    }
    delete static_cast<sink_link *>(data);
    return GST_PAD_PROBE_REMOVE;
}


GstPadProbeReturn MultiPoints::on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    if (!GST_BUFFER_FLAG_IS_SET(info->data, GST_BUFFER_FLAG_DELTA_UNIT)) {
        static int cnt = 0;
        GDateTime *date_time = g_date_time_new_now_local();
        gchar *s_date_time = g_date_time_format(date_time, "%H:%M:%S,%F");
        g_warning("Received keyframe(%u) @ (%s)", cnt++, s_date_time);
        g_free(s_date_time);
        g_date_time_unref(date_time);
    }
    g_print(".");
    return GST_PAD_PROBE_OK;
}

bool MultiPoints::MessageHandler(GstMessage *msg)
{
    return true;
}
