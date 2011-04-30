// Copyright Andreas Tunek 2010
// Licensed under GPL V3 or later

#include "tuna_extracter.h"
#include <string.h>

#define TE_DEBUG TRUE

G_DEFINE_TYPE (TunaExtracter, tuna_extracter, G_TYPE_OBJECT);

static gboolean continue_autodecoding (GstElement *pipeline,
				       GstPad* unknown_pad,
				       GstCaps *caps,
				       gpointer extracter);

void plug_mp3(TunaExtracter *extracter);
void plug_m4a(TunaExtracter *extracter);
void plug_m4a_and_mux(TunaExtracter *extracter);
void plug_raw(TunaExtracter *extracter);

void filetype_found(TunaExtracter* extracter,
		    GstPad* audio_pad);

static void found_audio(GstElement* object,
			GstPad* unknown_pad,
			gboolean arg1,
			gpointer extracter);

static gboolean extracter_bus_cb (GstBus* bus,
				  GstMessage* message,
				  gpointer self);

//will unref all objects created in tuna_extracter_init
static void tuna_extracter_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (tuna_extracter_parent_class)->dispose)
    G_OBJECT_CLASS (tuna_extracter_parent_class)->dispose (object);
  gst_element_set_state(((TunaExtracter*)object)->pipeline,
			GST_STATE_NULL);
  gst_object_unref(((TunaExtracter*)object)->pipeline);
}


static void tuna_extracter_finalize (GObject *object)
{
  G_OBJECT_CLASS (tuna_extracter_parent_class)->finalize (object);
}

static void
tuna_extracter_class_init (TunaExtracterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);


  object_class->dispose = tuna_extracter_dispose;
  object_class->finalize = tuna_extracter_finalize;

  klass->filetype_found_signal_id = 
    g_signal_new("filetype_found",
		 G_TYPE_FROM_CLASS(klass),
		 G_SIGNAL_RUN_LAST,
		 0,
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__STRING,
		 G_TYPE_NONE,
		 1,
		 G_TYPE_STRING);

  klass->extracting_done_signal_id = 
    g_signal_new("extracting_done",
		 G_TYPE_FROM_CLASS(klass),
		 G_SIGNAL_RUN_LAST,
		 0,
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE,
		 0);
}

static void tuna_extracter_init (TunaExtracter *self)
{
  self->time_format = GST_FORMAT_TIME;
  self->stream_length = 0;
  self->pipeline = gst_pipeline_new("pipeline"); 
  self->outfile = gst_element_factory_make("filesink", "outfile");
  self->filetype = "";
  self->audio_found = FALSE;
  self->video_found = FALSE;
  self->audio_plugged = FALSE;
  self->video_plugged = FALSE;
  self->m4a_probably_without_frame = FALSE;
}

TunaExtracter* tuna_extracter_new (void)
{
  return g_object_new (TUNA_TYPE_EXTRACTER, NULL);
}


gboolean tuna_extracter_set_filename(TunaExtracter *self,
				     const gchar* filename){
  
  GstElement* filesource = gst_element_factory_make ("filesrc", "source");
  g_print("Extracter file path: %s \n", filename);
  g_object_set (filesource,
		"location", 
		filename,
		NULL);
  
  gst_bin_add (GST_BIN (self->pipeline), 
	       filesource);

  GstElement* autodecoder = gst_element_factory_make("decodebin2", "autodecoder");
  //will continue to autoplug until continue_autodecoding returns false
  g_signal_connect(autodecoder, 
		   "autoplug-continue",
		   G_CALLBACK(continue_autodecoding),
		   self);

  //when continue_autodecoding returns false the "new-decoded-pad"signal will be emitted
  g_signal_connect(autodecoder,
		   "new-decoded-pad",
		   G_CALLBACK(found_audio),
		   self);

  if(TE_DEBUG) g_print ("Will plug autodecoder.\n");
 
  gst_bin_add (GST_BIN(self->pipeline), autodecoder);
  if (! gst_element_link (filesource, autodecoder)) {
    g_print ("Could not link filesrc to autodecoder.\n");
    return FALSE;
  }
  
  gst_element_set_state (GST_ELEMENT (self->pipeline),
			 GST_STATE_PLAYING);
  if(TE_DEBUG) g_print ("Have plugged autodecoder and set pipeline to PLAYING.\n");
  return TRUE;

}


gboolean is_video_pad(GstCaps* caps)
{
  gboolean ret = FALSE;
  if (g_strrstr (gst_caps_to_string(caps),"video/x-divx")){
    ret = TRUE;
  }
  else if (g_strrstr (gst_caps_to_string(caps),"video/x-h264")){
    ret = TRUE;
  }
  else if (g_strrstr (gst_caps_to_string(caps),"video/x-xvid")){
    ret = TRUE;
  }
  //slow fallback
  else if (g_strrstr (gst_caps_to_string(caps), 
		      "video/mpeg, mpegversion=(int)2, systemstream=(boolean)")) {
    ret = TRUE;
  }
  else if (g_strrstr (gst_caps_to_string(caps),"video/x-raw")) {
    ret = TRUE;
  }
  return ret;
}


