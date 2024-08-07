#include "gamemode.h"
#include "game.h"
#include "construction.h"
#include "keypress.h"
#include "rng.h"
#include "recent_msg.h"
#include <sstream>

enum caravan_category {
    CARAVAN_CART = 0,
    CARAVAN_MELEE,
    CARAVAN_GUNS,
    CARAVAN_COMPONENTS,
    CARAVAN_FOOD,
    CARAVAN_CLOTHES,
    CARAVAN_TOOLS,
    NUM_CARAVAN_CATEGORIES
};

std::vector<itype_id> caravan_items(caravan_category cat);

int caravan_price(const player &u, int price);

void draw_caravan_borders(WINDOW *w, int current_window);
void draw_caravan_categories(WINDOW *w, int category_selected, int total_price,
                             int cash);

std::string defense_style_name(defense_style style);
std::string defense_style_description(defense_style style);

defense_game::defense_game()
{
 current_wave = 0;
 hunger = false;
 thirst = false;
 sleep  = false;
 zombies = false;
 specials = false;
 spiders = false;
 triffids = false;
 robots = false;
 subspace = false;
 mercenaries = false;
 init_to_style(DEFENSE_EASY);
}

bool defense_game::init(game *g)
{
 messages.turn = HOURS(12); // Start at noon
 g->temperature = 65;
 if (!g->u.create(g, PLTYPE_CUSTOM)) return false;
 g->u.str_cur = g->u.str_max;
 g->u.per_cur = g->u.per_max;
 g->u.int_cur = g->u.int_max;
 g->u.dex_cur = g->u.dex_max;
 init_itypes();
 init_mtypes();
 init_constructions();
 init_recipes();
 current_wave = 0;
 hunger = false;
 thirst = false;
 sleep  = false;
 zombies = false;
 specials = false;
 spiders = false;
 triffids = false;
 robots = false;
 subspace = false;
 mercenaries = false;
 init_to_style(DEFENSE_EASY);
 setup();
 g->u.cash = initial_cash;
 popup_nowait("Please wait as the map generates [ 0%%]");
 init_map(g);
 caravan(g);
 return true;
}

void defense_game::per_turn(game *g)
{
 if (!thirst) g->u.thirst = 0;
 if (!hunger) g->u.hunger = 0;
 if (!sleep) g->u.fatigue = 0;
 if (int(messages.turn) % (time_between_waves * 10) == 0) {
  current_wave++;
  if (current_wave > 1 && current_wave % waves_between_caravans == 0) {
   popup("A caravan approaches!  Press spacebar...");
   caravan(g);
  }
  spawn_wave(g);
 }
}

static constexpr const std::pair<const char*, const char*> defense_location_name_desc[] = {
    std::pair("Nowhere?!  A bug!", "NULL Bug."),
    std::pair("Hospital", "One entrance and many rooms.  Some medical supplies."),
    std::pair("Megastore", "A large building with various supplies."),
    std::pair("Bar", "A small building with plenty of alcohol."),
    std::pair("Mansion", "A large house with many rooms and.")
};

static_assert(NUM_DEFENSE_LOCATIONS == std::end(defense_location_name_desc) - std::begin(defense_location_name_desc));

// tolerate std::string construction here -- will be needed when translations are attempted
static std::string _name(defense_location location) { return defense_location_name_desc[location].first; }
static std::string description(defense_location location) { return defense_location_name_desc[location].second; }

void defense_game::pre_action(game *g, action_id &act)
{
 if (act == ACTION_SLEEP && !sleep) {
  messages.add("You don't need to sleep!");
  act = ACTION_NULL;
 }
 if (act == ACTION_SAVE) {
  messages.add("You cannot save in defense mode!");
  act = ACTION_NULL;
 }

// Big ugly block for movement
 if (   (act == ACTION_MOVE_N && g->u.pos.y == SEEX * int(MAPSIZE / 2) && g->lev.y <= 93)
	 || (act == ACTION_MOVE_NE && 
	     (   (g->u.pos.y == SEEY * int(MAPSIZE / 2) && g->lev.y <=  93)
	      || (g->u.pos.x == SEEX * (1 + int(MAPSIZE / 2))-1 && g->lev.x >= 98)))
	 || (act == ACTION_MOVE_E && g->u.pos.x == SEEX * (1 + int(MAPSIZE / 2)) - 1 && g->lev.x >= 98)
	 || (act == ACTION_MOVE_SE && 
	     (   (g->u.pos.y == SEEY * (1 + int(MAPSIZE / 2))-1 && g->lev.y >= 98)
		  || (g->u.pos.x == SEEX * (1 + int(MAPSIZE / 2))-1 && g->lev.x >= 98)))
	 || (act == ACTION_MOVE_S && g->u.pos.y == SEEY * (1 + int(MAPSIZE / 2))-1 && g->lev.y >= 98)
	 || (act == ACTION_MOVE_SW && 
	     (   (g->u.pos.y == SEEY * (1 + int(MAPSIZE / 2))-1 && g->lev.y >= 98)
		  || (g->u.pos.x == SEEX * int(MAPSIZE / 2) && g->lev.x <=  93)))
	 || (act == ACTION_MOVE_W && g->u.pos.x == SEEX * int(MAPSIZE / 2) && g->lev.x <= 93)
	 || (act == ACTION_MOVE_NW &&
	     (   (g->u.pos.y == SEEY * int(MAPSIZE / 2) && g->lev.y <=  93)
		  || (g->u.pos.x == SEEX * int(MAPSIZE / 2) && g->lev.x <=  93)))) {
  messages.add("You cannot leave the %s behind!", _name(location).c_str());
  act = ACTION_NULL;
 }
}

void defense_game::post_action(game *g, action_id act)
{
}

void defense_game::game_over(game *g)
{
 popup("You managed to survive through wave %d!", current_wave);
}

