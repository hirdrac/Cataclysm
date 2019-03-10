// master implementation file for new-style saveload support

#include "mongroup.h"
#include "mapdata.h"
#include "saveload.h"
#include <string>
#include <istream>
#include <ostream>

// legacy implementations assume text mode streams
// this only makes a difference for ostream, and may not be correct (different API may be needed)

#ifdef BINARY_STREAMS
#define I_SEP
#else
#define I_SEP << " "
#endif

#define IO_OPS_ENUM(TYPE)	\
std::istream& operator>>(std::istream& is, TYPE& dest)	\
{	\
	int tmp;	\
	is >> tmp;	\
	dest = (TYPE)tmp;	\
	return is;	\
}	\
	\
std::ostream& operator<<(std::ostream& os, TYPE src)	\
{	\
	return os << int(src);	\
}


std::istream& operator>>(std::istream& is, point& dest)
{
	return is >> dest.x >> dest.y;
}

std::ostream& operator<<(std::ostream& os, const point& src)
{
	return os << src.x I_SEP << src.y;
}

std::istream& operator>>(std::istream& is, tripoint& dest)
{
	return is >> dest.x >> dest.y >> dest.z;
}

std::ostream& operator<<(std::ostream& os, const tripoint& src)
{
	return os << src.x I_SEP << src.y  I_SEP << src.z;
}

IO_OPS_ENUM(bionic_id)

bionic::bionic(std::istream& is)
{
	is >> id >> invlet >> powered >> charge;
}

std::ostream& operator<<(std::ostream& os, const bionic& src)
{
	return os << src.id I_SEP << src.invlet I_SEP << src.powered I_SEP << src.charge;
}

mongroup::mongroup(std::istream& is)
{
  int tmp_type;
  int tmp_radius;
  is >> tmp_type >> pos >> tmp_radius >> population;	// XXX note absence of src.dying: saveload cancels that
  type = (moncat_id)tmp_type;
  radius = tmp_radius;
}

std::ostream& operator<<(std::ostream& os, const mongroup& src)
{
  return os << src.type I_SEP << src.pos I_SEP << int(src.radius) I_SEP << src.population << std::endl;	// XXX note absence of src.dying: saveload cancels that
}

IO_OPS_ENUM(field_id)

std::istream& operator>>(std::istream& is, field& dest)
{
	int d;
	is >> dest.type >> d >> dest.age;
	dest.density = d;
	return is;
}

std::ostream& operator<<(std::ostream& os, const field& src)
{
	return os << src.type I_SEP << int(src.density) I_SEP << src.age;
}

spawn_point::spawn_point(std::istream& is)
{
	char tmpfriend;
	int t;
	is >> t >> count >> pos >> faction_id >> mission_id >> tmpfriend >> name;
	type = (mon_id)t;
	friendly = '1' == tmpfriend;
}

std::ostream& operator<<(std::ostream& os, const spawn_point& src)
{
	return os << int(src.type) I_SEP << src.count I_SEP << src.pos I_SEP <<
		src.faction_id I_SEP << src.mission_id << (src.friendly ? " 1 " : " 0 ") <<
		src.name;
}

std::istream& operator>>(std::istream& is, vehicle_part& dest)
{
	std::string databuff;
	int pid, pnit;

	is >> pid;
	dest.id = vpart_id(pid);

	is >> dest.mount_d >> dest.hp >> dest.amount >> dest.blood >> pnit;
	dest.items.clear();
	std::getline(is, databuff); // Clear EoL
	for (int j = 0; j < pnit; j++) {
			std::getline(is, databuff);
			item itm(databuff);
			dest.items.push_back(itm);
			int ncont;
			is >> ncont; // how many items inside container
			std::getline(is, databuff); // Clear EoL
			for (int k = 0; k < ncont; k++) {
				std::getline(is, databuff);
				item citm(databuff);
				dest.items.back().put_in(citm);
			}
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const vehicle_part& src)
{
	os << src.id << " " << src.mount_d << " " << src.hp << " " << src.amount << " " << src.blood << " " << src.items.size() << std::endl;
	for (int i = 0; i < src.items.size(); i++) {
		os << src.items[i].save_info() << std::endl;     // item info
		os << src.items[i].contents.size() << std::endl; // how many items inside this item
		for (int l = 0; l < src.items[i].contents.size(); l++)
			os << src.items[i].contents[l].save_info() << std::endl; // contents info
	}
	return os;
}
