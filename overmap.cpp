#include "overmap.h"
#include "game.h"
#include "keypress.h"
#include "saveload.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <vector>
#include <fstream>
#include <sstream>

#include "Zaimoni.STL/GDI/box.hpp"

#define STREETCHANCE 2
#define NUM_FOREST 250
#define TOP_HIWAY_DIST 140
#define MIN_ANT_SIZE 8
#define MAX_ANT_SIZE 20
#define MIN_GOO_SIZE 1
#define MAX_GOO_SIZE 2
#define MIN_RIFT_SIZE 6
#define MAX_RIFT_SIZE 16
#define SETTLE_DICE 2
#define SETTLE_SIDES 2
#define HIVECHANCE 180	//Chance that any given forest will be a hive
#define SWAMPINESS 8	//Affects the size of a swamp
#define SWAMPCHANCE 850	// 1/SWAMPCHANCE Chance that a swamp will spawn instead of forest

using namespace cataclysm;

template<> bool discard<bool>::x = false;
template<> oter_id discard<oter_id>::x = ot_null;

const map_extras no_extras(0);
const map_extras road_extras(
	// %%% HEL MIL SCI STA DRG SUP PRT MIN WLF PUD CRT FUM 1WY ART
	50, 40, 50, 120, 200, 30, 10, 5, 80, 20, 200, 10, 8, 2, 3);
const map_extras field_extras(
	60, 40, 15, 40, 80, 10, 10, 3, 50, 30, 300, 10, 8, 1, 3);
const map_extras subway_extras(
	// %%% HEL MIL SCI STA DRG SUP PRT MIN WLF PUD CRT FUM 1WY ART
	75, 0, 5, 12, 5, 5, 0, 7, 0, 0, 120, 0, 20, 1, 3);
const map_extras build_extras(
	90, 0, 5, 12, 0, 10, 0, 5, 5, 0, 0, 60, 8, 1, 3);

// LINE_**** corresponds to the ACS_**** macros in ncurses, and are patterned
// the same way; LINE_NESW, where X indicates a line and O indicates no line
// (thus, LINE_OXXX looks like 'T'). LINE_ is defined in output.h.  The ACS_
// macros can't be used here, since ncurses hasn't been initialized yet.

// Order MUST match enum oter_id above!

const oter_t oter_t::list[num_ter_types] = {
	{ "nothing",		'%',	c_white,	0, no_extras, false, false },
{ "crater",		'O',	c_red,		2, field_extras, false, false },
{ "field",		'.',	c_brown,	2, field_extras, false, false },
{ "forest",		'F',	c_green,	3, field_extras, false, false },
{ "forest",		'F',	c_green,	4, field_extras, false, false },
{ "swamp",		'F',	c_cyan,		4, field_extras, false, false },
{ "bee hive",		'8',	c_yellow,	3, field_extras, false, false },
{ "forest",		'F',	c_green,	3, field_extras, false, false },
/* The tile above is a spider pit. */
{ "fungal bloom",	'T',	c_ltgray,	2, field_extras, false, false },
{ "highway",		'H',	c_dkgray,	2, road_extras, false, false },
{ "highway",		'=',	c_dkgray,	2, road_extras, false, false },
{ "BUG",			'%',	c_magenta,	0, no_extras, false, false },
{ "road",          LINE_XOXO,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_OXOX,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_XXOO,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_OXXO,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_OOXX,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_XOOX,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_XXXO,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_XXOX,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_XOXX,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_OXXX,	c_dkgray,	2, road_extras, false, false },
{ "road",          LINE_XXXX,	c_dkgray,	2, road_extras, false, false },
{ "road, manhole", LINE_XXXX,	c_yellow,	2, road_extras, true, false },
{ "bridge",		'|',	c_dkgray,	2, road_extras, false, false },
{ "bridge",		'-',	c_dkgray,	2, road_extras, false, false },
{ "river",		'R',	c_blue,		1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "river bank",		'R',	c_ltblue,	1, no_extras, false, false },
{ "house",		'^',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'>',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'v',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'<',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'^',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'>',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'v',	c_ltgreen,	5, build_extras, false, false },
{ "house",		'<',	c_ltgreen,	5, build_extras, false, false },
{ "parking lot",		'O',	c_dkgray,	1, build_extras, false, false },
{ "park",		'O',	c_green,	2, build_extras, false, false },
{ "gas station",		'^',	c_ltblue,	5, build_extras, false, false },
{ "gas station",		'>',	c_ltblue,	5, build_extras, false, false },
{ "gas station",		'v',	c_ltblue,	5, build_extras, false, false },
{ "gas station",		'<',	c_ltblue,	5, build_extras, false, false },
{ "pharmacy",		'^',	c_ltred,	5, build_extras, false, false },
{ "pharmacy",		'>',	c_ltred,	5, build_extras, false, false },
{ "pharmacy",		'v',	c_ltred,	5, build_extras, false, false },
{ "pharmacy",		'<',	c_ltred,	5, build_extras, false, false },
{ "grocery store",	'^',	c_green,	5, build_extras, false, false },
{ "grocery store",	'>',	c_green,	5, build_extras, false, false },
{ "grocery store",	'v',	c_green,	5, build_extras, false, false },
{ "grocery store",	'<',	c_green,	5, build_extras, false, false },
{ "hardware store",	'^',	c_cyan,		5, build_extras, false, false },
{ "hardware store",	'>',	c_cyan,		5, build_extras, false, false },
{ "hardware store",	'v',	c_cyan,		5, build_extras, false, false },
{ "hardware store",	'<',	c_cyan,		5, build_extras, false, false },
{ "electronics store",   '^',	c_yellow,	5, build_extras, false, false },
{ "electronics store",   '>',	c_yellow,	5, build_extras, false, false },
{ "electronics store",   'v',	c_yellow,	5, build_extras, false, false },
{ "electronics store",   '<',	c_yellow,	5, build_extras, false, false },
{ "sporting goods store",'^',	c_ltcyan,	5, build_extras, false, false },
{ "sporting goods store",'>',	c_ltcyan,	5, build_extras, false, false },
{ "sporting goods store",'v',	c_ltcyan,	5, build_extras, false, false },
{ "sporting goods store",'<',	c_ltcyan,	5, build_extras, false, false },
{ "liquor store",	'^',	c_magenta,	5, build_extras, false, false },
{ "liquor store",	'>',	c_magenta,	5, build_extras, false, false },
{ "liquor store",	'v',	c_magenta,	5, build_extras, false, false },
{ "liquor store",	'<',	c_magenta,	5, build_extras, false, false },
{ "gun store",		'^',	c_red,		5, build_extras, false, false },
{ "gun store",		'>',	c_red,		5, build_extras, false, false },
{ "gun store",		'v',	c_red,		5, build_extras, false, false },
{ "gun store",		'<',	c_red,		5, build_extras, false, false },
{ "clothing store",	'^',	c_blue,		5, build_extras, false, false },
{ "clothing store",	'>',	c_blue,		5, build_extras, false, false },
{ "clothing store",	'v',	c_blue,		5, build_extras, false, false },
{ "clothing store",	'<',	c_blue,		5, build_extras, false, false },
{ "library",		'^',	c_brown,	5, build_extras, false, false },
{ "library",		'>',	c_brown,	5, build_extras, false, false },
{ "library",		'v',	c_brown,	5, build_extras, false, false },
{ "library",		'<',	c_brown,	5, build_extras, false, false },
{ "restaurant",		'^',	c_pink,		5, build_extras, false, false },
{ "restaurant",		'>',	c_pink,		5, build_extras, false, false },
{ "restaurant",		'v',	c_pink,		5, build_extras, false, false },
{ "restaurant",		'<',	c_pink,		5, build_extras, false, false },
{ "subway station",	'S',	c_yellow,	5, build_extras, true, false },
{ "subway station",	'S',	c_yellow,	5, build_extras, true, false },
{ "subway station",	'S',	c_yellow,	5, build_extras, true, false },
{ "subway station",	'S',	c_yellow,	5, build_extras, true, false },
{ "police station",	'^',	c_dkgray,	5, build_extras, false, false },
{ "police station",	'>',	c_dkgray,	5, build_extras, false, false },
{ "police station",	'v',	c_dkgray,	5, build_extras, false, false },
{ "police station",	'<',	c_dkgray,	5, build_extras, false, false },
{ "bank",		'^',	c_ltgray,	5, no_extras, false, false },
{ "bank",		'>',	c_ltgray,	5, no_extras, false, false },
{ "bank",		'v',	c_ltgray,	5, no_extras, false, false },
{ "bank",		'<',	c_ltgray,	5, no_extras, false, false },
{ "bar",			'^',	c_pink,		5, build_extras, false, false },
{ "bar",			'>',	c_pink,		5, build_extras, false, false },
{ "bar",			'v',	c_pink,		5, build_extras, false, false },
{ "bar",			'<',	c_pink,		5, build_extras, false, false },
{ "pawn shop",		'^',	c_white,	5, build_extras, false, false },
{ "pawn shop",		'>',	c_white,	5, build_extras, false, false },
{ "pawn shop",		'v',	c_white,	5, build_extras, false, false },
{ "pawn shop",		'<',	c_white,	5, build_extras, false, false },
{ "mil. surplus",	'^',	c_white,	5, build_extras, false, false },
{ "mil. surplus",	'>',	c_white,	5, build_extras, false, false },
{ "mil. surplus",	'v',	c_white,	5, build_extras, false, false },
{ "mil. surplus",	'<',	c_white,	5, build_extras, false, false },
{ "megastore",		'M',	c_ltblue,	5, build_extras, false, false },
{ "megastore",		'M',	c_blue,		5, build_extras, false, false },
{ "hospital",		'H',	c_ltred,	5, build_extras, false, false },
{ "hospital",		'H',	c_red,		5, build_extras, false, false },
{ "mansion",		'M',	c_ltgreen,	5, build_extras, false, false },
{ "mansion",		'M',	c_green,	5, build_extras, false, false },
{ "evac shelter",	'+',	c_white,	2, no_extras, true, false },
{ "evac shelter",	'+',	c_white,	2, no_extras, false, true },
{ "science lab",		'L',	c_ltblue,	5, no_extras, false, false },
{ "science lab",		'L',	c_blue,		5, no_extras, true, false },
{ "science lab",		'L',	c_ltblue,	5, no_extras, false, false },
{ "science lab",		'L',	c_cyan,		5, no_extras, false, false },
{ "nuclear plant",	'P',	c_ltgreen,	5, no_extras, false, false },
{ "nuclear plant",	'P',	c_ltgreen,	5, no_extras, false, false },
{ "military bunker",	'B',	c_dkgray,	2, no_extras, true, true },
{ "military outpost",	'M',	c_dkgray,	2, build_extras, false, false },
{ "missile silo",	'0',	c_ltgray,	2, no_extras, false, false },
{ "missile silo",	'0',	c_ltgray,	2, no_extras, false, false },
{ "strange temple",	'T',	c_magenta,	5, no_extras, true, false },
{ "strange temple",	'T',	c_pink,		5, no_extras, true, false },
{ "strange temple",	'T',	c_pink,		5, no_extras, false, false },
{ "strange temple",	'T',	c_yellow,	5, no_extras, false, false },
{ "sewage treatment",	'P',	c_red,		5, no_extras, true, false },
{ "sewage treatment",	'P',	c_green,	5, no_extras, false, true },
{ "sewage treatment",	'P',	c_green,	5, no_extras, false, false },
{ "mine entrance",	'M',	c_ltgray,	5, no_extras, true, false },
{ "mine shaft",		'O',	c_dkgray,	5, no_extras, true, true },
{ "mine",		'M',	c_brown,	2, no_extras, false, false },
{ "mine",		'M',	c_brown,	2, no_extras, false, false },
{ "mine",		'M',	c_brown,	2, no_extras, false, false },
{ "spiral cavern",	'@',	c_pink,		2, no_extras, false, false },
{ "spiral cavern",	'@',	c_pink,		2, no_extras, false, false },
{ "radio tower",         'X',    c_ltgray,       2, no_extras, false, false },
{ "toxic waste dump",	'D',	c_pink,		2, no_extras, false, false },
{ "cave",		'C',	c_brown,	2, field_extras, false, false },
{ "rat cave",		'C',	c_dkgray,	2, no_extras, true, false },
{ "cavern",		'0',	c_ltgray,	2, no_extras, false, false },
{ "anthill",		'%',	c_brown,	2, no_extras, true, false },
{ "solid rock",		'%',	c_dkgray,	5, no_extras, false, false },
{ "rift",		'^',	c_red,		2, no_extras, false, false },
{ "hellmouth",		'^',	c_ltred,	2, no_extras, true, false },
{ "slime pit",		'~',	c_ltgreen,	2, no_extras, true, false },
{ "slime pit",		'~',	c_ltgreen,	2, no_extras, false, false },
{ "triffid grove",	'T',	c_ltred,	5, no_extras, true, false },
{ "triffid roots",	'T',	c_ltred,	5, no_extras, true, true },
{ "triffid heart",	'T',	c_red,		5, no_extras, false, true },
{ "basement",		'O',	c_dkgray,	5, no_extras, false, true },
{ "subway station",	'S',	c_yellow,	5, subway_extras, false, true },
{ "subway",        LINE_XOXO,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_OXOX,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_XXOO,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_OXXO,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_OOXX,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_XOOX,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_XXXO,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_XXOX,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_XOXX,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_OXXX,	c_dkgray,	5, subway_extras, false, false },
{ "subway",        LINE_XXXX,	c_dkgray,	5, subway_extras, false, false },
{ "sewer",         LINE_XOXO,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_OXOX,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_XXOO,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_OXXO,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_OOXX,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_XOOX,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_XXXO,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_XXOX,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_XOXX,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_OXXX,	c_green,	5, no_extras, false, false },
{ "sewer",         LINE_XXXX,	c_green,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XOXO,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_OXOX,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XXOO,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_OXXO,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_OOXX,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XOOX,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XXXO,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XXOX,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XOXX,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_OXXX,	c_brown,	5, no_extras, false, false },
{ "ant tunnel",    LINE_XXXX,	c_brown,	5, no_extras, false, false },
{ "ant food storage",	'O',	c_green,	5, no_extras, false, false },
{ "ant larva chamber",	'O',	c_white,	5, no_extras, false, false },
{ "ant queen chamber",	'O',	c_red,		5, no_extras, false, false },
{ "cavern",		'0',	c_ltgray,	5, no_extras, false, false },
{ "tutorial room",	'O',	c_cyan,		5, no_extras, false, false }
};

