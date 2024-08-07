#ifndef _CRAFTING_H_
#define _CRAFTING_H_

#include "itype_enum.h"
#include "skill.h"

#include <vector>

enum craft_cat : int {
CC_NULL = 0,
CC_WEAPON,
CC_FOOD,
CC_ELECTRONIC,
CC_ARMOR,
CC_MISC,
NUM_CC
};

struct component
{
 itype_id type;
 int count;

 constexpr component(itype_id TYPE = itm_null, int COUNT = 0) noexcept : type (TYPE), count (COUNT) {}
 component(const component& src) = default;
 component(component&& src) = default;
 component& operator=(const component& src) = default;
 component& operator=(component&& src) = default;
 ~component() = default;
};

struct recipe
{
 static std::vector<recipe*> recipes;	// The list of valid recipes; const recipe* won't work due to defense subgame

 int id;
 itype_id result;
 craft_cat category;
 skill sk_primary;
 skill sk_secondary;
 int difficulty;
 int time; // total move cost

 std::vector<std::vector<component> > tools;
 std::vector<std::vector<component> > components;

 recipe(int pid, itype_id pres, craft_cat cat, skill p1, skill p2, int pdiff, int ptime) noexcept :
  id (pid), result (pres), category (cat), sk_primary (p1), sk_secondary (p2),
  difficulty (pdiff), time (ptime) {}
 // could default everything, above, but that would allow even weirder initializations
 recipe() noexcept : recipe(0, itm_null, CC_NULL, sk_null, sk_null, 0, 0) {}

 std::string time_desc() const;

 static void init();
};

class map;
class player;

void consume_items(player& u, const std::vector<component>& components);	// \todo enable NPC construction
void consume_tools(player& u, const std::vector<component>& tools);

#endif
