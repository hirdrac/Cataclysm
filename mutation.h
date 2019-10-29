#ifndef _MUTATION_H_
#define _MUTATION_H_

#include "pldata.h"	// See pldata.h for mutations--they're actually pl_flags

enum mutation_category
{
 MUTCAT_NULL = 0,
 MUTCAT_LIZARD,
 MUTCAT_BIRD,
 MUTCAT_FISH,
 MUTCAT_BEAST,
 MUTCAT_CATTLE,
 MUTCAT_INSECT,
 MUTCAT_PLANT,
 MUTCAT_SLIME,
 MUTCAT_TROGLO,
 MUTCAT_CEPHALOPOD,
 MUTCAT_SPIDER,
 MUTCAT_RAT,
 NUM_MUTATION_CATEGORIES
};

DECLARE_JSON_ENUM_SUPPORT_ATYPICAL(mutation_category,0)

struct trait {
	std::string name;
	int points;		// How many points it costs in character creation
	int visiblity;		// How visible it is--see below, at PF_MAX
	int ugliness;		// How ugly it is--see below, at PF_MAX
	std::string description;
};

// mutations_from_category() defines the lists; see mutation_data.cpp
std::vector<pl_flag> mutations_from_category(mutation_category cat);

// Zaimoni, 2018-07-10: C:DDA and C:Bright Nights indicate we need 
// to prepare for a constructor that takes a JSON representation.
struct mutation_branch
{
 static mutation_branch data[PF_MAX2]; // Mutation data
 static const trait traits[PF_MAX2];	// descriptions for above

 bool valid = false; // True if this is a valid mutation (only used for flags < PF_MAX)
 std::vector<pl_flag> prereqs; // Prerequisites; Only one is required
 std::vector<pl_flag> cancels; // Mutations that conflict with this one
 std::vector<pl_flag> replacements; // Mutations that replace this one
 std::vector<pl_flag> additions; // Mutations that add to this one

 static void init();
};
 

#endif