struct omspec_place
{
	// Able functions - true if p is valid
	static bool never(overmap *om, point p) { return false; }
	static bool always(overmap *om, point p) { return true; }
	static bool water(overmap *om, point p); // Only on rivers
	static bool land(overmap *om, point p); // Only on land (no rivers)
	static bool forest(overmap *om, point p); // Forest
	static bool wilderness(overmap *om, point p); // Forest or fields
	static bool by_highway(overmap *om, point p); // Next to existing highways
};

// Set min or max to -1 to ignore them

const overmap_special overmap_special::specials[NUM_OMSPECS] = {

// Terrain	 MIN MAX DISTANCE
{ ot_crater,	   0, 10,  0, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::land, mfb(OMS_FLAG_BLOB) },

{ ot_hive, 	   0, 50, 10, -1, mcat_bee, 20, 60, 2, 4,
&omspec_place::forest, mfb(OMS_FLAG_3X3) },

{ ot_house_north,   0,100,  0, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::by_highway, mfb(OMS_FLAG_ROTATE_ROAD) },

{ ot_s_gas_north,   0,100,  0, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::by_highway, mfb(OMS_FLAG_ROTATE_ROAD) },

{ ot_house_north,   0, 50, 20, -1, mcat_null, 0, 0, 0, 0,  // Woods cabin
&omspec_place::forest, mfb(OMS_FLAG_ROTATE_RANDOM) | mfb(OMS_FLAG_ROTATE_ROAD) },

{ ot_temple_stairs, 0,  3, 20, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::forest, 0 },

{ ot_lab_stairs,	   0, 30,  8, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::land, mfb(OMS_FLAG_ROAD) },

// Terrain	 MIN MAX DISTANCE
{ ot_bunker,	   2, 10,  4, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::land, mfb(OMS_FLAG_ROAD) },

{ ot_outpost,	   0, 10,  4, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness, 0 },

{ ot_silo,	   0,  1, 30, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness, mfb(OMS_FLAG_ROAD) },

{ ot_radio_tower,   1,  5,  0, 20, mcat_null, 0, 0, 0, 0,
&omspec_place::by_highway, 0 },

{ ot_mansion_entrance, 0, 8, 0, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::by_highway, mfb(OMS_FLAG_3X3_SECOND) },

{ ot_mansion_entrance, 0, 4, 10, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness, mfb(OMS_FLAG_3X3_SECOND) },

{ ot_megastore_entrance, 0, 5, 0, 10, mcat_null, 0, 0, 0, 0,
&omspec_place::by_highway, mfb(OMS_FLAG_3X3_SECOND) },

{ ot_hospital_entrance, 1, 5, 3, 15, mcat_null, 0, 0, 0, 0,
&omspec_place::by_highway, mfb(OMS_FLAG_3X3_SECOND) },

{ ot_sewage_treatment, 1,  5, 10, 20, mcat_null, 0, 0, 0, 0,
&omspec_place::land, mfb(OMS_FLAG_PARKING_LOT) },

{ ot_mine_entrance,  0,  5,  15, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness, mfb(OMS_FLAG_PARKING_LOT) },

// Terrain	 MIN MAX DISTANCE
{ ot_anthill,	   0, 30,  10, -1, mcat_ant, 10, 30, 1000, 2000,
&omspec_place::wilderness, 0 },

{ ot_spider_pit,	   0,500,  0, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::forest, 0 },

{ ot_slimepit,	   0,  4,  0, -1, mcat_goo, 2, 10, 100, 200,
&omspec_place::land, 0 },

{ ot_fungal_bloom,  0,  3,  5, -1, mcat_fungi, 600, 1200, 30, 50,
&omspec_place::wilderness, 0 },

{ ot_triffid_grove, 0,  4,  0, -1, mcat_triffid, 800, 1300, 12, 20,
&omspec_place::forest, 0 },

{ ot_river_center,  0, 10, 10, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::always, mfb(OMS_FLAG_BLOB) },

// Terrain	 MIN MAX DISTANCE
{ ot_shelter,       5, 10,  5, 10, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness, mfb(OMS_FLAG_ROAD) },

{ ot_cave,	   0, 30,  0, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness, 0 },

{ ot_toxic_dump,	   0,  5, 15, -1, mcat_null, 0, 0, 0, 0,
&omspec_place::wilderness,0 }

};

#if PROTOTYPE
void settlement_building(settlement &set, int x, int y);	// #include "settlement.h" when bringing this up
#endif

double dist(int x1, int y1, int x2, int y2)
{
 return sqrt(double(pow(x1-x2, 2.0) + pow(y1-y2, 2.0)));
}

bool is_river(oter_id ter)
{
 if (ter == ot_null || (ter >= ot_bridge_ns && ter <= ot_river_nw))
  return true;
 return false;
}

bool is_ground(oter_id ter)
{
 if (ter <= ot_road_nesw_manhole)
  return true;
 return false;
}

bool is_wall_material(oter_id ter)
{
 if (is_ground(ter) ||
     (ter >= ot_house_north && ter <= ot_nuke_plant))
  return true;
 return false;
}
 
oter_id shop(int dir)
{
 oter_id ret = ot_s_lot;
 int type = rng(0, 16);
 if (one_in(20))
  type = 17;
 switch (type) {
  case  0: ret = ot_s_lot;	         break;
  case  1: ret = ot_s_gas_north;         break;
  case  2: ret = ot_s_pharm_north;       break;
  case  3: ret = ot_s_grocery_north;     break;
  case  4: ret = ot_s_hardware_north;    break;
  case  5: ret = ot_s_sports_north;      break;
  case  6: ret = ot_s_liquor_north;      break;
  case  7: ret = ot_s_gun_north;         break;
  case  8: ret = ot_s_clothes_north;     break;
  case  9: ret = ot_s_library_north;     break;
  case 10: ret = ot_s_restaurant_north;  break;
  case 11: ret = ot_sub_station_north;   break;
  case 12: ret = ot_bank_north;          break;
  case 13: ret = ot_bar_north;           break;
  case 14: ret = ot_s_electronics_north; break;
  case 15: ret = ot_pawn_north;          break;
  case 16: ret = ot_mil_surplus_north;   break;
  case 17: ret = ot_police_north;        break;
 }
 if (ret == ot_s_lot)
  return ret;
 if (dir < 0) dir += 4;
 switch (dir) {
  case 0:                         break;
  case 1: ret = oter_id(ret + 1); break;
  case 2: ret = oter_id(ret + 2); break;
  case 3: ret = oter_id(ret + 3); break;
  default: debugmsg("Bad rotation of shop."); return ot_null;
 }
 return ret;
}

oter_id house(int dir)
{
 bool base = one_in(30);
 if (dir < 0) dir += 4;
 switch (dir) {
  case 0:  return base ? ot_house_base_north : ot_house_north;
  case 1:  return base ? ot_house_base_east  : ot_house_east;
  case 2:  return base ? ot_house_base_south : ot_house_south;
  case 3:  return base ? ot_house_base_west  : ot_house_west;
  default: debugmsg("Bad rotation of house."); return ot_null;
 }
}

// *** BEGIN overmap FUNCTIONS ***

oter_id& overmap::ter(int x, int y)
{
 if (x < 0 || x >= OMAPX || y < 0 || y >= OMAPY) return (discard<oter_id>::x = ot_null);
 return t[x][y];
}

std::vector<mongroup*> overmap::monsters_at(int x, int y)
{
 std::vector<mongroup*> ret;
 if (x < 0 || x >= OMAPX || y < 0 || y >= OMAPY) return ret;
 for (int i = 0; i < zg.size(); i++) {
  if (trig_dist(x, y, zg[i].pos) <= zg[i].radius)
   ret.push_back(&(zg[i]));
 }
 return ret;
}

bool overmap::is_safe(const point& pt) const
{
 for (auto& gr : const_cast<overmap*>(this)->monsters_at(pt.x, pt.y)) if (!gr->is_safe()) return false;
 return true;
}


bool& overmap::seen(int x, int y)
{
 if (x < 0 || x >= OMAPX || y < 0 || y >= OMAPY) return (discard<bool>::x = false);
 return s[x][y];
}

bool overmap::has_note(int x, int y) const
{
 for (int i = 0; i < notes.size(); i++) {
  if (notes[i].x == x && notes[i].y == y)
   return true;
 }
 return false;
}

std::string overmap::note(int x, int y) const
{
 for (int i = 0; i < notes.size(); i++) {
  if (notes[i].x == x && notes[i].y == y)
   return notes[i].text;
 }
 return "";
}

void overmap::add_note(int x, int y, std::string message)
{
 for (int i = 0; i < notes.size(); i++) {
  if (notes[i].x == x && notes[i].y == y) {
   if (message == "")
    notes.erase(notes.begin() + i);
   else
    notes[i].text = message;
   return;
  }
 }
 if (message.length() > 0)
  notes.push_back(om_note(x, y, notes.size(), message));
}

point overmap::find_note(point origin, const std::string& text) const
{
 int closest = 9999;
 point ret(-1, -1);
 for (int i = 0; i < notes.size(); i++) {
  int dist = rl_dist(origin, notes[i].x, notes[i].y);
  if (notes[i].text.find(text) != std::string::npos && dist < closest) {
   closest = dist;
   ret = point(notes[i].x, notes[i].y);
  }
 }
 return ret;
}

void overmap::delete_note(int x, int y)
{
 std::vector<om_note>::iterator it;
 for (it = notes.begin(); it < notes.end(); it++) {
  if (it->x == x && it->y == y){
  	notes.erase(it);
  }
 }
}

point overmap::display_notes() const
{
 std::string title = "Notes:";
 WINDOW* w_notes = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 const int maxitems = 20;	// Number of items to show at one time.
 char ch = '.';
 int start = 0, cur_it;
 mvwprintz(w_notes, 0, 0, c_ltgray, title.c_str());
 do{
  if (ch == '<' && start > 0) {
   for (int i = 1; i < VIEW; i++)
    mvwprintz(w_notes, i, 0, c_black, "                                                     ");
   start -= maxitems;
   if (start < 0) start = 0;
   mvwprintw(w_notes, maxitems + 2, 0, "         ");
  }
  if (ch == '>' && cur_it < notes.size()) {
   start = cur_it;
   mvwprintw(w_notes, maxitems + 2, 12, "            ");
   for (int i = 1; i < VIEW; i++)
    mvwprintz(w_notes, i, 0, c_black, "                                                     ");
  }
  int cur_line = 2;
  int last_line = -1;
  char cur_let = 'a';
  for (cur_it = start; cur_it < start + maxitems && cur_line < 23; cur_it++) {
   if (cur_it < notes.size()) {
   mvwputch (w_notes, cur_line, 0, c_white, cur_let++);
   mvwprintz(w_notes, cur_line, 2, c_ltgray, "- %s", notes[cur_it].text.c_str());
   } else{
    last_line = cur_line - 2;
    break;
   }
   cur_line++;
  }

  if(last_line == -1) last_line = 23;
  if (start > 0)
   mvwprintw(w_notes, maxitems + 4, 0, "< Go Back");
  if (cur_it < notes.size())
   mvwprintw(w_notes, maxitems + 4, 12, "> More notes"); 
  if(ch >= 'a' && ch <= 't'){
   int chosen_line = (int)(ch % (int)'a');
   if(chosen_line < last_line)
    return point(notes[start + chosen_line].x, notes[start + chosen_line].y); 
  }
  mvwprintz(w_notes, 0, 40, c_white, "Press letter to center on note");
  mvwprintz(w_notes, 24, 40, c_white, "Spacebar - Return to map  ");
  wrefresh(w_notes);
  ch = getch();
 } while(ch != ' ');
 delwin(w_notes);
 return point(-1,-1);
}

