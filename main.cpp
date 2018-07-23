/* Main Loop for cataclysm */

#include "game.h"
#include "options.h"
#include "mapbuffer.h"
#include "file.h"
#include "JSON.h"
#include <time.h>
#include <fstream>
#include <iostream>
#include <exception>

using namespace cataclysm;

static bool scalar_on_hard_drive(const JSON& x)
{
	if (!x.is_scalar()) return false;
	OS_dir working;
	// we use a hashtag to cope with tilesheets.
	const char* const test = x.scalar().c_str();
	const char* const index = strchr(test, '#');
	return working.exists(!index ? test : std::string(test,index-test).c_str());
}

static bool preload_image(const JSON& x)
{
	if (!x.is_scalar() || x.empty()) return false;
	return load_tile(x.scalar().c_str());
}

static void load_JSON(const char* const src, const char* const key, bool (ok)(const JSON&))
{
#ifdef OS_dir
	OS_dir working;
	if (!working.exists(src)) return;

	std::ifstream fin;
	fin.open(src);
	if (!fin.is_open()) return;
	try {
		JSON incoming(fin);
		fin.close();
		if (incoming.empty()) return;
		if (!incoming.destructive_grep(ok)) return;
		JSON::cache[key] = std::move(incoming);
	} catch (const std::exception& e) {
		fin.close();
		std::cerr << src << ": " << e.what();
	}
#endif
}

int main(int argc, char *argv[])
{
 srand(time(NULL));

 // XXX want to load tiles before initscr; implies errors before initscr() go to C stderr or C stdout	\todo IMPLEMENT
 // want a stderr.txt as well (cf. Wesnoth 1.12- [went away in Wesnoth 1.14])

 // C:DDA retained the data directory, but has a very different layout
 // following code was based on one file/tile, but C:DDA has pre-prepared tilesheets

// final format is an object of key-id, value-image filepath pairs  To fully support C:DDA tilesheets, 
// we have to specify what order tilesets are to be checked in (array) and then merge the objects accordingly
 load_JSON("data/json/tilespec.json", "tilespec", scalar_on_hard_drive);	// expected format array of JSON files
 // expected format of the files named by tilespec are
 // key name: value is display name of tileset
 // key tiles: value is an object, keys are enumeration values, values are tile images.  For the DeonApocalypse tileset we need to
 // use hashtag notation, and possibly also a CSS-like syntax for specifying rotations and flips
 // so ....32.png[#index][!rotate|!flip]
 // index says which image in tilesheet to extract
 // ! notation says what operations to apply to the image
//	JSON::cache["tilespec"].destructive_grep(init_tiles);

 load_JSON("data/json/tiles.json", "tiles", scalar_on_hard_drive);	// authoritative destination, but not how this is to be set up.

 // when we support mods, we load their tiles configuration from tiles.json as well (and check their filepaths are ok
 // value null could be used to unset a pre-existing value
 if (JSON::cache.count("tiles") && !JSON::cache["tiles"].destructive_grep(preload_image)) JSON::cache.erase("tiles");	// wires tiles to types
 flush_tilesheets();	// don't want to pay RAM overhead for tilesheets after we've extracted the tiles from them

// ncurses stuff
 initscr(); // Initialize ncurses
 noecho();  // Don't echo keypresses
 cbreak();  // C-style breaks (e.g. ^C to SIGINT)
 keypad(stdscr, true); // Numpad is numbers
 init_colors(); // See color.cpp
 curs_set(0); // Invisible cursor

 rand();  // For some reason a call to rand() seems to be necessary to avoid
          // repetion.
 bool quit_game = false;
 game *g = new game;
 MAPBUFFER = mapbuffer(g);
 MAPBUFFER.load();
 load_options();
 do {
  g->setup();
  while (!g->do_turn());
  if (g->game_quit())
   quit_game = true;
 } while (!quit_game);
 MAPBUFFER.save();
 erase(); // Clear screen
 endwin(); // End ncurses
 system("clear"); // Tell the terminal to clear itself
 return 0;
}