void defense_game::init_itypes()
{
 item::types[itm_2x4]->volume = 0;
 item::types[itm_2x4]->weight = 0;
 item::types[itm_landmine]->price = 300;
 item::types[itm_bot_turret]->price = 6000;
}

void defense_game::init_mtypes()
{
 for(const auto m_type : mtype::types) {
  mtype* const working = const_cast<mtype*>(m_type);
  working->difficulty *= 1.5;
  working->difficulty += int(m_type->difficulty / 5);
  working->flags |= mfb(MF_BASHES) | mfb(MF_SMELLS) | mfb(MF_HEARS) | mfb(MF_SEES);
 }
}

void defense_game::init_constructions()
{
 for (const auto construct : constructable::constructions) {
  for (auto& stage : construct->stages) stage.time = 1; // Everything takes 1 minute
 }
}

void defense_game::init_recipes()
{
 for (const auto& tmp : recipe::recipes) tmp->time /= 10; // Things take turns, not minutes
}

void defense_game::init_map(game *g)
{
 for (int x = 0; x < OMAPX; x++) {
  for (int y = 0; y < OMAPY; y++) {
   g->cur_om.ter(x, y) = ot_field;
   g->cur_om.seen(x, y) = true;
  }
 }

 g->cur_om.save(g->u.name, 0, 0, DEFENSE_Z);
 g->lev = tripoint(100,100,0);
 g->u.screenpos_set(point(SEE));

 switch (location) {

 case DEFLOC_HOSPITAL:
  for (int x = 49; x <= 51; x++) {
   for (int y = 49; y <= 51; y++)
    g->cur_om.ter(x, y) = ot_hospital;
  }
  g->cur_om.ter(50, 49) = ot_hospital_entrance;
  break;

 case DEFLOC_MALL:
  for (int x = 49; x <= 51; x++) {
   for (int y = 49; y <= 51; y++)
    g->cur_om.ter(x, y) = ot_megastore;
  }
  g->cur_om.ter(50, 49) = ot_megastore_entrance;
  break;

 case DEFLOC_BAR:
  g->cur_om.ter(50, 50) = ot_bar_north;
  break;

 case DEFLOC_MANSION:
  for (int x = 49; x <= 51; x++) {
   for (int y = 49; y <= 51; y++)
    g->cur_om.ter(x, y) = ot_mansion;
  }
  g->cur_om.ter(50, 49) = ot_mansion_entrance;
  break;
 }
// Init the map
 int old_percent = 0;
 for (int i = 0; i <= MAPSIZE * 2; i += 2) {
  for (int j = 0; j <= MAPSIZE * 2; j += 2) {
   int mx = g->lev.x - MAPSIZE + i, my = g->lev.y - MAPSIZE + j;
   int percent = 100 * ((j / 2 + MAPSIZE * (i / 2))) /
                       ((MAPSIZE) * (MAPSIZE + 1));
   if (percent >= old_percent + 1) {
    popup_nowait("Please wait as the map generates [%s%d%]",
                 (percent < 10 ? " " : ""), percent);
    old_percent = percent;
   }
// Round down to the nearest even number
   mx -= mx % 2;
   my -= my % 2;
   tinymap tm;
   tm.generate(g, &(g->cur_om), mx, my);
   tm.post_init(Badge<defense_game>());
   tm.save(g->cur_om.pos, int(messages.turn), point(mx, my));
  }
 }

 g->m.load(g, project_xy(g->lev));

 g->update_map(g->u.pos.x, g->u.pos.y);
 monster generator(mtype::types[mon_generator], g->u.pos + Direction::SE);
// Find a valid spot to spawn the generator
 std::vector<point> valid;
 for (decltype(auto) delta : Direction::vector) {
   const point pt(g->u.pos + delta);
   if (generator.can_move_to(g->m, pt) && g->is_empty(pt)) valid.push_back(pt);
 }
 if (!valid.empty()) generator.spawn(valid[rng(0, valid.size() - 1)]); // reset starting position to a legal one
 generator.friendly = -1;
 g->spawn(std::move(generator));
}

