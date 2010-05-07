// Copyright Andreas Tunek
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
void plug_raw(TunaExtracter *extracter);

void filetype_found(TunaExtracter* extracter,
		    GstPad* audio_pad);

static void found_audio(GstElement* object,
			GstPad* unknown_pad,
			gboolean arg1,
			gpointer extracter);

static gboolean extracter_bus_cb (GstBus *bus,
				  GstMessage *message,
				  gpointer self);

//will unref all objects created in tuna_extracter_init
static void
tuna_extracter_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (tuna_extracter_parent_class)->dispose)
    G_OBJECT_CLASS (tuna_extracter_parent_class)->dispose (object);
  gst_element_set_state(((TunaExtracter*)object)->pipeline,
			GST_STATE_NULL);
  gst_object_unref(((TunaExtracter*)object)->pipeline);
}

static void
tuna_extracter_finalize (GObject *object)
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

static void
tuna_extracter_init (TunaExtracter *self)
{
  self->time_format = GST_FORMAT_TIME;
  self->stream_length = 0;
  self->pipeline = gst_pipeline_new("pipeline"); 
  self->outfile = gst_element_factory_make("filesink", "outfile");
  self->filetype = "";
  self->audio_found = FALSE;
  self->video_found = FALSE;
}

TunaExtracter*
tuna_extracter_new (void)
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


void set_audio_pad(TunaExtracter* self, GstPad* audio_pad, gchar* type)
{
  self->audiopad = audio_pad;
  self->filetype = type;
  self->audio_found = TRUE;
}

//this function stop when it has found the right caps (return false;)
//if the function returns true the pipeline will be modified and decodebin2 will continue it's work
static gboolean continue_autodecoding (GstElement *pipeline,
				       GstPad* unknown_pad,
				       GstCaps *caps,
				       gpointer extractor_){

  //less casting
  TunaExtracter* extractor = (TunaExtracter*)extractor_;
  if(TE_DEBUG){
    g_print("In continue_autodecoding.\n");  
    GST_LOG ("Caps are %" GST_PTR_FORMAT, caps);
    g_print ("Media type %s found.\n", gst_caps_to_string(caps));
    g_print("audio_found %d\n", extractor->audio_found);
    g_print("video_found %d\n", extractor->video_found);
  }
  if (g_strrstr (gst_caps_to_string(caps), 
		 "audio/mpeg, mpegversion=(int)1, layer=(int)3")){
    g_print ("Found a mp3.\n");
    set_audio_pad(extractor, unknown_pad, "mp3");
  }
  else if (g_strrstr (gst_caps_to_string(caps), 
		 "audio/mpeg, mpegversion=(int)1, layer=(int)[ 1, 3 ]")){
    g_print ("Found a mp2.\n");
    set_audio_pad(extractor, unknown_pad, "mp2");
  }
  else if (g_strrstr (gst_caps_to_string(caps), 
		 "audio/x-raw")){
    g_print ("Found raw.\n");
    set_audio_pad(extractor, unknown_pad, "flac");
  }

  //this messes up the EOS signal but makes everything much faster
  else if (g_strrstr (gst_caps_to_string(caps),"video/x-divx")){
    if (!extractor->video_found) {  

      /* GstElement* fakesinker = gst_element_factory_make("fakesink", */
      /* 						      "fakesinker"); */
      /* //      gst_bin_add(GST_BIN(extractor->pipeline), fakesinker); */
      /* gst_bin_add(GST_BIN(pipeline), fakesinker); */
      /* GstPad* fakesinker_sink = gst_element_get_pad(fakesinker, "sink"); */
      /* g_print("Linking pads in continue_autodecoding: %d\n", */
      /* 	      gst_pad_link(unknown_pad, */
      /* 			   fakesinker_sink)); */
      extractor->videopad = unknown_pad;
      extractor->video_found = TRUE;
    }
  }

  return !(extractor->video_found || extractor->audio_found);
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
		    GstPad* audio_pad){
  
  GstElement* fakesinker = gst_element_factory_make("fakesink",
      						      "fakesinker");
  gst_bin_add(GST_BIN(self->pipeline), fakesinker);
  GstPad* fakesinker_sink = gst_element_get_pad(fakesinker, "sink");
  g_print("video_pad is linked %d\n", gst_pad_is_linked(self->videopad));
  g_print("Linking pads in filetype_found: %d\n",
	  gst_pad_link(self->videopad ,
		       fakesinker_sink));
  
  g_signal_emit_by_name(self,
			"filetype_found",
			self->filetype);
}