void overmap::generate(game *g, overmap* north, overmap* east, overmap* south,
                       overmap* west)
{
 erase();
 clear();
 move(0, 0);
 for (int i = 0; i < OMAPY; i++) {
  for (int j = 0; j < OMAPX; j++) {
   ter(i, j) = ot_field;
   seen(i, j) = false;
  }
 }
 std::vector<city> road_points;	// cities and roads_out together
 std::vector<point> river_start;// West/North endpoints of rivers
 std::vector<point> river_end;	// East/South endpoints of rivers

// Determine points where rivers & roads should connect w/ adjacent maps
 if (north != NULL) {
  for (int i = 2; i < OMAPX - 2; i++) {
   if (is_river(north->ter(i,OMAPY-1)))
    ter(i, 0) = ot_river_center;
   if (north->ter(i,     OMAPY - 1) == ot_river_center &&
       north->ter(i - 1, OMAPY - 1) == ot_river_center &&
       north->ter(i + 1, OMAPY - 1) == ot_river_center) {
    if (river_start.size() == 0 ||
        river_start[river_start.size() - 1].x < i - 6)
     river_start.push_back(point(i, 0));
   }
  }
  for (int i = 0; i < north->roads_out.size(); i++) {
   if (north->roads_out[i].y == OMAPY - 1)
    roads_out.push_back(city(north->roads_out[i].x, 0, 0));
  }
 }
 int rivers_from_north = river_start.size();
 if (west != NULL) {
  for (int i = 2; i < OMAPY - 2; i++) {
   if (is_river(west->ter(OMAPX - 1, i)))
    ter(0, i) = ot_river_center;
   if (west->ter(OMAPX - 1, i)     == ot_river_center &&
       west->ter(OMAPX - 1, i - 1) == ot_river_center &&
       west->ter(OMAPX - 1, i + 1) == ot_river_center) {
    if (river_start.size() == rivers_from_north ||
        river_start[river_start.size() - 1].y < i - 6)
     river_start.push_back(point(0, i));
   }
  }
  for (int i = 0; i < west->roads_out.size(); i++) {
   if (west->roads_out[i].x == OMAPX - 1)
    roads_out.push_back(city(0, west->roads_out[i].y, 0));
  }
 }
 if (south != NULL) {
  for (int i = 2; i < OMAPX - 2; i++) {
   if (is_river(south->ter(i, 0)))
    ter(i, OMAPY - 1) = ot_river_center;
   if (south->ter(i,     0) == ot_river_center &&
       south->ter(i - 1, 0) == ot_river_center &&
       south->ter(i + 1, 0) == ot_river_center) {
    if (river_end.size() == 0 ||
        river_end[river_end.size() - 1].x < i - 6)
     river_end.push_back(point(i, OMAPY - 1));
   }
   if (south->ter(i, 0) == ot_road_nesw)
    roads_out.push_back(city(i, OMAPY - 1, 0));
  }
  for (int i = 0; i < south->roads_out.size(); i++) {
   if (south->roads_out[i].y == 0)
    roads_out.push_back(city(south->roads_out[i].x, OMAPY - 1, 0));
  }
 }
 int rivers_to_south = river_end.size();
 if (east != NULL) {
  for (int i = 2; i < OMAPY - 2; i++) {
   if (is_river(east->ter(0, i)))
    ter(OMAPX - 1, i) = ot_river_center;
   if (east->ter(0, i)     == ot_river_center &&
       east->ter(0, i - 1) == ot_river_center &&
       east->ter(0, i + 1) == ot_river_center) {
    if (river_end.size() == rivers_to_south ||
        river_end[river_end.size() - 1].y < i - 6)
     river_end.push_back(point(OMAPX - 1, i));
   }
   if (east->ter(0, i) == ot_road_nesw)
    roads_out.push_back(city(OMAPX - 1, i, 0));
  }
  for (int i = 0; i < east->roads_out.size(); i++) {
   if (east->roads_out[i].x == 0)
    roads_out.push_back(city(OMAPX - 1, east->roads_out[i].y, 0));
  }
 }
// Even up the start and end points of rivers. (difference of 1 is acceptable)
// Also ensure there's at least one of each.
 std::vector<point> new_rivers;
 if (north == NULL || west == NULL) {
  while (river_start.empty() || river_start.size() + 1 < river_end.size()) {
   new_rivers.clear();
   if (north == NULL)
    new_rivers.push_back( point(rng(10, OMAPX - 11), 0) );
   if (west == NULL)
    new_rivers.push_back( point(0, rng(10, OMAPY - 11)) );
   river_start.push_back( new_rivers[rng(0, new_rivers.size() - 1)] );
  }
 }
 if (south == NULL || east == NULL) {
  while (river_end.empty() || river_end.size() + 1 < river_start.size()) {
   new_rivers.clear();
   if (south == NULL)
    new_rivers.push_back( point(rng(10, OMAPX - 11), OMAPY - 1) );
   if (east == NULL)
    new_rivers.push_back( point(OMAPX - 1, rng(10, OMAPY - 11)) );
   river_end.push_back( new_rivers[rng(0, new_rivers.size() - 1)] );
  }
 }
// Now actually place those rivers.
 if (river_start.size() > river_end.size() && river_end.size() > 0) {
  std::vector<point> river_end_copy = river_end;
  int index;
  while (!river_start.empty()) {
   index = rng(0, river_start.size() - 1);
   if (!river_end.empty()) {
    place_river(river_start[index], river_end[0]);
    river_end.erase(river_end.begin());
   } else
    place_river(river_start[index],
                river_end_copy[rng(0, river_end_copy.size() - 1)]);
   river_start.erase(river_start.begin() + index);
  }
 } else if (river_end.size() > river_start.size() && river_start.size() > 0) {
  std::vector<point> river_start_copy = river_start;
  int index;
  while (!river_end.empty()) {
   index = rng(0, river_end.size() - 1);
   if (!river_start.empty()) {
    place_river(river_start[0], river_end[index]);
    river_start.erase(river_start.begin());
   } else
    place_river(river_start_copy[rng(0, river_start_copy.size() - 1)],
                river_end[index]);
   river_end.erase(river_end.begin() + index);
  }
 } else if (river_end.size() > 0) {
  if (river_start.size() != river_end.size())
   river_start.push_back( point(rng(OMAPX * .25, OMAPX * .75),
                                rng(OMAPY * .25, OMAPY * .75)));
  for (int i = 0; i < river_start.size(); i++)
   place_river(river_start[i], river_end[i]);
 }
    
// Cities, forests, and settlements come next.
// These're agnostic of adjacent maps, so it's very simple.
 int mincit = 0;
 if (north == NULL && east == NULL && west == NULL && south == NULL)
  mincit = 1;	// The first map MUST have a city, for the player to start in!
 place_cities(cities, mincit);
 place_forest();

// Ideally we should have at least two exit points for roads, on different sides
 if (roads_out.size() < 2) { 
  std::vector<city> viable_roads;
  int tmp;
// Populate viable_roads with one point for each neighborless side.
// Make sure these points don't conflict with rivers. 
// TODO: In theory this is a potential infinte loop...
  if (north == NULL) {
   do
    tmp = rng(10, OMAPX - 11);
   while (is_river(ter(tmp, 0)) || is_river(ter(tmp - 1, 0)) ||
          is_river(ter(tmp + 1, 0)) );
   viable_roads.push_back(city(tmp, 0, 0));
  }
  if (east == NULL) {
   do
    tmp = rng(10, OMAPY - 11);
   while (is_river(ter(OMAPX - 1, tmp)) || is_river(ter(OMAPX - 1, tmp - 1))||
          is_river(ter(OMAPX - 1, tmp + 1)));
   viable_roads.push_back(city(OMAPX - 1, tmp, 0));
  }
  if (south == NULL) {
   do
    tmp = rng(10, OMAPX - 11);
   while (is_river(ter(tmp, OMAPY - 1)) || is_river(ter(tmp - 1, OMAPY - 1))||
          is_river(ter(tmp + 1, OMAPY - 1)));
   viable_roads.push_back(city(tmp, OMAPY - 1, 0));
  }
  if (west == NULL) {
   do
    tmp = rng(10, OMAPY - 11);
   while (is_river(ter(0, tmp)) || is_river(ter(0, tmp - 1)) ||
          is_river(ter(0, tmp + 1)));
   viable_roads.push_back(city(0, tmp, 0));
  }
  while (roads_out.size() < 2 && !viable_roads.empty()) {
   tmp = rng(0, viable_roads.size() - 1);
   roads_out.push_back(viable_roads[tmp]);
   viable_roads.erase(viable_roads.begin() + tmp);
  }
 }
// Compile our master list of roads; it's less messy if roads_out is first
 for (int i = 0; i < roads_out.size(); i++)
  road_points.push_back(roads_out[i]);
 for (int i = 0; i < cities.size(); i++)
  road_points.push_back(cities[i]);
// And finally connect them via "highways"
 place_hiways(road_points, ot_road_null);
// Place specials
 place_specials();
// Make the roads out road points;
 for (int i = 0; i < roads_out.size(); i++)
  ter(roads_out[i].x, roads_out[i].y) = ot_road_nesw;
// Clean up our roads and rivers
 polish();
// Place the monsters, now that the terrain is laid out
 place_mongroups();
 place_radios();
}

void overmap::generate_sub(overmap* above)
{
 std::vector<city> subway_points;
 std::vector<city> sewer_points;
 std::vector<city> ant_points;
 std::vector<city> goo_points;
 std::vector<city> lab_points;
 std::vector<point> shaft_points;
 std::vector<city> mine_points;
 std::vector<point> bunker_points;
 std::vector<point> shelter_points;
 std::vector<point> triffid_points;
 std::vector<point> temple_points;
 for (int i = 0; i < OMAPX; i++) {
  for (int j = 0; j < OMAPY; j++) {
   seen(i, j) = false;	// Start by setting all squares to unseen
   ter(i, j) = ot_rock;	// Start by setting everything to solid rock
  }
 }

 for (int i = 0; i < OMAPX; i++) {
  for (int j = 0; j < OMAPY; j++) {
   if (above->ter(i, j) >= ot_sub_station_north &&
       above->ter(i, j) <= ot_sub_station_west) {
    ter(i, j) = ot_subway_nesw;
    subway_points.push_back(city(i, j, 0));

   } else if (above->ter(i, j) == ot_road_nesw_manhole) {
    ter(i, j) = ot_sewer_nesw;
    sewer_points.push_back(city(i, j, 0));

   } else if (above->ter(i, j) == ot_sewage_treatment) {
    for (int x = i-1; x <= i+1; x++) {
     for (int y = j-1; y <= j+1; y++) {
      ter(x, y) = ot_sewage_treatment_under;
     }
    }
    ter(i, j) = ot_sewage_treatment_hub;
    sewer_points.push_back(city(i, j, 0));

   } else if (above->ter(i, j) == ot_spider_pit)
    ter(i, j) = ot_spider_pit_under;

   else if (above->ter(i, j) == ot_cave && pos.z == -1) {
	ter(i, j) = one_in(3) ? ot_cave_rat : ot_cave;

   } else if (above->ter(i, j) == ot_cave_rat && pos.z == -2)
    ter(i, j) = ot_cave_rat;

   else if (above->ter(i, j) == ot_anthill) {
    int size = rng(MIN_ANT_SIZE, MAX_ANT_SIZE);
    ant_points.push_back(city(i, j, size));
    zg.push_back(mongroup(mcat_ant, i * 2, j * 2, size * 1.5, rng(6000, 8000)));

   } else if (above->ter(i, j) == ot_slimepit_down) {
    int size = rng(MIN_GOO_SIZE, MAX_GOO_SIZE);
    goo_points.push_back(city(i, j, size));

   } else if (above->ter(i, j) == ot_forest_water)
    ter(i, j) = ot_cavern;

   else if (above->ter(i, j) == ot_triffid_grove ||
            above->ter(i, j) == ot_triffid_roots)
    triffid_points.push_back( point(i, j) );

   else if (above->ter(i, j) == ot_temple_stairs)
    temple_points.push_back( point(i, j) );

   else if (above->ter(i, j) == ot_lab_core ||
            (pos.z == -1 && above->ter(i, j) == ot_lab_stairs))
    lab_points.push_back(city(i, j, rng(1, 5 + pos.z)));

   else if (above->ter(i, j) == ot_lab_stairs)
    ter(i, j) = ot_lab;

   else if (above->ter(i, j) == ot_bunker && pos.z == -1)
    bunker_points.push_back( point(i, j) );

   else if (above->ter(i, j) == ot_shelter)
    shelter_points.push_back( point(i, j) );

   else if (above->ter(i, j) == ot_mine_entrance)
    shaft_points.push_back( point(i, j) );

   else if (above->ter(i, j) == ot_mine_shaft ||
            above->ter(i, j) == ot_mine_down    ) {
    ter(i, j) = ot_mine;
    mine_points.push_back(city(i, j, rng(6 + pos.z, 10 + pos.z)));

   } else if (above->ter(i, j) == ot_mine_finale) {
    for (int x = i - 1; x <= i + 1; x++) {
     for (int y = j - 1; y <= j + 1; y++)
      ter(x, y) = ot_spiral;
    }
    ter(i, j) = ot_spiral_hub;
    zg.push_back(mongroup(mcat_spiral, i * 2, j * 2, 2, 200));

   } else if (above->ter(i, j) == ot_silo) {
	const auto abs_z = (0<pos.z) ? pos.z : -pos.z;
	ter(i, j) = (rng(2, 7) < abs_z || rng(2, 7) < abs_z) ? ot_silo_finale : ot_silo;
   }
  }
 }

 for (int i = 0; i < goo_points.size(); i++)
  build_slimepit(goo_points[i].x, goo_points[i].y, goo_points[i].s);
 place_hiways(sewer_points,  ot_sewer_nesw);
 polish(ot_sewer_ns, ot_sewer_nesw);
 place_hiways(subway_points, ot_subway_nesw);
 for (int i = 0; i < subway_points.size(); i++)
  ter(subway_points[i].x, subway_points[i].y) = ot_subway_station;
 for (int i = 0; i < lab_points.size(); i++)
  build_lab(lab_points[i].x, lab_points[i].y, lab_points[i].s);
 for (int i = 0; i < ant_points.size(); i++)
  build_anthill(ant_points[i].x, ant_points[i].y, ant_points[i].s);
 polish(ot_subway_ns, ot_subway_nesw);
 polish(ot_ants_ns, ot_ants_nesw);
 for (int i = 0; i < above->cities.size(); i++) {
  if (one_in(3))
   zg.push_back(
    mongroup(mcat_chud, above->cities[i].x * 2, above->cities[i].y * 2,
             above->cities[i].s, above->cities[i].s * 20));
  if (!one_in(8))
   zg.push_back(
    mongroup(mcat_sewer, above->cities[i].x * 2, above->cities[i].y * 2,
             above->cities[i].s * 3.5, above->cities[i].s * 70));
 }
 place_rifts();
 for (int i = 0; i < mine_points.size(); i++)
  build_mine(mine_points[i].x, mine_points[i].y, mine_points[i].s);
// Basements done last so sewers, etc. don't overwrite them
 for (int i = 0; i < OMAPX; i++) {
  for (int j = 0; j < OMAPY; j++) {
   if (above->ter(i, j) >= ot_house_base_north &&
       above->ter(i, j) <= ot_house_base_west)
    ter(i, j) = ot_basement;
  }
 }

 for (int i = 0; i < shaft_points.size(); i++)
  ter(shaft_points[i].x, shaft_points[i].y) = ot_mine_shaft;

 for (int i = 0; i < bunker_points.size(); i++)
  ter(bunker_points[i].x, bunker_points[i].y) = ot_bunker;

 for (int i = 0; i < shelter_points.size(); i++)
  ter(shelter_points[i].x, shelter_points[i].y) = ot_shelter_under;

 for (int i = 0; i < triffid_points.size(); i++) {
  ter( triffid_points[i].x, triffid_points[i].y ) = (pos.z == -1) ? ot_triffid_roots : ot_triffid_finale;
 }

 for (int i = 0; i < temple_points.size(); i++) {
  ter( temple_points[i].x, temple_points[i].y ) = (pos.z == -5) ? ot_temple_finale : ot_temple_stairs;
 }

}