void defense_game::init_to_style(defense_style new_style)
{
 style = new_style;
 hunger = false;
 thirst = false;
 sleep  = false;
 zombies = false;
 specials = false;
 spiders = false;
 triffids = false;
 robots = false;
 subspace = false;
 mercenaries = false;

 switch (new_style) {
 case DEFENSE_EASY:
  location = DEFLOC_HOSPITAL;
  initial_difficulty = 15;
  wave_difficulty = 10;
  time_between_waves = 30;
  waves_between_caravans = 3;
  initial_cash = 10000;
  cash_per_wave = 1000;
  cash_increase = 300;
  specials = true;
  spiders = true;
  triffids = true;
  mercenaries = true;
  break;

 case DEFENSE_MEDIUM:
  location = DEFLOC_MALL;
  initial_difficulty = 30;
  wave_difficulty = 15;
  time_between_waves = 20;
  waves_between_caravans = 4;
  initial_cash = 6000;
  cash_per_wave = 800;
  cash_increase = 200;
  specials = true;
  spiders = true;
  triffids = true;
  robots = true;
  hunger = true;
  mercenaries = true;
  break;

 case DEFENSE_HARD:
  location = DEFLOC_BAR;
  initial_difficulty = 50;
  wave_difficulty = 20;
  time_between_waves = 10;
  waves_between_caravans = 5;
  initial_cash = 2000;
  cash_per_wave = 600;
  cash_increase = 100;
  specials = true;
  spiders = true;
  triffids = true;
  robots = true;
  subspace = true;
  hunger = true;
  thirst = true;
  break;

  break;

 case DEFENSE_SHAUN:
  location = DEFLOC_BAR;
  initial_difficulty = 30;
  wave_difficulty = 15;
  time_between_waves = 5;
  waves_between_caravans = 6;
  initial_cash = 5000;
  cash_per_wave = 500;
  cash_increase = 100;
  zombies = true;
  break;

 case DEFENSE_DAWN:
  location = DEFLOC_MALL;
  initial_difficulty = 60;
  wave_difficulty = 20;
  time_between_waves = 30;
  waves_between_caravans = 4;
  initial_cash = 8000;
  cash_per_wave = 500;
  cash_increase = 0;
  zombies = true;
  hunger = true;
  thirst = true;
  mercenaries = true;
  break;

 case DEFENSE_SPIDERS:
  location = DEFLOC_MALL;
  initial_difficulty = 60;
  wave_difficulty = 10;
  time_between_waves = 10;
  waves_between_caravans = 4;
  initial_cash = 6000;
  cash_per_wave = 500;
  cash_increase = 100;
  spiders = true;
  break;

 case DEFENSE_TRIFFIDS:
  location = DEFLOC_MANSION;
  initial_difficulty = 60;
  wave_difficulty = 20;
  time_between_waves = 30;
  waves_between_caravans = 2;
  initial_cash = 10000;
  cash_per_wave = 600;
  cash_increase = 100;
  triffids = true;
  hunger = true;
  thirst = true;
  sleep = true;
  mercenaries = true;
  break;

 case DEFENSE_SKYNET:
  location = DEFLOC_HOSPITAL;
  initial_difficulty = 20;
  wave_difficulty = 20;
  time_between_waves = 20;
  waves_between_caravans = 6;
  initial_cash = 12000;
  cash_per_wave = 1000;
  cash_increase = 200;
  robots = true;
  hunger = true;
  thirst = true;
  mercenaries = true;
  break;

 case DEFENSE_LOVECRAFT:
  location = DEFLOC_MANSION;
  initial_difficulty = 20;
  wave_difficulty = 20;
  time_between_waves = 120;
  waves_between_caravans = 8;
  initial_cash = 4000;
  cash_per_wave = 1000;
  cash_increase = 100;
  subspace = true;
  hunger = true;
  thirst = true;
  sleep = true;
 }
}

void defense_game::setup()
{
 WINDOW* w = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 int selection = 1;
 refresh_setup(w, selection);

 static auto redraw_numeric = [&](int y, int num, int ub) {
     draw_hline(w, y, c_black, 'x', 25 - int_log10(ub) , 25);
     mvwprintz(w, y, 25 - int_log10(num), c_yellow, "%d", num);
 };

 while (true) {
  int ch = input();
 
  if (ch == 'S') {
   if (!zombies && !specials && !spiders && !triffids && !robots && !subspace) {
    popup("You must choose at least one monster group!");
    refresh_setup(w, selection);
   } else
    return;
  } else if (ch == '+' || ch == '>' || ch == 'j') {
   if (selection == 19)
    selection = 1;
   else
    selection++;
   refresh_setup(w, selection);
  } else if (ch == '-' || ch == '<' || ch == 'k') {
   if (selection == 1)
    selection = 19;
   else
    selection--;
   refresh_setup(w, selection);
  } else if (ch == '!') {
   std::string name = string_input_popup(20, "Template Name:");
   refresh_setup(w, selection);
  } else if (ch == 'S')
   return;
 
  else {
   switch (selection) {
    case 1:	// Scenario selection
     if (ch == 'l') {
      if (style == defense_style(NUM_DEFENSE_STYLES - 1))
       style = defense_style(1);
      else
       style = defense_style(style + 1);
     }
     if (ch == 'h') {
      if (style == defense_style(1))
       style = defense_style(NUM_DEFENSE_STYLES - 1);
      else
       style = defense_style(style - 1);
     }
     init_to_style(style);
     break;
 
    case 2:	// Location selection
     if (ch == 'l') {
      if (location == defense_location(NUM_DEFENSE_LOCATIONS - 1))
       location = defense_location(1);
      else
       location = defense_location(location + 1);
     }
     if (ch == 'h') {
      if (location == defense_location(1))
       location = defense_location(NUM_DEFENSE_LOCATIONS - 1);
      else
       location = defense_location(location - 1);
     }
     draw_hline(w, 5, c_black, 'x', 2, 80);
     mvwaddstrz(w, 5, 2, c_yellow, _name(location).c_str());
     mvwaddstrz(w,  5, 28, c_ltgray, description(location).c_str());
     break;
 
    case 3:	// Difficulty of the first wave
     if (ch == 'h' && initial_difficulty > 10) initial_difficulty -= 5;
     if (ch == 'l' && initial_difficulty < 995) initial_difficulty += 5;
     redraw_numeric(7, initial_difficulty, 995);
     break;
 
    case 4:	// Wave Difficulty
     if (ch == 'h' && wave_difficulty > 10) wave_difficulty -= 5;
     if (ch == 'l' && wave_difficulty < 995) wave_difficulty += 5;
     redraw_numeric(8, wave_difficulty, 995);
     break;
 
    case 5:
     if (ch == 'h' && time_between_waves > 5) time_between_waves -= 5;
     if (ch == 'l' && time_between_waves < 995) time_between_waves += 5;
     redraw_numeric(10, time_between_waves, 995);
     break;
 
    case 6:
     if (ch == 'h' && waves_between_caravans > 1) waves_between_caravans -= 1;
     if (ch == 'l' && waves_between_caravans < 50) waves_between_caravans += 1;
     redraw_numeric(11, waves_between_caravans, 50);
     break;
 
    case 7:
     if (ch == 'h' && initial_cash > 0) initial_cash -= 100;
     if (ch == 'l' && initial_cash < 99900) initial_cash += 100;
     redraw_numeric(13, initial_cash, 99900);
     break;

    case 8:
     if (ch == 'h' && cash_per_wave > 0) cash_per_wave -= 100;
     if (ch == 'l' && cash_per_wave < 9900) cash_per_wave += 100;
     redraw_numeric(14, cash_per_wave, 9900);
     break;

    case 9:
     if (ch == 'h' && cash_increase > 0) cash_increase -= 50;
     if (ch == 'l' && cash_increase < 9950) cash_increase += 50;
     redraw_numeric(15, cash_increase, 9950);
     break;

    case 10:
     if (ch == ' ' || ch == '\n') {
      zombies = !zombies;
      specials = false;
     }
     mvwaddstrz(w, 18, 2, (zombies ? c_ltgreen : c_yellow), "Zombies");
     mvwaddstrz(w, 18, 14, c_yellow, "Special Zombies");
     break;

    case 11:
     if (ch == ' ' || ch == '\n') {
      specials = !specials;
      zombies = false;
     }
     mvwaddstrz(w, 18, 2, c_yellow, "Zombies");
     mvwaddstrz(w, 18, 14, (specials ? c_ltgreen : c_yellow), "Special Zombies");
     break;

    case 12:
     if (ch == ' ' || ch == '\n') spiders = !spiders;
     mvwaddstrz(w, 18, 34, (spiders ? c_ltgreen : c_yellow), "Spiders");
     break;

    case 13:
     if (ch == ' ' || ch == '\n') triffids = !triffids;
     mvwaddstrz(w, 18, 46, (triffids ? c_ltgreen : c_yellow), "Triffids");
     break;

    case 14:
     if (ch == ' ' || ch == '\n') robots = !robots;
     mvwaddstrz(w, 18, 59, (robots ? c_ltgreen : c_yellow), "Robots");
     break;

    case 15:
     if (ch == ' ' || ch == '\n') subspace = !subspace;
     mvwaddstrz(w, 18, 70, (subspace ? c_ltgreen : c_yellow), "Subspace");
     break;

    case 16:
     if (ch == ' ' || ch == '\n') hunger = !hunger;
     mvwaddstrz(w, 21, 2, (hunger ? c_ltgreen : c_yellow), "Food");
     break;

    case 17:
     if (ch == ' ' || ch == '\n') thirst = !thirst;
     mvwaddstrz(w, 21, 16, (thirst ? c_ltgreen : c_yellow), "Water");
     break;

    case 18:
     if (ch == ' ' || ch == '\n') sleep = !sleep;
     mvwaddstrz(w, 21, 31, (sleep ? c_ltgreen : c_yellow), "Sleep");
     break;

    case 19:
     if (ch == ' ' || ch == '\n') mercenaries = !mercenaries;
     mvwaddstrz(w, 21, 46, (mercenaries ? c_ltgreen : c_yellow), "Mercenaries");
     break;
   }
  }
  if (ch == 'h' || ch == 'l' || ch == ' ' || ch == '\n')
   refresh_setup(w, selection);
 }
}

