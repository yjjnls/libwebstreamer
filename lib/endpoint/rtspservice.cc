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

#include <webstreamer.h>
#include "rtspservice.h"
#include <gst/rtsp-server/rtsp-onvif-server.h>

GST_DEBUG_CATEGORY_STATIC(my_category);
#define GST_CAT_DEFAULT my_category

IRTSPService::IRTSPService(IApp *app,
                           const std::string &name,
                           RTSPServer::Type type)
    : IEndpoint(app, name)
    , factory_(NULL)
{
    GST_DEBUG_CATEGORY_INIT(my_category, "webstreamer", 2, "libWebStreamer");
    server_ = app->webstreamer().GetRTSPServer(type);
}

IRTSPService::~IRTSPService()
{
}

std::mutex IRTSPService::client_mutex_;

// static void Notify(gpointer data, GObject *where_the_object_was)
// {
//     g_print(" data : %x\n", data);
//     g_print(" where_the_object_was : %x\n", where_the_object_was);
// }

void IRTSPService::on_new_session(GstRTSPClient *client,
                                  GstRTSPSession *session,
                                  gpointer user_data)
{
    IRTSPService *rtsp_service = static_cast<IRTSPService *>(user_data);
    char *str;
    g_object_get(G_OBJECT(client), "path", &str, NULL);
    std::string path(str);
    if (path != rtsp_service->path_)
        return;
    // std::string sessid(gst_rtsp_session_get_sessionid(session));
    // printf("rtsp-server sessionid:%s\n", sessid.c_str());

    client_mutex_.lock();
    rtsp_service->clients_[session] = client;
    client_mutex_.unlock();
    g_signal_connect(client, "closed", (GCallback)(rtsp_service->onclosed), user_data);
    GST_DEBUG("[rtsp-server] (path: %s) client: %p connected.", rtsp_service->path_.c_str(), client);
}
void IRTSPService::onclosed(GstRTSPClient *client, gpointer user_data)
{
    IRTSPService *rtsp_service = static_cast<IRTSPService *>(user_data);
    client_mutex_.lock();
    auto it = std::find_if(rtsp_service->clients_.begin(),
                           rtsp_service->clients_.end(),
                           [client](std::pair<GstRTSPSession *, GstRTSPClient *> curr_client) {
                               return (curr_client.second == client);
                           });
    if (it != rtsp_service->clients_.end()) {
        rtsp_service->clients_.erase(it);
        GST_DEBUG("[rtsp-server] (path: %s) client: %p closed and removed.", rtsp_service->path_.c_str(), client);
    }
    client_mutex_.unlock();
}

void IRTSPService::on_client_connected(GstRTSPServer *gstrtspserver,
                                       GstRTSPClient *client,
                                       gpointer user_data)
{
    IRTSPService *rtsp_service = static_cast<IRTSPService *>(user_data);
    g_signal_connect(client, "new-session", (GCallback)(rtsp_service->on_new_session), user_data);
}
bool IRTSPService::Launch(const std::string &path,
                          const std::string &launch,
                          GCallback media_constructed,
                          GCallback media_configure)
{
    GstRTSPServer *server = server_->server();
    GstRTSPMountPoints *mount_points =
        gst_rtsp_server_get_mount_points(server);

    factory_ = gst_rtsp_media_factory_new();
    // if you want multiple clients to see the same video,
    // set the shared property to TRUE
    gst_rtsp_media_factory_set_shared(factory_, TRUE);

    gst_rtsp_media_factory_set_launch(factory_, launch.c_str());
    if (media_constructed) {
        g_signal_connect(factory_, "media-constructed", (GCallback)media_constructed, (gpointer)(this));
    }

    if (media_configure) {
        g_signal_connect(factory_, "media-configure", (GCallback)media_configure, (gpointer)(this));
    }


    gst_rtsp_mount_points_add_factory(mount_points, path.c_str(), factory_);
    g_object_unref(mount_points);

    GST_DEBUG("[rtsp-server] %s launched to %s", name().c_str(), path.c_str());
    path_ = path;
    // g_object_weak_ref(G_OBJECT(factory_), Notify, factory_);

    g_signal_connect(server, "client-connected", (GCallback)on_client_connected, (gpointer)(this));

    GST_DEBUG("[rtsp-server] (path: %s) initialize done.", path_.c_str());

    return true;
}


