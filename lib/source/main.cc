#include <stdio.h>
#include <flatbuffers/flatbuffers.h>
#include <owr/owr.h>
#include <json-glib/json-glib.h>

#include "node_plugin_interface.h"
#include <dispatcher.hpp>



#define __VERSION__ 0.1.0

static GMainContext *libwebstreamer_main_context = NULL;

static void init(const void *self, const void *data, size_t size, void (*cb)(const void *self, int status, char *msg))
{
    //TODO:
    //data is JSON string (utf8)
    //do as your needs
    if (data)
    {
        //do parse
        std::string param((const char *)data, size);
        printf("init param <%s>\n", param.c_str());
        //parse webstreamer parameter
        //todo...
    }
    printf("gstreamer init!\n");
    libwebstreamer_main_context = g_main_context_ref_thread_default();
    owr_init(libwebstreamer_main_context);
    owr_run_in_background();
    printf("gstreamer init done.\n");



    if (cb)
    {
        cb(self, 0, ">>>>>Initialize done!<<<<<");
        //error callback
        // cb(self, 1 ,"Initalize error!");
    }
}

static void call(const void *self, const void *context,
                 const void *data, size_t size)
{
    libwebstreamer::call(self, context, data, size);
    //static int counter = 0;
    //node_plugin_interface_t *iface = (node_plugin_interface_t *)self;
    //if (iface->call_return)
    //{
    //	int status = 0;
    //	char retval[256];
    //
    //	try
    //	{
    //		std::string expr((const char *)data, size);
    //		int result = calculator::eval(expr);
    //		sprintf(retval, "%d", result);
    //	}
    //	catch (calculator::error &e)
    //	{
    //		strcpy(retval, e.what());
    //		status = 1;
    //	}
    //
    //	iface->call_return(self, context, retval, strlen(retval) + 1, status, NULL, NULL);
    //}
    //
    //counter++;
    //if (iface->notify)
    //{
    //	char log[256];
    //	sprintf(log, "* %d request has been procced.", counter);
    //	iface->notify(self, log, strlen(log) + 1, NULL, NULL);
    //}
}

static void terminate(const void *self, void (*cb)(const void *self, int status, char *msg))
{
    owr_quit();
    printf("gstreamer quite.\n");
    if (cb)
    {
        cb(self, 0, ">>>>>Terminate done!<<<<<");
        //error callback
        // cb(self, 1 ,"Terminate error!");
    }
}

NODE_PLUGIN_IMPL(__VERSION__, init, call, terminate)


/////////////////////////////////////////////////////////////////////////////////////////////
void dispatch(GSourceFunc callback, gpointer user_data)
{
    GSource *source = g_idle_source_new();

    g_source_set_callback(source, callback, user_data, NULL);
    g_source_set_priority(source, G_PRIORITY_DEFAULT);
    g_source_attach(source, libwebstreamer_main_context);
}