void overmap::make_tutorial()
{
 if (pos.z == 9) {
  for (int i = 0; i < OMAPX; i++) {
   for (int j = 0; j < OMAPY; j++)
    ter(i, j) = ot_rock;
  }
 }
 ter(50, 50) = ot_tutorial;
 zg.clear();
}

point overmap::find_closest(point origin, oter_id type, int type_range, int &dist, bool must_be_seen) const
{
 int max = (dist == 0 ? OMAPX / 2 : dist);
 auto t_at = ter(origin);
 if (t_at >= type && t_at < type + type_range && (!must_be_seen || seen(origin))) return origin;
 
 for (dist = 1; dist <= max; dist++) {
  int i = 8*dist;
  while (0 < i--) {
    point pt = origin+zaimoni::gdi::Linf_border_sweep<point>(dist,i,origin.x,origin.y);
	if ((t_at = ter(pt)) >= type && t_at < type + type_range && (!must_be_seen || seen(pt))) return pt;
  }
 }
 return point(-1, -1);
}

#if DEAD_FUNC
// reimplement as above before taking live
std::vector<point> overmap::find_all(point origin, oter_id type, int type_range,
                            int &dist, bool must_be_seen)
{
 std::vector<point> res;
 int max = (dist == 0 ? OMAPX / 2 : dist);
 for (dist = 0; dist <= max; dist++) {
  for (int x = origin.x - dist; x <= origin.x + dist; x++) {
   for (int y = origin.y - dist; y <= origin.y + dist; y++) {
    if (ter(x, y) >= type && ter(x, y) < type + type_range &&
        (!must_be_seen || seen(x, y)))
     res.push_back(point(x, y));
   }
  }
 }
 return res;
}
#endif

std::vector<point> overmap::find_terrain(const std::string& term) const
{
 std::vector<point> found;
 for (int x = 0; x < OMAPX; x++) {
  for (int y = 0; y < OMAPY; y++) {
   if (seen(x, y) && oter_t::list[ter(x, y)].name.find(term) != std::string::npos)
    found.push_back( point(x, y) );
  }
 }
 return found;
}

int overmap::closest_city(point p) const
{
 int distance = 999, ret = -1;
 for (int i = 0; i < cities.size(); i++) {
  int dist = rl_dist(p, cities[i].x, cities[i].y);
  if (dist < distance || (dist == distance && cities[i].s < cities[ret].s)) {
   ret = i;
   distance = dist;
  }
 }

 return ret;
}

point overmap::random_house_in_city(int city_id) const
{
 if (city_id < 0 || city_id >= cities.size()) {
  debugmsg("overmap::random_house_in_city(%d) (max %d)", city_id,
           cities.size() - 1);
  return point(-1, -1);
 }
 std::vector<point> valid;
 int startx = cities[city_id].x - cities[city_id].s,
     endx   = cities[city_id].x + cities[city_id].s,
     starty = cities[city_id].y - cities[city_id].s,
     endy   = cities[city_id].y + cities[city_id].s;
 for (int x = startx; x <= endx; x++) {
  for (int y = starty; y <= endy; y++) {
   if (ter(x, y) >= ot_house_north && ter(x, y) <= ot_house_west)
    valid.push_back( point(x, y) );
  }
 }
 if (valid.empty())
  return point(-1, -1);

 return valid[ rng(0, valid.size() - 1) ];
}

int overmap::dist_from_city(point p) const
{
 int distance = 999;
 for (int i = 0; i < cities.size(); i++) {
  int dist = rl_dist(p, cities[i].x, cities[i].y);
  dist -= cities[i].s;
  if (dist < distance)
   distance = dist;
 }
 return distance;
}

void overmap::draw(WINDOW *w, game *g, int &cursx, int &cursy, 
                   int &origx, int &origy, char &ch, bool blink) const
{
 bool legend = true, note_here = false, npc_here = false;
 std::string note_text, npc_name;
 
 int omx, omy;
 overmap hori, vert, diag; // Adjacent maps
 point target(-1, -1);
 if (g->u.active_mission >= 0 &&
     g->u.active_mission < g->u.active_missions.size())
  target = g->find_mission(g->u.active_missions[g->u.active_mission])->target;
  bool see;
  oter_id cur_ter;
  nc_color ter_color;
  long ter_sym;
/* First, determine if we're close enough to the edge to need to load an
 * adjacent overmap, and load it/them. */
  if (cursx < 25) {
   hori = overmap(g, pos.x - 1, pos.y, pos.z);
   if (cursy < 12)
    diag = overmap(g, pos.x - 1, pos.y - 1, pos.z);
   if (cursy > OMAPY - 14)
    diag = overmap(g, pos.x - 1, pos.y + 1, pos.z);
  }
  if (cursx > OMAPX - 26) {
   hori = overmap(g, pos.x + 1, pos.y, pos.z);
   if (cursy < 12)
    diag = overmap(g, pos.x + 1, pos.y - 1, pos.z);
   if (cursy > OMAPY - 14)
    diag = overmap(g, pos.x + 1, pos.y + 1, pos.z);
  }
  if (cursy < 12)
   vert = overmap(g, pos.x, pos.y - 1, pos.z);
  if (cursy > OMAPY - 14)
   vert = overmap(g, pos.x, pos.y + 1, pos.z);

// Now actually draw the map
  bool csee = false;
  oter_id ccur_ter;
  for (int i = -25; i < 25; i++) {
   for (int j = -12; j <= (ch == 'j' ? 13 : 12); j++) {
    omx = cursx + i;
    omy = cursy + j;
    see = false;
    npc_here = false;
    if (omx >= 0 && omx < OMAPX && omy >= 0 && omy < OMAPY) { // It's in-bounds
     cur_ter = ter(omx, omy);
     see = seen(omx, omy);
     if (note_here = has_note(omx, omy))
      note_text = note(omx, omy);
     for (int n = 0; n < npcs.size(); n++) {
      if ((npcs[n].mapx + 1) / 2 == omx && (npcs[n].mapy + 1) / 2 == omy) {
       npc_here = true;
       npc_name = npcs[n].name;
       n = npcs.size();
      } else {
       npc_here = false;
       npc_name = "";
      }
     }
// <Out of bounds placement>
    } else if (omx < 0) {
     omx += OMAPX;
     if (omy < 0 || omy >= OMAPY) {
      omy += (omy < 0 ? OMAPY : 0 - OMAPY);
      cur_ter = diag.ter(omx, omy);
      see = diag.seen(omx, omy);
      if ((note_here = diag.has_note(omx, omy)))
       note_text = diag.note(omx, omy);
     } else {
      cur_ter = hori.ter(omx, omy);
      see = hori.seen(omx, omy);
      if (note_here = hori.has_note(omx, omy))
       note_text = hori.note(omx, omy);
     }
    } else if (omx >= OMAPX) {
     omx -= OMAPX;
     if (omy < 0 || omy >= OMAPY) {
      omy += (omy < 0 ? OMAPY : 0 - OMAPY);
      cur_ter = diag.ter(omx, omy);
      see = diag.seen(omx, omy);
      if (note_here = diag.has_note(omx, omy))
       note_text = diag.note(omx, omy);
     } else {
      cur_ter = hori.ter(omx, omy);
      see = hori.seen(omx, omy);
      if ((note_here = hori.has_note(omx, omy)))
       note_text = hori.note(omx, omy);
     }
    } else if (omy < 0) {
     omy += OMAPY;
     cur_ter = vert.ter(omx, omy);
     see = vert.seen(omx, omy);
     if ((note_here = vert.has_note(omx, omy)))
      note_text = vert.note(omx, omy);
    } else if (omy >= OMAPY) {
     omy -= OMAPY;
     cur_ter = vert.ter(omx, omy);
     see = vert.seen(omx, omy);
     if ((note_here = vert.has_note(omx, omy)))
      note_text = vert.note(omx, omy);
    } else
     debugmsg("No data loaded! omx: %d omy: %d", omx, omy);
// </Out of bounds replacement>
    if (see) {
     if (note_here && blink) {
      ter_color = c_yellow;
      ter_sym = 'N';
     } else if (omx == origx && omy == origy && blink) {
      ter_color = g->u.color();
      ter_sym = '@';
     } else if (npc_here && blink) {
      ter_color = c_pink;
      ter_sym = '@';
     } else if (omx == target.x && omy == target.y && blink) {
      ter_color = c_red;
      ter_sym = '*';
     } else {
      if (cur_ter >= num_ter_types || cur_ter < 0) {
       debugmsg("Bad ter %d (%d, %d)", cur_ter, omx, omy);
	   cur_ter = ot_null;	// certainly don't use an invalid array dereference
	  }
	  decltype(oter_t::list[0])& terrain = oter_t::list[cur_ter];
      ter_color = terrain.color;
      ter_sym = terrain.sym;
     }
    } else { // We haven't explored this tile yet
     ter_color = c_dkgray;
     ter_sym = '#';
    }
    if (j == 0 && i == 0) {
     mvwputch_hi (w, 12,     25,     ter_color, ter_sym);
     csee = see;
     ccur_ter = cur_ter;
    } else
     mvwputch    (w, 12 + j, 25 + i, ter_color, ter_sym);
   }
  }
  if (target.x != -1 && target.y != -1 && blink &&
      (target.x < cursx - 25 || target.x > cursx + 25  ||
       target.y < cursy - 12 || target.y > cursy + 12    )) {
   switch (direction_from(cursx, cursy, target)) {
    case NORTH:      mvwputch(w,  0, 25, c_red, '^');       break;
    case NORTHEAST:  mvwputch(w,  0, 49, c_red, LINE_OOXX); break;
    case EAST:       mvwputch(w, 12, 49, c_red, '>');       break;
    case SOUTHEAST:  mvwputch(w, 24, 49, c_red, LINE_XOOX); break;
    case SOUTH:      mvwputch(w, 24, 25, c_red, 'v');       break;
    case SOUTHWEST:  mvwputch(w, 24,  0, c_red, LINE_XXOO); break;
    case WEST:       mvwputch(w, 12,  0, c_red, '<');       break;
    case NORTHWEST:  mvwputch(w,  0,  0, c_red, LINE_OXXO); break;
   }
  }
  if (has_note(cursx, cursy)) {
   note_text = note(cursx, cursy);
   for (int i = 0; i < note_text.length(); i++)
    mvwputch(w, 1, i, c_white, LINE_OXOX);
   mvwputch(w, 1, note_text.length(), c_white, LINE_XOOX);
   mvwputch(w, 0, note_text.length(), c_white, LINE_XOXO);
   mvwprintz(w, 0, 0, c_yellow, note_text.c_str());
  } else if (npc_here) {
   for (int i = 0; i < npc_name.length(); i++)
    mvwputch(w, 1, i, c_white, LINE_OXOX);
   mvwputch(w, 1, npc_name.length(), c_white, LINE_XOOX);
   mvwputch(w, 0, npc_name.length(), c_white, LINE_XOXO);
   mvwprintz(w, 0, 0, c_yellow, npc_name.c_str());
  }
  if (legend) {
   cur_ter = ter(cursx, cursy);
// Draw the vertical line
   for (int j = 0; j < VIEW; j++)
    mvwputch(w, j, 51, c_white, LINE_XOXO);
// Clear the legend
   for (int i = 51; i < SCREEN_WIDTH; i++) {
    for (int j = 0; j < VIEW; j++)
     mvwputch(w, j, i, c_black, 'x');
   }

   if (csee) {
	decltype(oter_t::list[ccur_ter])& terrain = oter_t::list[ccur_ter];
    mvwputch(w, 1, 51, terrain.color, terrain.sym);
    mvwprintz(w, 1, 53, terrain.color, "%s", terrain.name.c_str());
   } else
    mvwprintz(w, 1, 51, c_dkgray, "# Unexplored");

   if (target.x != -1 && target.y != -1) {
    int distance = rl_dist(origx, origy, target);
    mvwprintz(w, 3, 51, c_white, "Distance to target: %d", distance);
   }
   mvwprintz(w, 17, 51, c_magenta,           "Use movement keys to pan.  ");
   mvwprintz(w, 18, 51, c_magenta,           "0 - Center map on character");
   mvwprintz(w, 19, 51, c_magenta,           "t - Toggle legend          ");
   mvwprintz(w, 20, 51, c_magenta,           "/ - Search                 ");
   mvwprintz(w, 21, 51, c_magenta,           "N - Add a note             ");
   mvwprintz(w, 22, 51, c_magenta,           "D - Delete a note          ");
   mvwprintz(w, 23, 51, c_magenta,           "L - List notes             ");
   mvwprintz(w, 24, 51, c_magenta,           "Esc or q - Return to game  ");
  }
// Done with all drawing!
  wrefresh(w);
}

