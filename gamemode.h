#ifndef _GAMEMODE_H_
#define _GAMEMODE_H_

#include "action.h"
#include "mtype.h"

class game;

enum special_game_id {
SGAME_NULL = 0,
SGAME_TUTORIAL,
SGAME_DEFENSE,
NUM_SPECIAL_GAMES
};

const char* special_game_name(special_game_id id);

struct special_game
{
 virtual ~special_game() = default;

 virtual special_game_id id() const { return SGAME_NULL; };
// init is run when the game begins
 virtual bool init(game *g) { return true; };
// per_turn is run every turn--before any player actions
 virtual void per_turn(game *g) { };
// pre_action is run after a keypress, but before the game handles the action
// It may modify the action, e.g. to cancel it
 virtual void pre_action(game *g, action_id &act) { };
// post_action is run after the game handles the action
 virtual void post_action(game *g, action_id act) { };
// game_over is run when the player dies (or the game otherwise ends)
 virtual void game_over(game *g) { };
};

special_game* get_special_game(special_game_id id);

// TUTORIAL:

enum tut_lesson {
LESSON_INTRO,
LESSON_MOVE, LESSON_LOOK, LESSON_OPEN, LESSON_CLOSE, LESSON_SMASH,
 LESSON_WINDOW, LESSON_PICKUP, LESSON_EXAMINE, LESSON_INTERACT,

LESSON_FULL_INV, LESSON_WIELD_NO_SPACE, LESSON_AUTOWIELD, LESSON_ITEM_INTO_INV,
 LESSON_GOT_ARMOR, LESSON_GOT_WEAPON, LESSON_GOT_FOOD, LESSON_GOT_TOOL,
 LESSON_GOT_GUN, LESSON_GOT_AMMO, LESSON_WORE_ARMOR, LESSON_WORE_STORAGE,
 LESSON_WORE_MASK,

LESSON_WEAPON_INFO, LESSON_HIT_MONSTER, LESSON_PAIN, LESSON_BUTCHER,

LESSON_TOOK_PAINKILLER, LESSON_TOOK_CIG, LESSON_DRANK_WATER,

LESSON_ACT_GRENADE, LESSON_ACT_BUBBLEWRAP,

LESSON_OVERLOADED,

LESSON_GUN_LOAD, LESSON_GUN_FIRE, LESSON_RECOIL,

LESSON_STAIRS, LESSON_DARK_NO_FLASH, LESSON_DARK, LESSON_PICKUP_WATER,

NUM_LESSONS
};

struct tutorial_game : public special_game
{
 special_game_id id() const override { return SGAME_TUTORIAL; };
 bool init(game *g) override;
 void per_turn(game *g) override;
 void pre_action(game *g, action_id &act) override;
 void post_action(game *g, action_id act) override;

private:
 void add_message(game *g, tut_lesson lesson);

 bool tutorials_seen[NUM_LESSONS];
};

// DEFENSE

enum defense_style {
DEFENSE_CUSTOM = 0,
DEFENSE_EASY,
DEFENSE_MEDIUM,
DEFENSE_HARD,
DEFENSE_SHAUN,
DEFENSE_DAWN,
DEFENSE_SPIDERS,
DEFENSE_TRIFFIDS,
DEFENSE_SKYNET,
DEFENSE_LOVECRAFT,
NUM_DEFENSE_STYLES
};

enum defense_location {
DEFLOC_NULL = 0,
DEFLOC_HOSPITAL,
DEFLOC_MALL,
DEFLOC_BAR,
DEFLOC_MANSION,
NUM_DEFENSE_LOCATIONS
};

struct defense_game : public special_game
{
 defense_game();

 special_game_id id() const override { return SGAME_DEFENSE; };
 bool init(game *g) override;
 void per_turn(game *g) override;
 void pre_action(game *g, action_id &act) override;
 void post_action(game *g, action_id act) override;
 void game_over(game *g) override;

private:
 void init_to_style(defense_style new_style);

 void setup();
 void refresh_setup(WINDOW *w, int selection);
 static void init_itypes();
 static void init_mtypes();
 static void init_constructions();
 static void init_recipes();
 void init_map(game *g);

 void spawn_wave(game *g);
 void caravan(game *g);
 std::vector<mon_id> pick_monster_wave(game *g);
 void spawn_wave_monster(game *g, const mtype *type);

 std::string special_wave_message(std::string name);

// DATA
 int current_wave;

 defense_style style;	// What type of game is it?
 defense_location location; // Where are we?

 int initial_difficulty; // Total "level" of monsters in first wave
 int wave_difficulty; // Increased "level" of monsters per wave

 int time_between_waves; // Cooldown / building / healing time
 int waves_between_caravans; // How many waves until we get to trade?

 int initial_cash;	// How much cash do we start with?
 int cash_per_wave;	// How much cash do we get per wave?
 int cash_increase;	// How much does the above increase per wave?

 bool zombies;
 bool specials;
 bool spiders;
 bool triffids;
 bool robots;
 bool subspace;

 bool hunger;		// Do we hunger?
 bool thirst;		// Do we thirst?
 bool sleep;		// Do we need to sleep?

 bool mercenaries;	// Do caravans offer the option of hiring a mercenary?
};

#endif
