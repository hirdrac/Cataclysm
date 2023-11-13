#ifndef _CONSTRUCTION_H_ 
#define _CONSTRUCTION_H_

#include "crafting.h"
#include "mapdata.h"
#include "enums.h"

struct construction_stage
{
 ter_id terrain;
 int time; // In minutes, i.e. 10 turns
 // Intent appears to be AND of OR for each of these.
 std::vector<std::vector<itype_id> > tools;
 std::vector<std::vector<component> > components;

 construction_stage(ter_id Terrain, int Time) noexcept : terrain (Terrain), time (Time) { };
 construction_stage(const construction_stage& src) = default;
 construction_stage(construction_stage&& src) = default;
 construction_stage& operator=(const construction_stage& src) = default;
 construction_stage& operator=(construction_stage&& src) = default;
 ~construction_stage() = default;
};

struct constructable
{
 static std::vector<constructable*> constructions; // The list of constructions

 int id;
 std::string name; // Name as displayed
 int difficulty; // Carpentry skill level required
 std::vector<construction_stage> stages;
#ifndef SOCRATES_DAIMON
 bool (*able)  (map&, point);
 void (*done)  (game *, point);
 void (*done_pl)  (player&);
#endif

private:
 constructable(int Id, std::string Name, int Diff
#ifndef SOCRATES_DAIMON
     , bool (*Able) (map&, point),
       void (*Done) (game *, point)
#endif
 ) :
  id (Id), name (Name), difficulty (Diff)
#ifndef SOCRATES_DAIMON
     , able (Able), done (Done), done_pl(nullptr)
#endif
 {};

#ifndef SOCRATES_DAIMON
 constructable(int Id, std::string Name, int Diff
     , bool (*Able) (map&, point)
 ) :
     id(Id), name(Name), difficulty(Diff)
     , able(Able), done(nullptr), done_pl(nullptr)
 {};

 constructable(int Id, std::string Name, int Diff
     , bool (*Able) (map&, point),
     void (*Done) (player&)
 ) :
     id(Id), name(Name), difficulty(Diff)
     , able(Able), done(nullptr), done_pl(Done)
 {};
#endif

public:
 static void init();
};

#endif