point overmap::choose_point(game *g)
{
 WINDOW* w_map = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 WINDOW* w_search = newwin(13, 27, 3, 51);
 timeout(BLINK_SPEED);	// Enable blinking!
 bool blink = true;
 point curs((g->lev.x + int(MAPSIZE / 2)), (g->lev.y + int(MAPSIZE / 2)));
 curs /= 2;
 point orig(curs);
 char ch = 0;
 point ret(-1, -1);
 
 do {  
  draw(w_map, g, curs.x, curs.y, orig.x, orig.y, ch, blink);
  ch = input();
  if (ch != ERR) blink = true;	// If any input is detected, make the blinkies on
  point dir(get_direction(ch));
  if (dir.x != -2) curs += dir;
  else if (ch == '0') curs = orig;
  else if (ch == '\n') ret = curs;
  else if (ch == KEY_ESCAPE || ch == 'q' || ch == 'Q') ret = point(-1, -1);
  else if (ch == 'N') {
   timeout(-1);
   add_note(curs.x, curs.y, string_input_popup(49, "Enter note")); // 49 char max
   timeout(BLINK_SPEED);
  } else if(ch == 'D'){
   timeout(-1);
   if (has_note(curs.x, curs.y)){
    if (query_yn("Really delete note?")) delete_note(curs.x, curs.y);
   }
   timeout(BLINK_SPEED);
  } else if (ch == 'L'){
   timeout(-1);
   point p(display_notes());
   if (p.x != -1) curs = p;
   timeout(BLINK_SPEED);
   wrefresh(w_map);
  } else if (ch == '/') {
   point tmp(curs);
   timeout(-1);
   std::string term = string_input_popup("Search term:");
   timeout(BLINK_SPEED);
   draw(w_map, g, curs.x, curs.y, orig.x, orig.y, ch, blink);
   point found = find_note(curs, term);
   if (found.x == -1) {	// Didn't find a note
    std::vector<point> terlist(find_terrain(term));
    if (!terlist.empty()){
     int i = 0;
     //Navigate through results
     do {
      //Draw search box
      wborder(w_search, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
              LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
      mvwprintz(w_search, 1, 1, c_red, "Find place:");
      mvwprintz(w_search, 2, 1, c_ltblue, "                         ");
      mvwprintz(w_search, 2, 1, c_ltblue, "%s", term.c_str());
      mvwprintz(w_search, 4, 1, c_white,
       "'<' '>' Cycle targets.");
      mvwprintz(w_search, 10, 1, c_white, "Enter/Spacebar to select.");
      mvwprintz(w_search, 11, 1, c_white, "q to return.");
      ch = input();
      if (ch == ERR) blink = !blink;
      else if (ch == '<') {
       if(++i > terlist.size() - 1) i = 0;
      } else if(ch == '>'){
       if(--i < 0) i = terlist.size() - 1;
      }
      curs = terlist[i];
      draw(w_map, g, curs.x, curs.y, orig.x, orig.y, ch, blink);
      wrefresh(w_search);
      timeout(BLINK_SPEED);
     } while(ch != '\n' && ch != ' ' && ch != 'q'); 
     //If q is hit, return to the last position
     if(ch == 'q') curs = tmp;
     ch = '.';
    }
   }
   if (found.x != -1) curs = found;
  }/* else if (ch == 't')  *** Legend always on for now! ***
   legend = !legend;
*/
  else if (ch == ERR)	// Hit timeout on input, so make characters blink
   blink = !blink;
 } while (ch != KEY_ESCAPE && ch != 'q' && ch != 'Q' && ch != ' ' &&
          ch != '\n');
 timeout(-1);
 werase(w_map);
 wrefresh(w_map);
 delwin(w_map);
 werase(w_search);
 wrefresh(w_search);
 delwin(w_search);
 erase();
 g->refresh_all();
 return ret;
}

void overmap::first_house(int &x, int &y)
{
 std::vector<point> valid;
 for (int i = 0; i < OMAPX; i++) {
  for (int j = 0; j < OMAPY; j++) {
   if (ter(i, j) == ot_shelter)
    valid.push_back( point(i, j) );
  }
 }
 if (valid.size() == 0) {
  debugmsg("Couldn't find a shelter!");
  x = 1;
  y = 1;
  return;
 }
 int index = rng(0, valid.size() - 1);
 x = valid[index].x;
 y = valid[index].y;
}

void overmap::process_mongroups()
{
 for (int i = 0; i < zg.size(); i++) {
  if (zg[i].dying) {
   zg[i].population *= .8;
   zg[i].radius *= .9;
  }
 }
}
  
void overmap::place_forest()
{
 int x, y;
 int forx;
 int fory;
 int fors;
 for (int i = 0; i < NUM_FOREST; i++) {
// forx and fory determine the epicenter of the forest
  forx = rng(0, OMAPX - 1);
  fory = rng(0, OMAPY - 1);
// fors determinds its basic size
  fors = rng(15, 40);
  for (int j = 0; j < cities.size(); j++) {
   while (dist(forx,fory,cities[j].x,cities[j].y) - fors / 2 < cities[j].s ) {
// Set forx and fory far enough from cities
    forx = rng(0, OMAPX - 1);
    fory = rng(0, OMAPY - 1);
// Set fors to determine the size of the forest; usually won't overlap w/ cities
    fors = rng(15, 40);
    j = 0;
   }
  } 
  int swamps = SWAMPINESS;	// How big the swamp may be...
  x = forx;
  y = fory;
// Depending on the size on the forest...
  for (int j = 0; j < fors; j++) {
   int swamp_chance = 0;
   for (int k = -2; k <= 2; k++) {
    for (int l = -2; l <= 2; l++) {
     if (ter(x + k, y + l) == ot_forest_water ||
         (ter(x+k, y+l) >= ot_river_center && ter(x+k, y+l) <= ot_river_nw))
      swamp_chance += 5;
    }  
   }
   bool swampy = false;
   if (swamps > 0 && swamp_chance > 0 && !one_in(swamp_chance) &&
       (ter(x, y) == ot_forest || ter(x, y) == ot_forest_thick ||
        ter(x, y) == ot_field  || one_in(SWAMPCHANCE))) {
// ...and make a swamp.
    ter(x, y) = ot_forest_water;
    swampy = true;
    swamps--;
   } else if (swamp_chance == 0)
    swamps = SWAMPINESS;
   if (ter(x, y) == ot_field)
    ter(x, y) = ot_forest;
   else if (ter(x, y) == ot_forest)
    ter(x, y) = ot_forest_thick;

   if (swampy && (ter(x, y-1) == ot_field || ter(x, y-1) == ot_forest))
    ter(x, y-1) = ot_forest_water;
   else if (ter(x, y-1) == ot_forest)
    ter(x, y-1) = ot_forest_thick;
   else if (ter(x, y-1) == ot_field)
    ter(x, y-1) = ot_forest;

   if (swampy && (ter(x, y+1) == ot_field || ter(x, y+1) == ot_forest))
    ter(x, y+1) = ot_forest_water;
   else if (ter(x, y+1) == ot_forest)
     ter(x, y+1) = ot_forest_thick;
   else if (ter(x, y+1) == ot_field)
     ter(x, y+1) = ot_forest;

   if (swampy && (ter(x-1, y) == ot_field || ter(x-1, y) == ot_forest))
    ter(x-1, y) = ot_forest_water;
   else if (ter(x-1, y) == ot_forest)
    ter(x-1, y) = ot_forest_thick;
   else if (ter(x-1, y) == ot_field)
     ter(x-1, y) = ot_forest;

   if (swampy && (ter(x+1, y) == ot_field || ter(x+1, y) == ot_forest))
    ter(x+1, y) = ot_forest_water;
   else if (ter(x+1, y) == ot_forest)
    ter(x+1, y) = ot_forest_thick;
   else if (ter(x+1, y) == ot_field)
    ter(x+1, y) = ot_forest;
// Random walk our forest
   x += rng(-2, 2);
   if (x < 0    ) x = 0;
   if (x > OMAPX) x = OMAPX;
   y += rng(-2, 2);
   if (y < 0    ) y = 0;
   if (y > OMAPY) y = OMAPY;
  }
 }
}

void overmap::place_river(point pa, point pb)
{
 int x = pa.x, y = pa.y;
 do {
  x += rng(-1, 1);
  y += rng(-1, 1);
  if (x < 0) x = 0;
  if (x > OMAPX - 1) x = OMAPX - 2;
  if (y < 0) y = 0;
  if (y > OMAPY - 1) y = OMAPY - 1;
  for (int i = -1; i <= 1; i++) {
   for (int j = -1; j <= 1; j++) {
    if (y+i >= 0 && y+i < OMAPY && x+j >= 0 && x+j < OMAPX)
     ter(x+j, y+i) = ot_river_center;
   }
  }
  if (pb.x > x && (rng(0, int(OMAPX * 1.2) - 1) < pb.x - x ||
                   (rng(0, int(OMAPX * .2) - 1) > pb.x - x &&
                    rng(0, int(OMAPY * .2) - 1) > abs(pb.y - y))))
   x++;
  if (pb.x < x && (rng(0, int(OMAPX * 1.2) - 1) < x - pb.x ||
                   (rng(0, int(OMAPX * .2) - 1) > x - pb.x &&
                    rng(0, int(OMAPY * .2) - 1) > abs(pb.y - y))))
   x--;
  if (pb.y > y && (rng(0, int(OMAPY * 1.2) - 1) < pb.y - y ||
                   (rng(0, int(OMAPY * .2) - 1) > pb.y - y &&
                    rng(0, int(OMAPX * .2) - 1) > abs(x - pb.x))))
   y++;
  if (pb.y < y && (rng(0, int(OMAPY * 1.2) - 1) < y - pb.y ||
                   (rng(0, int(OMAPY * .2) - 1) > y - pb.y &&
                    rng(0, int(OMAPX * .2) - 1) > abs(x - pb.x))))
   y--;
  x += rng(-1, 1);
  y += rng(-1, 1);
  if (x < 0) x = 0;
  if (x > OMAPX - 1) x = OMAPX - 2;
  if (y < 0) y = 0;
  if (y > OMAPY - 1) y = OMAPY - 1;
  for (int i = -1; i <= 1; i++) {
   for (int j = -1; j <= 1; j++) {
// We don't want our riverbanks touching the edge of the map for many reasons
    if ((y+i >= 1 && y+i < OMAPY - 1 && x+j >= 1 && x+j < OMAPX - 1) ||
// UNLESS, of course, that's where the river is headed!
        (abs(pb.y - (y+i)) < 4 && abs(pb.x - (x+j)) < 4))
     ter(x+j, y+i) = ot_river_center;
   }
  }
 } while (pb.x != x || pb.y != y);
}

void overmap::place_cities(std::vector<city> &cities, int min)
{
 int NUM_CITIES = dice(2, 7) + rng(min, min + 4);
 int cx, cy, cs;
 int start_dir;

 for (int i = 0; i < NUM_CITIES; i++) {
  cx = rng(20, OMAPX - 41);
  cy = rng(20, OMAPY - 41);
  cs = rng(4, 17);
  if (ter(cx, cy) == ot_field) {
   ter(cx, cy) = ot_road_nesw;
   city tmp; tmp.x = cx; tmp.y = cy; tmp.s = cs;
   cities.push_back(tmp);
   start_dir = rng(0, 3);
   for (int j = 0; j < 4; j++)
    make_road(cx, cy, cs, (start_dir + j) % 4, tmp);
  }
 }
}

void overmap::put_buildings(int x, int y, int dir, city town)
{
 int ychange = dir % 2, xchange = (dir + 1) % 2;
 for (int i = -1; i <= 1; i += 2) {
  if ((ter(x+i*xchange, y+i*ychange) == ot_field) && !one_in(STREETCHANCE)) {
   if (rng(0, 99) > 80 * dist(x,y,town.x,town.y) / town.s)
    ter(x+i*xchange, y+i*ychange) = shop(((dir%2)-i)%4);
   else {
    if (rng(0, 99) > 130 * dist(x, y, town.x, town.y) / town.s)
     ter(x+i*xchange, y+i*ychange) = ot_park;
    else
     ter(x+i*xchange, y+i*ychange) = house(((dir%2)-i)%4);
   }
  }
 }
}

void overmap::make_road(int cx, int cy, int cs, int dir, city town)
{
 int x = cx, y = cy;
 int c = cs, croad = cs;
 switch (dir) {
 case 0:
  while (c > 0 && y > 0 && (ter(x, y-1) == ot_field || c == cs)) {
   y--;
   c--;
   ter(x, y) = ot_road_ns;
   for (int i = -1; i <= 0; i++) {
    for (int j = -1; j <= 1; j++) {
     if (abs(j) != abs(i) && (ter(x+j, y+i) == ot_road_ew ||
                              ter(x+j, y+i) == ot_road_ns)) {
      ter(x, y) = ot_road_null;
      c = -1;
     }
    }
   }
   put_buildings(x, y, dir, town);
   if (c < croad - 1 && c >= 2 && ter(x - 1, y) == ot_field &&
                                  ter(x + 1, y) == ot_field) {
    croad = c;
    make_road(x, y, cs - rng(1, 3), 1, town);
    make_road(x, y, cs - rng(1, 3), 3, town);
   }
  }
  if (is_road(x, y-2))
   ter(x, y-1) = ot_road_ns;
  break;
 case 1:
  while (c > 0 && x < OMAPX-1 && (ter(x+1, y) == ot_field || c == cs)) {
   x++;
   c--;
   ter(x, y) = ot_road_ew;
   for (int i = -1; i <= 1; i++) {
    for (int j = 0; j <= 1; j++) {
     if (abs(j) != abs(i) && (ter(x+j, y+i) == ot_road_ew ||
                              ter(x+j, y+i) == ot_road_ns)) {
      ter(x, y) = ot_road_null;
      c = -1;
     }
    }
   }
   put_buildings(x, y, dir, town);
   if (c < croad-2 && c >= 3 && ter(x, y-1) == ot_field &&
                                ter(x, y+1) == ot_field) {
    croad = c;
    make_road(x, y, cs - rng(1, 3), 0, town);
    make_road(x, y, cs - rng(1, 3), 2, town);
   }
  }
  if (is_road(x-2, y))
   ter(x-1, y) = ot_road_ew;
  break;
 case 2:
  while (c > 0 && y < OMAPY-1 && (ter(x, y+1) == ot_field || c == cs)) {
   y++;
   c--;
   ter(x, y) = ot_road_ns;
   for (int i = 0; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
     if (abs(j) != abs(i) && (ter(x+j, y+i) == ot_road_ew ||
                              ter(x+j, y+i) == ot_road_ns)) {
      ter(x, y) = ot_road_null;
      c = -1;
     }
    }
   }
   put_buildings(x, y, dir, town);
   if (c < croad-2 && ter(x-1, y) == ot_field && ter(x+1, y) == ot_field) {
    croad = c;
    make_road(x, y, cs - rng(1, 3), 1, town);
    make_road(x, y, cs - rng(1, 3), 3, town);
   }
  }
  if (is_road(x, y+2))
   ter(x, y+1) = ot_road_ns;
  break;
 case 3:
  while (c > 0 && x > 0 && (ter(x-1, y) == ot_field || c == cs)) {
   x--;
   c--;
   ter(x, y) = ot_road_ew;
   for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 0; j++) {
     if (abs(j) != abs(i) && (ter(x+j, y+i) == ot_road_ew ||
                              ter(x+j, y+i) == ot_road_ns)) {
      ter(x, y) = ot_road_null;
      c = -1;
     }
    }
   }
   put_buildings(x, y, dir, town);
   if (c < croad - 2 && c >= 3 && ter(x, y-1) == ot_field &&
       ter(x, y+1) == ot_field) {
    croad = c;
    make_road(x, y, cs - rng(1, 3), 0, town);
    make_road(x, y, cs - rng(1, 3), 2, town);
   }
  }
  if (is_road(x+2, y))
   ter(x+1, y) = ot_road_ew;
  break;
 }
 cs -= rng(1, 3);
 if (cs >= 2 && c == 0) {
  int dir2;
  if (dir % 2 == 0)
   dir2 = rng(0, 1) * 2 + 1;
  else
   dir2 = rng(0, 1) * 2;
  make_road(x, y, cs, dir2, town);
  if (one_in(5))
   make_road(x, y, cs, (dir2 + 2) % 4, town);
 }
}

