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

#include "webrtc.h"
#include <utils/typedef.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

using json = nlohmann::json;

GST_DEBUG_CATEGORY_STATIC(my_category);
#define GST_CAT_DEFAULT my_category

WebRTC::WebRTC(IApp *app, const std::string &name)
    : IEndpoint(app, name)
    , pipeline_(NULL)
    , bin_(NULL)
    , webrtc_(NULL)
{
}

WebRTC::~WebRTC()
{
    destroy_pipe_joint(&video_input_joint_);
    destroy_pipe_joint(&video_output_joint_);
    destroy_pipe_joint(&audio_input_joint_);
    destroy_pipe_joint(&audio_output_joint_);
}

void WebRTC::on_ice_candidate(GstElement *webrtc_element G_GNUC_UNUSED,
                              guint mlineindex,
                              gchar *candidate,
                              gpointer user_data G_GNUC_UNUSED)
{
    WebRTC *webrtc = static_cast<WebRTC *>(user_data);
    json data;
    data["candidate"] = candidate;
    data["sdpMLineIndex"] = mlineindex;

    std::string ice_candidate = data.dump();

    // printf("\nice:%s\n", ice_candidate.c_str());

    json meta;
    meta["topic"] = "webrtc@" + webrtc->name();
    meta["origin"] = webrtc->app()->uname();
    meta["type"] = "ice";

    GST_LOG("[webrtc] %p (%s) local candidate created.", webrtc->webrtc_, webrtc->role_.c_str());

    webrtc->app()->Notify(data, meta);
}
void WebRTC::on_sdp_created(GstPromise *promise, gpointer user_data)
{
    WebRTC *webrtc = static_cast<WebRTC *>(user_data);
    GstWebRTCSessionDescription *sdp = NULL;
    const GstStructure *reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, webrtc->role_.c_str(), GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &sdp, NULL);
    gst_promise_unref(promise);
    // TODO(yuanjunjie): only add in H264, if `m=audio` is firstly meet in sdp, it occurred error
    // gst_sdp_media_add_attribute((GstSDPMedia *)&g_array_index(sdp->sdp->medias, GstSDPMedia, 0),
    //                             "fmtp",
    //                             "96 profile-level-id=42e01f");
    for (int i = 0; i < sdp->sdp->medias->len; ++i) {
        GstSDPMedia *sdp_media = (GstSDPMedia *)&g_array_index(sdp->sdp->medias, GstSDPMedia, i);
        if (g_strcmp0(sdp_media->media, "video") == 0) {
            for (int j = 0; j < sdp_media->attributes->len; ++j) {
                GstSDPAttribute *arrtibute = (GstSDPAttribute *)&g_array_index(sdp_media->attributes,
                                                                               GstSDPAttribute,
                                                                               j);
                std::string target = "H264/90000";
                if (g_strcmp0(arrtibute->key, "rtpmap") == 0 &&
                    g_str_has_suffix(arrtibute->value, target.c_str())) {
                    std::string str(arrtibute->value);
                    std::size_t pos = str.find(target);
                    std::string prop = str.substr(0, pos);
                    prop += std::string("profile-level-id=42e01f");

                    gst_sdp_media_add_attribute(sdp_media,
                                                "fmtp",
                                                prop.c_str());
                }
            }
        }
    }

    /* Send sdp to peer */
    std::string sdp_offer(gst_sdp_message_as_text(sdp->sdp));
    json data;
    data["type"] = webrtc->role_;
    data["sdp"] = sdp_offer;
    json meta;
    meta["topic"] = "webrtc@" + webrtc->name();
    meta["origin"] = webrtc->app()->uname();
    meta["type"] = "sdp";
    meta["from"] = webrtc->name();

    g_signal_emit_by_name(webrtc->webrtc_, "set-local-description", sdp, NULL);
    GST_INFO("[webrtc] %s (%p %s) local description created.", webrtc->name().c_str(), webrtc->webrtc_, webrtc->role_.c_str());

    webrtc->app()->Notify(data, meta);

    gst_webrtc_session_description_free(sdp);
}
void WebRTC::on_negotiation_needed(GstElement *element, gpointer user_data)
{
    WebRTC *webrtc = static_cast<WebRTC *>(user_data);
    GstPromise *promise = gst_promise_new_with_change_func(on_sdp_created, user_data, NULL);
    g_signal_emit_by_name(webrtc->webrtc_, "create-offer", NULL, promise);
}
static GstPadProbeReturn cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    printf("-");
    return GST_PAD_PROBE_OK;
}
void WebRTC::on_webrtc_pad_added(GstElement *webrtc_element, GstPad *new_pad, gpointer user_data)
{
    // this function is just for test, it could only show h264 video.
    WebRTC *webrtc = static_cast<WebRTC *>(user_data);
    // printf("=========on_webrtc_pad_added %s===========\n", webrtc->role_.c_str());

    GstElement *out = NULL;
    GstPad *sink = NULL;
    GstCaps *caps;
    GstStructure *s;
    const gchar *encoding_name;

    if (GST_PAD_DIRECTION(new_pad) != GST_PAD_SRC)
        return;

    caps = gst_pad_get_current_caps(new_pad);
    if (!caps)
        caps = gst_pad_query_caps(new_pad, NULL);
    // GST_ERROR_OBJECT(new_pad, "caps %" GST_PTR_FORMAT, caps);
    // g_assert(gst_caps_is_fixed(caps));
    s = gst_caps_get_structure(caps, 0);
    encoding_name = gst_structure_get_string(s, "encoding-name");

    if (g_strcmp0(encoding_name, "VP8") == 0) {
        GST_INFO("[webrtc] %s (%p %s) receive video: VP8.", webrtc->name().c_str(), webrtc, webrtc->role_.c_str());
        out = gst_parse_bin_from_description(
            // "rtpvp8depay ! tee name=local_tee allow-not-linked=true ! queue ! vp8dec ! videoconvert ! ximagesink sync=false",
            "rtpvp8depay ! tee name=local_tee allow-not-linked=true",
            TRUE,
            NULL);

        g_warn_if_fail(gst_bin_add(GST_BIN(out), webrtc->video_input_joint_.upstream_joint));
        GstElement *local_tee = gst_bin_get_by_name_recurse_up(GST_BIN(out), "local_tee");
        g_warn_if_fail(gst_element_link(local_tee, webrtc->video_input_joint_.upstream_joint));
        gst_object_unref(local_tee);

        // webrtc->app()->add_pipe_joint(NULL, webrtc->video_input_joint_.downstream_joint);

    } else if (g_strcmp0(encoding_name, "H264") == 0) {
        GST_INFO("[webrtc] %s (%p %s) receive video: H264.", webrtc->name().c_str(), webrtc, webrtc->role_.c_str());
        out = gst_parse_bin_from_description(
            "rtph264depay ! tee name=local_tee allow-not-linked=true",
            TRUE,
            NULL);

        g_warn_if_fail(gst_bin_add(GST_BIN(out), webrtc->video_input_joint_.upstream_joint));
        GstElement *local_tee = gst_bin_get_by_name_recurse_up(GST_BIN(out), "local_tee");
        g_warn_if_fail(gst_element_link(local_tee, webrtc->video_input_joint_.upstream_joint));
        gst_object_unref(local_tee);
        // webrtc->app()->add_pipe_joint(NULL, webrtc->video_input_joint_.downstream_joint);

    } else if (g_strcmp0(encoding_name, "PCMA") == 0) {
        GST_INFO("[webrtc] %s (%p %s) receive audio: PCMA.", webrtc->name().c_str(), webrtc, webrtc->role_.c_str());
        out = gst_parse_bin_from_description(
            // "rtppcmadepay ! tee name=local_audio_tee allow-not-linked=true ! queue ! alawdec ! audioconvert ! spectrascope shader=3 ! ximagesink sync=false",
            "rtppcmadepay ! tee name=local_audio_tee allow-not-linked=true",
            TRUE,
            NULL);

        g_warn_if_fail(gst_bin_add(GST_BIN(out), webrtc->audio_input_joint_.upstream_joint));
        GstElement *local_tee = gst_bin_get_by_name_recurse_up(GST_BIN(out), "local_audio_tee");
        g_warn_if_fail(gst_element_link(local_tee, webrtc->audio_input_joint_.upstream_joint));

        // GstPad *pad = gst_element_get_static_pad(webrtc->audio_input_joint_.upstream_joint, "sink");
        // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, NULL, NULL);
        // gst_object_unref(pad);

        gst_object_unref(local_tee);
        // webrtc->app()->add_pipe_joint(NULL, webrtc->audio_input_joint_.downstream_joint);
    } else if (g_strcmp0(encoding_name, "OPUS") == 0) {
        GST_INFO("[webrtc] %s (%p %s) receive video: OPUS.", webrtc->name().c_str(), webrtc, webrtc->role_.c_str());
        out = gst_parse_bin_from_description(
            // "rtppcmadepay ! tee name=local_audio_tee allow-not-linked=true ! queue ! alawdec ! audioconvert ! spectrascope shader=3 ! ximagesink sync=false",
            "rtpopusdepay ! tee name=local_audio_tee allow-not-linked=true",
            TRUE,
            NULL);

        g_warn_if_fail(gst_bin_add(GST_BIN(out), webrtc->audio_input_joint_.upstream_joint));
        GstElement *local_tee = gst_bin_get_by_name_recurse_up(GST_BIN(out), "local_audio_tee");
        // g_warn_if_fail(gst_element_link(local_tee, webrtc->audio_input_joint_.upstream_joint));
    } else {
        g_critical("Unknown encoding name %s", encoding_name);
        g_assert_not_reached();
        gst_caps_unref(caps);
        return;
    }
    gst_bin_add(GST_BIN(webrtc->bin_), out);
    gst_element_sync_state_with_parent(out);
    sink = (GstPad *)out->sinkpads->data;

    gst_pad_link(new_pad, sink);

    gst_caps_unref(caps);
}