void defense_game::refresh_setup(WINDOW* w, int selection)
{
 werase(w);
 mvwaddstrz(w,  0,  1, c_ltred, "DEFENSE MODE");
 mvwaddstrz(w,  0, 28, c_ltred, "Press +/- or >/< to cycle, spacebar to toggle");
 mvwaddstrz(w,  1, 28, c_ltred, "Press S to start, ! to save as a template");
 mvwaddstrz(w,  2,  2, c_ltgray, "Scenario:");
 mvwaddstrz(w,  3,  2, 1 == selection ? c_yellow : c_blue, defense_style_name(style).c_str());
 mvwaddstrz(w,  3, 28, c_ltgray, defense_style_description(style).c_str());
 mvwaddstrz(w,  4,  2, c_ltgray, "Location:");
 mvwaddstrz(w,  5,  2, 2 == selection ? c_yellow : c_blue, _name(location).c_str());
 mvwaddstrz(w,  5, 28, c_ltgray, description(location).c_str());

 static auto draw_numeric_row = [&](int y, const char* label, nc_color col, int num, const char* desc) {
     mvwaddstrz(w, y, 2, c_ltgray, label);
     mvwprintz(w, y, 25 - int_log10(num), col, "%d", num);
     mvwaddstrz(w, y, 28, c_ltgray, desc);
 };

 draw_numeric_row(7, "Initial Difficulty:", 3 == selection ? c_yellow : c_blue, initial_difficulty, "The difficulty of the first wave.");
 draw_numeric_row(8, "Wave Difficulty:", 4 == selection ? c_yellow : c_blue, wave_difficulty, "The increase of difficulty with each wave.");

 draw_numeric_row(10, "Time b/w Waves:", 5 == selection ? c_yellow : c_blue, time_between_waves, "The time, in minutes, between waves.");
 draw_numeric_row(11, "Waves b/w Caravans:", 6 == selection ? c_yellow : c_blue, waves_between_caravans, "The number of waves in between caravans.");

 draw_numeric_row(13, "Initial Cash:", 7 == selection ? c_yellow : c_blue, initial_cash, "The amount of money the player starts with.");
 draw_numeric_row(14, "Cash for 1st Wave:", 8 == selection ? c_yellow : c_blue, cash_per_wave, "The cash awarded for the first wave.");
 draw_numeric_row(15, "Cash Increase:", 9 == selection ? c_yellow : c_blue, cash_increase, "The increase in the award each wave.");

#define TOGCOL(n, b) (selection == (n) ? (b ? c_ltgreen : c_yellow) : (b ? c_green : c_dkgray))

 mvwaddstrz(w, 17,  2, c_ltgray, "Enemy Selection:");
 mvwaddstrz(w, 18,  2, TOGCOL(10, zombies), "Zombies");
 mvwaddstrz(w, 18, 14, TOGCOL(11, specials), "Special Zombies");
 mvwaddstrz(w, 18, 34, TOGCOL(12, spiders), "Spiders");
 mvwaddstrz(w, 18, 46, TOGCOL(13, triffids), "Triffids");
 mvwaddstrz(w, 18, 59, TOGCOL(14, robots), "Robots");
 mvwaddstrz(w, 18, 70, TOGCOL(15, subspace), "Subspace");

 mvwaddstrz(w, 20,  2, c_ltgray, "Needs:");
 mvwaddstrz(w, 21,  2, TOGCOL(16, hunger), "Food");
 mvwaddstrz(w, 21, 16, TOGCOL(17, thirst), "Water");
 mvwaddstrz(w, 21, 31, TOGCOL(18, sleep), "Sleep");
 mvwaddstrz(w, 21, 46, TOGCOL(19, mercenaries), "Mercenaries");
 wrefresh(w);

#undef TOGCOL
}