void overmap::build_lab(int x, int y, int s)
{
 ter(x, y) = ot_lab;
 for (int n = 0; n <= 1; n++) {	// Do it in two passes to allow diagonals
  for (int i = 1; i <= s; i++) {
   for (int lx = x - i; lx <= x + i; lx++) {
    for (int ly = y - i; ly <= y + i; ly++) {
     if ((ter(lx - 1, ly) == ot_lab || ter(lx + 1, ly) == ot_lab ||
         ter(lx, ly - 1) == ot_lab || ter(lx, ly + 1) == ot_lab) &&
         one_in(i))
      ter(lx, ly) = ot_lab;
    }
   }
  }
 }
 ter(x, y) = ot_lab_core;
 int numstairs = 0;
 if (s > 1) {	// Build stairs going down
  while (!one_in(6)) {
   int stairx, stairy;
   int tries = 0;
   do {
    stairx = rng(x - s, x + s);
    stairy = rng(y - s, y + s);
    tries++;
   } while (ter(stairx, stairy) != ot_lab && tries < 15);
   if (tries < 15)
    ter(stairx, stairy) = ot_lab_stairs;
   numstairs++;
  }
 }
 if (numstairs == 0) {	// This is the bottom of the lab;  We need a finale
  int finalex, finaley;
  int tries = 0;
  do {
   finalex = rng(x - s, x + s);
   finaley = rng(y - s, y + s);
   tries++;
  } while (tries < 15 && ter(finalex, finaley) != ot_lab &&
                         ter(finalex, finaley) != ot_lab_core);
  ter(finalex, finaley) = ot_lab_finale;
 }
 zg.push_back(mongroup(mcat_lab, (x * 2), (y * 2), s, 400));
}

void overmap::build_anthill(int x, int y, int s)
{
 build_tunnel(x, y, s - rng(0, 3), 0);
 build_tunnel(x, y, s - rng(0, 3), 1);
 build_tunnel(x, y, s - rng(0, 3), 2);
 build_tunnel(x, y, s - rng(0, 3), 3);
 std::vector<point> queenpoints;
 for (int i = x - s; i <= x + s; i++) {
  for (int j = y - s; j <= y + s; j++) {
   if (ter(i, j) >= ot_ants_ns && ter(i, j) <= ot_ants_nesw)
    queenpoints.push_back(point(i, j));
  }
 }
 int index = rng(0, queenpoints.size() - 1);
 ter(queenpoints[index].x, queenpoints[index].y) = ot_ants_queen;
}

void overmap::build_tunnel(int x, int y, int s, int dir)
{
 if (s <= 0)
  return;
 if (ter(x, y) < ot_ants_ns || ter(x, y) > ot_ants_queen)
  ter(x, y) = ot_ants_ns;
 point next;
 switch (dir) {
  case 0: next = point(x    , y - 1);
  case 1: next = point(x + 1, y    );
  case 2: next = point(x    , y + 1);
  case 3: next = point(x - 1, y    );
 }
 if (s == 1)
  next = point(-1, -1);
 std::vector<point> valid;
 for (int i = x - 1; i <= x + 1; i++) {
  for (int j = y - 1; j <= y + 1; j++) {
   if ((ter(i, j) < ot_ants_ns || ter(i, j) > ot_ants_queen) &&
       abs(i - x) + abs(j - y) == 1)
    valid.push_back(point(i, j));
  }
 }
 for (int i = 0; i < valid.size(); i++) {
  if (valid[i].x != next.x || valid[i].y != next.y) {
   if (one_in(s * 2)) {
    if (one_in(2))
     ter(valid[i].x, valid[i].y) = ot_ants_food;
    else
     ter(valid[i].x, valid[i].y) = ot_ants_larvae;
   } else if (one_in(5)) {
    int dir2;
    if (valid[i].y == y - 1) dir2 = 0;
    if (valid[i].x == x + 1) dir2 = 1;
    if (valid[i].y == y + 1) dir2 = 2;
    if (valid[i].x == x - 1) dir2 = 3;
    build_tunnel(valid[i].x, valid[i].y, s - rng(0, 3), dir2);
   }
  }
 }
 build_tunnel(next.x, next.y, s - 1, dir);
}

void overmap::build_slimepit(int x, int y, int s)
{
 for (int n = 1; n <= s; n++) {
  for (int i = x - n; i <= x + n; i++) {
   for (int j = y - n; j <= y + n; j++) {
    if (rng(1, s * 2) >= n)
     ter(i, j) = (one_in(8) ? ot_slimepit_down : ot_slimepit);
    }
   }
 }
}

void overmap::build_mine(int x, int y, int s)
{
 bool finale = (s <= rng(1, 3));
 int built = 0;
 if (s < 2)
  s = 2;
 while (built < s) {
  ter(x, y) = ot_mine;
  std::vector<point> next;
  for (int i = -1; i <= 1; i += 2) {
   if (ter(x, y + i) == ot_rock)
    next.push_back( point(x, y + i) );
   if (ter(x + i, y) == ot_rock)
    next.push_back( point(x + i, y) );
  }
  if (next.empty()) { // Dead end!  Go down!
   ter(x, y) = (finale ? ot_mine_finale : ot_mine_down);
   return;
  }
  point p = next[ rng(0, next.size() - 1) ];
  x = p.x;
  y = p.y;
  built++;
 }
 ter(x, y) = (finale ? ot_mine_finale : ot_mine_down);
}

void overmap::place_rifts()
{
 int num_rifts = rng(0, 2) * rng(0, 2);
 std::vector<point> riftline;
 if (!one_in(4))
  num_rifts++;
 for (int n = 0; n < num_rifts; n++) {
  int x = rng(MAX_RIFT_SIZE, OMAPX - MAX_RIFT_SIZE);
  int y = rng(MAX_RIFT_SIZE, OMAPY - MAX_RIFT_SIZE);
  int xdist = rng(MIN_RIFT_SIZE, MAX_RIFT_SIZE),
      ydist = rng(MIN_RIFT_SIZE, MAX_RIFT_SIZE);
// We use rng(0, 10) as the t-value for this Bresenham Line, because by
// repeating this twice, we can get a thick line, and a more interesting rift.
  for (int o = 0; o < 3; o++) {
   if (xdist > ydist)
    riftline = line_to(x - xdist, y - ydist+o, x + xdist, y + ydist, rng(0,10));
   else
    riftline = line_to(x - xdist+o, y - ydist, x + xdist, y + ydist, rng(0,10));
   for (int i = 0; i < riftline.size(); i++) {
    if (i == riftline.size() / 2 && !one_in(3))
     ter(riftline[i].x, riftline[i].y) = ot_hellmouth;
    else
     ter(riftline[i].x, riftline[i].y) = ot_rift;
   }
  }
 }
}

void overmap::make_hiway(int x1, int y1, int x2, int y2, oter_id base)
{
 std::vector<point> next;
 int dir = 0;
 int x = x1, y = y1;
 int xdir, ydir;
 int tmp = 0;
 bool bridge_is_okay = false;
 bool found_road = false;
 do {
  next.clear(); // Clear list of valid points
  // Add valid points -- step in the right x-direction
  if (x2 > x)
   next.push_back(point(x + 1, y));
  else if (x2 < x)
   next.push_back(point(x - 1, y));
  else
   next.push_back(point(-1, -1)); // X is right--don't change it!
  // Add valid points -- step in the right y-direction
  if (y2 > y)
   next.push_back(point(x, y + 1));
  else if (y2 < y)
   next.push_back(point(x, y - 1));
  for (int i = 0; i < next.size(); i++) { // Take an existing road if we can
   if (next[i].x != -1 && is_road(base, next[i].x, next[i].y)) {
    x = next[i].x;
    y = next[i].y;
    dir = i; // We are moving... whichever way that highway is moving
// If we're closer to the destination than to the origin, this highway is done!
    if (dist(x, y, x1, y1) > dist(x, y, x2, y2))
     return;
    next.clear();
   } 
  }
  if (!next.empty()) { // Assuming we DIDN'T take an existing road...
   if (next[0].x == -1) { // X is correct, so we're taking the y-change
    dir = 1; // We are moving vertically
    x = next[1].x;
    y = next[1].y;
    if (is_river(ter(x, y)))
     ter(x, y) = ot_bridge_ns;
    else if (!is_road(base, x, y))
     ter(x, y) = base;
   } else if (next.size() == 1) { // Y must be correct, take the x-change
    if (dir == 1)
     ter(x, y) = base;
    dir = 0; // We are moving horizontally
    x = next[0].x;
    y = next[0].y;
    if (is_river(ter(x, y)))
     ter(x, y) = ot_bridge_ew;
    else if (!is_road(base, x, y))
     ter(x, y) = base;
   } else {	// More than one eligable route; pick one randomly
    if (one_in(12) &&
       !is_river(ter(next[(dir + 1) % 2].x, next[(dir + 1) % 2].y)))
     dir = (dir + 1) % 2; // Switch the direction (hori/vert) in which we move
    x = next[dir].x;
    y = next[dir].y;
    if (dir == 0) {	// Moving horizontally
     if (is_river(ter(x, y))) {
      xdir = -1;
      bridge_is_okay = true;
      if (x2 > x)
       xdir = 1;
      tmp = x;
      while (is_river(ter(tmp, y))) {
       if (is_road(base, tmp, y))
        bridge_is_okay = false;	// Collides with another bridge!
       tmp += xdir;
      }
      if (bridge_is_okay) {
       while(is_river(ter(x, y))) {
        ter(x, y) = ot_bridge_ew;
        x += xdir;
       }
       ter(x, y) = base;
      }
     } else if (!is_road(base, x, y))
      ter(x, y) = base;
    } else {		// Moving vertically
     if (is_river(ter(x, y))) {
      ydir = -1;
      bridge_is_okay = true;
      if (y2 > y)
       ydir = 1;
      tmp = y;
      while (is_river(ter(x, tmp))) {
       if (is_road(base, x, tmp))
        bridge_is_okay = false;	// Collides with another bridge!
       tmp += ydir;
      }
      if (bridge_is_okay) {
       while (is_river(ter(x, y))) {
        ter(x, y) = ot_bridge_ns;
        y += ydir;
       }
       ter(x, y) = base;
      }
     } else if (!is_road(base, x, y))
      ter(x, y) = base;
    }
   }
/*
   if (one_in(50) && posz == 0)
    building_on_hiway(x, y, dir);
*/
  }
  found_road = (
        ((ter(x, y - 1) > ot_road_null && ter(x, y - 1) < ot_river_center) ||
         (ter(x, y + 1) > ot_road_null && ter(x, y + 1) < ot_river_center) ||
         (ter(x - 1, y) > ot_road_null && ter(x - 1, y) < ot_river_center) ||
         (ter(x + 1, y) > ot_road_null && ter(x + 1, y) < ot_river_center)  ) &&
        rl_dist(x, y, x1, y2) > rl_dist(x, y, x2, y2));
 } while ((x != x2 || y != y2) && !found_road);
}

void overmap::building_on_hiway(int x, int y, int dir)
{
 int xdif = dir * (1 - 2 * rng(0,1));
 int ydif = (1 - dir) * (1 - 2 * rng(0,1));
 int rot = 0;
      if (ydif ==  1)
  rot = 0;
 else if (xdif == -1)
  rot = 1;
 else if (ydif == -1)
  rot = 2;
 else if (xdif ==  1)
  rot = 3;

 switch (rng(1, 3)) {
 case 1:
  if (!is_river(ter(x + xdif, y + ydif)))
   ter(x + xdif, y + ydif) = ot_lab_stairs;
  break;
 case 2:
  if (!is_river(ter(x + xdif, y + ydif)))
   ter(x + xdif, y + ydif) = house(rot);
  break;
 case 3:
  if (!is_river(ter(x + xdif, y + ydif)))
   ter(x + xdif, y + ydif) = ot_radio_tower;
  break;
/*
 case 4:
  if (!is_river(ter(x + xdif, y + ydif)))
   ter(x + xdir, y + ydif) = ot_sewage_treatment;
  break;
*/
 }
}

