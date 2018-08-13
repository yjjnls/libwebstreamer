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


#ifndef _LIBWEBSTREAMER_PROCESSOR_H_
#define _LIBWEBSTREAMER_PROCESSOR_H_


#include "endpoint.h"
class WebStreamer;
class IApp
{
 public:
    IApp(const std::string &name, WebStreamer *ws)
        : name_(name)
        , pipeline_(NULL)
        , webstreamer_(ws)

    {
    }
    virtual ~IApp() {}

    virtual bool Initialize(Promise *promise);
    /*
     * return false if the destroy not complete
    */
    virtual bool Destroy(Promise *promise);

    virtual void On(Promise *promise) = 0;

    virtual const char *type() const = 0;
    virtual std::string uname() = 0;

    virtual void Notify(const nlohmann::json &data, const nlohmann::json &meta);
    WebStreamer &webstreamer() { return *webstreamer_; }
    GstElement *pipeline() { return pipeline_; }
    std::string name() { return name_; }
    const std::string &video_encoding() const { return video_encoding_; }
    std::string &video_encoding() { return video_encoding_; }
    const std::string &audio_encoding() const { return audio_encoding_; }
    std::string &audio_encoding() { return audio_encoding_; }

    virtual void add_pipe_joint(GstElement *upstream_joint, GstElement *downstream_joint) {}
    virtual void remove_pipe_joint(GstElement *upstream_joint, GstElement *downstream_joint) {}

 protected:
    std::string name_;
    GstElement *pipeline_;
    WebStreamer *webstreamer_;

    std::string video_encoding_;
    std::string audio_encoding_;
};

#define APP(klass)                               \
    static const char *CLASS_NAME()              \
    {                                            \
        return #klass;                           \
    };                                           \
                                                 \
    const char *type() const                     \
    {                                            \
        return CLASS_NAME();                     \
    }                                            \
                                                 \
    std::string uname()                          \
    {                                            \
        return this->name_ + "@" + this->type(); \
    }


#endif  // _LIBWEBSTREAMER_PROCESSOR_H_
