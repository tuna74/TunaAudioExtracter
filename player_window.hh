// Copyright Andreas Tunek 2010
// Licensed under GPL V3 or later

#ifndef PLAYER_WINDOW_H
#define PLAYER_WINDOW_H

#include <gtkmm.h>
#include "tuna_player.h"
#include "tuna_extracter.h"

class PlayerWindow : public Gtk::Window
{
public:
  PlayerWindow();
  virtual void set_filename_and_play(const Glib::ustring&);

  //since need to access object statically, see found_outfiletype_cb in .cc file
  virtual void extracting_done(); 
  bool found_audio;
  gchar* outfiletype;

  virtual ~PlayerWindow();

protected:
  // Signal handlers:
   virtual void playb_clicked();
   virtual void fileb_clicked();
   virtual void pauseb_clicked(); 
   virtual void forwardb_clicked(gint64);
   virtual void rewindb_clicked(gint64);
   virtual void scale_moved(); 
   virtual bool move_scale(); 

  //other functions
  void play_media();
  void pause_media();
  virtual bool save_audio_file();
  virtual void extract_audio();
  virtual void cleanup_extracting();
  virtual bool check_found_outfiletype();
  virtual void start_extracting_choosen_file();
  virtual bool check_extracting_progress();

  // Control buttons
 private:
  Gtk::VBox v_widget_box;
  Gtk::HBox h_button_box; 
  Gtk::Button playb, pauseb, fileb,
    quitb, forwardb,rewindb, extractb;
  Gtk::HScale h_scale;
  Gtk::ProgressBar extracter_progress; 
  Glib::ustring filename, out_filename;

  int scale_range;
  int x_size, y_size; //so you can resize the height after extracting is done. x_size should not change.

  //C GOjbect based objects
  TunaPlayer *player;
  TunaExtracter *extracter;

  bool moving_scale, playing;
  sigc::connection move_scale_handler_id; 
  sigc::connection found_filetype_handler_id;
  sigc::connection updating_extracting_handler_id;
/*   sigc::connection checking_filetype_handler_id; */
};

#endif // PLAYER_WINDOW_H
