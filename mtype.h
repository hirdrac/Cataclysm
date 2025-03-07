#ifndef _MTYPE_H_
#define _MTYPE_H_
// SEE ALSO: monitemsdef.cpp, which defines data on which items any given
// monster may carry.

#include "mapitems.h"
#include "material_enum.h"
#include "enum_json.h"
#include "color.h"
#include "c_bitmap.h"
#include <map>
#include <vector>

struct itype;
#ifndef SOCRATES_DAIMON
class game;
class monster;
#endif

enum monster_species {
species_none = 0,
species_mammal,
species_insect,
species_worm,
species_zombie,
species_plant,
species_fungus,
species_nether,
species_robot,
species_hallu,
num_species
};

enum mon_id {
mon_null = 0,
// Wildlife
mon_squirrel, mon_rabbit, mon_deer, mon_wolf, mon_bear,
// Friendly animals
mon_dog,
// Ants
mon_ant_larva, mon_ant, mon_ant_soldier, mon_ant_queen, mon_ant_fungus,
// Bees
mon_fly, mon_bee, mon_wasp,
// Worms
mon_graboid, mon_worm, mon_halfworm,
// Zombies
mon_zombie, mon_zombie_shrieker, mon_zombie_spitter, mon_zombie_electric,
 mon_zombie_fast, mon_zombie_brute, mon_zombie_hulk, mon_zombie_fungus,
 mon_boomer, mon_boomer_fungus, mon_skeleton, mon_zombie_necro,
 mon_zombie_scientist, mon_zombie_soldier, mon_zombie_grabber,
 mon_zombie_master,
// Triffids
mon_triffid, mon_triffid_young, mon_triffid_queen, mon_creeper_hub,
 mon_creeper_vine, mon_biollante, mon_vinebeast, mon_triffid_heart,
// Fungaloids
mon_fungaloid, mon_fungaloid_dormant, mon_fungaloid_young, mon_spore,
 mon_fungaloid_queen, mon_fungal_wall,
// Blobs
mon_blob, mon_blob_small,
// Sewer mutants
mon_chud, mon_one_eye, mon_crawler,
// Sewer animals
mon_sewer_fish, mon_sewer_snake, mon_sewer_rat, mon_rat_king,
// Swamp monsters
mon_mosquito, mon_dragonfly, mon_centipede, mon_frog, mon_slug,
 mon_dermatik_larva, mon_dermatik,
// SPIDERS
mon_spider_wolf, mon_spider_web, mon_spider_jumping, mon_spider_trapdoor,
 mon_spider_widow,
// Unearthed Horrors
mon_dark_wyrm, mon_amigara_horror, mon_dog_thing, mon_headless_dog_thing,
 mon_thing,
// Spiral monsters
mon_human_snail, mon_twisted_body, mon_vortex,
// Subspace monsters
// Mi-Go: H.P. Lovecraft's "Whisperer in Darkness"
mon_flying_polyp, mon_hunting_horror, mon_mi_go, mon_yugg, mon_gelatin,
 mon_flaming_eye, mon_kreck, mon_blank, mon_gozu, mon_shadow, mon_breather_hub,
 mon_breather, mon_shadow_snake,
// Robots
mon_eyebot, mon_manhack, mon_skitterbot, mon_secubot, mon_copbot, mon_molebot,
 mon_tripod, mon_chickenbot, mon_tankbot, mon_turret, mon_exploder,
// Hallucinations
mon_hallu_zom, mon_hallu_bee, mon_hallu_ant, mon_hallu_mom,
// Special monsters
mon_generator,
num_monsters
};

DECLARE_JSON_ENUM_SUPPORT_ATYPICAL(mon_id,0)

enum m_size {
MS_TINY = 0,	// Rodent
MS_SMALL,	// Half human
MS_MEDIUM,	// Human
MS_LARGE,	// Cow
MS_HUGE		// TAAAANK
};

// These are triggers which may affect the monster's anger or morale.
// They are handled in monster::check_triggers(), in monster.cpp
enum monster_trigger {
MTRIG_NULL = 0,
MTRIG_TIME,		// Random over time.
MTRIG_MEAT,		// Meat or a corpse nearby
MTRIG_PLAYER_WEAK,	// The player is hurt
MTRIG_PLAYER_CLOSE,	// The player gets within a few tiles
MTRIG_HURT,		// We are hurt
MTRIG_FIRE,		// Fire nearby
MTRIG_FRIEND_DIED,	// A monster of the same type died
MTRIG_FRIEND_ATTACKED,	// A monster of the same type attacked
MTRIG_SOUND,		// Heard a sound
N_MONSTER_TRIGGERS
};
static_assert(CHAR_BIT * sizeof(unsigned long long) >= N_MONSTER_TRIGGERS);

// Feel free to add to m_flags.  Order shouldn't matter, just keep it tidy!
// And comment them well. ;)

