// #include <owr/owr.h>
// #include <json-glib/json-glib.h>
#include <memory>
#include <glib.h>
#include <dispatcher.hpp>
#include <framework/pipeline_manager.hpp>
#include <application/endpoint/rtspserver.hpp>

namespace libwebstreamer
{

    static GMainContext *libwebstreamer_main_context = NULL;

    static std::unique_ptr<GAsyncQueue, decltype(&g_async_queue_unref)> call_queue(g_async_queue_new(), &g_async_queue_unref);
    static std::unique_ptr<framework::PipelineManager> pipeline_manager(new framework::PipelineManager());
    static gboolean on_pipeline_manager_dispatch(gpointer pipeline_manager);

    static guint rtsp_server_source_id = 0;
    static GMainLoop *gst_main_loop = NULL;

    static gboolean gst_running_callback(GAsyncQueue *msg_queue)
    {
        g_return_val_if_fail(msg_queue, FALSE);
        g_async_queue_push(msg_queue, "ready");
        return G_SOURCE_REMOVE;
    }
    void gst_run(void)
    {
        GMainLoop *main_loop;

        g_return_if_fail(libwebstreamer_main_context);
        g_return_if_fail(!gst_main_loop);

        main_loop = g_main_loop_new(libwebstreamer_main_context, FALSE);
        gst_main_loop = main_loop;
        g_main_loop_run(main_loop);
        g_main_loop_unref(main_loop);
    }
    static gpointer gst_run_thread_func(GAsyncQueue *msg_queue)
    {
        GSource *idle_source;

        g_return_val_if_fail(msg_queue, NULL);
        g_return_val_if_fail(libwebstreamer_main_context, NULL);

        idle_source = g_idle_source_new();
        g_source_set_callback(idle_source, (GSourceFunc)gst_running_callback, msg_queue, NULL);
        g_source_set_priority(idle_source, G_PRIORITY_DEFAULT);
        g_source_attach(idle_source, libwebstreamer_main_context);
        msg_queue = NULL;

        gst_run();
        return NULL;
    }

    void gst_run_in_background(void)
    {
        GThread *gst_main_thread;
        GAsyncQueue *msg_queue = g_async_queue_new();

        g_return_if_fail(libwebstreamer_main_context);
        g_return_if_fail(!gst_main_loop);

        gst_main_thread = g_thread_new("gst_main_loop", (GThreadFunc)gst_run_thread_func, msg_queue);
        g_thread_unref(gst_main_thread);
        g_async_queue_pop(msg_queue);
        g_async_queue_unref(msg_queue);
    }
    void gst_quit(void)
    {
        g_return_if_fail(gst_main_loop);
        g_main_loop_quit(gst_main_loop);
        gst_main_loop = NULL;
    }
    //-------------------------------------------------------------//
    void initialize()
    {
        libwebstreamer_main_context = g_main_context_ref_thread_default();
        // gst_init(libwebstreamer_main_context);
        gst_init(NULL, NULL);
        gst_run_in_background();

        GstRTSPServer *rtsp_server = libwebstreamer::application::endpoint::get_rtsp_server();
        rtsp_server_source_id = gst_rtsp_server_attach(rtsp_server, libwebstreamer_main_context);
    }

    void terminate()
    {
        GstRTSPServer *rtsp_server = libwebstreamer::application::endpoint::get_rtsp_server();
        g_warn_if_fail(g_source_remove(rtsp_server_source_id));
        g_object_unref(rtsp_server);

        gst_quit();
    }

    void dispatch(GSourceFunc callback, gpointer user_data)
    {
        GSource *source = g_idle_source_new();

        g_source_set_callback(source, callback, user_data, NULL);
        g_source_set_priority(source, G_PRIORITY_DEFAULT);
        g_source_attach(source, libwebstreamer_main_context);
    }

    struct call_t
    {
        call_t(const void *data, size_t size, callback_fn cb)
            : data_(data), size_(size), cb_(cb)
        {
        }

        // const void *self_;
        // const void *context_;
        const void *data_;
        size_t size_;
        callback_fn cb_;
    };

    void call(const void *data, size_t size, callback_fn cb)
    {
        call_t *call = new call_t(data, size, cb);

        g_async_queue_push(call_queue.get(), call);

        dispatch(on_pipeline_manager_dispatch, pipeline_manager.get());
    }

    gboolean on_pipeline_manager_dispatch(gpointer pipeline_manager)
    {
        call_t *call = (call_t *)g_async_queue_pop(call_queue.get());
        static_cast<framework::PipelineManager *>(pipeline_manager)->call(call->data_, call->size_, call->cb_);
        delete call;

        return G_SOURCE_REMOVE;
    }
}
