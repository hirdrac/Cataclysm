#include "construction.h"
#include "submap.h"
#ifndef SOCRATES_DAIMON
#include "game.h"
#include "keypress.h"
#include "rng.h"
#include <stdexcept>
#endif

std::vector<constructable*> constructable::constructions; // The list of constructions

#ifndef SOCRATES_DAIMON
namespace construct // Construction functions.
{
	// Able if tile is empty
	static bool able_empty(const GPS_loc& loc) { return 2 == loc.move_cost(); }

	// Any window tile
	static bool able_window(const GPS_loc& loc)
	{
		const auto t = loc.ter();
		return t_window_frame == t || t_window_empty == t || t_window == t;
	}

	// Only intact windows
	static bool able_window_pane(const GPS_loc& loc) { return t_window == loc.ter(); }

	// Able if tile is broken window
	static bool able_broken_window(const GPS_loc& loc) { return t_window_frame == loc.ter(); }

	// Any door tile
	static bool able_door(const GPS_loc& loc)
	{
		const auto t = loc.ter();
		return t_door_c == t || t_door_b == t || t_door_o == t || t_door_locked == t;
	}

	// Broken door
	static bool able_door_broken(const GPS_loc& loc) { return t_door_b == loc.ter(); }

	// Able if tile is wall
	static bool able_wall(const GPS_loc& loc)
	{
		const auto t = loc.ter();
		return t_wall_h == t || t_wall_v == t || t_wall_wood == t;
	}

	// Only player-built walls
	static bool able_wall_wood(const GPS_loc& loc) { return t_wall_wood == loc.ter(); }

	// Flood-fill contained by walls
	static bool able_between_walls(map&, point);

	// Able if diggable terrain
	static bool able_dig(const GPS_loc& loc) { return is<diggable>(loc.ter()); }

	// Able only on pits
	static bool able_pit(const GPS_loc& loc) {
		return t_pit == loc.ter() /* || t_pit_shallow == loc.ter() */;
	}

	// Able on trees
	static bool able_tree(const GPS_loc& loc) { return t_tree == loc.ter(); }

	// Able on logs
	static bool able_log(const GPS_loc& loc) { return t_log == loc.ter(); }

#if FAIL
	// does not work as expected: type not recognized when passing to constructor in MSVC++
	template<class... terrains> bool able(const GPS_loc& loc) {
		static_assert(0 < sizeof...(terrains));
		const ter_id ter = loc.ter();
		const std::array scan({ terrains... });
		for(decltype(auto) x : scan) {
			if (ter == x) return true;
		}
		return false;
	}
#endif

	// Does anything special happen when we're finished?

	static void done_window_pane(player& u)
	{
		u.GPSpos.add(submap::for_drop(u.GPSpos.ter(), item::types[itm_glass_sheet], 0).value());
	}

	static void done_tree(GPS_loc p)
	{
		mvprintz(0, 0, c_red, "Press a direction for the tree to fall in:");
		point delta(-2, -2);
		do {
			delta = get_direction(input());
		} while (-2 == delta.x);
		(delta *= 3) += rng(within_rldist<1>);

		auto lines = lines_to(point(0, 0), delta);
		for (const auto& pt : lines[rng(0, lines.size() - 1)]) {
			auto dest = p + pt;
			dest.destroy(true);
			dest.ter() = t_log;
		}
	}

	static void done_vehicle(GPS_loc dest)
	{
		std::string name = string_input_popup(20, "Enter new vehicle name");
		if (auto veh = dest.add_vehicle(veh_custom, 270)) {
			veh->name = std::move(name);
			veh->install_part(0, 0, vp_frame_v2);
			return;
		}
		debugmsg("error constructing vehicle");
	}

	static void done_log(GPS_loc dest)
	{
		int num_sticks = rng(10, 20);
		for (int i = 0; i < num_sticks; i++)
			dest.add(item(item::types[itm_2x4], int(messages.turn)));
	}
};

static bool will_flood_stop(map *m, bool (&fill)[SEEX * MAPSIZE][SEEY * MAPSIZE],
                            int x, int y)
{
 if (x == 0 || y == 0 || x == SEEX * MAPSIZE - 1 || y == SEEY * MAPSIZE - 1)
  return false;

 fill[x][y] = true;
 bool skip_north = (fill[x][y - 1] || m->has_flag(supports_roof, x, y - 1)),
      skip_south = (fill[x][y + 1] || m->has_flag(supports_roof, x, y + 1)),
      skip_east  = (fill[x + 1][y] || m->has_flag(supports_roof, x + 1, y)),
      skip_west  = (fill[x - 1][y] || m->has_flag(supports_roof, x - 1, y));

 return ((skip_north || will_flood_stop(m, fill, x    , y - 1)) &&
         (skip_east  || will_flood_stop(m, fill, x + 1, y    )) &&
         (skip_south || will_flood_stop(m, fill, x    , y + 1)) &&
         (skip_west  || will_flood_stop(m, fill, x - 1, y    ))   );
}
#endif

