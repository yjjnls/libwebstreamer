#include "webstreamer.h"
#include "elementwatcher.h"
using json = nlohmann::json;






/* receive spectral data from element message */
static gboolean
message_handler(GstBus * bus, GstMessage * message, gpointer data)
{
	ElementWatcher* This = static_cast<ElementWatcher*>(data);
	This->OnMessage(bus, message);

	return TRUE;
}




bool ElementWatcher::Initialize(Promise* promise)
{
	
	// promise->resolve();
	return true;
}

void ElementWatcher::On(Promise* promise)
{
	const json& j = promise->meta();
	std::string action = j["action"];
	if (action == "startup")
	{
		Startup(promise);
	}
	else if (action == "stop")
	{
		Stop(promise);
	}
}

void ElementWatcher::Startup(Promise* promise)
{
	GError* error = NULL;
	
	const json& j = promise->data();
	const std::string& launch = j["launch"];

	/* Build the pipeline */
	const char* name = this->uname().c_str();
	pipeline_ = gst_pipeline_new(name); 
	GstElement* bin = gst_parse_launch(launch.c_str(), &error);
	gst_bin_add(GST_BIN(pipeline_), bin);


	gst_element_set_state(pipeline_, GST_STATE_PLAYING);

	GstBus* bus = gst_element_get_bus(pipeline_);
	gst_bus_add_watch(bus, message_handler, this);
	gst_object_unref(bus);

	promise->resolve();


}


void ElementWatcher::Stop(Promise* promise)
{

	gst_element_set_state(pipeline_, GST_STATE_NULL);
	promise->resolve();


}



bool ElementWatcher::Destroy(Promise* promise)
{
	IApp::Destroy(promise);
	return true;
}



void ElementWatcher::OnMessage(GstBus * bus, GstMessage * message)
{
	if (message->type == GST_MESSAGE_ELEMENT)
	{

		const gchar *name = GST_MESSAGE_SRC_NAME(message);
		const GstStructure *s = gst_message_get_structure(message);
		const gchar *type = gst_structure_get_name(s);


		if (!g_strcmp0(type, "spectrum"))
		{
			OnSpectrum(name, s);
		}
		else if (!g_strcmp0(type, "GstMultiFileSink"))
		{
			OnMultifilesink(name, s);
		}

	}
}

void ElementWatcher::OnSpectrum(const std::string& name, 
	 const GstStructure * s)
{
	GstClockTime endtime;
	const GValue *magnitudes;
	const GValue *phases;
	const GValue *mag, *phase;
	guint size;

	if (!gst_structure_get_clock_time(s, "endtime", &endtime))
		endtime = GST_CLOCK_TIME_NONE;

	magnitudes = gst_structure_get_value(s, "magnitude");
	phases = gst_structure_get_value(s, "phase");
	size = gst_value_list_get_size(magnitudes);
	json data;


	json::array_t mags;
	for (guint i = 0; i < size; ++i) {

		mag = gst_value_list_get_value(magnitudes, i);
		phase = gst_value_list_get_value(phases, i);
		mags.push_back(g_value_get_float(mag));
	}
	data["name"] = name;
	data["endtime"] = endtime;
	data["magnitude"] = mags;

	json meta;
	meta["topic"] = "spectrum";
	meta["origin"] = this->uname();

	Notify(data, meta);

}

void ElementWatcher::OnMultifilesink(const std::string& name,  const GstStructure * s)
{
	GstClockTime duration,stream_time;
	const GValue* val;

	if (!gst_structure_get_clock_time(s, "duration", &duration))
		duration = GST_CLOCK_TIME_NONE;

	if (!gst_structure_get_clock_time(s, "stream-time", &stream_time))
		stream_time = GST_CLOCK_TIME_NONE;

	val = gst_structure_get_value(s, "filename");
	const gchar* filename = g_value_get_string(val);

	json data;
	data["name"] = name;
	data["duration"] = duration;
	data["stream-time"] = stream_time;

	data["filename"] = filename;

	json meta;
	meta["topic"] = "multifilesink";
	meta["origin"] = this->uname();

	Notify(data, meta);
}
