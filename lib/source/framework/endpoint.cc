#include <framework/endpoint.hpp>
#include <application/endpoint/rtspclient.hpp>
#include <application/endpoint/rtspserver.hpp>
#include <application/endpoint/testsink.hpp>
#include <application/pipeline/livestream.hpp>

namespace libwebstreamer
{
    namespace framework
    {
        Endpoint::Endpoint(const std::string &id, const std::string &type, const std::shared_ptr<Pipeline> pipeline_owner)
            : id_(id)
            , type_(type)
            , pipeline_owner_(pipeline_owner)
        {
        }

        // void Endpoint::add_to_pipeline()
        // {
        //     on_add_to_pipeline();
        // }

        // void Endpoint::remove_from_pipeline()
        // {
        //     on_remove_from_pipeline();
        // }



        std::shared_ptr<Endpoint> MakeEndpoint(const std::string &type, const std::string &id,
                                               const std::string &url, const std::shared_ptr<Pipeline> pipeline_owner)
        {
            switch (get_endpoint_type(type))
            {
                case EndpointType::RTSP_CLIENT:
                {
                    typedef libwebstreamer::application::endpoint::RtspClient RtspClient;
                    std::shared_ptr<RtspClient> ep = std::make_shared<RtspClient>(id, type, pipeline_owner);
                    ep->initialize(url);
                    return ep;
                }
                case EndpointType::RTSP_SERVER:
                {
                    typedef libwebstreamer::application::endpoint::RtspServer RtspServer;
                    std::shared_ptr<RtspServer> ep = std::make_shared<RtspServer>(id, type, pipeline_owner);
                    ep->initialize(url);
                    return ep;
                }
                case EndpointType::TEST_SINK:
                {
                    typedef libwebstreamer::application::endpoint::TestSink TestSink;
                    std::shared_ptr<TestSink> ep = std::make_shared<TestSink>(id, type, pipeline_owner);
                    return ep;
                }
                // case StringValue::WEBRTC:
                //     return std::make_shared<webstreamer::pipeline::endpoint::RtspServer>(
                //         *static_cast<webstreamer::transport::mediaservice::RtspServer *>(&endpoint));
                default:
                    return NULL;
            }
        }
    }
}