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

#ifndef LIB_WEBSTREAMER_H_
#define LIB_WEBSTREAMER_H_

#include <stdio.h>

#include <app/rtsptestserver.h>
#include <app/elementwatcher.h>
#include <app/rtsptestclient.h>
#include <app/livestream.h>
#include <app/multipoints.h>
#include <app/webrtctestclient.h>
#include <framework/rtspserver.h>



#define const_error_msg(type, msg) \
    "{\"type\": \"" type "\", \"message\": \"" msg "\"}"



// forward
template <typename... Args>
struct AppFactory;

// basic
template <typename First, typename... Rest>
struct AppFactory<First, Rest...>
{
    static IApp *Instantiate(const std::string &type,
                             const std::string &name,
                             WebStreamer *ws)
    {
        if (type == First::CLASS_NAME()) {
            return new First(name, ws);
        } else {
            return AppFactory<Rest...>::Instantiate(type, name, ws);
        }
    }
};

// end
template <typename Last>
struct AppFactory<Last>
{
    static IApp *Instantiate(const std::string &type,
                             const std::string &name,
                             WebStreamer *ws)
    {
        if (type == Last::CLASS_NAME()) {
            return new Last(name, ws);
        }
        return NULL;
    }
};





class RTSPService;

struct plugin_interface_t;
struct plugin_buffer_t;

#define WEBSTREAMER_STATE_IDLE 0
#define WEBSTREAMER_STATE_INITIALIZING (1 << 2)
#define WEBSTREAMER_STATE_RUNING (1 << 3)
#define WEBSTREAMER_STATE_TERMINATING (1 << 4)

class WebStreamer
{
 public:
    enum class State
    {
        IDLE,
        INITIALIZING,
        RUNNING,
        TERMINATING,
    };

    explicit WebStreamer(plugin_interface_t *iface);

    void Initialize(Promise *promise);

    void Terminate(Promise *promise);

    void Call(Promise *promise);

    void Notify(plugin_buffer_t *data, plugin_buffer_t *meta);


    static GMainLoop *main_loop;
    static GMainContext *main_context;
    static gpointer MainloopEntry(Promise *);


    State state() { return state_; }

    std::string InitRTSPServer(const nlohmann::json *option);
    void DestroyRTSPServer();
    RTSPServer *GetRTSPServer(RTSPServer::Type type = RTSPServer::RFC7826)
    {
        if (type >= RTSPServer::SIZE) {
            return NULL;
        }
        return rtspserver_[type];
    }

 protected:
    typedef AppFactory<RTSPTestServer,
                       ElementWatcher,
                       LiveStream,
                       MultiPoints,
                       WebRTCTestClient>
        Factory;


    void MainLoop(Promise *promise);
    bool Prepare(Promise *promise);
    bool Cleanup();

    void CreateApp(Promise *promise);
    void DestroyApp(Promise *promise);

    void OnPromise(Promise *promise);
    static gboolean OnPromise(gpointer user_data);

    inline IApp *GetApp(const std::string &name, const std::string &type)
    {
        return GetApp(name + "@" + type);
    }

    inline IApp *GetApp(const std::string &uname)
    {
        std::map<std::string, IApp *>::iterator it = apps_.find(uname);
        return (it == apps_.end()) ? NULL : it->second;
    }

 private:
    RTSPServer *rtspserver_[RTSPServer::SIZE];
    GstRTSPSessionPool *rtsp_session_pool_;
    guint pool_clean_id_;
    std::map<std::string, IApp *> apps_;
    plugin_interface_t *iface_;
    State state_;
    Promise *terminate_promise_;
};


#endif  // LIB_WEBSTREAMER_H_
