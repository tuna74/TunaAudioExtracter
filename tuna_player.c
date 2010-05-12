// Copyright Andreas Tunek
// Licensed under GPL V3 or later

#include "tuna_player.h"

G_DEFINE_TYPE (TunaPlayer, tuna_player, G_TYPE_OBJECT);

static void
tuna_player_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (tuna_player_parent_class)->dispose)
    G_OBJECT_CLASS (tuna_player_parent_class)->dispose (object);
  gst_object_unref(((TunaPlayer*)object)->player);
}

static void
tuna_player_finalize (GObject *object)
{
  G_OBJECT_CLASS (tuna_player_parent_class)->finalize (object);
}

static void
tuna_player_class_init (TunaPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);


  object_class->dispose = tuna_player_dispose;
  object_class->finalize = tuna_player_finalize;

klass->playing_signal_id = 
  g_signal_new("playing",
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
tuna_player_init (TunaPlayer *self)
{
  self->time_format = GST_FORMAT_TIME;
  self->stream_length = 0;
  self->player = gst_element_factory_make("playbin", "player");
  
}

TunaPlayer*
tuna_player_new (void)
{
  return g_object_new (TUNA_TYPE_PLAYER, NULL);
}


gboolean tuna_player_set_filename(TunaPlayer *self,
				  const gchar* filename){
  
  gst_element_set_state (GST_ELEMENT (self->player),
			 GST_STATE_READY); 
  self->stream_length = 0;
  g_object_set (self->player, 
		"uri", 
		g_filename_to_uri(filename, 
				  NULL,
				  NULL),
		NULL);
  return TRUE;
}

gboolean tuna_player_play(TunaPlayer *self){
  if (gst_element_set_state (GST_ELEMENT (self->player),
			     GST_STATE_PLAYING) == 0){
    g_print ("Something bad happened in tuna_player_play()\n");
    return FALSE;
  }
  g_signal_emit(self,
		TUNA_PLAYER_GET_CLASS(self)->playing_signal_id,
		0,
		NULL);
  return TRUE;
}

gboolean tuna_player_pause(TunaPlayer *self){
  if(gst_element_set_state (GST_ELEMENT (self->player),
			    GST_STATE_PAUSED) == 0){
    g_print ("Something bad happened in GstPlayer::pause()\n");
    return FALSE;
  }
  return TRUE;
}

gboolean tuna_player_seek_relative(TunaPlayer *self,
				   gint64 time){
  gint64 current_position;
  gst_element_query_position(self->player,
			     &(self->time_format),
			     &current_position);
//   gint64 new_position = current_position + (time * 1000000000);
//   g_print("current_postion:" "%" GST_TIME_FORMAT "\n", GST_TIME_ARGS (current_position));
//   g_print("new_position:"    "%" GST_TIME_FORMAT "\n", GST_TIME_ARGS (new_position));

  if (!gst_element_seek (self->player, //the element
			 1.0, //new playback rate, same as before
			 self->time_format, 
			 GST_SEEK_FLAG_FLUSH, //don't know really what this means
			 GST_SEEK_TYPE_CUR, // change from the current position
			 time * GST_SECOND,
			 GST_SEEK_TYPE_NONE,
			 GST_CLOCK_TIME_NONE)) {
    g_print ("Seek failed!\n");
    return FALSE;
  }
  return TRUE;
}


gboolean tuna_player_seek_absolute(TunaPlayer *self,
				   gdouble new_relative_pos){
  if(self->stream_length == 0) 
    gst_element_query_duration(self->player,
			       &self->time_format,
			       &self->stream_length);

 //  g_print("new__relative_pos: %f\n", new_relative_pos);  
//   g_print("stream_length:     %f\n", stream_length);
  gint64 new_pos = (gint64)(new_relative_pos * ((gdouble) self->stream_length));
//   g_print("new_pos: %i\n", new_pos);
  
  if (!gst_element_seek_simple(self->player, //the element
			       self->time_format, //the format of the time is in microseconds
			       GST_SEEK_FLAG_FLUSH, //don't know really what this means
			       new_pos)) {
    g_print ("Seek failed!\n");
    return TRUE;
  }
  return TRUE;
}


gdouble tuna_player_get_current_pos(TunaPlayer *self){
  gint64 current_pos;  
  if(self->stream_length == 0) 
    gst_element_query_duration(self->player,
			       &self->time_format,
			       &self->stream_length);
  gst_element_query_position(self->player,
			     &self->time_format,
			     &current_pos);
  gdouble current_position = ((gdouble) (((gdouble)current_pos) / ((gdouble) self->stream_length)));
//   g_print("GstPlayer::get_current_pos() will return %f.\n",
// 	  current_position);
  return current_position;
}

