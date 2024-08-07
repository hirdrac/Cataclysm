#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "skill.h"
#include "bionics.h"
#include "morale.h"
#include "inventory.h"
#include "mutation.h"
#include "mobile.h"
#include "bodypart.h"
#include "pldata.h"
#include "zero.h"
#include <functional>

enum art_effect_passive;
enum craft_cat : int;
class game;
struct mission;
class monster;
struct recipe;
struct trap;

// zaimoni 2023-11-28: GPS coordinate conversion makes this too include-heavy for pldata.h
struct player_activity
{
    activity_type type;
    int moves_left;
    int index;
    std::vector<int> values; // used by ACT_BUILD, ACT_VEHICLE
    point placement; // low priority to convert to GPS coordinates (needed for NPC activities outside of reality bubble) 2020-08-13 zaimoni
    GPS_loc gps_placement;

    player_activity(activity_type t = ACT_NULL, int turns = 0, int Index = -1) noexcept
        : type(t), moves_left(turns), index(Index), placement(-1, -1), gps_placement(_ref<GPS_loc>::invalid) {}
    player_activity(const player_activity& copy) = default;
    player_activity(player_activity&& copy) = default;
    player_activity& operator=(const player_activity& copy) = default;
    player_activity& operator=(player_activity&& copy) = default;

    void clear() noexcept {    // Technically ok to sink this into a *.cpp 2020-08-11 zaimoni
        type = ACT_NULL;
        decltype(values)().swap(values);
    }
};

struct special_attack
{
 std::string text;
 int bash;
 int cut;
 int stab;

 special_attack() noexcept : bash(0), cut(0), stab(0) { };
 special_attack(std::string&& src) noexcept : text(std::move(src)), bash(0), cut(0), stab(0) { };
};

std::string random_first_name(bool male);
std::string random_last_name();

// C:Whales considered npc a subclass of player but allowed player to have PC-specific UI.
// It's too invasive to try to extract the player vs NPC differences into an actor controller class,
// but we don't want per-player UI within the game object either.
class player : public mobile {
protected:
 player();
 player(const cataclysm::JSON& src);
 player(const player &rhs) = default;
 player(player&& rhs) = default;
 virtual ~player() = default;
 friend cataclysm::JSON toJSON(const player& src);

 player& operator=(const player& rhs) = default;
 player& operator=(player&& rhs) = default;

public:
// newcharacter.cpp
 void normalize();	// Starting set up of HP
// end newcharacter.cpp

 void pick_name(); // Picks a name from NAMES_*

 virtual bool is_npc() const { return false; }	// Overloaded for NPCs in npc.h
 nc_color color() const;				// What color to draw us as
 std::pair<int, nc_color> hp_color(hp_part part) const;

 void disp_info(game *g);	// '@' key; extended character info
 void disp_morale();		// '%' key; morale info
 void disp_status(WINDOW* w, game *g);// On-screen data

 void reset(const Badge<game>&);// Resets movement points, stats, applies effects
 void update_morale();	// Ticks down morale counters and removes them
 int  current_speed() const override; // Number of movement points we get a turn
 int  theoretical_speed() const; // ", ignoring some environmental modifiers
 int  run_cost(int base_cost) const; // Adjust base_cost
 int  swim_speed();	// Our speed when swimming
 virtual void swim(const GPS_loc& loc);
 bool swimming_surface();
 bool swimming_dive();
 /// <returns>C error code convention. 0 no problem, 1 "about to" (warn), -1 "is" (error)</returns>
 int is_drowning() const;

 int is_cold_blooded() const;
 int has_light_bones() const;
 bool has_trait(int flag) const;
 bool has_mutation(int flag) const;
 void toggle_trait(int flag);

 // \todo once MSVC++ has std::ranges support, consider cutting over to that
 const char* interpret_trait(const std::pair<pl_flag, const char*>* origin, ptrdiff_t ub) const;

 // bionics
 bool has_bionic(bionic_id b) const;
 bool has_active_bionic(bionic_id b) const;
 void add_bionic(bionic_id b);
 void charge_power(int amount);
 void activate_bionic(int b); // V 0.2.1 extend to NPCs

 void mutate();
 void mutate_towards(pl_flag mut);
 bool remove_mutation(pl_flag mut);
 pl_flag has_child_flag(pl_flag mut) const;
 bool remove_child_flag(pl_flag mut);