void constructable::init()
{
 int id = -1;

/* CONSTRUCT( name, time, able, done )
 * Name is the name as it appears in the menu; 30 characters or less, please.
 * time is the time in MINUTES that it takes to finish this construction.
 *  note that 10 turns = 1 minute.
 * able is a function which returns true if you can build it on a given tile
 *  See construction.h for options, and this file for definitions.
 * done is a function which runs each time the construction finishes.
 *  This is useful, for instance, for removing the trap from a pit, or placing
 *  items after a deconstruction.
 */

#ifndef SOCRATES_DAIMON
#define HANDLERS(...) , __VA_ARGS__
#else
#define HANDLERS(...)
#endif

 constructions.push_back(new constructable(++id, "Dig Pit", 0 HANDLERS(&construct::able_dig)));
  constructions.back()->stages.push_back(construction_stage(t_pit_shallow, 10));
   constructions.back()->stages.back().tools.push_back({ itm_shovel });
  constructions.back()->stages.push_back(construction_stage(t_pit, 10));
   constructions.back()->stages.back().tools.push_back({ itm_shovel });

 constructions.push_back(new constructable(++id, "Spike Pit", 0 HANDLERS(&construct::able_pit)));
  constructions.back()->stages.push_back(construction_stage(t_pit_spiked, 5));
   constructions.back()->stages.back().components.push_back({ { itm_spear_wood, 4 } });

 constructions.push_back(new constructable(++id, "Fill Pit", 0 HANDLERS(&construct::able_pit)));
  constructions.back()->stages.push_back(construction_stage(t_pit_shallow, 5));
   constructions.back()->stages.back().tools.push_back({ itm_shovel});
  constructions.back()->stages.push_back(construction_stage(t_dirt, 5));
   constructions.back()->stages.back().tools.push_back({ itm_shovel});

 constructions.push_back(new constructable(++id, "Chop Down Tree", 0 HANDLERS(&construct::able_tree)));
  constructions.back()->stages.push_back(construction_stage(t_dirt, 10));
   constructions.back()->stages.back().tools.push_back({ itm_ax, itm_chainsaw_on});

 constructions.push_back(new constructable(++id, "Chop Up Log", 0 HANDLERS(&construct::able_log, &construct::done_log)));
  constructions.back()->stages.push_back(construction_stage(t_dirt, 20));
   constructions.back()->stages.back().tools.push_back({ itm_ax, itm_chainsaw_on});

 constructions.push_back(new constructable(++id, "Clean Broken Window", 0 HANDLERS(&construct::able_broken_window)));
  constructions.back()->stages.push_back(construction_stage(t_window_empty, 5));

 constructions.push_back(new constructable(++id, "Remove Window Pane",  1 HANDLERS(&construct::able_window_pane, &construct::done_window_pane)));
  constructions.back()->stages.push_back(construction_stage(t_window_empty, 10));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_rock, itm_hatchet});
   constructions.back()->stages.back().tools.push_back({ itm_screwdriver, itm_knife_butter, itm_toolset });

 constructions.push_back(new constructable(++id, "Repair Door", 1 HANDLERS(&construct::able_door_broken)));
  constructions.back()->stages.push_back(construction_stage(t_door_c, 10));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 3 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 12 } });

 constructions.push_back(new constructable(++id, "Board Up Door", 0 HANDLERS(&construct::able_door)));
  constructions.back()->stages.push_back(construction_stage(t_door_boarded, 8));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 4 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 8 } });

 constructions.push_back(new constructable(++id, "Board Up Window", 0 HANDLERS(&construct::able_window)));
  constructions.back()->stages.push_back(construction_stage(t_window_boarded, 5));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 4 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 8 } });

 constructions.push_back(new constructable(++id, "Build Wall", 2 HANDLERS(&construct::able_empty)));
  constructions.back()->stages.push_back(construction_stage(t_wall_half, 10));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 10 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 20 } });
  constructions.back()->stages.push_back(construction_stage(t_wall_wood, 10));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 10 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 20 } });

 constructions.push_back(new constructable(++id, "Build Window", 3 HANDLERS(&construct::able_wall_wood)));
  constructions.back()->stages.push_back(construction_stage(t_window_empty, 10));
   constructions.back()->stages.back().tools.push_back({ itm_saw });
  constructions.back()->stages.push_back(construction_stage(t_window, 5));
  constructions.back()->stages.back().components.push_back({ { itm_glass_sheet, 1 } });

 constructions.push_back(new constructable(++id, "Build Door", 4 HANDLERS(&construct::able_wall_wood)));
  constructions.back()->stages.push_back(construction_stage(t_door_frame, 15));
   constructions.back()->stages.back().tools.push_back({ itm_saw });
  constructions.back()->stages.push_back(construction_stage(t_door_b, 15));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 4 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 12 } });
  constructions.back()->stages.push_back(construction_stage(t_door_c, 15));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 4 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 12 } });

/*  Removed until we have some way of auto-aligning fences!
 constructions.push_back(new constructable(++id, "Build Fence", 1, 15, &construct::able_empty));
  constructions[id]->stages.push_back(construction_stage(t_fence_h, 10));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet });
   constructions.back()->stages.back().components({ { itm_2x4, 5, itm_nail, 8 } });
*/

 constructions.push_back(new constructable(++id, "Build Roof", 4 HANDLERS(&construct::able_between_walls)));
  constructions.back()->stages.push_back(construction_stage(t_floor, 40));
   constructions.back()->stages.back().tools.push_back({ itm_hammer, itm_hatchet, itm_nailgun });
   constructions.back()->stages.back().components.push_back({ { itm_2x4, 8 } });
   constructions.back()->stages.back().components.push_back({ { itm_nail, 40 } });

 constructions.push_back(new constructable(++id, "Start vehicle construction", 0 HANDLERS(&construct::able_empty, &construct::done_vehicle)));
  constructions.back()->stages.push_back(construction_stage(t_null, 10));
  constructions.back()->stages.back().components.push_back({ { itm_frame, 1 } });

}

#ifndef SOCRATES_DAIMON
bool construct::able_between_walls(map& m, point p)
{
 bool fill[SEEX * MAPSIZE][SEEY * MAPSIZE];
 for (int x = 0; x < SEEX * MAPSIZE; x++) {
  for (int y = 0; y < SEEY * MAPSIZE; y++)
   fill[x][y] = false;
 }

 return will_flood_stop(&m, fill, p.x, p.y);
}
#endif
