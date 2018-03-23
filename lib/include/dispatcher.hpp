#ifndef LIBWEBSTREAMER_DISPATCHER_HPP
#define LIBWEBSTREAMER_DISPATCHER_HPP

#include <utils/common.hpp>

namespace libwebstreamer
{
    void initialize();
    void terminate();
    void call(const void *data, size_t size,
              std::function<void(int status, void *data, size_t size)> cb);
}

#endif