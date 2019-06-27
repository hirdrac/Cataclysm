#ifndef _MORALE_H_
#define _MORALE_H_

#include <string>

#define MIN_MORALE_READ		(-40)
#define MIN_MORALE_CRAFT	(-50)

enum morale_type
{
 MORALE_NULL = 0,
 MORALE_FOOD_GOOD,
 MORALE_MUSIC,
 MORALE_MARLOSS,
 MORALE_FEELING_GOOD,

 MORALE_CRAVING_NICOTINE,
 MORALE_CRAVING_CAFFEINE,
 MORALE_CRAVING_ALCOHOL,
 MORALE_CRAVING_OPIATE,
 MORALE_CRAVING_SPEED,
 MORALE_CRAVING_COCAINE,

 MORALE_FOOD_BAD,
 MORALE_VEGETARIAN,
 MORALE_WET,
 MORALE_FEELING_BAD,
 MORALE_KILLED_INNOCENT,
 MORALE_KILLED_FRIEND,
 MORALE_KILLED_MONSTER,

 MORALE_MOODSWING,
 MORALE_BOOK,

 MORALE_SCREAM,

 NUM_MORALE_TYPES
};

DECLARE_JSON_ENUM_SUPPORT(morale_type)

struct morale_point
{
 static const std::string data[NUM_MORALE_TYPES];

 morale_type type;
 const itype* item_type;
 int bonus;

 morale_point(morale_type T = MORALE_NULL, const itype* I = NULL, int B = 0) :
              type (T), item_type (I), bonus (B) {};
 morale_point(std::istream& is);
 friend std::ostream& operator<<(std::ostream& os, const morale_point& src);

 std::string name() const;
};
 
#endif