void overmap::place_hiways(std::vector<city> cities, oter_id base)
{
 if (cities.size() == 1)
  return;
 city best;
 int closest = -1;
 int distance;
 bool maderoad = false;
 for (int i = 0; i < cities.size(); i++) {
  maderoad = false;
  closest = -1;
  for (int j = i + 1; j < cities.size(); j++) {
   distance = dist(cities[i].x, cities[i].y, cities[j].x, cities[j].y);
   if (distance < closest || closest < 0) {
    closest = distance; 
    best = cities[j];
   }
   if (distance < TOP_HIWAY_DIST) {
    maderoad = true;
    make_hiway(cities[i].x, cities[i].y, cities[j].x, cities[j].y, base);
   }
  }
  if (!maderoad && closest > 0)
   make_hiway(cities[i].x, cities[i].y, best.x, best.y, base);
 }
}

// Polish does both good_roads and good_rivers (and any future polishing) in
// a single loop; much more efficient
void overmap::polish(oter_id min, oter_id max)
{
// Main loop--checks roads and rivers that aren't on the borders of the map
 for (int x = 0; x < OMAPX; x++) {
  for (int y = 0; y < OMAPY; y++) {
   if (ter(x, y) >= min && ter(x, y) <= max) {
    if (ter(x, y) >= ot_road_null && ter(x, y) <= ot_road_nesw)
     good_road(ot_road_ns, x, y);
    else if (ter(x, y) >= ot_bridge_ns && ter(x, y) <= ot_bridge_ew &&
             ter(x - 1, y) >= ot_bridge_ns && ter(x - 1, y) <= ot_bridge_ew &&
             ter(x + 1, y) >= ot_bridge_ns && ter(x + 1, y) <= ot_bridge_ew &&
             ter(x, y - 1) >= ot_bridge_ns && ter(x, y - 1) <= ot_bridge_ew &&
             ter(x, y + 1) >= ot_bridge_ns && ter(x, y + 1) <= ot_bridge_ew)
     ter(x, y) = ot_road_nesw;
    else if (ter(x, y) >= ot_subway_ns && ter(x, y) <= ot_subway_nesw)
     good_road(ot_subway_ns, x, y);
    else if (ter(x, y) >= ot_sewer_ns && ter(x, y) <= ot_sewer_nesw)
     good_road(ot_sewer_ns, x, y);
    else if (ter(x, y) >= ot_ants_ns && ter(x, y) <= ot_ants_nesw)
     good_road(ot_ants_ns, x, y);
    else if (ter(x, y) >= ot_river_center && ter(x, y) < ot_river_nw)
     good_river(x, y);
// Sometimes a bridge will start at the edge of a river, and this looks ugly
// So, fix it by making that square normal road; bit of a kludge but it works
    else if (ter(x, y) == ot_bridge_ns &&
             (!is_river(ter(x - 1, y)) || !is_river(ter(x + 1, y))))
     ter(x, y) = ot_road_ns;
    else if (ter(x, y) == ot_bridge_ew &&
             (!is_river(ter(x, y - 1)) || !is_river(ter(x, y + 1))))
     ter(x, y) = ot_road_ew;
   }
  }
 }
// Fixes stretches of parallel roads--turns them into two-lane highways
// Note that this fixes 2x2 areas... a "tail" of 1x2 parallel roads may be left.
// This can actually be a good thing; it ensures nice connections
// Also, this leaves, say, 3x3 areas of road.  TODO: fix this?  courtyards etc?
 for (int y = 0; y < OMAPY - 1; y++) {
  for (int x = 0; x < OMAPX - 1; x++) {
   if (ter(x, y) >= min && ter(x, y) <= max) {
    if (ter(x, y) == ot_road_nes && ter(x+1, y) == ot_road_nsw &&
        ter(x, y+1) == ot_road_nes && ter(x+1, y+1) == ot_road_nsw) {
     ter(x, y) = ot_hiway_ns;
     ter(x+1, y) = ot_hiway_ns;
     ter(x, y+1) = ot_hiway_ns;
     ter(x+1, y+1) = ot_hiway_ns;
    } else if (ter(x, y) == ot_road_esw && ter(x+1, y) == ot_road_esw &&
               ter(x, y+1) == ot_road_new && ter(x+1, y+1) == ot_road_new) {
     ter(x, y) = ot_hiway_ew;
     ter(x+1, y) = ot_hiway_ew;
     ter(x, y+1) = ot_hiway_ew;
     ter(x+1, y+1) = ot_hiway_ew;
    }
   }
  }
 }
}

bool overmap::is_road(int x, int y)
{
 if (ter(x, y) == ot_rift || ter(x, y) == ot_hellmouth)
  return true;
 if (x < 0 || x >= OMAPX || y < 0 || y >= OMAPY) {
  for (int i = 0; i < roads_out.size(); i++) {
   if (abs(roads_out[i].x - x) + abs(roads_out[i].y - y) <= 1)
    return true;
  }
 }
 if ((ter(x, y) >= ot_road_null && ter(x, y) <= ot_bridge_ew) ||
     (ter(x, y) >= ot_subway_ns && ter(x, y) <= ot_subway_nesw) ||
     (ter(x, y) >= ot_sewer_ns  && ter(x, y) <= ot_sewer_nesw) ||
     ter(x, y) == ot_sewage_treatment_hub ||
     ter(x, y) == ot_sewage_treatment_under)
  return true;
 return false;
}

bool overmap::is_road(oter_id base, int x, int y)
{
 oter_id min, max;
        if (base >= ot_road_null && base <= ot_bridge_ew) {
  min = ot_road_null;
  max = ot_bridge_ew;
 } else if (base >= ot_subway_ns && base <= ot_subway_nesw) {
  min = ot_subway_station;
  max = ot_subway_nesw;
 } else if (base >= ot_sewer_ns && base <= ot_sewer_nesw) {
  min = ot_sewer_ns;
  max = ot_sewer_nesw;
  if (ter(x, y) == ot_sewage_treatment_hub ||
      ter(x, y) == ot_sewage_treatment_under )
   return true;
 } else if (base >= ot_ants_ns && base <= ot_ants_queen) {
  min = ot_ants_ns;
  max = ot_ants_queen;
 } else	{ // Didn't plan for this!
  debugmsg("Bad call to is_road, %s", oter_t::list[base].name.c_str());
  return false;
 }
 if (x < 0 || x >= OMAPX || y < 0 || y >= OMAPY) {
  for (int i = 0; i < roads_out.size(); i++) {
   if (abs(roads_out[i].x - x) + abs(roads_out[i].y - y) <= 1)
    return true;
  }
 }
 if (ter(x, y) >= min && ter(x, y) <= max)
  return true;
 return false;
}

void overmap::good_road(oter_id base, int x, int y)
{
 int d = ot_road_ns;
 if (is_road(base, x, y-1)) {
  if (is_road(base, x+1, y)) { 
   if (is_road(base, x, y+1)) {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_nesw - d);
    else
     ter(x, y) = oter_id(base + ot_road_nes - d);
   } else {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_new - d);
    else
     ter(x, y) = oter_id(base + ot_road_ne - d);
   } 
  } else {
   if (is_road(base, x, y+1)) {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_nsw - d);
    else
     ter(x, y) = oter_id(base + ot_road_ns - d);
   } else {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_wn - d);
    else
     ter(x, y) = oter_id(base + ot_road_ns - d);
   } 
  }
 } else {
  if (is_road(base, x+1, y)) { 
   if (is_road(base, x, y+1)) {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_esw - d);
    else
     ter(x, y) = oter_id(base + ot_road_es - d);
   } else
    ter(x, y) = oter_id(base + ot_road_ew - d);
  } else {
   if (is_road(base, x, y+1)) {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_sw - d);
    else
     ter(x, y) = oter_id(base + ot_road_ns - d);
   } else {
    if (is_road(base, x-1, y))
     ter(x, y) = oter_id(base + ot_road_ew - d);
    else {// No adjoining roads/etc. Happens occasionally, esp. with sewers.
     ter(x, y) = oter_id(base + ot_road_nesw - d);
    }
   } 
  }
 }
 if (ter(x, y) == ot_road_nesw && one_in(4))
  ter(x, y) = ot_road_nesw_manhole;
}

void overmap::good_river(int x, int y)
{
 if (is_river(ter(x - 1, y))) {
  if (is_river(ter(x, y - 1))) {
   if (is_river(ter(x, y + 1))) {
    if (is_river(ter(x + 1, y))) {
// River on N, S, E, W; but we might need to take a "bite" out of the corner
     if (!is_river(ter(x - 1, y - 1)))
      ter(x, y) = ot_river_c_not_nw;
     else if (!is_river(ter(x + 1, y - 1)))
      ter(x, y) = ot_river_c_not_ne;
     else if (!is_river(ter(x - 1, y + 1)))
      ter(x, y) = ot_river_c_not_sw;
     else if (!is_river(ter(x + 1, y + 1)))
      ter(x, y) = ot_river_c_not_se;
     else
      ter(x, y) = ot_river_center;
    } else
     ter(x, y) = ot_river_east;
   } else {
    if (is_river(ter(x + 1, y)))
     ter(x, y) = ot_river_south;
    else
     ter(x, y) = ot_river_se;
   }
  } else {
   if (is_river(ter(x, y + 1))) {
    if (is_river(ter(x + 1, y)))
     ter(x, y) = ot_river_north;
    else
     ter(x, y) = ot_river_ne;
   } else {
    if (is_river(ter(x + 1, y))) // Means it's swampy
     ter(x, y) = ot_forest_water;
   }
  }
 } else {
  if (is_river(ter(x, y - 1))) {
   if (is_river(ter(x, y + 1))) {
    if (is_river(ter(x + 1, y)))
     ter(x, y) = ot_river_west;
    else // Should never happen
     ter(x, y) = ot_forest_water;
   } else {
    if (is_river(ter(x + 1, y)))
     ter(x, y) = ot_river_sw;
    else // Should never happen
     ter(x, y) = ot_forest_water;
   }
  } else {
   if (is_river(ter(x, y + 1))) {
    if (is_river(ter(x + 1, y)))
     ter(x, y) = ot_river_nw;
    else // Should never happen
     ter(x, y) = ot_forest_water;
   } else // Should never happen
    ter(x, y) = ot_forest_water;
  }
 }
}

void overmap::place_specials()
{
 int placed[NUM_OMSPECS];
 for (int i = 0; i < NUM_OMSPECS; i++)
  placed[i] = 0;

 std::vector<point> sectors;
 for (int x = 0; x < OMAPX; x += OMSPEC_FREQ) {
  for (int y = 0; y < OMAPY; y += OMSPEC_FREQ)
   sectors.push_back(point(x, y));
 }

 while (!sectors.empty()) {
  int sector_pick = rng(0, sectors.size() - 1);
  int x = sectors[sector_pick].x, y = sectors[sector_pick].y;
  sectors.erase(sectors.begin() + sector_pick);
  std::vector<omspec_id> valid;
  int tries = 0;
  point p;
  do {
   p = point(rng(x, x + OMSPEC_FREQ - 1), rng(y, y + OMSPEC_FREQ - 1));
   if (p.x >= OMAPX - 1)
    p.x = OMAPX - 2;
   if (p.y >= OMAPY - 1)
    p.y = OMAPY - 2;
   if (p.x == 0)
    p.x = 1;
   if (p.y == 0)
    p.y = 1;
   for (int i = 0; i < NUM_OMSPECS; i++) {
    overmap_special special = overmap_special::specials[i];
    int min = special.min_dist_from_city, max = special.max_dist_from_city;
    if ((placed[i] < special.max_appearances || special.max_appearances <= 0) &&
        (min == -1 || dist_from_city(p) >= min) &&
        (max == -1 || dist_from_city(p) <= max) &&
        (special.able)(this, p))
     valid.push_back( omspec_id(i) );
   }
   tries++;
  } while (valid.empty() && tries < 15); // Done looking for valid spot

  if (tries < 15) { // We found a valid spot!
// Place the MUST HAVE ones first, to try and guarantee that they appear
   std::vector<omspec_id> must_place;
   for (int i = 0; i < valid.size(); i++) {
    if (placed[i] < overmap_special::specials[ valid[i] ].min_appearances)
     must_place.push_back(valid[i]);
   }
   if (must_place.empty()) {
    int selection = rng(0, valid.size() - 1);
    overmap_special special = overmap_special::specials[ valid[selection] ];
    placed[ valid[selection] ]++;
    place_special(special, p);
   } else {
    int selection = rng(0, must_place.size() - 1);
    overmap_special special = overmap_special::specials[ must_place[selection] ];
    placed[ must_place[selection] ]++;
    place_special(special, p);
   }
  } // Done with <Found a valid spot>

 } // Done picking sectors...
}

