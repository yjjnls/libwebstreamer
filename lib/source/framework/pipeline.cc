#include <framework/pipeline.hpp>

namespace libwebstreamer
{
    namespace framework
    {

        Pipeline::Pipeline(const std::string &id)
            : id_(id)
            , bus_(NULL)
            , bus_watcher_(0)
        {
            pipeline_ = gst_pipeline_new(id.c_str());
            g_warn_if_fail(pipeline_);

            bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
            bus_watcher_ = gst_bus_add_watch(bus_, message_handler, this);
            g_warn_if_fail(bus_);
            g_warn_if_fail(bus_watcher_);
        }

        gboolean Pipeline::message_handler(GstBus *bus, GstMessage *message, gpointer data)
        {
            Pipeline *This = static_cast<Pipeline *>(data);
            return This->MessageHandler(message);
        }

        Pipeline::~Pipeline()
        {
            if (bus_ != NULL)
            {
                g_source_remove(bus_watcher_);
                gst_object_unref(bus_);
            }

            gst_element_set_state(pipeline_, GST_STATE_NULL);
            g_assert(endpoints_.empty());

            // for (int i = 0; i < endpoints_.size(); i++)
            // {
            //     g_assert(endpoints_[i].unique());
            //     on_remove_endpoint(endpoints_[i]);
            // }
            // endpoints_.erase(endpoints_.begin(), endpoints_.end());

            gst_object_unref(pipeline_);
        }

        bool Pipeline::add_endpoint(const std::shared_ptr<Endpoint> endpoint)
        {
            auto it = std::find_if(endpoints_.begin(), endpoints_.end(), [&endpoint](std::shared_ptr<Endpoint> &ep) {
                return endpoint->id() == ep->id();
            });
            if (it != endpoints_.end())
            {
                return false;
            }
            if (!on_add_endpoint(endpoint))
            {
                return false;
            }
            endpoints_.push_back(endpoint);
            return true;
        }

        bool Pipeline::remove_endpoint(const std::string &endpoint_id)
        {
            auto it = std::find_if(endpoints_.begin(), endpoints_.end(), [&endpoint_id](std::shared_ptr<Endpoint> &ep) {
                return endpoint_id == ep->id();
            });
            if (it == endpoints_.end())
            {
                return false;
            }
            g_assert(it->unique());
            on_remove_endpoint(*it);
            endpoints_.erase(it);
            printf("-------endpoints left: %d----\n", endpoints_.size());
            return true;
        }
        bool Pipeline::remove_all_endpoints()
        {
            for (int i = 0; i < endpoints_.size(); i++)
            {
                g_assert(endpoints_[i].unique());
                on_remove_endpoint(endpoints_[i]);
            }
            endpoints_.erase(endpoints_.begin(), endpoints_.end());
            printf("-------endpoints left: %d----\n", endpoints_.size());
            return true;
        }
    }
}