void set_audio_pad(TunaExtracter* self, GstPad* audio_pad, gchar* type)
{
  self->audiopad = audio_pad;
  self->filetype = type;
  self->audio_found = TRUE;
}


void set_video_pad(TunaExtracter* self, GstPad* video_pad)
{
  if(TE_DEBUG){
    g_print("In set_video_pad.\n");  
    g_print ("Pad is %s.\n", gst_caps_to_string(gst_pad_get_caps(video_pad)));
  }

  self->videopad = video_pad;
  self->video_found = TRUE;
}


//this function stop when it has found the right caps (return false;)
//if the function returns true the pipeline will be modified and decodebin2 will continue it's work
static gboolean continue_autodecoding (GstElement *pipeline,
				       GstPad* unknown_pad,
				       GstCaps *caps,
				       gpointer extracter_){

  //less casting
  TunaExtracter* extracter = (TunaExtracter*)extracter_;
  gboolean ret = TRUE;

  if(TE_DEBUG){
    g_print("In continue_autodecoding.\n");  
    GST_LOG ("Caps are %" GST_PTR_FORMAT, caps);
    g_print ("Media type %s found.\n", gst_caps_to_string(caps));
    g_print("m4a_probably_without_frame %d\n", extracter->m4a_probably_without_frame);
    g_print("audio_found %d\n", extracter->audio_found);
    g_print("video_found %d\n", extracter->video_found);
  }
  if (!extracter->audio_found) {

    //m4a/aac files are a bit special. If they are in .ts they seem to have frame, 
    //so no need to mux them with mp4mux
    //these are pretty illogical workarounds
    if (g_strrstr (gst_caps_to_string(caps),
		   "audio/x-m4a") ||
	g_strrstr (gst_caps_to_string(caps),
		   "audio/mpeg, mpegversion=(int)4, framed=(boolean)true")) {
      extracter->m4a_probably_without_frame = TRUE;
    }

    if (g_strrstr (gst_caps_to_string(caps), 
		   "audio/mpeg, mpegversion=(int)1, layer=(int)3")){
      g_print ("Found a mp3.\n");
      set_audio_pad(extracter, unknown_pad, "mp3");
      ret = FALSE;
    }
    else if (g_strrstr (gst_caps_to_string(caps), 
			"audio/mpeg, mpegversion=(int)1, layer=(int)[ 1, 3 ]")){
      g_print ("Found a mp2.\n");
      set_audio_pad(extracter, unknown_pad, "mp2");
      ret = FALSE;
    }
    else if (g_strrstr (gst_caps_to_string(caps), 
			"audio/mpeg, mpegversion=(int)4")) {
      g_print ("Found a m4a.\n");
      set_audio_pad(extracter, unknown_pad, "m4a");
      ret = FALSE;
    }
    else if (g_strrstr (gst_caps_to_string(caps), 
			"audio/x-raw")){
      g_print ("Found raw.\n");
      set_audio_pad(extracter, unknown_pad, "flac");
      ret = FALSE;
    }
  }

  if (!extracter->video_found && is_video_pad(caps)) {
    set_video_pad(extracter, unknown_pad);
    ret = FALSE;
  }

  if(TE_DEBUG){
    g_print("In continue_autodecoding 2.\n");  
    g_print("audio_found %d\n", extracter->audio_found);
    g_print("video_found %d\n", extracter->video_found);
  }
  return ret;
}


//should always happen after continue_autodecoding has returned false
static void found_audio(GstElement* object,
			GstPad* unknown_pad,
			gboolean arg1,
			gpointer extracter){
  if (g_strcmp0(((TunaExtracter *)extracter)->filetype, "") == 0) return;
  
  filetype_found((TunaExtracter *)extracter, 
		 unknown_pad);
}


void filetype_found(TunaExtracter *self,
		    GstPad* unknown_pad){
  
  if (self->video_found && !self->video_plugged) {
    GstElement* fakesinker = gst_element_factory_make("fakesink",
      						      "fakesinker");
    gst_bin_add(GST_BIN(self->pipeline), fakesinker);
    GstPad* fakesinker_sink = gst_element_get_pad(fakesinker, "sink");
    g_print("video_pad is linked %d\n", gst_pad_is_linked(self->videopad));
    g_print("Linking pads in filetype_found: %d\n",
	    gst_pad_link(self->videopad ,
			 fakesinker_sink));
    self->video_plugged = TRUE;
  }

  if (self->audio_found && !self->audio_plugged) {
    self->audio_plugged = TRUE;
    g_signal_emit_by_name(self,
			  "filetype_found",
			  self->filetype);
  }
}

