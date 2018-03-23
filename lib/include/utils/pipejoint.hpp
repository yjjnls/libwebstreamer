#ifndef LIBWEBSTREAMER_UTILS_PIPEJOINT_HPP
#define LIBWEBSTREAMER_UTILS_PIPEJOINT_HPP

#include <gst/gst.h>
#include <string>

namespace libwebstreamer
{
    struct PipeJoint
    {
        GstElement *upstream_joint = NULL;
        GstElement *downstream_joint = NULL;
    };

    PipeJoint make_pipe_joint(const std::string &media_type = "video", const std::string &name = "");
    void destroy_pipe_joint(PipeJoint pipejoint);
}

#endif