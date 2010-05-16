// Copyright Andreas Tunek 2010
// Licensed under GPL V3 or later

#ifndef _TUNA_EXTRACTER
#define _TUNA_EXTRACTER

#include <glib-object.h>
#include <gst/gst.h>

/* GObject stuff */

G_BEGIN_DECLS

#define TUNA_TYPE_EXTRACTER tuna_extracter_get_type()

#define TUNA_EXTRACTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TUNA_TYPE_EXTRACTER, TunaExtracter))

#define TUNA_EXTRACTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TUNA_TYPE_EXTRACTER, TunaExtracterClass))

#define TUNA_IS_EXTRACTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TUNA_TYPE_EXTRACTER))

#define TUNA_IS_EXTRACTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TUNA_TYPE_EXTRACTER))

#define TUNA_EXTRACTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TUNA_TYPE_EXTRACTER, TunaExtracterClass))

typedef struct {
  GObject parent;

  /*don't access these directly*/
  GstFormat time_format;
  gint64 stream_length;
  GstElement *pipeline;
  GstElement *outfile;
  gchar* filetype;

  /*the path (whole file name) for the outfile, could be replaced with some gio stuff in the future*/
  GstPad *audiopad;
  GstPad *videopad;
  gboolean audio_found;
  gboolean audio_plugged;
  gboolean video_found;
  gboolean video_plugged;
  gboolean m4a_probably_without_frame;
} TunaExtracter;

typedef struct {
  GObjectClass parent_class;
  
/* This signal is released if a mp3 has been found */
/* has type TunaExtracter*, gchar* */
  gint filetype_found_signal_id;

/* Released when extracting is done. Has type TunaExtracter*. */
  gint extracting_done_signal_id;
 } TunaExtracterClass;

GType tuna_extracter_get_type (void);

TunaExtracter* tuna_extracter_new (void);

/* Set the whole file path and name, eg /home/tuna/movie.avi */
gboolean tuna_extracter_set_filename(TunaExtracter*,
				     const gchar*);

gboolean tuna_extracter_start_extracting(TunaExtracter*);

/* Set the name of the outfile. Must include full path! */
gboolean tuna_extracter_set_outfilename(TunaExtracter*,
					const gchar*);

/* Returns the fractional position of the playing file 
   make sure you have a file set*/
gdouble tuna_extracter_get_current_pos(TunaExtracter*);

G_END_DECLS

#endif /* _TUNA_EXTRACTER */