 unsigned int seismic_range() const;
 unsigned int sight_range(int light_level) const;
 unsigned int sight_range() const; // uses light level for our GPSpos
 unsigned int overmap_sight_range() const;
 bool see(const monster& mon) const;
 std::optional<int> see(const player& u) const;
 std::optional<int> see(const GPS_loc& pt) const;
 std::optional<int> see(const point& pt) const;
 std::optional<int> see(int x, int y) const { return see(point(x, y)); }
 std::optional<std::vector<GPS_loc> > see(const std::variant<monster*, npc*, pc*>& dest, int range) const;
 int  clairvoyance() const; // Sight through walls &c
 bool has_two_arms() const;
// bool can_wear_boots();	// no definition
 bool is_armed() const;	// True if we're wielding something; true for bionics
 bool unarmed_attack() const; // False if we're wielding something; true for bionics
 bool avoid_trap(const trap *tr) const;

 void pause(); // '.' command; pauses & reduces recoil

 // npcmove.cpp (inherited location)
 // Physical movement from one tile to the next
 bool can_move_to(const map& m, const point& pt) const;
 bool can_enter(const GPS_loc& _GPSpos) const;
 bool landing_zone_ok();   // returns true if final location is deemed ok; that is, no-op is true
 std::optional<point> move_away_from(const map& m, const point& tar) const;
 virtual int can_reload() const; // Wielding a gun that is not fully loaded; return value is inventory index, or -1
 virtual bool can_fire();

 // melee.cpp
 int  hit_mon(game *g, monster *z, bool allow_grab = true);
 void hit_player(game *g, player &p, bool allow_grab = true);

 int base_damage(bool real_life = true, int stat = -999) const;
 int base_to_hit(bool real_life = true, int stat = -999) const;

 technique_id pick_defensive_technique(const monster *z, player *p);

 void perform_defensive_technique(technique_id technique, game *g, monster *z,
                                  player *p, body_part &bp_hit, int &side,
                                  int &bash_dam, int &cut_dam, int &stab_dam);

 int dodge() const; // Returns the players's dodge, modded by clothing etc
 int dodge_roll() const override; // For comparison to hit_roll()
 int melee_skill() const override { return dex_cur + sklevel[sk_melee]; }
 int min_technique_power() const override { return 1 + (2 + str_cur) / 4; }
 // V 0.2.1 \todo player variants of these so we can avoid null pointers as signal for player; alternately, unify API
 int roll_bash_damage(const monster* z, bool crit) const;
 int roll_cut_damage(const monster* z, bool crit) const;

// ranged.cpp (at some point, historically)
 unsigned int aiming_range(const item& aimed) const;
 unsigned int throw_range(const item& thrown) const; // Range of throwing item
 int ranged_dex_mod	(bool real_life = true) const;
 int ranged_per_mod	(bool real_life = true) const;
 int throw_dex_mod	(bool real_life = true) const;

// Mental skills and stats
 int comprehension_percent(skill s, bool real_life = true) const;
 int read_speed		(bool real_life = true) const;
 int talk_skill() const; // Skill at convincing NPCs of stuff
 int intimidation() const; // Physical intimidation
 double barter_price_adjustment() const; // charisma-like price scaling

// Converts bphurt to a hp_part (if side == 0, the left), then does/heals dam
// hit() processes damage through armor
 void hit (game *g, body_part bphurt, int side, int dam, int cut);
// hurt() doesn't--effects of disease, what have you
 void hurt(body_part bphurt, int side, int dam);
 bool hurt(int dam) override;

 void heal(body_part healed, int side, int dam);	// dead function?
 void heal(hp_part healed, int dam);
 void healall(int dam);
 void hurtall(int dam);
 // checks armor. if vary > 0, then damage to parts are random within 'vary' percent (1-100)
 bool hitall(int dam, int vary = 0) override;
 // calibration: STR 3 -> 1; STR 20 -> 4; STR 8 -> 2
 int knockback_size() const override { return (str_max - 2) / 5 + 1; /* C:Whales: (str_max - 6) / 4; */ }

 int hp_percentage() const;	// % of HP remaining, overall
 std::pair<hp_part, int> worst_injury() const;
 std::pair<itype_id, int> would_heal(const std::pair<hp_part, int>& injury) const;