std::string defense_style_name(defense_style style)
{
// 24 Characters Max!
 switch (style) {
  case DEFENSE_CUSTOM:		return "Custom";
  case DEFENSE_EASY:		return "Easy";
  case DEFENSE_MEDIUM:		return "Medium";
  case DEFENSE_HARD:		return "Hard";
  case DEFENSE_SHAUN:		return "Shaun of the Dead";
  case DEFENSE_DAWN:		return "Dawn of the Dead";
  case DEFENSE_SPIDERS:		return "Eight-Legged Freaks";
  case DEFENSE_TRIFFIDS:	return "Day of the Triffids";
  case DEFENSE_SKYNET:		return "Skynet";
  case DEFENSE_LOVECRAFT:	return "The Call of Cthulhu";
  default:			return "Bug!  Report this!";
 }
}

std::string defense_style_description(defense_style style)
{
// 51 Characters Max!
 switch (style) {
  case DEFENSE_CUSTOM:
   return "A custom game.";
  case DEFENSE_EASY:
   return "Easy monsters and lots of money.";
  case DEFENSE_MEDIUM:
   return "Harder monsters.  You have to eat.";
  case DEFENSE_HARD:
   return "All monsters.  You have to eat and drink.";
  case DEFENSE_SHAUN:
   return "Defend a bar against classic zombies.  Easy and fun.";
  case DEFENSE_DAWN:
   return "Classic zombies.  Slower and more realistic.";
  case DEFENSE_SPIDERS:
   return "Fast-paced spider-fighting fun!";
  case DEFENSE_TRIFFIDS:
   return "Defend your mansion against the triffids.";
  case DEFENSE_SKYNET:
   return "The robots have decided that humans are the enemy!";
  case DEFENSE_LOVECRAFT:
   return "Ward off legions of eldritch horrors.";
  default:
   return "What the heck is this I don't even know. A bug!";
 }
}

static void draw_caravan_items(WINDOW* w, const player& u, const std::vector<itype_id>& items,
    const std::vector<int>& counts, int offset, int item_selected)
{
    // Print the item info first.  This is important, because it contains \n which
    // will corrupt the item list.

    // Actually, clear the item info first.
    for (int i = NUM_CARAVAN_CATEGORIES + 5; i <= VIEW - 2; i++)
        draw_hline(w, i, c_black, 'x', 1, SCREEN_WIDTH / 2 - 1);
    // THEN print it--if item_selected is valid
    if (item_selected < items.size()) {
        item tmp(item::types[items[item_selected]], 0); // Dummy item to get info
        mvwaddstrz(w, 12, 0, c_white, tmp.info().c_str());
    }
    // Next, clear the item list on the right
    for (int i = 1; i <= VIEW - 2; i++)
        draw_hline(w, i, c_black, 'x', SCREEN_WIDTH / 2, SCREEN_WIDTH - 1);
    // Finally, print the item list on the right
    for (int i = offset; i <= offset + VIEW - 2 && i < items.size(); i++) {
        const itype* const i_type = item::types[items[i]];
        mvwaddstrz(w, i - offset + 1, 40, (item_selected == i ? h_white : c_white), i_type->name.c_str());
        wprintz(w, c_white, " x %s%d", (counts[i] >= 10 ? "" : " "), counts[i]);
        if (counts[i] > 0) {
            int price = caravan_price(u, i_type->price * counts[i]);
            wprintz(w, (price > u.cash ? c_red : c_green),
                "($%s%d)", (price >= 100000 ? "" : (price >= 10000 ? " " :
                    (price >= 1000 ? "  " : (price >= 100 ? "   " :
                        (price >= 10 ? "    " : "     "))))), price);
        }
    }
    wrefresh(w);
}

