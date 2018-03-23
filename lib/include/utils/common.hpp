#ifndef LIBWEBSTREAMER_UTILS_COMMON_HPP
#define LIBWEBSTREAMER_UTILS_COMMON_HPP

#include <vector>
#include <list>
#include <map>
#include <string>
#include <algorithm>
#include <functional>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC(my_category);
#define GST_CAT_DEFAULT my_category

#define USE_OWN_SAME_DEBUG() GST_DEBUG_CATEGORY_INIT(my_category, "libwebstreamer", 2, "libWebStreamer dll");

#endif