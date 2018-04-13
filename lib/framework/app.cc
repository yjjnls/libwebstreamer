#include "app.h"
#include "../webstreamer.h"

bool IApp::Initialize(Promise* promise)
{
	if (!pipeline_)
	{
		pipeline_ = gst_pipeline_new((const gchar*)uname().c_str());
	}

	if (pipeline_) {
		promise->resolve();
		return true;
	}
	else {
		promise->reject("create pipeline failed.");
		return false;
	}
}


bool IApp::Destroy(Promise* promise)
{
	if (pipeline_) {
		gst_element_set_state(pipeline_, GST_STATE_NULL);
		g_object_unref(pipeline_);
		pipeline_ = NULL;
	}
	webstreamer_ = NULL;
	return true; 

}


void IApp::Notify(const nlohmann::json& data, const nlohmann::json& meta)
{
	const std::string& jdata = data.dump();
	const std::string& jmeta = meta.dump();
	plugin_buffer_t d;
	plugin_buffer_t m;
	plugin_buffer_string_set(&d, jdata.c_str());
	plugin_buffer_string_set(&m, jmeta.c_str());
	this->webstreamer_->Notify(&d, &m);
}