void defense_game::caravan(game *g)
{
 caravan_category tab = CARAVAN_MELEE;
 std::vector<itype_id> items[NUM_CARAVAN_CATEGORIES];
 std::vector<int> item_count[NUM_CARAVAN_CATEGORIES];

// Init the items for each category
 for (int i = 0; i < NUM_CARAVAN_CATEGORIES; i++) {
  items[i] = caravan_items( caravan_category(i) );
  for (int j = 0; j < items[i].size(); j++) {
   if (current_wave == 0 || !one_in(4))
    item_count[i].push_back(0); // Init counts to 0 for each item
   else { // Remove the item
    EraseAt(items[i], j);
    j--;
   }
  }
 }

 int total_price = 0;

 WINDOW *w = newwin(VIEW, SCREEN_WIDTH, 0, 0);

 int offset = 0, item_selected = 0, category_selected = 0;

 int current_window = 0;

 draw_caravan_borders(w, current_window);
 draw_caravan_categories(w, category_selected, total_price, g->u.cash);

 const int scroll_delta = VIEW - 3;

 bool done = false;
 bool cancel = false;
 while (!done) {

  int ch = input();
  switch (ch) {
   case '?':
    popup_top("\
CARAVAN:\n\
Start by selecting a category using your favorite up/down keys.\n\
Switch between category selection and item selecting by pressing Tab.\n\
Pick an item with the up/down keys, press + to buy 1 more, - to buy 1 less.\n\
Press Enter to buy everything in your cart, Esc to buy nothing.");
    draw_caravan_categories(w, category_selected, total_price, g->u.cash);
    draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
    draw_caravan_borders(w, current_window);
    break;

   case 'j':
    if (current_window == 0) { // Categories
     if (NUM_CARAVAN_CATEGORIES <= ++category_selected) category_selected = CARAVAN_CART;
     draw_caravan_categories(w, category_selected, total_price, g->u.cash);
     offset = 0;
     item_selected = 0;
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    } else if (items[category_selected].size() > 0) { // Items
     if (item_selected < items[category_selected].size() - 1)
      item_selected++;
     else {
      item_selected = 0;
      offset = 0;
     }
     if (item_selected > offset + scroll_delta) offset++;
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    }
    break;

   case 'k':
    if (current_window == 0) { // Categories
     if (0 > --category_selected) category_selected = NUM_CARAVAN_CATEGORIES - 1;
     draw_caravan_categories(w, category_selected, total_price, g->u.cash);
     offset = 0;
     item_selected = 0;
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    } else if (items[category_selected].size() > 0) { // Items
     if (item_selected > 0) 
      item_selected--;
     else {
      item_selected = items[category_selected].size() - 1;
      offset = item_selected - scroll_delta;
      if (offset < 0)
       offset = 0;
     }
     if (item_selected < offset) offset--;
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    }
    break;

   case '+':
   case 'l':
    if (current_window == 1 && items[category_selected].size() > 0) {
     item_count[category_selected][item_selected]++;
     const itype_id tmp_itm = items[category_selected][item_selected];
     total_price += caravan_price(g->u, item::types[tmp_itm]->price);
     if (category_selected == CARAVAN_CART) { // Find the item in its category
      for (int i = 1; i < NUM_CARAVAN_CATEGORIES; i++) {
       for (int j = 0; j < items[i].size(); j++) {
        if (items[i][j] == tmp_itm)
         item_count[i][j]++;
       }
      }
     } else { // Add / increase the item in the shopping cart
      bool found_item = false;
      for (int i = 0; i < items[0].size() && !found_item; i++) {
       if (items[0][i] == tmp_itm) {
        found_item = true;
        item_count[0][i]++;
       }
      }
      if (!found_item) {
       items[0].push_back(items[category_selected][item_selected]);
       item_count[0].push_back(1);
      }
     }
     draw_caravan_categories(w, category_selected, total_price, g->u.cash);
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    }
    break;

   case '-':
   case 'h':
    if (current_window == 1 && items[category_selected].size() > 0 &&
        item_count[category_selected][item_selected] > 0) {
     item_count[category_selected][item_selected]--;
     const itype_id tmp_itm = items[category_selected][item_selected];
     total_price -= caravan_price(g->u, item::types[tmp_itm]->price);
     if (category_selected == CARAVAN_CART) { // Find the item in its category
      for (int i = 1; i < NUM_CARAVAN_CATEGORIES; i++) {
       for (int j = 0; j < items[i].size(); j++) {
        if (items[i][j] == tmp_itm)
         item_count[i][j]--;
       }
      }
     } else { // Decrease / remove the item in the shopping cart
      bool found_item = false;
      for (int i = 0; i < items[0].size() && !found_item; i++) {
       if (items[0][i] == tmp_itm) {
        found_item = true;
        item_count[0][i]--;
        if (item_count[0][i] == 0) {
         EraseAt(item_count[0], i);
         EraseAt(items[0], i);
        }
       }
      }
     }
     draw_caravan_categories(w, category_selected, total_price, g->u.cash);
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    }
    break;

   case '\t':
    current_window = (current_window + 1) % 2;
    draw_caravan_borders(w, current_window);
    break;

   case KEY_ESCAPE:
    if (query_yn("Really buy nothing?")) {
     cancel = true;
     done = true;
    } else {
     draw_caravan_categories(w, category_selected, total_price, g->u.cash);
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    }
    break;

   case '\n':
    if (total_price > g->u.cash)
     popup("You can't afford those items!");
    else if ((items[0].empty() && query_yn("Really buy nothing?")) ||
             (!items[0].empty() &&
              query_yn("Buy %d items, leaving you with $%d?", items[0].size(),
                       g->u.cash - total_price)))
     done = true;
    if (!done) { // We canceled, so redraw everything
     draw_caravan_categories(w, category_selected, total_price, g->u.cash);
     draw_caravan_items(w, g->u, items[category_selected], item_count[category_selected], offset, item_selected);
     draw_caravan_borders(w, current_window);
    }
    break;
  } // switch (ch)

 } // while (!done)

 if (!cancel) {
  g->u.cash -= total_price;
  bool dropped_some = false;
  for (int i = 0; i < items[0].size(); i++) {
   item tmp(item::types[ items[0][i] ], messages.turn);
   tmp = tmp.in_its_container();
   for (int j = 0; j < item_count[0][i]; j++) {
    if (g->u.volume_carried() + tmp.volume() <= g->u.volume_capacity() &&
        g->u.weight_carried() + tmp.weight() <= g->u.weight_capacity() &&
        g->u.inv.size() < 52)
     g->u.i_add(std::move(tmp));
    else { // Could not fit it in the inventory!
     dropped_some = true;
     g->u.GPSpos.add(std::move(tmp));
    }
   }
  }
  if (dropped_some) messages.add("You drop some items.");
 }
}

