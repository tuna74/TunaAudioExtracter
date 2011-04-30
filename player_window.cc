// Copyright Andreas Tunek 2010
// Licensed under GPL V3 or later

#include "player_window.hh"
#include <string>
#include <gtk/gtk.h>

#define SCALE_RANGE 1000
#define PW_DEBUG false


PlayerWindow::PlayerWindow() :   
  // creating and initializing the buttons and add gtk::stock buttons to them
  found_audio(false),
  playb(Gtk::Stock::MEDIA_PLAY),
  pauseb(Gtk::Stock::MEDIA_PAUSE),
  fileb(Gtk::Stock::OPEN),
  forwardb(Gtk::Stock::MEDIA_FORWARD),
  rewindb(Gtk::Stock::MEDIA_REWIND),
  extractb("Extract audio!"),
  scale_range(SCALE_RANGE),
  moving_scale(false),
  playing(false)
{
  //initialize our C GObject player
  player = tuna_player_new();

  // This just sets the title of our new window.
  set_title("Tuna Audio Extracter");

  // sets the border width of the window.
  set_border_width(10);

  // put the box into the main window.
  add(v_widget_box);

 //setting the scrollbars length
   h_scale.set_range(1, scale_range);
  //setting the scrollbar's sensitivity to increments
  h_scale.set_increments(5, 25);

  //packing and showing the buttons
  h_button_box.pack_start(fileb);
  h_button_box.pack_start(playb);
  h_button_box.pack_start(pauseb);
  h_button_box.pack_start(rewindb);  
  h_button_box.pack_start(forwardb);  
  h_button_box.pack_start(extractb);
 
  h_scale.set_draw_value(FALSE);
 
  v_widget_box.pack_start(h_scale);
  v_widget_box.pack_start(h_button_box);
  
  //connect the signal handlers to the buttons
  playb.signal_clicked().connect(sigc::mem_fun(*this,
					       &PlayerWindow::playb_clicked));

  //  connecting the fileb with the file chooser
  fileb.signal_clicked().connect(sigc::mem_fun(*this,
						 &PlayerWindow::fileb_clicked));
 
  pauseb.signal_clicked().connect(sigc::mem_fun(*this,
						&PlayerWindow::pauseb_clicked));
						
  forwardb.signal_clicked().connect(sigc::bind<gint64>(sigc::mem_fun(*this,
 								     &PlayerWindow::forwardb_clicked),
 						       2) );  

  rewindb.signal_clicked().connect(sigc::bind<gint64>(sigc::mem_fun(*this,
 								    &PlayerWindow::rewindb_clicked),
 						      -2) );
   
  h_scale.signal_value_changed().connect(sigc::mem_fun(*this,
						       &PlayerWindow::scale_moved));

  extractb.signal_clicked().connect(sigc::mem_fun(*this,
 						  &PlayerWindow::extract_audio));
  show_all_children();
  
  //get the size so we can put it back to original size later
  get_size(x_size, y_size);
 }



void PlayerWindow::set_filename_and_play(const Glib::ustring &filename_){
  filename = filename_;
  tuna_player_set_filename(player, filename.c_str());
  play_media();
}


void PlayerWindow::playb_clicked(){
  play_media();
}


void PlayerWindow::pauseb_clicked(){
  pause_media();
}


//pause the player and get  the scale moving
void PlayerWindow::play_media(){
  if (!playing){
    tuna_player_play(player);
    move_scale_handler_id = 
      Glib::signal_timeout().connect(sigc::mem_fun(*this,
						   &PlayerWindow::move_scale),
				     2000); 
    playing = true;
  }
}


//pause player and stop the scale
void PlayerWindow::pause_media(){
  if (playing){
    tuna_player_pause(player);
    move_scale_handler_id.disconnect();
    playing = false;
  }
}

void PlayerWindow::forwardb_clicked(gint64 skip){
  pause_media();
  tuna_player_seek_relative(player, skip);
  play_media();
}

void PlayerWindow::rewindb_clicked(gint64 skip){
  forwardb_clicked(skip);
}

void PlayerWindow::fileb_clicked(){
  //setting up the filechooser dialog, pretty messy
  //could add more exception handling here....TODO
  //took this from the tutorial
  Gtk::FileChooserDialog dialog("Please choose a file", 
				Gtk::FILE_CHOOSER_ACTION_OPEN);
  dialog.set_transient_for(*this);
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
  dialog.set_current_folder(Glib::ustring(Glib::get_home_dir()));
  int result = dialog.run();

  switch(result)
    {
    case(Gtk::RESPONSE_OK): //everything works, can add any file
      {
	//std::cout << "Open clicked." << std::endl;
	pause_media();
	set_filename_and_play(Glib::ustring(dialog.get_filename())); //Notice that it is a std::string, not a Glib::ustring.
	//std::cout << "File selected: " <<  filename << std::endl;
	break;
      }
    case(Gtk::RESPONSE_CANCEL):
      {
	//std::cout << "Cancel clicked." << std::endl;
	break;
      }
    default:
      {
	//std::cout << "Unexpected button clicked." << std::endl;
	break;
      }
    }
}