void overmap::place_special(overmap_special special, point p)
{
 bool rotated = false;
// First, place terrain...
 ter(p.x, p.y) = special.ter;
// Next, obey any special effects the flags might have
 if (special.flags & mfb(OMS_FLAG_ROTATE_ROAD)) {
  if (is_road(p.x, p.y - 1))
   rotated = true;
  else if (is_road(p.x + 1, p.y)) {
   ter(p.x, p.y) = oter_id( int(ter(p.x, p.y)) + 1);
   rotated = true;
  } else if (is_road(p.x, p.y + 1)) {
   ter(p.x, p.y) = oter_id( int(ter(p.x, p.y)) + 2);
   rotated = true;
  } else if (is_road(p.x - 1, p.y)) {
   ter(p.x, p.y) = oter_id( int(ter(p.x, p.y)) + 3);
   rotated = true;
  }
 }

 if (!rotated && special.flags & mfb(OMS_FLAG_ROTATE_RANDOM))
  ter(p.x, p.y) = oter_id( int(ter(p.x, p.y)) + rng(0, 3) );
  
 if (special.flags & mfb(OMS_FLAG_3X3)) {
  for (int x = -1; x <= 1; x++) {
   for (int y = -1; y <= 1; y++) {
    if (x == 0 && y == 0)
     y++; // Already handled
    point np(p.x + x, p.y + y);
    ter(np.x, np.y) = special.ter;
   }
  }
 }

 if (special.flags & mfb(OMS_FLAG_3X3_SECOND)) {
  int startx = p.x - 1, starty = p.y;
  if (is_road(p.x, p.y - 1)) { // Road to north
   startx = p.x - 1;
   starty = p.y;
  } else if (is_road(p.x + 1, p.y)) { // Road to east
   startx = p.x - 2;
   starty = p.y - 1;
  } else if (is_road(p.x, p.y + 1)) { // Road to south
   startx = p.x - 1;
   starty = p.y - 2;
  } else if (is_road(p.x - 1, p.y)) { // Road to west
   startx = p.x;
   starty = p.y - 1;
  }
  if (startx != -1) {
   for (int x = startx; x < startx + 3; x++) {
    for (int y = starty; y < starty + 3; y++)
     ter(x, y) = oter_id(special.ter + 1);
   }
   ter(p.x, p.y) = special.ter;
  }
 }

 if (special.flags & mfb(OMS_FLAG_BLOB)) {
  for (int x = -2; x <= 2; x++) {
   for (int y = -2; y <= 2; y++) {
    if (x == 0 && y == 0)
     y++; // Already handled
    point np(p.x + x, p.y + y);
    if (one_in(1 + abs(x) + abs(y)) && (special.able)(this, np))
     ter(p.x + x, p.y + y) = special.ter;
   }
  }
 }

 if (special.flags & mfb(OMS_FLAG_BIG)) {
  for (int x = -3; x <= 3; x++) {
   for (int y = -3; y <= 3; y++) {
    if (x == 0 && y == 0)
     y++; // Already handled
    point np(p.x + x, p.y + y);
    if ((special.able)(this, np))
     ter(p.x + x, p.y + y) = special.ter;
     ter(p.x + x, p.y + y) = special.ter;
   }
  }
 }

 if (special.flags & mfb(OMS_FLAG_ROAD)) {
  int closest = -1, distance = 999;
  for (int i = 0; i < cities.size(); i++) {
   int dist = rl_dist(p, cities[i].x, cities[i].y);
   if (dist < distance) {
    closest = i;
    distance = dist;
   }
  }
  make_hiway(p.x, p.y, cities[closest].x, cities[closest].y, ot_road_null);
 }

 if (special.flags & mfb(OMS_FLAG_PARKING_LOT)) {
  int closest = -1, distance = 999;
  for (int i = 0; i < cities.size(); i++) {
   int dist = rl_dist(p, cities[i].x, cities[i].y);
   if (dist < distance) {
    closest = i;
    distance = dist;
   }
  }
  ter(p.x, p.y - 1) = ot_s_lot;
  make_hiway(p.x, p.y - 1, cities[closest].x, cities[closest].y, ot_road_null);
 }

// Finally, place monsters if applicable
 if (special.monsters != mcat_null) {
  if (special.monster_pop_min == 0 || special.monster_pop_max == 0 ||
      special.monster_rad_min == 0 || special.monster_rad_max == 0   ) {
   debugmsg("Overmap special %s has bad spawn: pop(%d, %d) rad(%d, %d)",
            oter_t::list[special.ter].name.c_str(), special.monster_pop_min,
            special.monster_pop_max, special.monster_rad_min,
            special.monster_rad_max);
   return;
  }
       
  int population = rng(special.monster_pop_min, special.monster_pop_max);
  int radius     = rng(special.monster_rad_min, special.monster_rad_max);
  zg.push_back(
     mongroup(special.monsters, p.x * 2, p.y * 2, radius, population));
 }
}

void overmap::place_mongroups()
{
// Cities are full of zombies
 for (int i = 0; i < cities.size(); i++) {
  if (!one_in(16) || cities[i].s > 5)
   zg.push_back(
	mongroup(mcat_zombie, (cities[i].x * 2), (cities[i].y * 2),
	         int(cities[i].s * 2.5), cities[i].s * 80));
 }

// Figure out where swamps are, and place swamp monsters
 for (int x = 3; x < OMAPX - 3; x += 7) {
  for (int y = 3; y < OMAPY - 3; y += 7) {
   int swamp_count = 0;
   for (int sx = x - 3; sx <= x + 3; sx++) {
    for (int sy = y - 3; sy <= y + 3; sy++) {
     if (ter(sx, sy) == ot_forest_water)
      swamp_count += 2;
     else if (is_river(ter(sx, sy)))
      swamp_count++;
    }
   }
   if (swamp_count >= 25) // ~25% swamp or ~50% river
    zg.push_back(mongroup(mcat_swamp, x * 2, y * 2, 3,
                          rng(swamp_count * 8, swamp_count * 25)));
  }
 }

// Place the "put me anywhere" groups
 int numgroups = rng(0, 3);
 for (int i = 0; i < numgroups; i++) {
  zg.push_back(
	mongroup(mcat_worm, rng(0, OMAPX * 2 - 1), rng(0, OMAPY * 2 - 1),
	         rng(20, 40), rng(500, 1000)));
 }

// Forest groups cover the entire map
 zg.push_back(
	mongroup(mcat_forest, 0, OMAPY, OMAPY,
                 rng(2000, 12000)));
 zg.push_back(
	mongroup(mcat_forest, 0, OMAPY * 2 - 1, OMAPY,
                 rng(2000, 12000)));
 zg.push_back(
	mongroup(mcat_forest, OMAPX, 0, OMAPX,
                 rng(2000, 12000)));
 zg.push_back(
	mongroup(mcat_forest, OMAPX * 2 - 1, 0, OMAPX,
                 rng(2000, 12000)));
}

void overmap::place_radios()
{
 for (int i = 0; i < OMAPX; i++) {
  for (int j = 0; j < OMAPY; j++) {
   if (ter(i, j) == ot_radio_tower)
    radios.push_back(radio_tower(i*2, j*2, rng(80, 200),
   "This is the emergency broadcast system.  Please proceed quickly and calmly \
to your designated evacuation point."));
  }
 }
}

// \todo release block: overmap::save,load need at least partial JSON conversion
void overmap::save(const std::string& name, int x, int y, int z)
{
 std::stringstream plrfilename, terfilename;
 std::ofstream fout;
 plrfilename << "save/" << name << ".seen." << x << "." << y << "." << z;
 terfilename << "save/o." << x << "." << y << "." << z;
 fout.open(plrfilename.str().c_str());
 for (int j = 0; j < OMAPY; j++) {
  for (int i = 0; i < OMAPX; i++) {
   if (seen(i, j))
    fout << "1";
   else
    fout << "0";
  }
  fout << std::endl;
 }
 for(const auto& n : notes) fout << "N " << n << std::endl;
 fout.close();
 fout.open(terfilename.str().c_str(), std::ios_base::trunc);
 for (int j = 0; j < OMAPY; j++) {
  for (int i = 0; i < OMAPX; i++)
   fout << char(int(ter(i, j)) + 32);
 }
 fout << std::endl;
 for(const auto& zgroup : zg) fout << "Z " << zgroup << std::endl;
 for(const auto& c : cities) fout << "t " << c << std::endl;
 for(const auto& r : roads_out) fout << "R " << r << std::endl;
 for(const auto& r : radios) fout << "T " << r << std::endl;

 for (int i = 0; i < npcs.size(); i++)
  fout << "n " << npcs[i] << std::endl;

 fout.close();
}

void overmap::open(game *g)	// only called from constructor
{
 std::stringstream plrfilename, terfilename;
 std::ifstream fin;
 char datatype;
 int cx, cy, cs;
 city tmp;
 std::vector<item> npc_inventory;

 plrfilename << "save/" << g->u.name << ".seen." << pos.x << "." << pos.y << "." << pos.z;
 terfilename << "save/o." << pos.x << "." << pos.y << "." << pos.z;

 fin.open(terfilename.str().c_str());
 if (fin.is_open()) {
  for (int j = 0; j < OMAPY; j++) {
   for (int i = 0; i < OMAPX; i++) {
    ter(i, j) = oter_id(fin.get() - 32);
    if (ter(i, j) < 0 || ter(i, j) > num_ter_types)
     debugmsg("Loaded bad ter!  %s; ter %d",
              terfilename.str().c_str(), ter(i, j));
   }
  }
  // while the legacy ready can cope with "any order", the generation order is much stricter:
  // terrain data blob
  // Z: mongroup
  // t: cities
  // R: roads out
  // T: radios i.e. transmission towers
  // n: NPCs (with several sub-entries)
  int loading_stage = 0;
  while (fin >> datatype) {
   if (datatype == 'Z') {
	   zg.push_back(mongroup(fin));	// Monster group
	   loading_stage = 1;
   } else if (datatype == 't') {
	   cities.push_back(city(fin));		// City
	   loading_stage = 2;
   } else if (datatype == 'R') {
	   roads_out.push_back(city(fin, true));	// Road leading out
	   loading_stage = 3;
   }
   else if (datatype == 'T') {
	   radios.push_back(radio_tower(fin));	// Radio tower
	   loading_stage = 4;
   }
   else if (datatype == 'n') {	// NPC
/* When we start loading a new NPC, check to see if we've accumulated items for
   assignment to an NPC.
 */
    loading_stage = 5;
    if (!npc_inventory.empty() && !npcs.empty()) {
     npcs.back().inv.add_stack(npc_inventory);
     npc_inventory.clear();
    }
    npcs.push_back(npc(fin));
   } else if (datatype == 'I' || datatype == 'C' || datatype == 'W' ||
              datatype == 'w' || datatype == 'c') {
    if (npcs.empty()) {
     debugmsg("Overmap %d:%d:%d tried to load object data, without an NPC!", pos.x, pos.y, pos.z);
     std::string itemdata;
	 getline(fin, itemdata);	// flush the line so we don't corrupt out too badly
	 debugmsg(itemdata.c_str());
	} else {
     npc& last = npcs.back();
     switch (datatype) {
      case 'I': npc_inventory.push_back(item(fin));                 break;
      case 'C': npc_inventory.back().contents.push_back(item(fin)); break;
      case 'W': last.worn.push_back(item(fin));                    break;
      case 'w': last.weapon = item(fin);                           break;
      case 'c': last.weapon.contents.push_back(item(fin));         break;
     }
    }
   } else {
     debugmsg("Overmap %d:%d:%d tried to load unrecognized data!", pos.x, pos.y, pos.z);
     std::string itemdata;
	 getline(fin, itemdata);	// flush the line so we don't corrupt out too badly
	 debugmsg(itemdata.c_str());
   }
  }
// If we accrued an npc_inventory, assign it now
  if (!npc_inventory.empty() && !npcs.empty())
   npcs.back().inv.add_stack(npc_inventory);

// Private/per-character data
  fin.close();
  fin.open(plrfilename.str().c_str());
  if (fin.is_open()) {	// Load private seen data
   for (int j = 0; j < OMAPY; j++) {
    std::string dataline;
    getline(fin, dataline);
    for (int i = 0; i < OMAPX; i++) {
     if (dataline[i] == '1')
      seen(i, j) = true;
     else
      seen(i, j) = false;
    }
   }
   while (fin >> datatype) {	// Load private notes
    if (datatype == 'N') notes.push_back(om_note(fin));
   }
   fin.close();
  } else {
   for (int j = 0; j < OMAPY; j++) {
    for (int i = 0; i < OMAPX; i++)
    seen(i, j) = false;
   }
  }
 } else if (pos.z <= -1) {	// No map exists, and we are underground!
// Fetch the terrain above
  overmap* above = new overmap(g, pos.x, pos.y, pos.z + 1);
  generate_sub(above);
  save(g->u.name);
  delete above;
 } else {	// No map exists!  Prepare neighbors, and generate one.
  std::vector<overmap*> pointers;
// Fetch north and south
  for (int i = -1; i <= 1; i+=2) {
   std::stringstream tmpfilename;
   tmpfilename << "save/o." << pos.x << "." << pos.y + i << "." << pos.z;
   fin.open(tmpfilename.str().c_str());
   if (fin.is_open()) {
    fin.close();
    pointers.push_back(new overmap(g, pos.x, pos.y+i, pos.z));
   } else
    pointers.push_back(NULL);
  }
// Fetch east and west
  for (int i = -1; i <= 1; i+=2) {
   std::stringstream tmpfilename;
   tmpfilename << "save/o." << pos.x + i << "." << pos.y << "." << pos.z;
   fin.open(tmpfilename.str().c_str());
   if (fin.is_open()) {
    fin.close();
    pointers.push_back(new overmap(g, pos.x + i, pos.y, pos.z));
   } else
    pointers.push_back(NULL);
  }
// pointers looks like (north, south, west, east)
  generate(g, pointers[0], pointers[3], pointers[1], pointers[2]);
  for (int i = 0; i < 4; i++)
   delete pointers[i];
  save(g->u.name);
 }
}


// Overmap special placement functions

bool omspec_place::water(overmap *om, point p)
{
 oter_id ter = om->ter(p.x, p.y);
 return (ter >= ot_river_center && ter <= ot_river_nw);
}

bool omspec_place::land(overmap *om, point p)
{
 oter_id ter = om->ter(p.x, p.y);
 return (ter < ot_river_center || ter > ot_river_nw);
}

bool omspec_place::forest(overmap *om, point p)
{
 oter_id ter = om->ter(p.x, p.y);
 return (ter == ot_forest || ter == ot_forest_thick || ter == ot_forest_water);
}

bool omspec_place::wilderness(overmap *om, point p)
{
 oter_id ter = om->ter(p.x, p.y);
 return (ter == ot_forest || ter == ot_forest_thick || ter == ot_forest_water ||
         ter == ot_field);
}

bool omspec_place::by_highway(overmap *om, point p)
{
 oter_id north = om->ter(p.x, p.y - 1), east = om->ter(p.x + 1, p.y),
         south = om->ter(p.x, p.y + 1), west = om->ter(p.x - 1, p.y);

 return ((north == ot_hiway_ew || north == ot_road_ew) ||
         (east  == ot_hiway_ns || east  == ot_road_ns) ||
         (south == ot_hiway_ew || south == ot_road_ew) ||
         (west  == ot_hiway_ns || west  == ot_road_ns)   );
}
