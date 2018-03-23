#ifndef LIBWEBSTREAMER_FRAMEWORK_ENDPOINT_HPP
#define LIBWEBSTREAMER_FRAMEWORK_ENDPOINT_HPP


#include <utils/common.hpp>
#include <fbs/webstreamer_generated.h>
#include <utils/status_code.hpp>
#include <utils/type_def.hpp>

namespace libwebstreamer
{
    namespace framework
    {
        class Pipeline;
        class Endpoint
        {
        public:
            // Endpoint(const message::common::Endpoint &params);
            Endpoint(const std::string &id, const std::string &type, const std::shared_ptr<Pipeline> pipeline_owner);
            virtual ~Endpoint()
            {
            }

            const std::string &id() const
            {
                return id_;
            }
            const std::string &type() const
            {
                return type_;
            }

            virtual bool add_to_pipeline() = 0;
            virtual bool remove_from_pipeline() = 0;

            const std::weak_ptr<Pipeline> pipeline_owner() const
            {
                return pipeline_owner_;
            }

        private:
            std::string id_;
            std::string type_;
            std::weak_ptr<Pipeline> pipeline_owner_;//owner pipeline
        };

        std::shared_ptr<Endpoint> MakeEndpoint(const std::string &type, const std::string &id,
                                               const std::string &url, const std::shared_ptr<Pipeline> pipeline_owner);
    }
}


#endif