 void get_sick();	// Process diseases	\todo V 0.2.1 enable for NPCs
// infect() gives us a chance to save (mostly from armor)
 void infect(dis_type type, body_part vector, int strength, int duration);
// add_disease() does NOT give us a chance to save
 void add_disease(dis_type type, int duration, int intensity = 0, int max_intensity = -1);
 bool rem_disease(dis_type type);
 bool rem_disease(std::function<bool(disease&)> op);
 bool do_foreach(std::function<bool(disease&)> op);
 bool do_foreach(std::function<bool(const disease&)> op) const;
 bool has_disease(dis_type type) const;
 int  disease_level(dis_type type) const;
 int  disease_intensity(dis_type type) const;
 void add(effect src, int duration) override;
 bool has(effect src) const override;
 bool rude_awakening();

 void add_addiction(add_type type, int strength);
#if DEAD_FUNC
 void rem_addiction(add_type type);
#endif
 bool has_addiction(add_type type) const;
 int  addiction_level(add_type type) const;

 void die();
 void suffer(game *g);	// \todo V 0.2.1 extend fully to NPCs
 void vomit();	// \todo V 0.2.1 extend to NPCs
 void fling(int dir, int flvel); // thrown by impact (or something)

 // 2021-09-08: modernized standard for identifying local items for various sorts of use
 using item_spec = std::pair<item*, int>;
 using item_spec_const = std::pair<const item*, int>;
 /// <summary>
 /// legacy analog: item& i_at(char let)
 /// legacy analog: bool has_item(char let) const;
 /// legacy analog: int  lookup_item(char let) const; (conversion must exclude or handle worn armor)
 /// </summary>
 /// <param name="let">invlet from player UI</param>
 std::optional<item_spec> from_invlet(char let);
 std::optional<item_spec_const> from_invlet(char let) const;
 std::optional<item_spec> lookup(item* it);
 std::vector<item_spec> reject(std::function<std::optional<std::string>(const item_spec&)> fail);

 /// <summary>
 /// legacy analog: bool remove_item(item* it);
 /// legacy analog: item i_rem(char let); when return value unused
 /// legacy analog: item i_remn(int index); when return value unused
 /// </summary>
 /// <returns>true if and only if successful (but always succeeds if precondition met)</returns>
 bool remove_discard(const item_spec& it);

 int  lookup_item(char let) const;
 std::optional<std::string> cannot_eat(const item_spec& src) const;
 bool eat(const item_spec& src);	// Eat item; returns false on fail
 virtual bool wield(int index);// Wield item; returns false on fail
 void pick_style(); // Pick a style
 bool wear(const item_spec& wear_this);	// Wear item; returns false on fail
 bool wear_item(const item& to_wear);
 const it_armor* wear_is_performable(const item& to_wear) const;
 std::optional<std::string> cannot_read() const;  // can read at all (environment check, etc.)
 std::optional<std::string> cannot_read(const std::variant<const it_macguffin*, const it_book*>& src) const;
 void try_to_sleep();	// '$' command; adds DIS_LYING_DOWN	\todo V 0.2.1 extend to NPCs
 bool can_sleep(const map& m) const;	// Checked each turn during DIS_LYING_DOWN

 int warmth(body_part bp) const;	// Warmth provided by armor &c; \todo cf game::check_warmth which might belong over in player
 void check_warmth(int ambient_F);
 int encumb(body_part bp) const;	// Encumbrance from armor &c
 int armor_bash(body_part bp) const;	// Bashing resistance
 int armor_cut(body_part bp) const;	// Cutting  resistance
 int resist(body_part bp) const;	// Infection &c resistance
 bool wearing_something_on(body_part bp) const; // True if wearing something on bp

 void practice(skill s, int amount);	// Practice a skill
 void update_skills(); // Degrades practice levels, checks & upgrades skills

 void assign_activity(activity_type type, int moves, int index = -1);	// \todo V 0.2.5+ extend to NPCs
 void assign_activity(activity_type type, int moves, const point& pt, int index = -1);
 void cancel_activity();
 virtual void cancel_activity_query(const char* message, ...);

 void accept(mission* miss); // Just assign an existing mission
 void fail(const mission& miss); // move to failed list, if relevant
 void wrap_up(mission* miss);

 int weight_carried() const;
 int volume_carried() const;
 int weight_capacity(bool real_life = true) const;
 int volume_capacity() const;
 int morale_level() const;	// Modified by traits, &c
 void add_morale(morale_type type, int bonus, int max_bonus = 0, const itype* item_type = nullptr);
 int get_morale(morale_type type, const itype* item_type = nullptr) const;