enum m_flag {
MF_NULL = 0,	// Helps with setvector
MF_SEES,	// It can see you (and will run/follow)
MF_HEARS,	// It can hear you
MF_GOODHEARING,	// Pursues sounds more than most monsters
MF_SMELLS,	// It can smell you
MF_STUMBLES,	// Stumbles in its movement
MF_WARM,	// Warm blooded
MF_NOHEAD,	// Headshots not allowed!
MF_HARDTOSHOOT,	// Some shots are actually misses
MF_GRABS,	// Its attacks may grab us!
MF_BASHES,	// Bashes down doors
MF_DESTROYS,	// Bashes down walls and more
MF_POISON,	// Poisonous to eat
MF_VENOM,	// Attack may poison the player
MF_BADVENOM,	// Attack may SEVERELY poison the player
MF_WEBWALK,	// Doesn't destroy webs
MF_DIGS,	// Digs through the ground
MF_FLIES,	// Can fly (over water, etc)
MF_AQUATIC,	// Confined to water
MF_SWIMS,	// Treats water as 50 movement point terrain
MF_ATTACKMON,	// Attacks other monsters
MF_ANIMAL,	// Is an "animal" for purposes of the Animal Empath trait
MF_PLASTIC,	// Absorbs physical damage to a great degree
MF_SUNDEATH,	// Dies in full sunlight
MF_ELECTRIC,	// Shocks unarmed attackers
MF_ACIDPROOF,	// Immune to acid
MF_ACIDTRAIL,	// Leaves a trail of acid
MF_FIREY,	// Burns stuff and is immune to fire.  Fire elementals, etc.
MF_QUEEN,	// When it dies, local populations start to die off too
MF_ELECTRONIC,	// e.g. a robot; affected by emp blasts, and other stuff
MF_FUR,		// May produce fur when butchered.
MF_LEATHER,	// May produce leather when butchered
MF_IMMOBILE,	// Doesn't move (e.g. turrets)
MF_FRIENDLY_SPECIAL, // Use our special attack, even if friendly
MF_HIT_AND_RUN,	// Flee for several turns after a melee attack
MF_GUILT,	// You feel guilty for killing it
MF_MAX		// Sets the length of the flags - obviously MUST be last
};
static_assert(CHAR_BIT * sizeof(unsigned long long) >= MF_MAX);

struct mtype {
 static std::vector<const mtype*> types;
 static std::vector<items_location_and_chance> items[num_monsters];
 static std::map<int, std::string> tiles;

 enum {
	 MS_MAX = MS_HUGE+1
 };

 int id;	// should be mon_id id but once we have JSON-configured monsters that doesn't work so well
 std::string name;
 std::string description;
 monster_species species;
 char sym;	// Symbol on the map
 nc_color color;// Color of symbol (see color.h)

 m_size size;
 material mat;	// See enums.h for material list.  Generally, flesh; veggy?

 // \todo? hard-code these bitmaps as CHAR_BIT*sizeof(unsigned long long) when building out mod support
 cataclysm::bitmap<MF_MAX>::type flags;
 cataclysm::bitmap<N_MONSTER_TRIGGERS>::type anger;   // What angers us?
 cataclysm::bitmap<N_MONSTER_TRIGGERS>::type placate; // What reduces our anger?
 cataclysm::bitmap<N_MONSTER_TRIGGERS>::type fear; // What are we afraid of?

 unsigned char frequency;	// How often do these show up? 0 (never) to ??
 int difficulty;// Used all over; 30 min + (diff-3)*30 min = earliest appearance
 int agro;	// How likely to attack; -100 to 100
 int morale;	// Default morale level

 unsigned int  speed;		// Speed; human = 100
 unsigned char melee_skill;	// Melee skill; should be 0 to 5
 unsigned char melee_dice;	// Number of dice on melee hit
 unsigned char melee_sides;	// Number of sides those dice have
 unsigned char melee_cut;	// Bonus cutting damage
 unsigned char sk_dodge;	// Dodge skill; should be 0 to 5
 unsigned char armor_bash;	// Natural armor vs. bash
 unsigned char armor_cut;	// Natural armor vs. cut
 signed char item_chance;	// Higher # means higher chance of loot
				// Negative # means one item gen'd, tops
 int hp;

 unsigned char sp_freq;			// How long sp_attack takes to charge
#ifndef SOCRATES_DAIMON
 void (*dies)(game *, monster *); // What happens when this monster dies

private:
 void (*sp_attack)(game *, monster *); // This monster's special attack
 void (*special_attack)(monster&); // This monster's special attack
#endif
 
  // Default constructor
public:
 mtype();

 // Non-default (messy)
 mtype(int pid, std::string pname, monster_species pspecies, char psym,
	 nc_color pcolor, m_size psize, material pmat,
	 unsigned char pfreq, unsigned int pdiff, signed char pagro,
	 int pmorale, unsigned int pspeed, unsigned char pml_skill,
	 unsigned char pml_dice, unsigned char pml_sides, unsigned char pml_cut,
	 unsigned char pdodge, unsigned char parmor_bash,
	 unsigned char parmor_cut, signed char pitem_chance, int php,
	 unsigned char psp_freq,
#ifndef SOCRATES_DAIMON
	 void(*pdies)      (game *, monster *),
	 void(*psp_attack)(game *, monster *),
#endif
	 std::string pdescription);

#ifndef SOCRATES_DAIMON
 mtype(int pid, std::string pname, monster_species pspecies, char psym,
	 nc_color pcolor, m_size psize, material pmat,
	 unsigned char pfreq, unsigned int pdiff, signed char pagro,
	 int pmorale, unsigned int pspeed, unsigned char pml_skill,
	 unsigned char pml_dice, unsigned char pml_sides, unsigned char pml_cut,
	 unsigned char pdodge, unsigned char parmor_bash,
	 unsigned char parmor_cut, signed char pitem_chance, int php,
	 unsigned char psp_freq,
	 decltype(dies) pdies,
	 decltype(special_attack) psp_attack,
	 std::string pdescription);
#endif

 bool has_flag(m_flag flag) const { return flags & mfb(flag); }
 int chunk_count() const;
 const itype* chunk_material() const;
 nc_color danger() const;
#ifndef SOCRATES_DAIMON
 void do_special_attack(monster& viewpoint) const;
#endif

 static void init();
 static void init_items();
};

#endif