bool WebRTC::initialize(Promise *promise)
{
    GST_DEBUG_CATEGORY_INIT(my_category, "webstreamer", 2, "libWebStreamer");
    IEndpoint::protocol() = "webrtc";
    const json &j = promise->data();
    if (j.find("role") != j.end()) {
        role_ = j["role"];
    }
    if (role_ != "offer" && role_ != "answer") {
        GST_ERROR("[webrtc] %p initialize failed, invalid role: %s.", webrtc_, role_.c_str());
        return false;
    }

    bool launch_set = true;
    if (launch_.empty()) {
        launch_set = false;
        launch_ = "webrtcbin name=webrtc ";
        if (!app()->video_encoding().empty()) {
            std::string video_enc = app()->video_encoding();
            // launch += "rtspsrc location=rtsp://172.16.66.65/id=1 ! rtph264depay ! queue ! ";
            launch_ += "rtp" + video_enc + "pay name=pay0 ! queue ! " +
                       "application/x-rtp,media=video,encoding-name=" + uppercase(video_enc) +
                       ",payload=96 ! webrtc. ";
        }
        if (!app()->audio_encoding().empty()) {
            std::string audio_enc = app()->audio_encoding();
            launch_ += "rtp" + audio_enc + "pay name=pay1 ! queue ! " +
                       "application/x-rtp,media=audio,encoding-name=" + uppercase(audio_enc);
            if (uppercase(audio_enc) == "PCMA") {
                launch_ += ",payload=8 ! webrtc. ";
            } else {
                launch_ += ",payload=97 ! webrtc. ";
            }
        }
    }
    // printf("===============>  %s\n", launch_.c_str());
    GError *error = NULL;
    bin_ = gst_parse_launch(launch_.c_str(), &error);

    if (error) {
        GST_ERROR("[webrtc] Failed to parse launch: %s", error->message);
        g_error_free(error);
        g_clear_object(&bin_);
        bin_ = NULL;
        return false;
    }
    pipeline_ = gst_pipeline_new(NULL);
    gst_bin_add(GST_BIN(pipeline_), bin_);
    g_assert_nonnull(pipeline_);
    webrtc_ = gst_bin_get_by_name(GST_BIN(pipeline_), "webrtc");

    // gboolean sync = TRUE;
    // g_object_get(G_OBJECT(webrtc_), "sink-false", &sync, NULL);
    // if (!sync) {
    //     GST_FIXME("[webrtc] %p (%s) uses the fixed plugin.", webrtc_, role_.c_str());
    // } else {
    //     GST_FIXME("[webrtc] %p (%s) uses the origin plugin.", webrtc_, role_.c_str());
    // }
    // g_object_set(G_OBJECT(webrtc_), "sink-false", TRUE, NULL);

    // specific parameter
    if (app()->video_encoding() == "h264") {
        GstElement *payloader = gst_bin_get_by_name(GST_BIN(pipeline_), "pay0");
        g_object_set(G_OBJECT(payloader), "config-interval", -1, NULL);
        gst_object_unref(payloader);
    }


    static int session_count = 0;
    if (!app()->video_encoding().empty()) {
        GST_DEBUG("[webrtc] %s (%p %s) media constructed: video", name().c_str(), webrtc_, role_.c_str());

        static std::string video_media_type = "video";
        std::string video_output_pipejoint_name = std::string("webrtc_video_output_joint_") +
                                                  name() +
                                                  std::to_string(session_count);
        video_output_joint_ = make_pipe_joint(video_media_type, video_output_pipejoint_name);
        g_warn_if_fail(gst_bin_add(GST_BIN(pipeline_), video_output_joint_.downstream_joint));
        GstElement *video_pay = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline_), "pay0");
        g_warn_if_fail(gst_element_link(video_output_joint_.downstream_joint, video_pay));

        // GstPad *testpad = gst_element_get_static_pad(video_output_joint_.upstream_joint, "sink");
        // gst_pad_add_probe(testpad, GST_PAD_PROBE_TYPE_BUFFER, on_monitor_data, NULL, NULL);
        // gst_object_unref(testpad);

        std::string video_input_pipejoint_name = std::string("webrtc_video_input_joint_") +
                                                 name() +
                                                 std::to_string(session_count);
        video_input_joint_ = make_pipe_joint(video_media_type, video_input_pipejoint_name);
        app()->add_pipe_joint(video_output_joint_.upstream_joint,
                              video_input_joint_.downstream_joint);
    }
    if (!app()->audio_encoding().empty()) {
        GST_DEBUG("[webrtc] %s (%p %s) media constructed: audio", name().c_str(), webrtc_, role_.c_str());

        static std::string audio_media_type = "audio";
        std::string audio_output_pipejoint_name = std::string("webrtc_audio_endpoint_joint_") +
                                                  name() +
                                                  std::to_string(session_count);
        audio_output_joint_ = make_pipe_joint(audio_media_type, audio_output_pipejoint_name);
        g_warn_if_fail(gst_bin_add(GST_BIN(pipeline_), audio_output_joint_.downstream_joint));
        GstElement *audio_pay = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline_), "pay1");
        g_warn_if_fail(gst_element_link(audio_output_joint_.downstream_joint, audio_pay));

        std::string audio_input_pipejoint_name = std::string("webrtc_audio_input_joint_") +
                                                 name() +
                                                 std::to_string(session_count);
        audio_input_joint_ = make_pipe_joint(audio_media_type, audio_input_pipejoint_name);
        app()->add_pipe_joint(audio_output_joint_.upstream_joint,
                              audio_input_joint_.downstream_joint);
    }
    session_count++;


    g_signal_connect(webrtc_, "on-ice-candidate", G_CALLBACK(WebRTC::on_ice_candidate), this);
    // if (role_=="offer") {
    //     g_signal_connect(webrtc_, "pad-added", G_CALLBACK(WebRTC::on_webrtc_pad_added), this);
    // }
    if (role_ == "offer") {
        g_signal_connect(webrtc_, "on-negotiation-needed", G_CALLBACK(WebRTC::on_negotiation_needed), this);
    }

    if (!launch_set) {
        g_signal_connect(webrtc_, "pad-added", G_CALLBACK(WebRTC::on_webrtc_pad_added), this);
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        GST_DEBUG("[webrtc] %s (%p %s) initialize failed.", name().c_str(), webrtc_, role_.c_str());
    }
    GST_DEBUG("[webrtc] %s (%p %s) initialize done.", name().c_str(), webrtc_, role_.c_str());

    return true;
}

