#include "mondeath.h"
#include "monster.h"
#include "game.h"
#include "rng.h"
#include "line.h"
#include "recent_msg.h"

#include <sstream>

void mdeath::normal(game *g, monster *z)
{
 if (g->u_see(z)) messages.add("It dies!");
 if (z->made_of(FLESH) && z->has_flag(MF_WARM)) {
  auto& f = g->m.field_at(z->pos);
  if (f.type == fd_blood) {
	  if (f.density < 3) f.density++;
  } else g->m.add_field(g, z->pos, fd_blood, 1);
 }
// Drop a dang ol' corpse
// If their hp is less than -50, we destroyed them so badly no corpse was left
 if ((z->hp >= -50 || z->hp >= 0 - 2 * z->type->hp) && (z->made_of(FLESH) || z->made_of(VEGGY))) {
  g->m.add_item(z->pos, item(messages.turn, z->type->id));
 }
}

void mdeath::acid(game *g, monster *z)
{
 if (g->u_see(z)) messages.add("The %s's corpse melts into a pool of acid.", z->name().c_str());
 g->m.add_field(g, z->pos, fd_acid, 3);
}

void mdeath::boomer(game *g, monster *z)
{
 g->sound(z->pos, 24, "a boomer explode!");
 for (int i = -1; i <= 1; i++) {
  for (int j = -1; j <= 1; j++) {
   const point dest(z->pos.x + i, z->pos.y + j);
   g->m.bash(dest, 10);	// no need to spam player with sounds
   {
   auto& f = g->m.field_at(dest);
   if (f.type == fd_bile) {
	   if (f.density < 3) f.density++;
   } else g->m.add_field(g, dest, fd_bile, 1);
   }
   if (monster* const m_at = g->mon(dest)) {
	m_at->stumble(g, false);
	m_at->moves -= 250;
   }
  }
 }
 if (rl_dist(z->pos, g->u.pos) == 1)
  g->u.infect(DI_BOOMERED, bp_eyes, 2, 24);	// V 0.2.1 npcs
}

void mdeath::kill_vines(game *g, monster *z)
{
 std::vector<int> vines;
 std::vector<int> hubs;
 for (int i = 0; i < g->z.size(); i++) {
  if (g->z[i].type->id == mon_creeper_hub && g->z[i].pos != z->pos) hubs.push_back(i);
  if (g->z[i].type->id == mon_creeper_vine) vines.push_back(i);
 }

 for (int i = 0; i < vines.size(); i++) {
  monster *vine = &(g->z[vines[i]]);
  int dist = rl_dist(vine->pos, z->pos);
  bool closer_hub = false;
  for (int j = 0; j < hubs.size() && !closer_hub; j++) {
   if (rl_dist(vine->pos, g->z[hubs[j]].pos) < dist) {
    closer_hub = true;
	break;
   }
  }
  if (!closer_hub) vine->hp = 0;
 }
}

void mdeath::vine_cut(game *g, monster *z)
{
 std::vector<monster*> vines;
 for (int x = z->pos.x - 1; x <= z->pos.x + 1; x++) {
  for (int y = z->pos.y - 1; y <= z->pos.y + 1; y++) {
   if (x == z->pos.x && y == z->pos.y) y++; // Skip ourselves
   monster* m_at = g->mon(x,y);
   if (m_at && m_at->type->id == mon_creeper_vine) vines.push_back(m_at);
  }
 }

 for(auto vine : vines) {
  bool found_neighbor = false;
  for (int x = vine->pos.x - 1; x <= vine->pos.x + 1 && !found_neighbor; x++) {
   for (int y = vine->pos.y - 1; y <= vine->pos.y + 1 && !found_neighbor; y++) {
    if (x != z->pos.x || y != z->pos.y) { // Not the dying vine
	 monster* const m_at = g->mon(x,y);
     if (m_at && (m_at->type->id == mon_creeper_hub ||
		          m_at->type->id == mon_creeper_vine  ))
      found_neighbor = true;
    }
   }
  }
  if (!found_neighbor) vine->hp = 0;
 }
}

void mdeath::triffid_heart(game *g, monster *z)
{
 messages.add("The root walls begin to crumble around you.");
 g->add_event(EVENT_ROOTS_DIE, int(messages.turn) + 100);
}

void mdeath::fungus(game *g, monster *z)
{
 monster spore(mtype::types[mon_spore]);
 g->sound(z->pos, 10, "Pouf!");
 for (int i = -1; i <= 1; i++) {
  for (int j = -1; j <= 1; j++) {
   point dest(z->pos.x + i, z->pos.y + j);
   if (g->m.move_cost(dest) > 0 && one_in(5)) {
	monster* const m_at = g->mon(dest);
    if (m_at) {	// Spores hit a monster
     if (g->u_see(dest)) messages.add("The %s is covered in tiny spores!", m_at->name().c_str());
     if (!m_at->make_fungus(g)) g->kill_mon(*m_at, (z->friendly != 0));
    } else if (g->u.pos == dest)	// V 0.2.1 \todo infect npcs
     g->u.infect(DI_SPORES, bp_mouth, 4, 30);
    else {
     spore.spawn(dest);
     g->z.push_back(spore);
    }
   }
  }
 }
}

