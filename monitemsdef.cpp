#include "mtype.h"
#include "output.h"

std::vector<items_location_and_chance> mtype::items[num_monsters];

void mtype::init_items()
{
	items[mon_ant] = { {mi_bugs,	1} };
	items[mon_ant_soldier] = items[mon_ant];
	items[mon_ant_queen] = items[mon_ant];

	items[mon_zombie] =
		{ { mi_livingroom,10 },{ mi_kitchen,8 },{ mi_bedroom,20 },{ mi_dresser,30 },
		  { mi_softdrugs,5 },{ mi_harddrugs,2 },{ mi_tools,6 },{ mi_trash,7 },
		  { mi_ammo,18 },{ mi_pistols,3 },{ mi_shotguns,2 },{ mi_smg,1 } };

	items[mon_zombie_shrieker] = items[mon_zombie];
	items[mon_zombie_spitter] = items[mon_zombie];
	items[mon_zombie_electric] = items[mon_zombie];
	items[mon_zombie_fast] = items[mon_zombie];
	items[mon_zombie_brute] = items[mon_zombie];
	items[mon_zombie_hulk] = items[mon_zombie];
	items[mon_zombie_fungus] = items[mon_zombie];
	items[mon_boomer] = items[mon_zombie];
	items[mon_boomer_fungus] = items[mon_zombie];
	items[mon_zombie_necro] = items[mon_zombie];
	items[mon_zombie_grabber] = items[mon_zombie];
	items[mon_zombie_master] = items[mon_zombie];

	items[mon_zombie_scientist] =
		{ { mi_dresser, 10 },   { mi_harddrugs, 6 },  { mi_chemistry, 10 },
		  { mi_teleport, 6 },   { mi_goo, 8 },        { mi_cloning_vat, 1 },
		  { mi_dissection, 10 },{ mi_electronics, 9 },{ mi_bionics, 1 },
		  { mi_radio, 2 },      { mi_textbooks, 3 } };

	items[mon_zombie_soldier] =
		{ { mi_dresser, 5 },   { mi_ammo, 10 },      { mi_pistols, 5 },
		  { mi_shotguns, 2 },  { mi_smg, 5 },        { mi_bots, 1 },
		  { mi_launchers, 2 }, { mi_mil_rifles, 10 },{ mi_grenades, 5 },
		  { mi_mil_armor, 14 },{ mi_mil_food, 5 },   { mi_bionics_mil, 1 } };

	items[mon_biollante] = { { mi_biollante, 1 } };

	items[mon_chud] =
		{ { mi_subway, 40}, { mi_sewer, 20 },{ mi_trash, 5 },{ mi_bedroom, 1},
		  { mi_dresser, 5 },{ mi_ammo, 18 } };
	items[mon_one_eye] = items[mon_chud];

	items[mon_bee] = { { mi_bees, 1 } };
	items[mon_wasp] = { { mi_wasps, 1 } };

	items[mon_dragonfly] = { { mi_bugs, 1 } };
	items[mon_centipede] = items[mon_dragonfly];
	items[mon_spider_wolf] = items[mon_dragonfly];
	items[mon_spider_web] = items[mon_dragonfly];
	items[mon_spider_jumping] = items[mon_dragonfly];
	items[mon_spider_trapdoor] = items[mon_dragonfly];
	items[mon_spider_widow] = items[mon_dragonfly];

	items[mon_eyebot] = { { mi_robots,4 }, { mi_ammo,1 } };
    items[mon_manhack]		= items[mon_eyebot];
    items[mon_skitterbot]	= items[mon_eyebot];
    items[mon_secubot]		= items[mon_eyebot];
    items[mon_copbot]		= items[mon_eyebot];
    items[mon_molebot]		= items[mon_eyebot];
    items[mon_tripod]		= items[mon_eyebot];
    items[mon_chickenbot]	= items[mon_eyebot];
    items[mon_tankbot]		= items[mon_eyebot];
    items[mon_turret]		= items[mon_eyebot];
	items[mon_exploder]		= items[mon_eyebot];

	// now that both monster types and items are set up, we can check the invariants
	size_t i = num_monsters;
	while (0 < --i) {
		const auto type = mtype::types[i];
		if (mtype::types[i]->item_chance && items[i].empty()) {
			debuglog("Type %s has item_chance %d but no items assigned!", type->name.c_str(), type->item_chance);
			exit(EXIT_FAILURE);
		}
	}
}