bool IRTSPService::Stop()
{
    client_mutex_.lock();
    std::map<GstRTSPSession *, GstRTSPClient *> client_info;
    client_info.swap(clients_);
    client_mutex_.unlock();

    if (!client_info.empty()) {
        for (auto client : client_info) {
            // There's no bug here now, it will invoke 'gst_rtsp_client_finalize' firstly
            // and do 'g_main_context_unref (priv->watch_context)'; while 'gst_rtsp_client_close'
            // will unref the context again. The correct invoke order should be that invoke
            // 'gst_rtsp_client_finalize'(automaticlly) after we invoke `gst_rtsp_client_close`
            // seems like the bug below
            // https://bugzilla.gnome.org/show_bug.cgi?id=790909
            GST_DEBUG("[rtsp-server] (path: %s) start close client: %p actively.",
                      path_.c_str(),
                      client.second);
            g_object_set(G_OBJECT(client.second), "path", (gchar *)path_.c_str(), "close", client.first, NULL);

            GST_INFO("[rtsp-server] (path: %s) close client: %p actively.",
                     path_.c_str(),
                     client.second);
        }
    }
    if (factory_) {
        GstRTSPServer *server = server_->server();
        GstRTSPMountPoints *mount_points =
            gst_rtsp_server_get_mount_points(server);
        gst_rtsp_mount_points_remove_factory(mount_points, path_.c_str());
        g_object_unref(mount_points);

        factory_ = NULL;
        GST_DEBUG("[rtsp-server] (path: %s) terminate done.", path_.c_str());
    }
    return true;
}
bool IRTSPService::initialize(Promise *promise)
{
    if (launch_.empty()) {
        return false;
    }
    IEndpoint::protocol() = "rtspserver";
    return Launch(path_,
                  launch_,
                  (GCallback)IRTSPService::on_rtsp_media_constructed,
                  NULL);
}

void IRTSPService::terminate()
{
    // dynamicly unlink
    if (!app()->video_encoding().empty() &&
        video_joint_.upstream_joint != NULL) {
        app()->remove_pipe_joint(video_joint_.upstream_joint, video_joint_.downstream_joint);
    }
    if (!app()->audio_encoding().empty() &&
        audio_joint_.upstream_joint != NULL) {
        app()->remove_pipe_joint(audio_joint_.upstream_joint, audio_joint_.downstream_joint);
    }
    // stop itself
    Stop();
    IEndpoint::terminate();
}


/////////////////////////////////////////////////////////////
GstPadProbeReturn cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    printf("-");
    return GST_PAD_PROBE_OK;
}
void IRTSPService::on_rtsp_media_constructed(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data)
{
    auto rtspserver = static_cast<IRTSPService *>(user_data);
    GST_DEBUG("[rtsp-server] (path: %s) on_rtsp_media_constructed", rtspserver->path_.c_str());
    GstElement *rtsp_server_media_bin = gst_rtsp_media_get_element(media);

    GstRTSPStream *gstrtspstream = gst_rtsp_media_get_stream(media, 0);

    gboolean sync = TRUE;
    g_object_get(G_OBJECT(gstrtspstream), "sink-false", &sync, NULL);
    if (!sync) {
        GST_FIXME("[rtsp-server] (path: %s) uses the fixed plugin.", rtspserver->path_.c_str());
    } else {
        GST_FIXME("[rtsp-server] (path: %s) uses the origin plugin.", rtspserver->path_.c_str());
    }
    g_object_set(G_OBJECT(gstrtspstream), "sink-false", TRUE, NULL);

    static int session_count = 0;
    if (!rtspserver->app()->video_encoding().empty()) {
        GST_DEBUG("[rtsp-server] (path: %s) media constructed: video", rtspserver->path_.c_str());

        static std::string media_type = "video";
        std::string pipejoint_name = std::string("rtspserver_video_endpoint_joint_") +
                                     rtspserver->name() +
                                     std::to_string(session_count);
        rtspserver->video_joint_ = make_pipe_joint(media_type, pipejoint_name);

        rtspserver->app()->add_pipe_joint(rtspserver->video_joint_.upstream_joint, rtspserver->video_joint_.downstream_joint);

        g_warn_if_fail(gst_bin_add(GST_BIN(rtsp_server_media_bin), rtspserver->video_joint_.downstream_joint));

        GstElement *video_pay = gst_bin_get_by_name_recurse_up(GST_BIN(rtsp_server_media_bin), "pay0");
        g_warn_if_fail(gst_element_link(rtspserver->video_joint_.downstream_joint, video_pay));

        GstPad *pad = gst_element_get_static_pad(video_pay, "src");
        // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, cb_have_data, user_data, NULL);
        gst_object_unref(pad);
    }

    if (!rtspserver->app()->audio_encoding().empty()) {
        GST_DEBUG("[rtsp-server] (path: %s) media constructed: audio", rtspserver->path_.c_str());

        static std::string media_type = "audio";
        std::string pipejoint_name = std::string("rtspserver_audio_endpoint_joint_") +
                                     rtspserver->name() +
                                     std::to_string(session_count);
        rtspserver->audio_joint_ = make_pipe_joint(media_type, pipejoint_name);

        rtspserver->app()->add_pipe_joint(rtspserver->audio_joint_.upstream_joint, rtspserver->audio_joint_.downstream_joint);

        g_warn_if_fail(gst_bin_add(GST_BIN(rtsp_server_media_bin), rtspserver->audio_joint_.downstream_joint));

        GstElement *audio_pay = gst_bin_get_by_name_recurse_up(GST_BIN(rtsp_server_media_bin), "pay1");
        g_warn_if_fail(gst_element_link(rtspserver->audio_joint_.downstream_joint, audio_pay));

        // GstPad *pad = gst_element_get_static_pad(audio_pay, "src");
        // gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, rtspserver->cb_have_data, user_data, NULL);
        // gst_object_unref(pad);
    }
    session_count++;
}