 void sort_inv();	// Sort inventory by type
 std::string weapname(bool charges = true) const;

 void i_add(item&& it);
 void i_add(const item& it) { i_add(item(it)); }
 bool has_active_item(itype_id id) const;
 int  active_item_charges(itype_id id) const;
 void process_active_items(game *g);
 void remove_weapon();
 item unwield();    // like remove_weapon, but returns what was unwielded
 void remove_mission_items(int mission_id);
 item i_remn(int index);// Remove item from inventory; returns ret_null on fail
 item& i_of_type(itype_id type); // Returns the first item with this type
 const item& i_of_type(itype_id type) const { return const_cast<player*>(this)->i_of_type(type); };
 std::vector<item> inv_dump() const; // Inventory + weapon + worn (for death, etc)
 std::optional<int> butcher_factor() const;	// Automatically picks our best butchering tool
 item* pick_usb(); // Pick a usb drive, interactively if it matters
 bool is_wearing(itype_id it) const;	// Are we wearing a specific itype?
 bool has_artifact_with(art_effect_passive effect) const;

// has_amount works ONLY for quantity.
// has_charges works ONLY for charges.
 void use_amount(itype_id it, int quantity, bool use_container = false);
 unsigned int use_charges(itype_id it, int quantity);// Uses up charges
 bool has_amount(itype_id it, int quantity) const  { return amount_of(it)  >= quantity; }
 bool has_charges(itype_id it, int quantity) const { return charges_of(it) >= quantity; }
 int  amount_of(itype_id it) const;
 int  charges_of(itype_id it) const;

 bool has_watertight_container() const;
 bool has_weapon_or_armor(char let) const;	// Has an item with invlet let
 bool has_item(item *it) const;		// Has a specific item
 bool has_mission_item(int mission_id) const;	// Has item with mission_id
 bool has_ammo(ammotype at) const;// Returns a list of indices of the ammo
 std::vector<int> have_ammo(ammotype at) const;// Returns a list of indices of the ammo

 item* decode_item_index(int n);

// crafting.cpp
 void make_craft(const recipe* making);
 void pick_recipes(std::vector<const recipe*>& current,
     std::vector<bool>& available, craft_cat tab);

// construction.cpp
 void complete_construction();

// abstract ui
 bool is_enemy(const monster* z) const;
 virtual bool is_enemy(const player* survivor = nullptr) const = 0;
 virtual void subjective_message(const std::string& msg) const = 0;
 virtual void subjective_message(const char* msg) const = 0;
 virtual bool if_visible_message(std::function<std::string()> msg) const = 0;
 virtual bool if_visible_message(std::function<std::string()> me, std::function<std::string()> other) const = 0;
 virtual bool if_visible_message(const char* msg) const = 0;
 virtual bool ask_yn(const char* msg, std::function<bool()> ai = nullptr) const = 0;
 bool ask_yn(const std::string& msg, std::function<bool()> ai = nullptr) const { return ask_yn(msg.c_str(), ai); }
 virtual bool see_phantasm();  // would not be const for multi-PC case
 virtual std::vector<item>* use_stack_at(const point& pt) const;
 virtual int use_active(item& it) = 0;
 virtual void record_kill(const monster& m) {}
 virtual std::optional<item_spec> choose(const char* prompt, std::function<std::optional<std::string>(const item_spec&)> fail) = 0;

 // grammatical support
 bool is_proper() const final { return true; }
 unsigned gender() const final { return male ? 1 : 2; }

 // integrity checks
 void screenpos_set(point pt);
 void screenpos_set(int x, int y);
 void screenpos_add(point delta);

 // adapter for std::visit
 struct can_see {
     const player& p;

     can_see(const player& p) noexcept : p(p) {}
     can_see(const can_see& src) = delete;
     can_see(can_see&& src) = delete;
     can_see& operator=(const can_see& src) = delete;
     can_see& operator=(can_see&& src) = delete;
     ~can_see() = default;

     bool operator()(const monster* target) const { return p.see(*target); }
     bool operator()(const player* target) const { return p.see(*target) ? true : false; }
 };

 struct is_enemy_of {
     const player& p;