static gint prev_message_type = 0;
static gboolean extracter_bus_cb(GstBus* bus,
				 GstMessage* message,
				 gpointer self)
{
  if (prev_message_type == GST_MESSAGE_TYPE(message))return TRUE;

  prev_message_type = GST_MESSAGE_TYPE(message);

  switch (GST_MESSAGE_TYPE(message)){
  case GST_MESSAGE_EOS:{
    if (TE_DEBUG) {
      g_print("Got %s message in extracter_bus_callback.\n", 
	      GST_MESSAGE_TYPE_NAME(message));
    }
    g_signal_emit_by_name((TunaExtracter *)self,
			  "extracting_done");
    return FALSE;
  }
  case GST_MESSAGE_WARNING:{
    GError* error;
    gchar* debug;
    gst_message_parse_warning(message, &error, &debug);
    g_print("Got warning in extracter_bus_callback.\nWarning is %s\n", 
	    debug);
    
    g_error_free(error);
    g_free(debug);
    break;
  }
  default:{
    if (TE_DEBUG) {
      g_print ("Got %s message in extracter_bus_callback.\n", 
	       GST_MESSAGE_TYPE_NAME (message));
      g_print("Pipeline has state %d\n", 
	      GST_STATE(((TunaExtracter *)self)->pipeline));
    }
    break;
  }
  }
  return TRUE;
}
	

gboolean tuna_extracter_set_outfilename(TunaExtracter *self,
					const gchar* outfilename){
  g_object_set (self->outfile,
		"location", 
		outfilename,
		NULL);
  
  if(strcmp(self->filetype, "mp3") == 0 ||
     strcmp(self->filetype, "mp2") == 0) {
    plug_mp3(self);
    return TRUE;
  }
  if(strcmp(self->filetype, "m4a") == 0) {
    if (self->m4a_probably_without_frame) {
      plug_m4a_and_mux(self);
    }
    else {
      plug_m4a(self);
    }
    return TRUE;
  }
  else if(strcmp(self->filetype, "flac") == 0) {
    plug_raw(self);
    return TRUE;
  }
   
  g_print("Something went wrong in set_outfilename.\n");
  return FALSE;
}


