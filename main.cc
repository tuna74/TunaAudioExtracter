#include <gtkmm.h>
#include "player_window.hh"

int main(int argc, char* argv[]){
  gst_init(&argc, &argv);

  Gtk::Main kit(argc, argv);
  PlayerWindow playerwindow;

  if(argc == 2)
    playerwindow.set_filename_and_play(argv[1]);

  Gtk::Main::run(playerwindow);

  gst_deinit();
  g_print("Will quit\n");
  return 0;
}