void mdeath::fungusawake(game *g, monster *z)
{
 g->z.push_back(monster(mtype::types[mon_fungaloid], z->pos.x, z->pos.y));
}

void mdeath::disintegrate(game *g, monster *z)
{
 if (g->u_see(z)) messages.add("It disintegrates!");
}

void mdeath::worm(game *g, monster *z)
{
 if (g->u_see(z)) messages.add("The %s splits in two!", z->name().c_str());

 std::vector <point> wormspots;
 for (int i = -1; i <= 1; i++) {
  for (int j = -1; j <= 1; j++) {
   point worm(z->pos.x + i, z->pos.y + j);
   if (g->m.has_flag(diggable, worm) && !g->mon(worm) && g->u.pos != worm) {
    wormspots.push_back(worm);
   }
  }
 }
 monster worm(mtype::types[mon_halfworm]);
 for (int worms = 0; worms < 2 && wormspots.size() > 0; worms++) {
  const int rn = rng(0, wormspots.size() - 1);
  worm.spawn(wormspots[rn]);
  g->z.push_back(worm);
  wormspots.erase(wormspots.begin() + rn);
 }
}

void mdeath::disappear(game *g, monster *z)
{
 messages.add("The %s disappears!  Was it in your head?", z->name().c_str());
}

void mdeath::guilt(game *g, monster *z)
{
 if (g->u.has_trait(PF_HEARTLESS)) return;	// We don't give a shit!
 if (rl_dist(z->pos, g->u.pos) > 1) return;	// Too far away, we can deal with it
 if (z->hp >= 0) return;	// It probably didn't die from damage
 messages.add("You feel terrible for killing %s!", z->name().c_str());
 g->u.add_morale(MORALE_KILLED_MONSTER, -50, -250);
}

void mdeath::blobsplit(game *g, monster *z)
{
 const int speed = z->speed - rng(30, 50);
 if (speed <= 0) {
  if (g->u_see(z)) messages.add("The %s splatters into tiny, dead pieces.", z->name().c_str());
  return;
 }
 monster blob(mtype::types[(speed < 50 ? mon_blob_small : mon_blob)]);
 blob.speed = speed;
 blob.friendly = z->friendly; // If we're tame, our kids are too
 if (g->u_see(z)) messages.add("The %s splits!", z->name().c_str());
 blob.hp = blob.speed;
 std::vector <point> valid;

 for (int i = -1; i <= 1; i++) {
  for (int j = -1; j <= 1; j++) {
   point test(z->pos.x + i, z->pos.y + j);
   if (g->m.move_cost(test) > 0 && !g->mon(test) && g->u.pos != test)
    valid.push_back(test);
  }
 }
 
 for (int s = 0; s < 2 && valid.size() > 0; s++) {
  const int rn = rng(0, valid.size() - 1);
  blob.spawn(valid[rn]);
  g->z.push_back(blob);
  valid.erase(valid.begin() + rn);
 }
}

void mdeath::melt(game *g, monster *z)
{
 if (g->u_see(z)) messages.add("The %s melts away!", z->name().c_str());
}

void mdeath::amigara(game *g, monster *z)
{
 if (g->u.has_disease(DI_AMIGARA)) {
  int count = 0;
  for(const auto& _mon : g->z) {
   if (_mon.type->id == mon_amigara_horror) count++;
  }
  if (count <= 1) { // We're the last!
   g->u.rem_disease(DI_AMIGARA);
   messages.add("Your obsession with the fault fades away...");
   g->m.add_item(z->pos, item(g->new_artifact(), messages.turn));
  }
 }
 normal(g, z);
}

void mdeath::thing(game *g, monster *z)
{
 g->z.push_back(monster(mtype::types[mon_thing], z->pos.x, z->pos.y));
}

void mdeath::explode(game *g, monster *z)
{
 int size;
 switch (z->type->size) {
  case MS_TINY:   size =  4; break;
  case MS_SMALL:  size =  8; break;
  case MS_MEDIUM: size = 14; break;
  case MS_LARGE:  size = 20; break;
  case MS_HUGE:   size = 26; break;
 }
 g->explosion(z->pos, size, 0, false);
}

void mdeath::ratking(game *g, monster *z)
{
 g->u.rem_disease(DI_RAT);
}

void mdeath::gameover(game *g, monster *z)
{
 messages.add("Your %s is destroyed!  GAME OVER!", z->name().c_str());
 g->u.hp_cur[hp_torso] = 0;
}

void mdeath::kill_breathers(game *g, monster *z)
{
 for (int i = 0; i < g->z.size(); i++) {
  if (g->z[i].type->id == mon_breather_hub || g->z[i].type->id == mon_breather)
   g->z[i].dead = true;
 }
}
