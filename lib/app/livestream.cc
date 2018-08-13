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

#include "livestream.h"
#include <endpoint/rtspclient.h>
#include <endpoint/rtspservice.h>
#include <endpoint/webrtc.h>
#include <webstreamer.h>
#include <utils/typedef.h>

using json = nlohmann::json;

GST_DEBUG_CATEGORY_STATIC(my_category);
#define GST_CAT_DEFAULT my_category

std::mutex LiveStream::joint_mutex_;

LiveStream::LiveStream(const std::string &name, WebStreamer *ws)
    : IApp(name, ws)
    , video_tee_(NULL)
    , audio_tee_(NULL)
    , performer_(NULL)
    , video_tee_pad_(NULL)
    , fake_video_queue_(NULL)
    , fake_video_sink_(NULL)
    , audio_tee_pad_(NULL)
    , fake_audio_queue_(NULL)
    , fake_audio_sink_(NULL)
{
}

LiveStream::~LiveStream()
{
}
bool LiveStream::Initialize(Promise *promise)
{
    IApp::Initialize(promise);
    video_tee_ = gst_element_factory_make("tee", "video_tee");
    g_object_set(video_tee_, "allow-not-linked", true, NULL);
    audio_tee_ = gst_element_factory_make("tee", "audio_tee");
    g_object_set(audio_tee_, "allow-not-linked", true, NULL);
    g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), video_tee_));
    g_warn_if_fail(gst_bin_add(GST_BIN(pipeline()), audio_tee_));

    GST_DEBUG_CATEGORY_INIT(my_category, "webstreamer", 2, "libWebStreamer");

    return true;
}
bool LiveStream::Destroy(Promise *promise)
{
    gst_element_set_state(pipeline(), GST_STATE_NULL);
    if (video_tee_pad_) {
        gst_element_release_request_pad(video_tee_, video_tee_pad_);
    }
    if (audio_tee_pad_) {
        gst_element_release_request_pad(audio_tee_, audio_tee_pad_);
    }
    if (!sinks_.empty()) {
        for (auto info : sinks_) {
            GstElement *upstream_joint = info->upstream_joint;
            LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

            // remove pipeline dynamicly
            g_warn_if_fail(gst_bin_remove(GST_BIN(pipeline->pipeline()), upstream_joint));
            gst_element_unlink(pipeline->video_tee_, upstream_joint);

            gst_element_release_request_pad(pipeline->video_tee_, info->tee_pad);
            gst_object_unref(info->tee_pad);
            delete info;
        }
    }
    IApp::Destroy(promise);
    return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void LiveStream::On(Promise *promise)
{
    const json &j = promise->meta();
    std::string action = j["action"];
    if (action == "add_performer") {
        add_performer(promise);
    } else if (action == "add_audience") {
        add_audience(promise);
    } else if (action == "remove_audience") {
        remove_audience(promise);
    } else if (action == "startup") {
        Startup(promise);
    } else if (action == "stop") {
        Stop(promise);
    } else if (action == "remote_sdp") {
        set_remote_description(promise);
    } else if (action == "remote_candidate") {
        set_remote_candidate(promise);
    } else {
        GST_ERROR("[livestream] action: %s is not supported!", action.c_str());
        promise->reject("action: " + action + " is not supported!");
    }
}
void LiveStream::add_performer(Promise *promise)
{
    if (performer_ != NULL) {
        GST_ERROR("[livestream] there's already a performer!");
        promise->reject("there's already a performer!");
        return;
    }
    // create endpoint
    const json &j = promise->data();
    const std::string &name = j["name"];
    performer_ = new RtspClient(this, name);
    // initialize endpoint and add it to the pipeline
    bool rc = performer_->initialize(promise);
    if (rc) {
        // link endpoint to video/audio tee
        if (on_add_endpoint(performer_)) {
            GST_INFO("[livestream] add performer: %s (type: %s)", name.c_str(), performer_->protocol().c_str());
            promise->resolve();
            return;
        }
    }
    performer_->terminate();
    delete performer_;
    performer_ = NULL;
    GST_ERROR("[livestream] add performer: %s failed!", name.c_str());
    promise->reject("add performer " + name + " failed!");
}
std::list<IEndpoint *>::iterator LiveStream::find_audience(const std::string &name)
{
    return std::find_if(audiences_.begin(),
                        audiences_.end(),
                        [&name](IEndpoint *ep) {
                            return ep->name() == name;
                        });
}

void LiveStream::add_audience(Promise *promise)
{
    // get endpoint protocol
    const json &j = promise->data();
    if (j.find("protocol") == j.end()) {
        GST_ERROR("[livestream] no protocol in audience.");
        promise->reject("[livestream] no protocol in audience.");
        return;
    }
    const std::string &name = j["name"];
    const std::string protocol = j["protocol"];
    if (find_audience(name) != audiences_.end()) {
        GST_ERROR("[livestream] audience: %s has been added.", name.c_str());
        promise->reject("[livestream] audience: " + name + " has been added.");
        return;
    }
    // create endpoint
    IEndpoint *ep;
    switch (get_endpoint_type(protocol)) {
        case EndpointType::RTSP_SERVER: {
            ep = new IRTSPService(this, name, RTSPServer::RFC7826);
            // set path
            const std::string path = j["path"];
            static_cast<IRTSPService *>(ep)->path() = path;
            // set launch
            std::string launch = "( ";
            if (!video_encoding().empty())
                launch += "rtp" + video_encoding() + "pay pt=96 name=pay0";
            if (!audio_encoding().empty())
                launch += "  rtp" + audio_encoding() + "pay pt=97 name=pay1";
            launch += " )";
            static_cast<IRTSPService *>(ep)->launch() = launch;
        } break;
        case EndpointType::WEBRTC: {
            ep = new WebRTC(this, name);
        } break;
        default: {
            GST_ERROR("[livestream] protocol: %s not supported.", protocol.c_str());
            promise->reject("[livestream] protocol: " + protocol + " not supported.");
            return;
        }
    }
    ep->protocol() = protocol;
    bool rc = ep->initialize(promise);
    if (rc) {
        // add endpoint to pipeline and link with tee
        audiences_.push_back(ep);
        GST_INFO("[livestream] add audience: %s (type: %s)", name.c_str(), protocol.c_str());
        promise->resolve();
        return;
    }
    ep->terminate();
    delete ep;
    ep = NULL;
    GST_ERROR("[livestream] add audience: %s failed!", name.c_str());
    promise->reject("add audience " + name + " failed!");
}
void LiveStream::remove_audience(Promise *promise)
{
    const json &j = promise->data();
    const std::string &name = j["name"];
    auto it = find_audience(name);
    if (it == audiences_.end()) {
        GST_ERROR("[livestream] audience: %s has not been added.", name.c_str());
        promise->reject("[livestream] audience: " + name + " has not been added.");
        return;
    }
    IEndpoint *ep = *it;
    ep->terminate();
    audiences_.erase(it);

    GST_INFO("[livestream] remove audience: %s (type: %s)", ep->name().c_str(), ep->protocol().c_str());

    delete ep;
    promise->resolve();
}

void LiveStream::Startup(Promise *promise)
{
    if (!performer_) {
        GST_ERROR("[livestream] there's no performer now, can't startup!");
        promise->reject("there's no performer now, can't startup!");
        return;
    }
    gst_element_set_state(pipeline(), GST_STATE_PLAYING);
    promise->resolve();
}
void LiveStream::Stop(Promise *promise)
{
    gst_element_set_state(pipeline(), GST_STATE_NULL);
    if (performer_) {
        GST_DEBUG("[livestream] remove performer: %s (type: %s)",
                  performer_->name().c_str(),
                  performer_->protocol().c_str());
        performer_->terminate();
        delete performer_;
    }
    for (auto audience : audiences_) {
        GST_DEBUG("[livestream] remove audience: %s (type: %s)",
                  audience->name().c_str(),
                  audience->protocol().c_str());
        audience->terminate();
        delete audience;
    }
    promise->resolve();
}

void LiveStream::set_remote_description(Promise *promise)
{
    const json &j = promise->data();

    const std::string &name = j["name"];
    auto it = find_audience(name);
    if (it == audiences_.end()) {
        GST_ERROR("[livestream] audience: %s has not been added.", name.c_str());
        promise->reject("[livestream] audience: " + name + " has not been added.");
        return;
    }
    WebRTC *ep = static_cast<WebRTC *>(*it);
    ep->set_remote_description(promise);

    promise->resolve();
}
void LiveStream::set_remote_candidate(Promise *promise)
{
    const json &j = promise->data();

    const std::string &name = j["name"];
    auto it = find_audience(name);
    if (it == audiences_.end()) {
        GST_ERROR("[livestream] audience: %s has not been added.", name.c_str());
        promise->reject("[livestream] audience: " + name + " has not been added.");
        return;
    }
    WebRTC *ep = static_cast<WebRTC *>(*it);
    ep->set_remote_candidate(promise);

    promise->resolve();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// for rtspclient
bool LiveStream::on_add_endpoint(IEndpoint *endpoint)
{
    switch (get_endpoint_type(endpoint->protocol())) {
        case EndpointType::RTSP_CLIENT: {
            if (!video_encoding().empty()) {
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

                // monitor data probe
                // GstPad *pad = gst_element_get_static_pad(fake_video_queue_, "src");
                // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, this, NULL);
                // gst_object_unref(pad);
            }
            if (!audio_encoding().empty()) {
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
                gst_bin_add_many(GST_BIN(pipeline()),
                                 fake_audio_decodec_,
                                 fake_audio_queue_,
                                 fake_audio_sink_,
                                 fake_audio_convert_,
                                 fake_audio_resample_,
                                 NULL);
                gst_element_link_many(fake_audio_queue_,
                                      fake_audio_decodec_,
                                      fake_audio_convert_,
                                      fake_audio_resample_,
                                      fake_audio_sink_,
                                      NULL);
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
        }
        // gst_element_set_state(pipeline(), GST_STATE_PLAYING);
        break;
        case EndpointType::RTSP_SERVER: {
            // std::string launch = "( ";
            // if (!video_encoding().empty())
            //     launch += "rtp" + video_encoding() + "pay pt=96 name=pay0";
            // if (!audio_encoding().empty())
            //     launch += "  rtp" + audio_encoding() + "pay pt=97 name=pay1";
            // launch += " )";

            // IRTSPService *ep = static_cast<IRTSPService *>(endpoint);
            // bool rc = ep->Launch(ep->path(),
            //                      launch,
            //                      (GCallback)IRTSPService::on_rtsp_media_constructed,
            //                      NULL);
            // if (!rc)
            //     return false;
        } break;
        // case EndpointType::WEBRTC:
        //     break;
        default:
            g_warn_if_reached();
            return false;
    }
    return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void LiveStream::add_pipe_joint(GstElement *upstream_joint, GstElement *downstream_joint)
{
    link_stream_output_joint(upstream_joint);
}
void LiveStream::remove_pipe_joint(GstElement *upstream_joint, GstElement *downstream_joint)
{
    remove_stream_output_joint(upstream_joint);
}
// for other endpoint
void LiveStream::link_stream_output_joint(GstElement *upstream_joint)
{
    joint_mutex_.lock();
    gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
    if (g_str_equal(media_type, "video")) {
        GST_DEBUG("[livestream] add pipe joint: video");
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
    } else if (g_str_equal(media_type, "audio")) {
        GST_DEBUG("[livestream] add pipe joint: audio");
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
    joint_mutex_.unlock();
}
GstPadProbeReturn LiveStream::on_tee_pad_remove_video_probe(GstPad *teepad, GstPadProbeInfo *probe_info, gpointer data)
{
    sink_link *info = static_cast<sink_link *>(data);
    if (!g_atomic_int_compare_and_exchange(&info->video_probe_invoke_control, TRUE, FALSE)) {
        return GST_PAD_PROBE_OK;
    }

    GstElement *upstream_joint = info->upstream_joint;
    LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

    // remove pipeline dynamicaly
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
    if (!g_atomic_int_compare_and_exchange(&info->audio_probe_invoke_control, TRUE, FALSE)) {
        return GST_PAD_PROBE_OK;
    }

    GstElement *upstream_joint = info->upstream_joint;
    LiveStream *pipeline = static_cast<LiveStream *>(info->pipeline);

    // remove pipeline dynamicaly
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
void LiveStream::remove_stream_output_joint(GstElement *upstream_joint)
{
    joint_mutex_.lock();
    gchar *media_type = (gchar *)g_object_get_data(G_OBJECT(upstream_joint), "media-type");
    if (g_str_equal(media_type, "video")) {
        auto it = sinks_.begin();
        for (; it != sinks_.end(); ++it) {
            if ((*it)->upstream_joint == upstream_joint) {
                break;
            }
        }
        if (it == sinks_.end()) {
            g_warn_if_reached();
            // TODO(yuanjunjie) notify application
            return;
        }
        (*it)->video_probe_invoke_control = TRUE;
        gst_pad_add_probe((*it)->tee_pad, GST_PAD_PROBE_TYPE_IDLE, on_tee_pad_remove_video_probe, *it, NULL);
        sinks_.erase(it);
        GST_DEBUG("[livestream] remove video joint completed");
    } else if (g_str_equal(media_type, "audio")) {
        auto it = sinks_.begin();
        for (; it != sinks_.end(); ++it) {
            if ((*it)->upstream_joint == upstream_joint) {
                break;
            }
        }
        if (it == sinks_.end()) {
            g_warn_if_reached();
            // TODO(yuanjunjie) notify application
            return;
        }
        (*it)->audio_probe_invoke_control = TRUE;
        gst_pad_add_probe((*it)->tee_pad, GST_PAD_PROBE_TYPE_IDLE, on_tee_pad_remove_audio_probe, *it, NULL);
        sinks_.erase(it);
        GST_DEBUG("[livestream] remove audio joint completed");
    }
    joint_mutex_.unlock();
}



GstPadProbeReturn LiveStream::on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    // static int count = 0;
    // auto pipeline = static_cast<LiveStream *>(user_data);
    // printf("+%d", GST_STATE(pipeline->pipeline()));
    return GST_PAD_PROBE_OK;
}

bool LiveStream::MessageHandler(GstMessage *msg)
{
    return true;
}