void WebRTC::terminate()
{
    // dynamicly unlink
    if (!app()->video_encoding().empty() &&
        video_output_joint_.upstream_joint != NULL) {
        app()->remove_pipe_joint(video_output_joint_.upstream_joint, video_input_joint_.downstream_joint);
    }
    if (!app()->audio_encoding().empty() &&
        audio_output_joint_.upstream_joint != NULL) {
        app()->remove_pipe_joint(audio_output_joint_.upstream_joint, audio_input_joint_.downstream_joint);
    }
    if (pipeline_) {
        gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_NULL);
        gst_bin_remove(GST_BIN(pipeline_), bin_);
        gst_object_unref(pipeline_);
    }
    GST_DEBUG("[webrtc] %s %p (%s) terminate done.", name().c_str(), webrtc_, role_.c_str());
    if (webrtc_) {
        gst_object_unref(webrtc_);
        webrtc_ = NULL;
    }
}

void WebRTC::set_remote_description(Promise *promise)
{
    const json &j = promise->data();
    std::string sdp_info = j["sdp"];
    std::string type = j["type"];
    // printf("\n%s\n", sdp_info.c_str());

    GstWebRTCSessionDescription *sdp;
    GstSDPMessage *sdp_msg;
    gst_sdp_message_new(&sdp_msg);
    gst_sdp_message_parse_buffer((guint8 *)sdp_info.c_str(), (guint)sdp_info.size(), sdp_msg);
    sdp = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp_msg);
    if (type == "offer")
        sdp->type = GST_WEBRTC_SDP_TYPE_OFFER;
    else
        sdp->type = GST_WEBRTC_SDP_TYPE_ANSWER;
    g_signal_emit_by_name(webrtc_, "set-remote-description", sdp, NULL);
    GST_INFO("[webrtc] %s (%p %s) set remote description.", name().c_str(), webrtc_, role_.c_str());

    if (role_ == "answer") {
        GstPromise *promise = gst_promise_new_with_change_func(WebRTC::on_sdp_created, this, NULL);
        g_signal_emit_by_name(webrtc_, "create-answer", NULL, promise);
    }
}
void WebRTC::set_remote_candidate(Promise *promise)
{
    const json &j = promise->data();
    std::string candidate = j["candidate"];
    int sdpmlineindex = j["sdpMLineIndex"];
    g_signal_emit_by_name(webrtc_, "add-ice-candidate", sdpmlineindex, candidate.c_str());
    GST_LOG("[webrtc] %p (%s) set remote candidate.", webrtc_, role_.c_str());
}


GstPadProbeReturn WebRTC::on_monitor_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
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