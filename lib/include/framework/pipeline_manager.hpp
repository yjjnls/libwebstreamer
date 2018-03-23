#ifndef LIBWEBSTREAMER_FRAMEWORK_PIPELINE_MANAGER_HPP
#define LIBWEBSTREAMER_FRAMEWORK_PIPELINE_MANAGER_HPP

// #include <message/livestream.hpp>
#include <framework/pipeline.hpp>
/**
* ,-----------------, 1               * ,---------------, 1    shared_ptr   * ,---------------,
* | PipelineManager |------------------>|   Pipeline    |-------------------->|    Endpoint   |
* '-----------------'   shared_ptr      '---------------'                     '---------------'
*                                                   ^._1_ _ _ _ _ _ _ _ _ _ _ _1_/    1 .^
*                                                            weak_ptr                  /
*                                               ,-----------------,                   /           
*                                               |   WebrtcManager |_1_ _ _ _ _ _ _ _ /
*                                               '-----------------'     weak_ptr
*/

namespace libwebstreamer
{
    typedef std::function<void(int status, void *data, size_t size)> callback_fn;
    namespace framework
    {
        class PipelineManager
        {
        public:
            PipelineManager();
            void call(const void *data, size_t size, const callback_fn &cb);

        private://LiveStream
            void create_livestream(const webstreamer::livestreamer::Create &message, const callback_fn &cb);
            void destroy_livestream(const webstreamer::livestreamer::Destroy &message, const callback_fn &cb);
            void add_endpoint_in_livestream(const webstreamer::livestreamer::AddViewer &message, const callback_fn &cb);
            void remove_endpoint_in_livestream(const webstreamer::livestreamer::RemoveViewer &message, const callback_fn &cb);

        private:
            bool is_livestream_created(const std::string &id);

        private:
            std::vector<std::shared_ptr<Pipeline>> pipelines_;
        };
    }
}

#endif