     is_enemy_of(const player& p) noexcept : p(p) {}
     is_enemy_of(const is_enemy_of& src) = delete;
     is_enemy_of(is_enemy_of&& src) = delete;
     is_enemy_of& operator=(const is_enemy_of& src) = delete;
     is_enemy_of& operator=(is_enemy_of&& src) = delete;
     ~is_enemy_of() = default;

     auto operator()(const monster* target) const { return p.is_enemy(target); }
     auto operator()(const player* target) const { return p.is_enemy(target); }
 };

 struct cast
 {
     constexpr cast() = default;
     constexpr cast(const cast& src) = delete;
     constexpr cast(cast&& src) = delete;
     constexpr cast& operator=(const cast& src) = delete;
     constexpr cast& operator=(cast&& src) = delete;
     constexpr ~cast() = default;

     constexpr auto operator()(player* target) { return target; }
     constexpr auto operator()(const player* target) { return target; }
     constexpr player* operator()(mobile* target) { return nullptr; }
     constexpr const player* operator()(const mobile* target) { return nullptr; }
 };

 // ---------------VALUES-----------------
 point pos;
 bool in_vehicle;       // Means player sit inside vehicle on the tile he is now
 player_activity activity;
 player_activity backlog;
// _missions vectors are of mission IDs
 std::vector<int> active_missions;
 std::vector<int> completed_missions;
 std::vector<int> failed_missions;
 int active_mission;	// index into active_missions vector, above
 
 std::string name;
 bool male;
 bool my_traits[PF_MAX2];
 bool my_mutations[PF_MAX2];
 int mutation_category_level[NUM_MUTATION_CATEGORIES];
 std::vector<bionic> my_bionics;
// Current--i.e. modified by disease, pain, etc.
 int str_cur, dex_cur, int_cur, per_cur;
// Maximum--i.e. unmodified by disease
 int str_max, dex_max, int_max, per_max;
 int power_level, max_power_level;
 int hunger, thirst, fatigue, health;
 bool underwater;
 int oxygen;
 unsigned int recoil;
 unsigned int driving_recoil;
 unsigned int scent;
 int stim, pain, pkill, radiation;
 int cash; // \todo re-implement; this is pre-apocalyptic cash
 int hp_cur[num_hp_parts], hp_max[num_hp_parts];

 std::vector<morale_point> morale;

 int xp_pool;
 int sklevel   [num_skill_types];
 int skexercise[num_skill_types];
 
 bool inv_sorted;	// V 0.2.1+ use or eliminate, this appears to be a no-op tracer
 inventory inv;
 itype_id last_item;
 std::vector <item> worn;	// invariant: dynamic cast to it_armor is non-null
 std::vector<itype_id> styles;
 itype_id style_selected;
 item weapon;
 
 std::vector <addiction> addictions;

protected:
 int can_take_off_armor(const item& it) const; /// C error code convention.  1: to inventory; -1: drop it
 bool take_off(int i);// Take off item; returns false on fail

private:
 mutable int dodges_left;
 int blocks_left;
 std::vector <disease> illness;

 void _set_screenpos() override { if (auto pt = screen_pos()) pos = *pt; }
 bool handle_knockback_into_impassable(const GPS_loc& dest) override;
 virtual void consume(item& food) = 0;

 // melee.cpp
 int  hit_roll() const; // Our basic hit roll, compared to our target's dodge roll
 bool scored_crit(int target_dodge = 0) const; // Critical hit?

 // V 0.2.1 \todo player variants of these so we can avoid null pointers as signal for player; alternately, unify API
 int roll_stab_damage(const monster *z, bool crit) const;

 technique_id pick_technique(const monster *z, const player *p, bool crit, bool allowgrab) const;
 void perform_technique(technique_id technique, game *g, monster *z, player *p,
                       int &bash_dam, int &cut_dam, int &pierce_dam, int &pain);

 void perform_special_attacks(game *g, monster *z, player *p, int &bash_dam, int &cut_dam, int &pierce_dam);	// V 0.2.1 \todo enable for NPCs
 std::vector<special_attack> mutation_attacks(const mobile& mob) const;	// V 0.2.1 \todo enable for NPCs

 void melee_special_effects(game *g, monster *z, player *p, bool crit, int &bash_dam, int &cut_dam, int &stab_dam);

 // player.cpp
// absorb() reduces dam and cut by your armor (and bionics, traits, etc)
 void absorb(body_part bp, int &dam, int &cut);	// \todo V 0.2.1 enable for NPCs?
};

inventory crafting_inventory(const player& u);

#endif