static gboolean
extracter_bus_cb(GstBus *bus,
		 GstMessage *message,
		 gpointer self)
{
  switch (GST_MESSAGE_TYPE(message)){
  case GST_MESSAGE_EOS:{
    g_print("Got %s message in extracter_bus_callback.\n", GST_MESSAGE_TYPE_NAME(message));
    g_signal_emit_by_name((TunaExtracter *)self,
			  "extracting_done");
    return FALSE;
  }
  default:{
    g_print ("Got %s message in extracter_bus_callback.\n", GST_MESSAGE_TYPE_NAME (message));
    if (TE_DEBUG) {
      g_print("Pos in extracter_bus_cb %f\n", tuna_extracter_get_current_pos(self));
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
  gst_element_set_state(self->outfile, GST_STATE_PLAYING);
  GstPad *outfile_in = gst_element_get_pad(self->outfile, "sink");
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  gst_bus_add_watch (bus, 
		     extracter_bus_cb,
		     self);
  g_print ("self->audiopad is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(self->audiopad)));
  
  g_print("Linking pads in plug_mp3: %d\n",
	  gst_pad_link(self->audiopad,
		       outfile_in));
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


void plug_raw(TunaExtracter *self){
  if (TE_DEBUG){
    g_print ("In plug_raw.\n");
    g_print ("Audio type is %s.\n",
	     self->filetype);
  } 

  GstElement* audioconverter = gst_element_factory_make("audioconvert",
						       "audioconverter");
  gst_bin_add(GST_BIN(self->pipeline), audioconverter);
  GstPad* audioconverter_sink = gst_element_get_pad(audioconverter, "sink");
  g_print ("self->audiopad is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(self->audiopad)));
  g_print ("audioconverter_sink is %s\n",
	   gst_caps_to_string(gst_pad_get_caps(audioconverter_sink)));
  
  g_print("Linking pads in plug_raw: %d\n",
	  gst_pad_link(self->audiopad,
		       audioconverter_sink));
  gst_element_set_state(audioconverter, GST_STATE_PLAYING);


  /* gst_element_set_state (GST_ELEMENT (self->pipeline), */
  /* 			 GST_STATE_PAUSED); */

  GstElement* flacencoder = gst_element_factory_make("flacenc",
						     "flacencoder");
  gst_bin_add(GST_BIN(self->pipeline), flacencoder);
  if (! gst_element_link (audioconverter, flacencoder)) {
    g_print ("Could not link audioconverter to flacencoder.\n");
  } 
  gst_element_set_state(flacencoder, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN(self->pipeline), 
	      self->outfile);
  if (! gst_element_link (flacencoder, self->outfile)) {
    g_print ("Could not link flacencoder to outfile.\n");
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

  gst_element_seek_simple(self->pipeline,
  			  self->time_format,
  			  GST_SEEK_FLAG_FLUSH,
  			  0);
  gst_element_set_state (GST_ELEMENT (self->pipeline),
  			 GST_STATE_PLAYING);
  
  if (TE_DEBUG) {
    g_print("Pos after %f\n", tuna_extracter_get_current_pos(self));
    g_print("Pipeline has state %d\n", 
	    GST_STATE(self->pipeline));
}
 
  //unref all objects used here
  gst_object_unref (bus);
}
 
 
gdouble tuna_extracter_get_current_pos(TunaExtracter *self){
  if (TE_DEBUG) {
    //g_print("In tuna_extracter_get_current_pos(TunaExtracter *self).\n"); 
  }
  if(self->stream_length == 0) 
    gst_element_query_duration(self->pipeline,
			       &(self->time_format),
			       &(self->stream_length));
  gint64 current_pos;  
  gst_element_query_position(self->pipeline,
			     &(self->time_format),
			     &current_pos);
  gdouble current_position = 
    (((gdouble)current_pos) / 
     ((gdouble) self->stream_length));

  if (TE_DEBUG) {
    //g_print("self->stream_length: %"G_GINT64_FORMAT"\n", self->stream_length);
    //g_print("current_pos: %"G_GINT64_FORMAT"\n", current_pos);
  }
  g_print("tuna_extracter_get_current_pos() will return %f.\n",
	  current_position);
  return current_position;
}
