
tae: main.o player_window.o tuna_player.o tuna_extracter.o
	g++ `pkg-config --cflags --libs gstreamer-0.10`  `pkg-config --cflags --libs  gtkmm-2.4` -o tae main.o tuna_player.o player_window.o tuna_extracter.o -ggdb3 -Wall

main.o: main.cc player_window.hh
	g++ -c `pkg-config --cflags  gstreamer-0.10`  `pkg-config --cflags  gtkmm-2.4` main.cc -ggdb3 -Wall

player_window.o: player_window.cc tuna_player.h tuna_extracter.h
	g++ -c `pkg-config --cflags  gstreamer-0.10`  `pkg-config --cflags  gtkmm-2.4` player_window.cc -ggdb3 -Wall

tuna_player.o: tuna_player.c tuna_player.h
	gcc -c  `pkg-config --cflags gstreamer-0.10` tuna_player.c -ggdb3 -Wall

tuna_extracter.o: tuna_extracter.c tuna_extracter.h
	gcc -c  `pkg-config --cflags gstreamer-0.10` tuna_extracter.c -ggdb3 -Wall

clean:
	rm tae main.o tuna_player.o player_window.o tuna_extracter.o