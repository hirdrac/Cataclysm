#include "event.h"
#include "npc.h"
#include "game.h"
#include "line.h"
#include "rng.h"
#include "recent_msg.h"
#include "json.h"

#include <stdexcept>

std::vector<event> event::_events;

static const char* const JSON_transcode_events[] = {
    "HELP",
    "WANTED",
    "ROBOT_ATTACK",
    "SPAWN_WYRMS",
    "AMIGARA",
    "ROOTS_DIE",
    "TEMPLE_OPEN",
    "TEMPLE_FLOOD",
    "TEMPLE_SPAWN",
    "DIM",
    "ARTIFACT_LIGHT"
};

DEFINE_JSON_ENUM_SUPPORT_TYPICAL(event_type, JSON_transcode_events)

#define JSON_ENUM(TYPE)	\
cataclysm::JSON toJSON(TYPE src) {	\
	auto x = JSON_key(src);	\
	if (x) return cataclysm::JSON(x);	\
	throw std::runtime_error(std::string("encoding failure: " #TYPE " value ")+std::to_string((int)src));	\
}	\
	\
bool fromJSON(const cataclysm::JSON& src, TYPE& dest)	\
{	\
	if (!src.is_scalar()) return false;	\
	cataclysm::JSON_parse<TYPE> parse;	\
	dest = parse(src.scalar());	\
	return true;	\
}

JSON_ENUM(event_type)

cataclysm::JSON toJSON(const event& src) {
    cataclysm::JSON ret(cataclysm::JSON::object);
    point map_point;   // usage is against game::lev.x,y

    if (auto json = JSON_key(src.type)) {
        ret.set("type", json);
        ret.set("turn", std::to_string(src.turn));
        if (faction::MIN_ID <= src.faction_id) ret.set("faction", std::to_string(src.faction_id));
        if (src.map_point != point(-1, -1)) ret.set("map_point", toJSON(src.map_point));
    }

    return ret;
}

bool fromJSON(const cataclysm::JSON& src, event& dest)
{
    if (!src.has_key("turn") || !src.has_key("type")) return false;
    bool ret = fromJSON(src["type"], dest.type);
    if (!fromJSON(src["turn"], dest.turn)) ret = false;
    if (src.has_key("faction")) fromJSON(src["faction"], dest.faction_id);
    if (src.has_key("map_point")) fromJSON(src["map_point"], dest.map_point);
    return ret;
}

const event* event::queued(event_type type)
{
    for (decltype(auto) ev : _events) if (type == ev.type) return &ev;
    return nullptr;
}

void event::global_reset(const Badge<game>& badge) { _events.clear(); }

void event::global_fromJSON(const cataclysm::JSON& src)
{
    _events.clear();
    if (src.has_key("events")) src["events"].decode(_events);
}

void event::global_toJSON(cataclysm::JSON& dest)
{
    if (!_events.empty()) dest.set("events", cataclysm::JSON::encode(_events));
}

void event::process(const Badge<game>& badge)
{
    // We want to go forward, to allow for the possibility of events scheduling events for same-turn
    for (int i = 0; i < _events.size(); i++) {
        decltype(auto) e = _events[i];
        if (!e.per_turn()) {
            EraseAt(_events, i--);
            continue;
        }
        if (e.turn <= int(messages.turn)) {
            e.actualize();
            EraseAt(_events, i--);
            continue;
        }
    }
}

void event::actualize() const
{
 auto g = game::active();

 switch (type) {

  case EVENT_HELP: {
   int num = (0 <= faction_id) ? rng(1, 6) : 1;
   faction* fac = (0 <= faction_id) ? faction::from_id(faction_id) : nullptr;
   if (0 <= faction_id && !fac) debugmsg("EVENT_HELP run with invalid faction_id");
   for (int i = 0; i < num; i++) {
    npc tmp;
    tmp.randomize_from_faction(fac);
    tmp.attitude = NPCATT_DEFEND;
    point delta = point(2 * SEE) + rng(within_rldist<5>);
	tmp.screenpos_set(g->u.pos + delta); // XXX nowhere close to reality bubble edge
    g->spawn(std::move(tmp));
   }
  } break;

  case EVENT_ROBOT_ATTACK: {
   if (rl_dist(g->lev.x, g->lev.y, map_point) <= 4) {
    const mtype *robot_type = mtype::types[mon_tripod];
    if (faction_id == 0) // The cops!
     robot_type = mtype::types[mon_copbot];
    int robx = (g->lev.x > map_point.x ? 0 - SEEX * 2 : SEEX * 4),
        roby = (g->lev.y > map_point.y ? 0 - SEEY * 2 : SEEY * 4);
    g->spawn(monster(robot_type, robx, roby));
   }
  } break;

  case EVENT_SPAWN_WYRMS: {
      if (g->lev.z >= 0) return;

      static constexpr const zaimoni::gdi::box<point> spawn_zone(point(0), point(SEE * MAPSIZE));
      static std::function<point()> candidate = [&]() { return g->u.pos + rng(spawn_zone); };

      static std::function<bool(const point&)> ok = [&](const point& pt) {
          return g->is_empty(pt) && 2 < rl_dist(pt, g->u.pos);
      };

      int num_wyrms = rng(1, 4);
      for (int i = 0; i < num_wyrms; i++) {
          if (auto pt = LasVegasChoice(10, candidate, ok)) {
              g->spawn(monster(mtype::types[mon_dark_wyrm], *pt));
          }
      }
      if (!one_in(25)) // They just keep coming!
          event::add(event(EVENT_SPAWN_WYRMS, int(messages.turn) + TURNS(rng(15, 25))));
  } break;

  case EVENT_AMIGARA: {
   const auto fault = find_first(map::reality_bubble_extent, [&](point pt) { return t_fault == g->m.ter(pt);});
   if (!fault) {
       debuglog("Amigara event without faultline");
#ifndef NDEBUG
       throw std::logic_error("Amigara event without faultline");
#else
       debugmsg("Amigara event without faultline");
       return;
#endif
   }
   const bool horizontal = (t_fault == g->m.ter(*fault + Direction::W) || t_fault == g->m.ter(*fault + Direction::E));
   // ? : of two lambda functions hard-errors (incompatible types)
   auto amigara_position = horizontal ? std::function<point()>([&]() {
       point ret(*fault);
       ret.x += rng(0, 16); // C:Whales 2*SEE-8
       for (int n = -1; n <= 1; n++) {
           const int try_y = fault->y + n;
           if (t_rock_floor == g->m.ter(ret.x, try_y)) ret.y = try_y;
       }
       return ret;
   }) : [&]() { // vertical fault
       point ret(*fault);
       ret.y += rng(0, 16); // C:Whales 2*SEE-8
       for (int n = -1; n <= 1; n++) {
           const int try_x = fault->x + n;
           if (t_rock_floor == g->m.ter(try_x, ret.y)) ret.x = try_x;
       }
       return ret;
   };

   const int num_horrors = rng(3, 5);
   monster horror(mtype::types[mon_amigara_horror]);
   for (int i = 0; i < num_horrors; i++) {
       const auto mon = LasVegasChoice<point>(10, amigara_position, [&](const point& pt) { return t_rock_floor == g->m.ter(pt) && g->is_empty(pt); });
       if (mon) {
           horror.spawn(*mon);
           g->spawn(horror);
       }
   }
  } break;

  case EVENT_ROOTS_DIE:
   for (int x = 0; x < SEEX * MAPSIZE; x++) {
    for (int y = 0; y < SEEY * MAPSIZE; y++) {
	 auto& t = g->m.ter(x,y);
     if (t_root_wall == t && one_in(3)) t = t_underbrush;
    }
   }
   break;

  case EVENT_TEMPLE_OPEN: {
   bool saw_grate = false;
   for (int x = 0; x < SEEX * MAPSIZE; x++) {
    for (int y = 0; y < SEEY * MAPSIZE; y++) {
	 if (g->m.rewrite_test<t_grate, t_stairs_down>(x,y)) {
      if (!saw_grate && g->u.see(x, y)) saw_grate = true;
     }
    }
   }
   if (saw_grate) messages.add("The nearby grates open to reveal a staircase!");
  } break;

  case EVENT_TEMPLE_FLOOD: {
   bool flooded = false;
   map copy;
   for (int x = 0; x < SEEX * MAPSIZE; x++) {
    for (int y = 0; y < SEEY * MAPSIZE; y++)
     copy.ter(x, y) = g->m.ter(x, y);
   }
   for (int x = 0; x < SEEX * MAPSIZE; x++) {
    for (int y = 0; y < SEEY * MAPSIZE; y++) {
     if (g->m.ter(x, y) == t_water_sh) {
      bool deepen = false;
      for (int wx = x - 1;  wx <= x + 1 && !deepen; wx++) {
       for (int wy = y - 1;  wy <= y + 1 && !deepen; wy++) {
        if (g->m.ter(wx, wy) == t_water_dp) deepen = true;
       }
      }
      if (deepen) {
       copy.ter(x, y) = t_water_dp;
       flooded = true;
      }
     } else if (g->m.ter(x, y) == t_rock_floor) {
      bool flood = false;
      for (int wx = x - 1;  wx <= x + 1 && !flood; wx++) {
       for (int wy = y - 1;  wy <= y + 1 && !flood; wy++) {
        if (g->m.ter(wx, wy) == t_water_dp || g->m.ter(wx, wy) == t_water_sh) flood = true;
       }
      }
      if (flood) {
       copy.ter(x, y) = t_water_sh;
       flooded = true;
      }
     }
    }
   }
   if (!flooded) return; // We finished flooding the entire chamber!
// Check if we should print a message
   if (copy.ter(g->u.pos) != g->m.ter(g->u.pos)) {
    if (copy.ter(g->u.pos) == t_water_sh)
     messages.add("Water quickly floods up to your knees.");
    else { // Must be deep water!
     messages.add("Water fills nearly to the ceiling!");
     g->u.swim(g->u.GPSpos);
    }
   }
// copy is filled with correct tiles; now copy them back to g->m
   for (int x = 0; x < SEEX * MAPSIZE; x++) {
    for (int y = 0; y < SEEY * MAPSIZE; y++)
     g->m.ter(x, y) = copy.ter(x, y);
   }
   event::add(event(EVENT_TEMPLE_FLOOD, int(messages.turn) + TURNS(rng(2, 3))));
  } break;

  case EVENT_TEMPLE_SPAWN: {
      static constexpr const mon_id temple_spawn[] = { mon_sewer_snake, mon_centipede, mon_dermatik, mon_spider_widow };
      static constexpr const zaimoni::gdi::box<point> spawn_zone(point(-5), point(5));

      static std::function<point()> candidate = [&]() { return g->u.pos + rng(spawn_zone); };

      static std::function<bool(const point&)> ok = [&](const point& pt) {
          return g->is_empty(pt) && 2 < rl_dist(pt, g->u.pos);
      };

      if (auto pt = LasVegasChoice(20, candidate, ok)) {
          g->spawn(monster(mtype::types[temple_spawn[rng(0, (std::end(temple_spawn) - std::begin(temple_spawn)) - 1)]], *pt));
      }
  } break;

  default:
   break; // Nothing happens for other events
 }
}

// formerly map::random_outdoor_tile
static std::optional<GPS_loc> random_outdoor_tile(GPS_loc loc)
{
    static std::function<std::optional<GPS_loc>(point)> ok = [&](point delta) {
        auto test = loc + delta;
        if (test.is_outside()) return std::optional(test);
        return std::optional<decltype(test)>();
    };

    // C:Whales: reality bubble scope (11*SEE x 11*SEE but not strictly centered on player)
    auto options = grep(within_rldist<5 * SEE>, ok);

    if (options.empty()) return std::nullopt; // Nowhere is outdoors!
    return options[rng(0, options.size() - 1)];
}

bool event::per_turn()
{
 auto g = game::active();

 switch (type) {
  case EVENT_WANTED: {
   if (g->lev.z >= 0 && one_in(100)) { // About once every 10 minutes
       if (auto dest = random_outdoor_tile(g->u.GPSpos)) { // someplace outside, close enough
           if (auto pos = g->toScreen(*dest)) { // and within reality bubble
               monster eyebot(mtype::types[mon_eyebot], *pos);
               eyebot.faction_id = faction_id;
               g->spawn(std::move(eyebot));
               if (g->u.see(*dest)) messages.add("An eyebot swoops down nearby!");
               // \todo They shouldn't be coming *indefinitely*
               // but this event is up for complete re-implementation anyway
           }
       }
   }
  }
   return true;

  case EVENT_SPAWN_WYRMS:
   if (g->lev.z >= 0) {
    turn--;
    return true;
   }
   if (int(messages.turn) % 3 == 0) messages.add("You hear screeches from the rock above and around you!");
   return true;

  case EVENT_AMIGARA:
   messages.add("The entire cavern shakes!");
   return true;

  case EVENT_TEMPLE_OPEN:
   messages.add("The earth rumbles.");
   return true;

  default: return true; // Nothing happens for other events
 }
}
