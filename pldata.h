#ifndef _PLDATA_H_
#define _PLDATA_H_

#include "enums.h"
#include "enum_json.h"
#include "pldata_enum.h"

#include <vector>

DECLARE_JSON_ENUM_SUPPORT(dis_type)
DECLARE_JSON_ENUM_SUPPORT(add_type)

struct stat_delta {
    int Str;
    int Dex;
    int Int;
    int Per;
};

static_assert(sizeof(stat_delta) == sizeof(int[4])); // for reinterpret_cast in player::disp_info

struct disease
{
 dis_type type;
 int intensity;
 int duration;
 disease(dis_type t = DI_NULL, int d = 0, int i = 0) noexcept : type(t),intensity(i),duration(d) {}

 int speed_boost() const;
 const char* name() const;
 std::string invariant_desc() const;
};

struct addiction
{
 add_type type;
 int intensity;
 int sated;	// time-scaled
 addiction() : type(ADD_NULL),intensity(0),sated(600) {}
 addiction(add_type t, int i = 1) : type(t), intensity(i), sated(600) {}
};

stat_delta addict_stat_effects(const addiction& add);   // \todo -> member function
std::string addiction_name(const addiction& cur);   // \todo -> member function
std::string addiction_text(const addiction& cur);   // \todo -> member function

DECLARE_JSON_ENUM_SUPPORT(activity_type)

DECLARE_JSON_ENUM_SUPPORT(pl_flag)
DECLARE_JSON_ENUM_SUPPORT_ATYPICAL(hp_part,0)

#endif