std::vector<itype_id> caravan_items(caravan_category cat)
{
 std::vector<itype_id> ret;
 switch (cat) {
// case CARAVAN_CART: return ret;

 case CARAVAN_MELEE:
  ret = {
itm_hammer, itm_bat, itm_mace, itm_morningstar, itm_hammer_sledge, itm_hatchet,
itm_knife_combat, itm_rapier, itm_machete, itm_katana, itm_spear_knife,
itm_pike, itm_chainsaw_off
  };
  break;

 case CARAVAN_GUNS:
  ret = {
itm_crossbow, itm_bolt_steel, itm_compbow, itm_arrow_cf, itm_marlin_9a,
itm_22_lr, itm_hk_mp5, itm_9mm, itm_taurus_38, itm_38_special, itm_deagle_44,
itm_44magnum, itm_m1911, itm_hk_ump45, itm_45_acp, itm_fn_p90, itm_57mm,
itm_remington_870, itm_shot_00, itm_shot_slug, itm_browning_blr, itm_3006,
itm_ak47, itm_762_m87, itm_m4a1, itm_556, itm_savage_111f, itm_hk_g3,
itm_762_51, itm_hk_g80, itm_12mm, itm_plasma_rifle, itm_plasma
  };
  break;

 case CARAVAN_COMPONENTS:
  ret = {
itm_rag, itm_fur, itm_leather, itm_superglue, itm_string_36, itm_chain,
itm_processor, itm_RAM, itm_power_supply, itm_motor, itm_hose, itm_pot,
itm_2x4, itm_battery, itm_nail, itm_gasoline
  };
  break;

 case CARAVAN_FOOD:
  ret = {
itm_1st_aid, itm_water, itm_energy_drink, itm_whiskey, itm_can_beans,
itm_mre_beef, itm_flour, itm_inhaler, itm_codeine, itm_oxycodone, itm_adderall,
itm_cig, itm_meth, itm_royal_jelly, itm_mutagen, itm_purifier
  };
 break;

 case CARAVAN_CLOTHES:
  ret = {
itm_backpack, itm_vest, itm_trenchcoat, itm_jacket_leather, itm_kevlar,
itm_gloves_fingerless, itm_mask_filter, itm_mask_gas, itm_glasses_eye,
itm_glasses_safety, itm_goggles_ski, itm_goggles_nv, itm_helmet_ball,
itm_helmet_riot
  };
  break;

 case CARAVAN_TOOLS:
  ret = {
itm_screwdriver, itm_wrench, itm_saw, itm_hacksaw, itm_lighter, itm_sewing_kit,
itm_scissors, itm_extinguisher, itm_flashlight, itm_hotplate,
itm_soldering_iron, itm_shovel, itm_jackhammer, itm_landmine, itm_teleporter,
itm_grenade, itm_flashbang, itm_EMPbomb, itm_smokebomb, itm_bot_manhack,
itm_bot_turret, itm_UPS_off, itm_mininuke
  };
  break;
 }

 return ret;
}

void draw_caravan_borders(WINDOW *w, int current_window)
{
// First, do the borders for the category window
 nc_color col = (0 == current_window ? c_yellow : c_ltgray);

 mvwputch(w, 0, 0, col, LINE_OXXO);
 draw_hline(w, 0, col, LINE_OXOX, 1, SCREEN_WIDTH / 2 - 1);
 draw_hline(w, NUM_CARAVAN_CATEGORIES + 4, col, LINE_OXOX, 1, SCREEN_WIDTH / 2 - 1);
 for (int i = 1; i < NUM_CARAVAN_CATEGORIES + 4; i++) {
  mvwputch(w, i,  0, col, LINE_XOXO);
  mvwputch(w, i, SCREEN_WIDTH / 2 - 1, c_yellow, LINE_XOXO); // Shared border, always yellow
 }
 mvwputch(w, NUM_CARAVAN_CATEGORIES + 4,  0, col, LINE_XXXO);

// These are shared with the items window, and so are always "on"
 mvwputch(w,  0, SCREEN_WIDTH / 2 - 1, c_yellow, LINE_OXXX);
 mvwputch(w, NUM_CARAVAN_CATEGORIES + 4, SCREEN_WIDTH / 2 - 1, c_yellow, LINE_XOXX);

 col = (1 == current_window ? c_yellow : c_ltgray);
// Next, draw the borders for the item description window--always "off" & gray
 for (int i = NUM_CARAVAN_CATEGORIES + 5; i < VIEW - 1; i++) {
  mvwputch(w, i,  0, c_ltgray, LINE_XOXO);
  mvwputch(w, i, SCREEN_WIDTH / 2 - 1, col, LINE_XOXO);
 }
 draw_hline(w, VIEW - 1, c_ltgray, LINE_OXOX, 1, SCREEN_WIDTH / 2 - 1);

 mvwputch(w, VIEW - 1,  0, c_ltgray, LINE_XXOO);
 mvwputch(w, VIEW - 1, SCREEN_WIDTH / 2 - 1, c_ltgray, LINE_XXOX);

// Finally, draw the item section borders
 draw_hline(w, 0, col, LINE_OXOX, SCREEN_WIDTH / 2, SCREEN_WIDTH - 1);
 draw_hline(w, VIEW - 1, col, LINE_OXOX, SCREEN_WIDTH / 2, SCREEN_WIDTH - 1);
 for (int i = 1; i < VIEW - 1; i++)
  mvwputch(w, i, SCREEN_WIDTH - 1, col, LINE_XOXO);

 mvwputch(w, VIEW - 1, SCREEN_WIDTH / 2 - 1, col, LINE_XXOX);
 mvwputch(w,  0, SCREEN_WIDTH - 1, col, LINE_OOXX);
 mvwputch(w, VIEW - 1, SCREEN_WIDTH / 2 - 1, col, LINE_XOOX);

// Quick reminded about help.
 mvwaddstrz(w, VIEW - 1, 2, c_red, "Press ? for help.");
 wrefresh(w);
}

