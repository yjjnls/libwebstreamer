#include <fbs/webstreamer_generated.h>
#include <framework/pipeline_manager.hpp>
#include <application/pipeline/livestream.hpp>

namespace libwebstreamer
{
    namespace framework
    {
        using namespace libwebstreamer::utils;
        using namespace libwebstreamer::application::pipeline;
        PipelineManager::PipelineManager()
        {
            USE_OWN_SAME_DEBUG();
        }
        void PipelineManager::call(const void *data, size_t size, const callback_fn &cb)
        {
            // printf("-----PipelineManager::call-----\n");
            ::flatbuffers::Verifier verifier((const uint8_t *)data, size);
            if (webstreamer::VerifyrootBuffer(verifier))
            {
                // printf("-----webstreamer::Getroot-----\n");
                auto livestream_any = webstreamer::Getroot((const uint8_t *)data);
                switch (livestream_any->any_type())
                {
                    case webstreamer::Any_webstreamer_livestreamer_Create:
                    {
                        // printf("-----create livestream-----\n");
                        auto create = livestream_any->any_as_webstreamer_livestreamer_Create();
                        create_livestream(*create, cb);
                        return;
                    }
                    break;
                    case webstreamer::Any_webstreamer_livestreamer_Destroy:
                    {
                        // printf("-----destroy livestream-----\n");
                        auto destroy = livestream_any->any_as_webstreamer_livestreamer_Destroy();
                        destroy_livestream(*destroy, cb);
                        return;
                    }
                    break;
                    case webstreamer::Any_webstreamer_livestreamer_AddViewer:
                    {
                        // printf("-----add viewer-----\n");
                        auto add_endpoint = livestream_any->any_as_webstreamer_livestreamer_AddViewer();
                        add_endpoint_in_livestream(*add_endpoint, cb);
                        return;
                    }
                    break;
                    case webstreamer::Any_webstreamer_livestreamer_RemoveViewer:
                    {
                        // printf("-----remove viewer-----\n");
                        auto remove_endpoint = livestream_any->any_as_webstreamer_livestreamer_RemoveViewer();
                        remove_endpoint_in_livestream(*remove_endpoint, cb);
                        return;
                    }
                    break;
                }
            }
            // printf("-----Invalid data -----\n");
            std::string reason("Invalid data from 'call'");
            cb(static_cast<int>(status_code::Bad_Request),
               static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
        }

        void PipelineManager::create_livestream(const webstreamer::livestreamer::Create &message, const callback_fn &cb)
        {
            std::string stream_id(message.name()->c_str());
            if (is_livestream_created(stream_id))
            {
                //already exists
                std::string reason("The stream: " + stream_id + " is already existed!");
                cb(static_cast<int>(status_code::Conflict),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }

            // create stream pipeline
            std::shared_ptr<LiveStream> livestream = std::make_shared<LiveStream>(stream_id);
            // set stream encoding
            const webstreamer::Endpoint *endpoint = message.source();
            for (auto it : *(endpoint->channel()))
            {
                std::string name(((webstreamer::Channel *)it)->name()->c_str());
                std::string codec(((webstreamer::Channel *)it)->codec()->c_str());
                if (name == "video")
                {
                    // printf("----set video----\n");
                    livestream->video_encoding() = codec;
                }
                else if (name == "audio")
                {
                    // printf("----set audio----\n");
                    livestream->audio_encoding() = codec;
                }
                else
                {
                    std::string reason("The codec: " + codec + " is invalid!");
                    cb(static_cast<int>(status_code::Bad_Request),
                       static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                    return;
                }
            }
            // // set pipeline sysclock
            // GstClock *sys_clock = gst_system_clock_obtain();
            // gst_pipeline_use_clock(GST_PIPELINE(livestream->pipeline()), sys_clock);
            // g_object_unref(sys_clock);

            // set rtsp source url
            std::string endpoint_url(endpoint->url()->c_str());

            // create source endpoint
            std::string endpoint_id(endpoint->name()->c_str());
            std::string endpoint_type(endpoint->protocol()->c_str());
            std::shared_ptr<Endpoint> ep = MakeEndpoint(endpoint_type, endpoint_id, endpoint_url, livestream);

            // add endpoint to pipeline
            if (!livestream->add_endpoint(ep))
            {
                std::string reason("Add endpoint failed!");
                cb(static_cast<int>(status_code::Bad_Request),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }

            // add pipeline to pipeline manager
            pipelines_.push_back(livestream);
            cb(0, NULL, 0);
            GST_INFO("create_live_stream: %s\n", stream_id.c_str());
        }
        void PipelineManager::destroy_livestream(const webstreamer::livestreamer::Destroy &message, const callback_fn &cb)
        {
            std::string stream_id(message.name()->c_str());
            auto it = std::find_if(pipelines_.begin(), pipelines_.end(), [&stream_id](std::shared_ptr<Pipeline> &pipeline) {
                return pipeline->id() == stream_id;
            });

            if (it == pipelines_.end())
            {
                std::string reason("LiveStream Not Found!");
                cb(static_cast<int>(status_code::Not_Found),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }
            //remove all endpoints in the pipeline, and then remove the pipeline
            g_assert(it->unique());
            (*it)->remove_all_endpoints();
            pipelines_.erase(it);

            cb(0, NULL, 0);

            GST_INFO("delete_live_stream: %s\n", stream_id.c_str());
        }
        void PipelineManager::add_endpoint_in_livestream(const webstreamer::livestreamer::AddViewer &message, const callback_fn &cb)
        {
            // get stream id and find pipeline
            std::string stream_id(message.component()->c_str());
            auto it = std::find_if(pipelines_.begin(), pipelines_.end(), [&stream_id](std::shared_ptr<Pipeline> &pipeline) {
                return pipeline->id() == stream_id;
            });

            if (it == pipelines_.end())
            {
                // pipeline exists
                std::string reason("The stream: " + stream_id + " is not existed!");
                cb(static_cast<int>(status_code::Not_Found),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }
            // set viewer url (for rtsp server use)
            const webstreamer::Endpoint *endpoint = message.viewer();
            std::string endpoint_url(endpoint->url()->c_str());

            // create viewer endpoint
            std::string endpoint_id(endpoint->name()->c_str());
            std::string endpoint_type(endpoint->protocol()->c_str());
            std::shared_ptr<Endpoint> ep = MakeEndpoint(endpoint_type, endpoint_id, endpoint_url, *it);

            // add to pipeline
            if (!(*it)->add_endpoint(ep))
            {
                std::string reason("Add endpoint failed!");
                cb(static_cast<int>(status_code::Bad_Request),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }
            cb(0, NULL, 0);
        }
        void PipelineManager::remove_endpoint_in_livestream(const webstreamer::livestreamer::RemoveViewer &message, const callback_fn &cb)
        {
            // get stream id and find pipeline
            std::string stream_id(message.component()->c_str());
            auto it = std::find_if(pipelines_.begin(), pipelines_.end(), [&stream_id](std::shared_ptr<Pipeline> &pipeline) {
                return pipeline->id() == stream_id;
            });

            if (it == pipelines_.end())
            {
                // pipeline not exists
                std::string reason("The stream: " + stream_id + " is not existed!");
                cb(static_cast<int>(status_code::Not_Found),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }
            // get endpoint id
            std::string endpoint_id(message.endpoint()->c_str());
            // remove endpoint
            if ((*it)->remove_endpoint(endpoint_id))
                cb(0, NULL, 0);
            else
            {
                // endpoint not exists
                std::string reason("The endpoint: " + endpoint_id + " is not existed!");
                cb(static_cast<int>(status_code::Not_Found),
                   static_cast<void *>(const_cast<char *>(reason.c_str())), reason.size());
                return;
            }
        }

        bool PipelineManager::is_livestream_created(const std::string &id)
        {
            auto it = std::find_if(pipelines_.begin(), pipelines_.end(), [&id](std::shared_ptr<Pipeline> &pipeline) {
                return pipeline->id() == id;
            });

            if (it != pipelines_.end())
            {
                return true;
            }

            return false;
        }
    }
}