void plug_mp3(TunaExtracter *self){
  if (TE_DEBUG){
    g_print ("In plug_mp3.\n");
    g_print ("Audio type is %s.\n",
	     self->filetype);
  } 

  gst_bin_add (GST_BIN(self->pipeline), 
	       self->outfile);
  g_print ("self->audiopad is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(self->audiopad)));
  
  GstPad* outfile_in = gst_element_get_pad(self->outfile, "sink");
  gint linkOk = gst_pad_link(self->audiopad,
			     outfile_in);
  if (GST_PAD_LINK_FAILED(linkOk)) {
    g_print("Could not link self->audiopad to outfile_in plug_mp3.\n");
  }
  gst_element_set_state(self->outfile, GST_STATE_PLAYING);

  GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  gst_bus_add_watch (bus, 
		     extracter_bus_cb,
		     self);

  //try to go to the start of the stream
  //don't know if the player is at the end
  gst_element_seek_simple(self->pipeline,
			  self->time_format,
			  GST_SEEK_FLAG_FLUSH,
			  0);

  if (TE_DEBUG)	   
    tuna_extracter_get_current_pos(self);
  
  //unref all objects used here
  gst_object_unref (bus);
}


void plug_m4a(TunaExtracter *self)
{
  if (TE_DEBUG){
    g_print ("In plug_m4a.\n");
    g_print ("Audio type is %s.\n",
	     self->filetype);
  } 

  gst_bin_add (GST_BIN(self->pipeline), 
	       self->outfile);
  g_print ("self->audiopad is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(self->audiopad)));
  
  GstPad* outfile_in = gst_element_get_pad(self->outfile, "sink");
  gint linkOk = gst_pad_link(self->audiopad,
			     outfile_in);
  if (GST_PAD_LINK_FAILED(linkOk)) {
    g_print("Could not link self->audiopad to outfile_in plug_m4a.\n");
  }
  gst_element_set_state(self->outfile, GST_STATE_PLAYING);

  GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  gst_bus_add_watch (bus, 
		     extracter_bus_cb,
		     self);

  //try to go to the start of the stream
  //don't know if the player is at the end
  gst_element_seek_simple(self->pipeline,
			  self->time_format,
			  GST_SEEK_FLAG_FLUSH,
			  0);

  if (TE_DEBUG)	   
    tuna_extracter_get_current_pos(self);
  
  //unref all objects used here
  gst_object_unref (bus);
}


void plug_m4a_and_mux(TunaExtracter *self)
{
  if (TE_DEBUG){
    g_print ("In plug_m4a_and_mux.\n");
    g_print ("Audio type is %s.\n",
	     self->filetype);
  } 

  GstElement* mp4muxer = gst_element_factory_make("mp4mux",
						  "mp4muxer");
  gst_bin_add(GST_BIN(self->pipeline), mp4muxer);

  GstPad* mp4muxer_sink = gst_element_get_pad(mp4muxer, "audio_4");
  g_print ("mp4muxer_sink is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(mp4muxer_sink)));

  gint linkOk = gst_pad_link(self->audiopad,
			     mp4muxer_sink);
  if (GST_PAD_LINK_FAILED(linkOk)) {
    g_print("Could not link self->audiopad to mp4muxer_sink_in plug_m4a_and_mux.\n");
  }
  gst_element_set_state(mp4muxer, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN(self->pipeline), 
	       self->outfile);
  g_print ("self->audiopad is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(self->audiopad)));
  
  if (!gst_element_link (mp4muxer, self->outfile)) {
    g_print ("Could not link mp4muxer to self->outfile in plug_m4a_and_mux.\n");
  } 

  gst_element_set_state(self->outfile, GST_STATE_PLAYING);

  GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  gst_bus_add_watch (bus, 
		     extracter_bus_cb,
		     self);

  //try to go to the start of the stream
  //don't know if the player is at the end
  gboolean seeked = gst_element_seek_simple(self->pipeline,
					    self->time_format,
					    GST_SEEK_FLAG_FLUSH,
					    0);

  if (TE_DEBUG){
    g_print("seeked %d.\n", seeked);
  } 

  
  gst_element_set_state(self->pipeline, GST_STATE_PLAYING);

  //unref all objects used here
  gst_object_unref (bus);
}


void plug_raw(TunaExtracter *self){
  if (TE_DEBUG){
    g_print ("In plug_raw.\n");
    g_print ("Audio type is %s.\n",
	     self->filetype);
  } 
  g_print ("self->audiopad is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(self->audiopad)));
  
  GstElement* audioconverter = gst_element_factory_make("audioconvert",
							"audioconverter");
  gst_bin_add(GST_BIN(self->pipeline), audioconverter);
  GstPad* audioconverter_sink = gst_element_get_pad(audioconverter, "sink");
  g_print ("audioconverter_sink is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(audioconverter_sink)));
  

  gint linkOk = gst_pad_link(self->audiopad,
			     audioconverter_sink);
  if (GST_PAD_LINK_FAILED(linkOk)) {
    g_print("Could not link self->audiopad to audioconverter_sink_in plug_raw.\n");
  }

  gst_element_set_state(audioconverter, GST_STATE_PLAYING);

  GstElement* flacencoder = gst_element_factory_make("flacenc",
						     "flacencoder");
  gst_bin_add(GST_BIN(self->pipeline), flacencoder);
  if (!gst_element_link(audioconverter,
			flacencoder)) {
    g_print("Could not link audioconverter to flacencoder_sink_in plug_raw.\n");
  }
  gst_element_set_state(flacencoder, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN(self->pipeline), 
	       self->outfile);
  if (!gst_element_link (flacencoder, self->outfile)) {
    g_print ("Could not link flacencoder to self->outfile in plug_raw.\n");
  } 
  gst_element_set_state(self->outfile, GST_STATE_PLAYING);


  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  gst_bus_add_watch (bus, 
		     extracter_bus_cb,
		     self);
 
 //try to go to the start of the stream
  //don't know if the player is at the end 
  if (TE_DEBUG) {
    g_print("Pos before %f\n", tuna_extracter_get_current_pos(self));
  }

  gboolean seeked = gst_element_seek_simple(self->pipeline,
					    self->time_format,
					    GST_SEEK_FLAG_FLUSH,
					    0);
 
  if (TE_DEBUG){
    g_print("seeked %d.\n",
	     seeked);
    g_print("Pos after %f\n", tuna_extracter_get_current_pos(self));
    g_print("Pipeline has state %d\n", 
	    GST_STATE(self->pipeline));
  }
 
  //unref all objects used here
  gst_object_unref (bus);
}
 
 
gdouble tuna_extracter_get_current_pos(TunaExtracter* self){
  if(self->stream_length == 0) {
    gst_element_query_duration(self->pipeline,
			       &(self->time_format),
			       &(self->stream_length));
  }
  gint64 current_pos;  
  gst_element_query_position(self->pipeline,
			     &(self->time_format),
			     &current_pos);
  gdouble current_position = 
    (((gdouble)current_pos) / 
     ((gdouble) self->stream_length));

  if (TE_DEBUG) {
    g_print("tuna_extracter_get_current_pos() will return %f.\n",
	    current_position);
    g_print("current_pos = %i.\n", current_pos);
    g_print("stream_length = %i.\n", self->stream_length);
  }
  return current_position;
}
