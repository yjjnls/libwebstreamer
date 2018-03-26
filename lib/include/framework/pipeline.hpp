#ifndef LIBWEBSTREAMER_FRAMEWORK_PIPELINE_HPP
#define LIBWEBSTREAMER_FRAMEWORK_PIPELINE_HPP

#include <framework/endpoint.hpp>
namespace libwebstreamer
{
    namespace framework
    {
        class Pipeline
        {
        public:
            Pipeline(const std::string &id);
            virtual ~Pipeline();

            bool add_endpoint(const std::shared_ptr<Endpoint> endpoint);
            bool remove_endpoint(const std::string &endpoint_id);
            bool remove_all_endpoints();
            virtual void add_pipe_joint(GstElement *upstream_joint) = 0;
            virtual void remove_pipe_joint(GstElement *upstream_joint) = 0;

            const std::string &id() const
            {
                return id_;
            };

            const std::string &video_encoding() const
            {
                return video_encoding_;
            }
            std::string &video_encoding()
            {
                return video_encoding_;
            }
            const std::string &audio_encoding() const
            {
                return audio_encoding_;
            }
            std::string &audio_encoding()
            {
                return audio_encoding_;
            }
            GstElement *pipeline()
            {
                return pipeline_;
            }


        protected:
            virtual bool on_add_endpoint(const std::shared_ptr<Endpoint> endpoint) = 0;
            virtual bool on_remove_endpoint(const std::shared_ptr<Endpoint> endpoint) = 0;
            virtual bool MessageHandler(GstMessage *message)
            {
                return true;
            }
            std::vector<std::shared_ptr<Endpoint>> &endpoints()
            {
                return endpoints_;
            }

        private:
            std::string id_;
            std::string video_encoding_;
            std::string audio_encoding_;
            std::vector<std::shared_ptr<Endpoint>> endpoints_;
            GstElement *pipeline_;

            //add for monitoring error messages from elements
            GstBus *bus_;
            guint bus_watcher_;
            static gboolean message_handler(GstBus *bus, GstMessage *message, gpointer data);
        };
    }
}

#endif