void PlayerWindow::scale_moved(){
  if (moving_scale) return;
  double new_scale_pos = h_scale.get_value();
  //g_print("New pos: %f\n", new_scale_pos);
  pause_media();
  tuna_player_seek_absolute(player,
			    new_scale_pos / scale_range);
  play_media();
}


bool PlayerWindow::move_scale(){
  moving_scale = true;
  h_scale.set_value(tuna_player_get_current_pos(player) * scale_range);
  moving_scale = false;
  //g_print("Have moved scale.\n");
  return true;
}


PlayerWindow::~PlayerWindow(){
  g_object_unref(player);
}


//outside the main object since it will be called from a signal emitted from TunaExtracter
static void found_outfiletype_cb(TunaExtracter* extracter,
				 const gchar* filetype,
				 gpointer object) {
  g_print("Filetype is %s.\n", filetype);
  //changed this to polling architecture to avoid thread problems  
  // original was: ((PlayerWindow *)object)->save_audio_file(filetype); 
  ((PlayerWindow *)object)->outfiletype = g_strdup(filetype);
  ((PlayerWindow *)object)->found_audio = true;
}


void PlayerWindow::extract_audio(){
  extracter = tuna_extracter_new();
  g_signal_connect(extracter,
		   "filetype_found",
		   G_CALLBACK(found_outfiletype_cb),
		   this);
  //set up poll for checking if we found the outfile type
  found_filetype_handler_id = 
    Glib::signal_timeout().connect(sigc::mem_fun(*this,
						 &PlayerWindow::check_found_outfiletype),
				   1000); 

  tuna_extracter_set_filename(extracter,
			      filename.c_str());
}


bool PlayerWindow::check_found_outfiletype(){
  if (found_audio) {
    found_filetype_handler_id.disconnect();
    save_audio_file();
  }
  return true;
}


//lots of code is taken from the file-chooser tutorial
//found the type of audio, has to ask the user where to save it
bool PlayerWindow::save_audio_file(){
  if (PW_DEBUG) g_print("In PlayerWindow::save_audio_file.\n");
  g_assert  (GTK_IS_WINDOW (this->gobj()));
  Gtk::FileChooserDialog dialog("Please choose where to save the audio file", 
				Gtk::FILE_CHOOSER_ACTION_SAVE);
  dialog.set_transient_for(*this);
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

  std::string outfilepath(Glib::path_get_basename(filename));
  outfilepath.append( "."); outfilepath.append(outfiletype);
  if (PW_DEBUG) g_print("Filename to present to the user in save_audio_file is %s.\n",
			outfilepath.c_str());
  dialog.set_uri(Glib::filename_to_uri(filename));  
  if (PW_DEBUG) g_print("Have set uri in PlayerWindow::save_audio_file.\n");
  dialog.set_current_name(Glib::ustring(outfilepath));

  int result = dialog.run();

  switch(result)
    {
    case(Gtk::RESPONSE_OK): 
      {
	out_filename = Glib::ustring(dialog.get_filename()); //get.filename return std::string
	g_print("Out file is %s in save_audio_file.\n",
		out_filename.c_str());
	start_extracting_choosen_file();
	return true;
      }
    case(Gtk::RESPONSE_CANCEL):
      {
	cleanup_extracting();
      }
    default:
      {
	cleanup_extracting();
      }
    }
  return false;
}


bool PlayerWindow::check_extracting_progress(){
  if(PW_DEBUG) g_print("In PlayerWindow::check_extracting().\n");
  gdouble pos = tuna_extracter_get_current_pos(extracter);
  if(PW_DEBUG) g_print("pos = %f\n", pos);
  if (pos <= 1.0){  //extracting is not done. When done signal "extracting_done" should come
    extracter_progress.set_fraction(pos);
    return true;
  }
  g_print("Something messed up in check_extracting_progress().\n");
  return false;
}


//static function is needed here since start_extracting_choosen_file() use the C function g_signal_connect
static void extracting_done_cb(TunaExtracter* extracter,
			       gpointer playwin){
  ((PlayerWindow*)playwin)->extracting_done();
}


void PlayerWindow::start_extracting_choosen_file(){
  tuna_extracter_set_outfilename(extracter,
				 out_filename.c_str()); 

  g_signal_connect(extracter,
		   "extracting_done",
		   G_CALLBACK(extracting_done_cb),
		   this);

  updating_extracting_handler_id = 
    Glib::signal_timeout().connect(sigc::mem_fun(*this,
						 &PlayerWindow::check_extracting_progress),
				   4000); 
  extracter_progress.set_text("Extracting the audio file.");
  v_widget_box.pack_start(extracter_progress);
  extracter_progress.show();
}


void PlayerWindow::extracting_done(){
  g_print("extracting_done");
  updating_extracting_handler_id.disconnect();
  extracter_progress.hide();
  v_widget_box.remove(extracter_progress); 
  resize(x_size, y_size);
  cleanup_extracting();
}

void PlayerWindow::cleanup_extracting(){
  g_free(outfiletype); //is this necessary?
  found_filetype_handler_id.disconnect();
  g_object_unref(extracter);
}