void draw_caravan_categories(WINDOW *w, int category_selected, int total_price,
                             int cash)
{
    static constexpr const char* caravan_category_name[] = { // cf. enum caravan_category
        "Shopping Cart",
        "Melee Weapons",
        "Firearms & Ammo",
        "Crafting & Construction Components",
        "Food & Drugs",
        "Clothing & Armor",
        "Tools, Traps & Grenades"
    };
    static_assert(NUM_CARAVAN_CATEGORIES == std::end(caravan_category_name)-std::begin(caravan_category_name));

// Clear the window
 for (int i = 1; i < NUM_CARAVAN_CATEGORIES + 4; i++)
  draw_hline(w, i, c_black, 'x', 1, SCREEN_WIDTH / 2 - 1);
 mvwaddstrz(w, 1, 1, c_white, "Your Cash:");
 mvwprintz(w, 1, 17 - int_log10(cash), c_white, "%d", cash);
 waddstrz(w, c_ltgray, " -> ");
 wprintz(w, (total_price > cash ? c_red : c_green), "%d", cash - total_price);

 for (int i = 0; i < NUM_CARAVAN_CATEGORIES; i++)
  mvwaddstrz(w, i + 3, 1, (i == category_selected ? h_white : c_white), caravan_category_name[i]);
 wrefresh(w);
}
 
int caravan_price(const player &u, int price)
{
 if (u.sklevel[sk_barter] >= 10) return price/2;
 return int( double(price) * (1.0 - double(u.sklevel[sk_barter]) * .05));
}

void defense_game::spawn_wave(game *g)
{
 constexpr const int SPECIAL_WAVE_CHANCE = 5; // One in X chance of single-flavor wave
 constexpr const int SPECIAL_WAVE_MIN = 5; // Don't use a special wave with < X monsters

 messages.add("********");
 int diff = initial_difficulty + current_wave * wave_difficulty;
 bool themed_wave = one_in(SPECIAL_WAVE_CHANCE); // All a single monster type
 g->u.cash += cash_per_wave + (current_wave - 1) * cash_increase;
 std::vector<mon_id> valid(pick_monster_wave(g));
 while (diff > 0) {
// Clear out any monsters that exceed our remaining difficulty
  int i = valid.size();
  while(0 < i--) {
	if (mtype::types[valid[i]]->difficulty > diff) valid.erase(valid.begin() + i);
  }
  if (valid.empty()) {
   messages.add("Welcome to Wave %d!", current_wave);
   messages.add("********");
   return;
  }
  const auto type = mtype::types[valid[rng(0, valid.size() - 1)]];
  if (themed_wave) {
   int num = diff / type->difficulty;
   if (num >= SPECIAL_WAVE_MIN) {
// TODO: Do we want a special message here?
    for (int i = 0; i < num; i++) spawn_wave_monster(g, type);
	messages.add( special_wave_message(type->name).c_str() );
	messages.add("********");
    return;
   } else themed_wave = false; // No partially-themed waves
  }
  diff -= type->difficulty;
  spawn_wave_monster(g, type);
 }
 messages.add("Welcome to Wave %d!", current_wave);
 messages.add("********");
}

std::vector<mon_id> defense_game::pick_monster_wave(game *g)
{
 std::vector<mon_id> ret;
 {
 std::vector<moncat_id> valid;

 if (zombies || specials) {
  if (specials)
   valid.push_back(mcat_zombie);
  else
   valid.push_back(mcat_vanilla_zombie);
 }
 if (spiders) valid.push_back(mcat_spider);
 if (triffids) valid.push_back(mcat_triffid);
 if (robots) valid.push_back(mcat_robot);
 if (subspace) valid.push_back(mcat_nether);

 if (valid.empty())
  debugmsg("Couldn't find a valid monster group for defense!");
 else
  ret = mongroup::moncats[ valid[rng(0, valid.size() - 1)] ];
 }
 return ret;
}

void defense_game::spawn_wave_monster(game *g, const mtype *type)
{
 monster tmp(type);
 if (location == DEFLOC_HOSPITAL || location == DEFLOC_MALL) { // Always spawn to the north!
  tmp.screenpos_set(rng(SEEX * (MAPSIZE / 2), SEEX * (1 + MAPSIZE / 2)), SEEY);
 } else if (one_in(2)) {
  point pt(rng(SEEX * (MAPSIZE / 2), SEEX * (1 + MAPSIZE / 2)), rng(1, SEEY));
  if (one_in(2)) pt.y = SEE * MAPSIZE - 1 - pt.y;
  tmp.spawn(pt);
 } else {
  point pt(rng(1, SEEX), rng(SEEY * (MAPSIZE / 2), SEEY * (1 + MAPSIZE / 2)));
  if (one_in(2)) pt.x = SEE * MAPSIZE - 1 - pt.x;
  tmp.spawn(pt);
 }
 tmp.wand.set(g->u.pos, 150);
// We wanna kill!
 tmp.anger = 100;
 tmp.morale = 100;
 g->spawn(std::move(tmp));
}

std::string defense_game::special_wave_message(std::string name)
{
 std::ostringstream ret;
 ret << "Wave " << current_wave << ": ";
 name[0] += 'A' - 'a'; // Capitalize

 for (int i = 2; i < name.size(); i++) {
  if (name[i - 1] == ' ')
   name[i] += 'A' - 'a';
 }

 switch (rng(1, 6)) {
  case 1: ret << name << " Invasion!"; break;
  case 2: ret << "Attack of the " << name << "s!"; break;
  case 3: ret << name << " Attack!"; break;
  case 4: ret << name << "s from Hell!"; break;
  case 5: ret << "Beware! " << name << "!"; break;
  case 6: ret << "The Day of the " << name << "!"; break;
  case 7: ret << name << " Party!"; break;
  case 8: ret << "Revenge of the " << name << "s!"; break;
  case 9: ret << "Rise of the " << name << "s!"; break;
 }

 return ret.str();
}
