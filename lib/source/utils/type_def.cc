#include <string>
#include <map>
#include <utils/type_def.hpp>

namespace libwebstreamer
{
    std::map<std::string, EndpointType> endpoint_type = {{"rtspclient", EndpointType::RTSP_CLIENT},
                                                         {"rtspserver", EndpointType::RTSP_SERVER},
                                                         {"testsink", EndpointType::TEST_SINK},
                                                         {"webrtc", EndpointType::WEBRTC}};
    EndpointType get_endpoint_type(const std::string &type)
    {
        return endpoint_type[type];
    }
    std::map<std::string, VideoEncodingType> video_encoding_type = {{"h264", VideoEncodingType::H264},
                                                                    {"vp8", VideoEncodingType::VP8},
                                                                    {"h265", VideoEncodingType::H265}};
    VideoEncodingType get_video_encoding_type(const std::string &type)
    {
        return video_encoding_type[type];
    }
    std::map<std::string, AudioEncodingType> audio_encoding_type = {{"pcma", AudioEncodingType::PCMA},
                                                                    {"pcmu", AudioEncodingType::PCMU},
                                                                    {"opus", AudioEncodingType::OPUS}};
    AudioEncodingType get_audio_encoding_type(const std::string &type)
    {
        return audio_encoding_type[type];
    }
}