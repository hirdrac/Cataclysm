#include "game.h"
#include "keypress.h"
#include "output.h"
#include "line.h"
#include "skill.h"
#include "rng.h"
#include "item.h"
#include "options.h"
#include "mondeath.h"
#include "gui.hpp"
#include "posix_time.h"
#include "recent_msg.h"

#include <math.h>

// monster type currently doesn't have field_id in scope
field_id bleeds(const monster& mon)
{
    if (!mon.made_of(FLESH)) return fd_null;
    if (mon.type->dies == &mdeath::boomer) return fd_bile;
    if (mon.type->dies == &mdeath::acid) return fd_acid;
    return fd_blood;
}

#define LONG_RANGE 10

void shoot_monster(game *g, player &p, monster &mon, int &dam, double goodhit);
void shoot_player(game *g, player &p, player *h, int &dam, double goodhit);

static void ammo_effects(game* g, GPS_loc loc, long flags)
{
    if (flags & mfb(IF_AMMO_EXPLOSIVE)) loc.explosion(24, 0, false);
    if (flags & mfb(IF_AMMO_FRAG)) loc.explosion(12, 28, false);
    if (flags & mfb(IF_AMMO_NAPALM)) loc.explosion(18, 0, true);
    if (flags & mfb(IF_AMMO_EXPLOSIVE_BIG)) loc.explosion(40, 0, false);

    static auto teargas = [&](const point& delta) {
        auto dest(loc + delta);
        dest.add(field(fd_tear_gas, 3));
    };

    static auto smoke = [&](const point& delta) {
        auto dest(loc + delta);
        dest.add(field(fd_tear_gas, 3));
    };

    if (flags & mfb(IF_AMMO_TEARGAS)) forall_do_inclusive(within_rldist<2>, teargas);
    if (flags & mfb(IF_AMMO_SMOKE)) forall_do_inclusive(within_rldist<1>, smoke);
    if (flags & mfb(IF_AMMO_FLASHBANG)) g->flashbang(loc);
    if (flags & mfb(IF_AMMO_FLAME)) loc.add(field(fd_fire, 1, 800));
}

// no real advantage to const member function of player over free static function
static int recoil_add(const player &p)
{
	const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
	int ret = p.weapon.recoil();
	ret -= rng(p.str_cur / 2, p.str_cur);
	ret -= rng(0, p.sklevel[firing->skill_used] / 2);
	return (0 < ret) ? ret : 0;
}

static void splatter(const std::vector<GPS_loc>& trajectory, int dam, monster* mon = nullptr)
{
    field_id blood = bleeds(mon);   // null mon would be a player
    if (!blood) return;

    int distance = 1;
    if (dam > 50) distance = 3;
    else if (dam > 20) distance = 2;

    for (auto& tar : continue_line(trajectory, distance)) {
        auto& fd = tar.field_at();
        if (blood == fd.type) {
            if (3 > fd.density) fd.density++;
        }
        else tar.add(field(blood, 1));
    }
}


static int time_to_fire(player& p, const it_gun* const firing)
{
    int time = 0;

    switch (firing->skill_used) {
    case sk_pistol: return (6 < p.sklevel[sk_pistol]) ? 10 : (80 - 10 * p.sklevel[sk_pistol]);
    case sk_shotgun: return (3 < p.sklevel[sk_shotgun]) ? 70 : (150 - 25 * p.sklevel[sk_shotgun]);
    case sk_smg: return (5 < p.sklevel[sk_smg]) ? 20 : (80 - 10 * p.sklevel[sk_smg]);
    case sk_rifle: return (8 < p.sklevel[sk_rifle]) ? 30 : (150 - 15 * p.sklevel[sk_rifle]);
    case sk_archery: return (8 < p.sklevel[sk_archery]) ? 20 : (220 - 25 * p.sklevel[sk_archery]);
    case sk_launcher: return (8 < p.sklevel[sk_launcher]) ? 30 : (200 - 20 * p.sklevel[sk_launcher]);

    default:
        debugmsg("Why is shooting %s using %s skill?", (firing->name).c_str(), skill_name(skill(firing->skill_used))); // this is a check for C memory corruption, should be caught at game start
        return 0;
    }

    return time;
}

static std::optional<std::pair<int, const char*> > gun_sound(const item& gun, bool burst)
{
    switch (gun.curammo->type)
    {
    case AT_FUSION:
    case AT_BATT:
    case AT_PLUT: return std::pair(8, "Fzzt!");
    case AT_40MM: return std::pair(8, "Thunk!");
    case AT_GAS: return std::pair(4, "Fwoosh!");
    case AT_BOLT:
    case AT_ARROW: return std::nullopt;  // negligible compared to firearms
    default: {
        int noise = gun.noise();
        if (noise < 5) return std::pair(noise, burst ? "Brrrip!" : "plink!");
        else if (noise < 25) return std::pair(noise, burst ? "Brrrap!" : "bang!");
        else if (noise < 60) return std::pair(noise, burst ? "P-p-p-pow!" : "blam!");
        else return std::pair(noise, burst ? "Kaboom!!" : "kerblam!");;
    }
    break;
    }
}

static void make_gun_sound_effect(game* g, player& p, bool burst)
{
    if (const auto sound = gun_sound(p.weapon, burst)) g->sound(p.pos, sound->first, sound->second);
}

static int calculate_range(player& p, const point& tar)
{
    int trange = rl_dist(p.pos, tar);
    const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
    if (trange < int(firing->volume / 3) && firing->ammo != AT_SHOT)
        trange = int(firing->volume / 3);
    else if (p.has_bionic(bio_targeting)) {
        trange = int(trange * ((LONG_RANGE < trange) ? .65 : .8));
    }

    if (firing->skill_used == sk_rifle && trange > LONG_RANGE)
        trange = LONG_RANGE + .6 * (trange - LONG_RANGE);

    return trange;
}

static int calculate_range(player& p, const GPS_loc& tar)
{
    int trange = rl_dist(p.GPSpos, tar);
    const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
    if (trange < int(firing->volume / 3) && firing->ammo != AT_SHOT)
        trange = int(firing->volume / 3);
    else if (p.has_bionic(bio_targeting)) {
        trange = int(trange * ((LONG_RANGE < trange) ? .65 : .8));
    }

    if (firing->skill_used == sk_rifle && trange > LONG_RANGE)
        trange = LONG_RANGE + .6 * (trange - LONG_RANGE);

    return trange;
}

static double calculate_missed_by(player& p, int trange)	// XXX real-world deviation is normal distribution with arithmetic mean zero \todo fix
{
    const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
    // Calculate deviation from intended target (assuming we shoot for the head)
    double deviation = 0.; // Measured in quarter-degrees
  // Up to 1.5 degrees for each skill point < 4; up to 1.25 for each point > 4
    if (p.sklevel[firing->skill_used] < 4)
        deviation += rng(0, 6 * (4 - p.sklevel[firing->skill_used]));
    else if (p.sklevel[firing->skill_used] > 4)
        deviation -= rng(0, 5 * (p.sklevel[firing->skill_used] - 4));

    if (p.sklevel[sk_gun] < 3)
        deviation += rng(0, 3 * (3 - p.sklevel[sk_gun]));
    else
        deviation -= rng(0, 2 * (p.sklevel[sk_gun] - 3));

    deviation += p.ranged_dex_mod();
    deviation += p.ranged_per_mod();

    deviation += rng(0, 2 * p.encumb(bp_arms)) + rng(0, 4 * p.encumb(bp_eyes));

    deviation += rng(0, p.weapon.curammo->accuracy);
    deviation += rng(0, p.weapon.accuracy());
    const int adj_recoil = p.recoil + p.driving_recoil;
    deviation += rng(adj_recoil / 4, adj_recoil);

    // .013 * trange is a computationally cheap version of finding the tangent.
    // (note that .00325 * 4 = .013; .00325 is used because deviation is a number
    //  of quarter-degrees)
    // It's also generous; missed_by will be rather short.
    return (.00325 * deviation * trange);
}

struct shot_that {
    game* const g;
    player& p;
    const std::vector<GPS_loc>& trajectory;
    int dam;
    double missed_by;
    bool missed;
    bool on_target;

    shot_that(player& p, const std::vector<GPS_loc>& trajectory, int dam, double missed_by, bool missed, int offset) noexcept : g(game::active()), p(p), trajectory(trajectory), dam(dam), missed_by(missed_by), missed(missed), on_target(trajectory.size() - 1 <= offset) {}
    shot_that(const shot_that& src) = delete;
    shot_that(shot_that&& src) = delete;
    shot_that& operator=(const shot_that& src) = delete;
    shot_that& operator=(shot_that&& src) = delete;
    ~shot_that() = default;

    auto operator()(monster* target) {
        if (   (!target->has_flag(MF_DIGS) || rl_dist(p.GPSpos, target->GPSpos) <= 1)
            && ((!missed && on_target) || one_in((5 - int(target->type->size))))) {

            double goodhit = missed_by;
            if (!on_target) goodhit = (rand() / (RAND_MAX + 1.0)) / 2; // Unintentional hit

        // Penalize for the monster's speed
            if (target->speed > 80) goodhit *= target->speed / 80.;

            auto blood_traj(trajectory);
            blood_traj.insert(blood_traj.begin(), p.GPSpos);
            splatter(blood_traj, dam, target);
            shoot_monster(g, p, *target, dam, goodhit);
        }
    }
    auto operator()(player* target) {
        if ((!missed || one_in(3))) {
            double goodhit = missed_by;
            if (!on_target) goodhit = (rand() / (RAND_MAX + 1.0)) / 2;	 // Unintentional hit

            auto blood_traj(trajectory);
            blood_traj.insert(blood_traj.begin(), p.GPSpos);
            splatter(blood_traj, dam);
            shoot_player(g, p, target, dam, goodhit);
        }
    }
};

void game::fire(player& p, std::vector<GPS_loc>& trajectory, bool burst)
{
#ifndef NDEBUG
    assert(p.weapon.is_gun());
#else
    if (!p.weapon.is_gun()) {
        debuglog("%s tried to fire a non-gun (%s).", p.name.c_str(), p.weapon.tname().c_str());
        return;
    }
#endif
    auto tar = trajectory.back();
    decltype(auto) origin = p.GPSpos; // \todo we want to allow for remote-controlled weapons

    item ammotmp;
    if (p.weapon.has_flag(IF_CHARGE)) { // It's a charger gun, so make up a type
   // Charges maxes out at 8.
        it_ammo* const tmpammo = dynamic_cast<it_ammo*>(item::types[itm_charge_shot]);	// XXX should be copy-construction \todo fix
        tmpammo->damage = p.weapon.charges * p.weapon.charges;
        tmpammo->pierce = (p.weapon.charges >= 4 ? (p.weapon.charges - 3) * 2.5 : 0);
        tmpammo->range = 5 + p.weapon.charges * 5;
        if (p.weapon.charges <= 4)
            tmpammo->accuracy = 14 - p.weapon.charges * 2;
        else // 5, 12, 21, 32
            tmpammo->accuracy = p.weapon.charges * (p.weapon.charges - 4);
        tmpammo->recoil = tmpammo->accuracy * .8;
        tmpammo->item_flags = 0;
        if (p.weapon.charges == 8) tmpammo->item_flags |= mfb(IF_AMMO_EXPLOSIVE_BIG);
        else if (p.weapon.charges >= 6) tmpammo->item_flags |= mfb(IF_AMMO_EXPLOSIVE);
        if (p.weapon.charges >= 5) tmpammo->item_flags |= mfb(IF_AMMO_FLAME);
        else if (p.weapon.charges >= 4) tmpammo->item_flags |= mfb(IF_AMMO_INCENDIARY);

        ammotmp = item(tmpammo, 0);
        p.weapon.curammo = tmpammo;
        p.weapon.active = false;
        p.weapon.charges = 0;

    }
    else // Just a normal gun. If we're here, we know curammo is valid.
        ammotmp = item(p.weapon.curammo, 0);

    ammotmp.charges = 1;
    unsigned int flags = p.weapon.curammo->item_flags;
    // Bolts and arrows are silent
    const bool is_bolt = (p.weapon.curammo->type == AT_BOLT || p.weapon.curammo->type == AT_ARROW);

    const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
    if (p.has_trait(PF_TRIGGERHAPPY) && one_in(30)) burst = true;
    if (burst && p.weapon.burst_size() < 2) burst = false; // Can't burst fire a semi-auto

    const bool u_see_shooter = (bool)u.see(p.pos);
    // Use different amounts of time depending on the type of gun and our skill
    p.moves -= time_to_fire(p, firing);
    // Decide how many shots to fire
    int num_shots = 1;
    if (burst) num_shots = p.weapon.burst_size();
    if (num_shots > p.weapon.charges && !p.weapon.has_flag(IF_CHARGE)) num_shots = p.weapon.charges;

#ifndef NDEBUG
    assert(0 < num_shots);
#else
    if (0 >= num_shots) debuglog("game::fire() - num_shots = 0!");
#endif

    // Make a sound at our location - Zombies will chase it
    make_gun_sound_effect(this, p, burst);
    // Set up a timespec for use in the nanosleep function below
    timespec ts = { 0,  BULLET_SPEED };

    bool missed = false;
    for (int curshot = 0; curshot < num_shots; curshot++) {
        // Burst-fire weapons allow us to pick a new target after killing the first
        monster* m_at = mon(tar);	// code below assumes kill processing is not "immediate"
        if (curshot > 0 && (!m_at || m_at->hp <= 0)) {
            std::vector<GPS_loc> new_targets;
            for (int radius = 1; radius <= 2 + p.sklevel[sk_gun] && new_targets.empty(); radius++) {
                for (int diff = 0 - radius; diff <= radius; diff++) {
                    auto test(tar + point(diff, -radius));
                    m_at = mon(test);
                    if (m_at && 0 < m_at->hp && m_at->is_enemy(&p)) new_targets.push_back(test);

                    test = tar + point(diff, radius);
                    m_at = mon(test);
                    if (m_at && 0 < m_at->hp && m_at->is_enemy(&p)) new_targets.push_back(test);

                    if (diff != 0 - radius && diff != radius) { // Corners were already checked
                        test = tar + point(-radius, diff);
                        m_at = mon(test);
                        if (m_at && 0 < m_at->hp && m_at->is_enemy(&p)) new_targets.push_back(test);

                        test = tar + point(radius, diff);
                        m_at = mon(test);
                        if (m_at && 0 < m_at->hp && m_at->is_enemy(&p)) new_targets.push_back(test);
                    }
                }
            }
retry_new_target:
            if (!new_targets.empty()) {
                const auto n = rng(0, new_targets.size() - 1);
                tar = new_targets[n];
                if (auto line = origin.sees(tar, 0)) trajectory = std::move(*line);
                else {
                    new_targets.erase(new_targets.begin() + n);
                    goto retry_new_target;
                }
            }
            else if ((!p.has_trait(PF_TRIGGERHAPPY) || one_in(3)) &&
                (p.sklevel[sk_gun] >= 7 || one_in(7 - p.sklevel[sk_gun])))
                return; // No targets, so return
        }
        // Use up a round (or 100)
        if (p.weapon.has_flag(IF_FIRE_100))
            p.weapon.charges -= 100;
        else
            p.weapon.charges--;

        int trange = calculate_range(p, tar);
        double missed_by = calculate_missed_by(p, trange);
        // Calculate a penalty based on the monster's speed
        double monster_speed_penalty = 1.;
        if (monster* const m_at = mon(tar)) {
            monster_speed_penalty = double(m_at->speed) / 80.;
            if (monster_speed_penalty < 1.) monster_speed_penalty = 1.;
        }

        const auto recoil_delta = recoil_add(p);
        if (curshot > 0) {
            if (recoil_delta % 2 == 1) p.recoil++;
            p.recoil += recoil_delta / 2;
        }
        else
            p.recoil += recoil_delta;

        if (missed_by >= 1.) {
            // We missed D:
            // Shoot a random nearby space?
            const int delta = int(sqrt(missed_by));
            tar += point(rng(-delta, delta), rng(-delta, delta));
            if (auto line = origin.sees(tar, -1)) trajectory = std::move(*line); // \todo prevent false-negatives, but don't infinite-loop
            missed = true;
            if (!burst) {
                if (&p == &u) messages.add("You miss!");
                else if (u_see_shooter) messages.add("%s misses!", p.name.c_str());
            }
        }
        else if (missed_by >= .7 / monster_speed_penalty) {
            // Hit the space, but not necessarily the monster there
            missed = true;
            if (!burst) {
                if (&p == &u) messages.add("You barely miss!");
                else if (u_see_shooter) messages.add("%s barely misses!", p.name.c_str());
            }
        }

        int dam = p.weapon.gun_damage();
        for (int i = 0; i < trajectory.size() && (dam > 0 || (flags & IF_AMMO_FLAME)); i++) {
            if (i > 0)
                map::drawsq(w_terrain, u, trajectory[i - 1], false, true);
            // Drawing the bullet uses player u, and not player p, because it's drawn
            // relative to YOUR position, which may not be the gunman's position.
            if (u.see(trajectory[i])) {
                if (const auto pos = toScreen(trajectory[i])) {
                    char bullet = (flags & mfb(IF_AMMO_FLAME)) ? '#' : '*';
                    const point pt(*pos + point(VIEW_CENTER) - u.pos);
                    mvwputch(w_terrain, pt.y, pt.x, c_red, bullet);
                    wrefresh(w_terrain);
                    if (&p == &u) nanosleep(&ts, nullptr);
                }
            }

            if (dam <= 0) { // Ran out of momentum.
                ammo_effects(this, trajectory[i], flags);
                if (is_bolt &&
                    ((p.weapon.curammo->m1 == WOOD && !one_in(4)) ||
                        (p.weapon.curammo->m1 != WOOD && !one_in(15))))
                    trajectory[i].add(std::move(ammotmp));
                if (p.weapon.charges == 0) p.weapon.curammo = nullptr;
                return;
            }

            // If there's a monster in the path of our bullet, and either our aim was true,
            //  OR it's not the monster we were aiming at and we were lucky enough to hit it
            if (auto _mob = mob_at(trajectory[i])) {
                std::visit(shot_that(p, trajectory, dam, missed_by, missed, i), *_mob);
            } else {
                trajectory[i].shoot(dam, i == trajectory.size() - 1, flags);
            }
        } // Done with the trajectory!

        auto last(trajectory.back());
        ammo_effects(this, last, flags);

        if (0 == last.move_cost()) last = trajectory[trajectory.size() - 2];
        if (is_bolt &&
            ((p.weapon.curammo->m1 == WOOD && !one_in(5)) ||	// leave this verbose in case we want additional complexity
                (p.weapon.curammo->m1 != WOOD && !one_in(15))))
            last.add(std::move(ammotmp));
    }

    if (p.weapon.charges == 0) p.weapon.curammo = nullptr;
}

void game::throw_item(player& p, item&& thrown, std::vector<GPS_loc>& trajectory)
{
    auto tar = trajectory.back();
    decltype(auto) origin = p.GPSpos; // \todo we want to allow for remote-controlled weapons

    int deviation = 0;
    int trange = 1.5 * rl_dist(origin, tar);

    // Throwing attempts below "Basic Competency" level are extra-bad
    if (p.sklevel[sk_throw] < 3) deviation += rng(0, 8 - p.sklevel[sk_throw]);

    if (p.sklevel[sk_throw] < 8) deviation += rng(0, 8 - p.sklevel[sk_throw]);
    else deviation -= p.sklevel[sk_throw] - 6;

    deviation += p.throw_dex_mod();

    if (p.per_cur < 6) deviation += rng(0, 8 - p.per_cur);
    else if (p.per_cur > 8) deviation -= p.per_cur - 8;

    const int thrown_vol = thrown.volume();
    deviation += rng(0, p.encumb(bp_hands) * 2 + p.encumb(bp_eyes) + 1);
    if (5 < thrown_vol) deviation += rng(0, 1 + (thrown_vol - 5) / 4);
    else if (0 >= thrown_vol) deviation += rng(0, 3);

    deviation += rng(0, 1 + abs(p.str_cur - thrown.weight()));

    double missed_by = .01 * deviation * trange;
    bool missed = false;

    if (missed_by >= 1) {
        // We missed D:
        // Shoot a random nearby space?
        if (missed_by > 9) missed_by = 9;
        const int delta = int(sqrt(double(missed_by)));
        tar += point(rng(-delta, delta), rng(-delta, delta));
        if (auto traj = origin.sees(tar, -1)) trajectory = std::move(*traj);
        missed = true;
        p.subjective_message("You miss!");
    } else if (missed_by >= .6) {
        // Hit the space, but not necessarily the monster there
        missed = true;
        p.subjective_message("You barely miss!");
    }

    const int thrown_wgt = thrown.weight();

    std::string message;
    int dam = (thrown_wgt / 4 + thrown.type->melee_dam / 2 + p.str_cur / 2) / (2.0 + thrown_vol / 4);
    clamp_lb(dam, thrown_wgt * 3);

    int i = 0;
    GPS_loc t;
    for (i = 0; i < trajectory.size() && dam > -10; i++) {
        message = "";
        double goodhit = missed_by;
        t = trajectory[i];
        monster* const m_at = mon(trajectory[i]);
        // If there's a monster in the path of our item, and either our aim was true,
        //  OR it's not the monster we were aiming at and we were lucky enough to hit it
        if (m_at && (!missed || one_in(7 - int(m_at->type->size)))) {
            if (0 < thrown.type->melee_cut && rng(0, 100) < 20 + p.sklevel[sk_throw] * 12) {
                if (!p.is_npc()) {
                    message += " You cut the ";
                    message += m_at->name();
                    message += "!";
                }
                const auto c_armor = m_at->armor_cut();
                if (thrown.type->melee_cut > c_armor) dam += (thrown.type->melee_cut - c_armor);
            }
            if (t.hard_landing(std::move(thrown), &p)) {
                const int glassdam = rng(0, thrown_vol * 2);
                const auto c_armor = m_at->armor_cut();
                if (glassdam > c_armor) dam += (glassdam - c_armor);
            }
            if (i < trajectory.size() - 1) goodhit = (double(rand()) / RAND_MAX) / 2.0;
            if (goodhit < .1 && !m_at->has_flag(MF_NOHEAD)) {
                message = "Headshot!";
                dam = rng(dam, dam * 3);
                p.practice(sk_throw, 5);
            } else if (goodhit < .2) {
                message = "Critical!";
                dam = rng(dam, dam * 2);
                p.practice(sk_throw, 2);
            } else if (goodhit < .4)
                dam = rng(int(dam / 2), int(dam * 1.5));
            else if (goodhit < .5) {
                message = "Grazing hit.";
                dam = rng(0, dam);
            }
            if (!p.is_npc())
                messages.add("%s You hit the %s for %d damage.", message.c_str(), m_at->name().c_str(), dam);
            else if (u.see(t))
                messages.add("%s hits the %s for %d damage.", message.c_str(), m_at->name().c_str(), dam);
            if (m_at->hurt(dam)) kill_mon(*m_at, &p);
            return;
        }
        else // No monster hit, but the terrain might be.
            t.shoot(dam, false, 0);
        if (0 >= t.move_cost()) {
            t = (i > 0) ? trajectory[i - 1] : origin;
            break;
        }
    }
    if (0 >= t.move_cost()) t = (i > 1) ? trajectory[i - 2] : origin;
    if (!t.hard_landing(std::move(thrown), &p)) {
        t.sound(8, "thud.");
    }
}

// At some point, C:Whales also used this to target vehicles for refilling (different UI when forked).
std::optional<std::vector<GPS_loc> > game::target(GPS_loc& tar, const zaimoni::gdi::box<point>& bounds, const std::vector<const monster*>& t, int &target, const std::string& prompt)
{
 std::vector<GPS_loc> ret;

// First, decide on a target among the monsters, if there are any in range
 if (t.size() > 0) {
// Check for previous target
  if (target == -1) {
// If no previous target, target the closest there is
   int closest = INT_MAX;
   for (int i = 0; i < t.size(); i++) {
    if (int dist = rl_dist(t[i]->GPSpos, u.GPSpos); dist < closest) {
     closest = dist;
     target = i;
    }
   }
  }
  tar = t[target]->GPSpos;
 } else
  target = -1;	// No monsters in range, don't use target, reset to -1

 // unclear why we should have a dependency on map dimensions
 WINDOW* w_target = newwin(VIEW - SEEY, PANELX - MINIMAP_WIDTH_HEIGHT, SEEY, VIEW + MINIMAP_WIDTH_HEIGHT);
 wborder(w_target, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                 LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );

 mvwaddstrz(w_target, 1, 1, c_red, prompt.c_str());
 mvwaddstrz(w_target, 2, 1, c_white, "Move cursor to target with directional keys.");
 mvwaddstrz(w_target, 3, 1, c_white, "'<' '>' Cycle targets; 'f' or '.' to fire.");
 mvwaddstrz(w_target, 4, 1, c_white,  "'0' target self; '*' toggle snap-to-target");

 wrefresh(w_target);
 bool snap_to_target = option_table::get()[OPT_SNAP_TO_TARGET];
// The main loop.
 do {
  GPS_loc center_GPS(snap_to_target ? tar : u.GPSpos);
  const point center = toScreen(center_GPS).value();
// Clear the target window.
  for (int i = 5; i < SEEY; i++) {
   for (int j = 1; j < PANELX - MINIMAP_WIDTH_HEIGHT - 2; j++)
    mvwputch(w_target, i, j, c_white, ' ');
  }
  m.draw(w_terrain, u, center);
// Draw the Monsters
  for(const auto& mon : z) {
   if (bounds.contains(mon.pos) && u.see(mon))
    mon.draw(w_terrain, center, false);
  }
// Draw the NPCs
  forall_do([&](const npc& NPC) { if (u.see(NPC)) NPC.draw(w_terrain, center, false); });

  if (tar != u.GPSpos) {
// Calculate the return vector (and draw it too)
// Draw the player
   const point at(u.pos-center+point(VIEW_CENTER));
   if (at.x >= 0 && at.x < VIEW && at.y >= 0 && at.y < VIEW)
    mvwputch(w_terrain, at.y, at.x, u.color(), '@');

   if (auto los = u.GPSpos.sees(tar, -1)) { // Selects a valid line-of-sight
       const auto sight_dist = u.sight_range();
       while (sight_dist < los->size()) los->pop_back();
       draw_mob render_inverted(w_terrain, center, true);
       for (decltype(auto) loc : *los) {
           // NPCs and monsters get drawn with inverted colors
           if (auto mob = mob_at(loc)) std::visit(render_inverted, *mob);
           else {
               if (auto pos = toScreen(loc)) m.drawsq(w_terrain, u, pos->x, pos->y, true, true, center);
           }
       }
       ret = std::move(*los);
   }

   mvwprintw(w_target, 5, 1, "Range: %d", rl_dist(u.GPSpos, tar));

   if (monster* const m_at = mon(tar)) {
       if (u.see(*m_at)) m_at->print_info(u, w_target);
   } else {
       mvwprintw(w_status, 0, 9, "                             ");
       if (snap_to_target)
           mvwputch(w_terrain, VIEW_CENTER, VIEW_CENTER, c_red, '*');
       else {
           auto delta = tar - u.GPSpos;
           if (auto pos = std::get_if<point>(&delta)) {
               const point pt = point(VIEW_CENTER) + *pos;
               mvwputch(w_terrain, pt.y, pt.x, c_red, '*');
           }
       }
   }
  }
  wrefresh(w_target);
  wrefresh(w_terrain);
  wrefresh(w_status);
  refresh();
  int ch = input();
  point dir(get_direction(ch));
  if (dir.x != -2 && ch != '.') {	// Direction character pressed
   draw_mob render(w_terrain, center, false);
   auto pos = toScreen(tar);
   if (auto mob = mob_at(tar)) std::visit(render, *mob);
   else if (u.GPSpos.can_see(tar, -1)) {
       if (pos) m.drawsq(w_terrain, u, pos->x, pos->y, false, true, center);
   } else mvwputch(w_terrain, VIEW_CENTER, VIEW_CENTER, c_black, 'X');

   tar += dir;
   if (pos) {
       *pos += dir;
       point in_bounds_delta(0);
       if (pos->x < bounds.tl_c().x) in_bounds_delta += Direction::E;
       else if (pos->x > bounds.br_c().x) in_bounds_delta += Direction::W;
       if (pos->y < bounds.tl_c().y) in_bounds_delta += Direction::S;
       else if (pos->y > bounds.br_c().y) in_bounds_delta += Direction::N;
       if (point(0) != in_bounds_delta) tar += in_bounds_delta;
   }
  } else if ((ch == '<') && (0 <= target)) {
   target--;
   if (0 > target) target = t.size() - 1;
   tar = t[target]->GPSpos;
  } else if ((ch == '>') && (0 <= target)) {
   target++;
   if (target == t.size()) target = 0;
   tar = t[target]->GPSpos;
  } else if (ch == '.' || ch == 'f' || ch == 'F' || ch == '\n') {
   for (int i = 0; i < t.size(); i++) {
    if (t[i]->GPSpos == tar) target = i;
   }
   return ret;
  } else if (ch == '0') tar = u.GPSpos;
  else if (ch == '*') snap_to_target = !snap_to_target;
  else if (ch == KEY_ESCAPE || ch == 'q') return std::nullopt; // cancel
 } while (true);
}

// monster class currently doesn't have IF_AMMO_... flags visible
static void hit_monster_with_flags(monster& z, unsigned int flags)
{
    static constexpr const material easy_ignite[] = { VEGGY, COTTON, WOOL, PAPER, WOOD };
    static auto m_mat = [&z](material src) { return z.made_of(src); };

    // 2021-05-28: unclear whether always-on-fire monsters can be ignited by ammunition (fields do not ignite them).
    // C:Whales was ignitable by ammunition, but not by fire fields.
    // Cf. monster::ignitable
    if (!z.has_flag(MF_FIREY)) {
        if (flags & mfb(IF_AMMO_FLAME)) {

            if (std::ranges::any_of(easy_ignite, m_mat))
                z.add_effect(ME_ONFIRE, rng(8, 20));
            else if (z.made_of(FLESH))
                z.add_effect(ME_ONFIRE, rng(5, 10));

        }
        else if (flags & mfb(IF_AMMO_INCENDIARY)) {

            if (std::ranges::any_of(easy_ignite, m_mat))
                z.add_effect(ME_ONFIRE, rng(2, 6));
            else if (z.made_of(FLESH) && one_in(4))
                z.add_effect(ME_ONFIRE, rng(1, 4));
        }
    }
}

void shoot_monster(game *g, player &p, monster &mon, int &dam, double goodhit)
{
 const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
 const bool u_see_mon = g->u.see(mon);
 if (mon.has_flag(MF_HARDTOSHOOT) && !one_in(4) &&
     p.weapon.curammo->m1 != LIQUID && 
     p.weapon.curammo->accuracy >= 4) { // Buckshot hits anyway
  if (u_see_mon)
   messages.add("The shot passes through the %s without hitting.", mon.name().c_str());
  goodhit = 1;
 } else { // Not HARDTOSHOOT
// Armor blocks BEFORE any critical effects.
  int zarm = mon.armor_cut();
  zarm -= p.weapon.curammo->pierce;
  if (p.weapon.curammo->m1 == LIQUID) zarm = 0;
  else if (p.weapon.curammo->accuracy < 4) // Shot doesn't penetrate armor well
   zarm *= rng(2, 4);
  if (zarm > 0) dam -= zarm;
  if (dam <= 0) {
   if (u_see_mon)
    messages.add("The shot reflects off the %s!", mon.name_with_armor().c_str());
   dam = 0;
   goodhit = 1;
  }
  const char* message = "";
  if (goodhit < .1 && !mon.has_flag(MF_NOHEAD)) {
   message = "Headshot! ";
   dam = rng(5 * dam, 8 * dam);
   p.practice(firing->skill_used, 5);
  } else if (goodhit < .2) {
   message = "Critical! ";
   dam = rng(dam * 2, dam * 3);
   p.practice(firing->skill_used, 2);
  } else if (goodhit < .4) {
   dam = rng(int(dam * .9), int(dam * 1.5));
   p.practice(firing->skill_used, rng(0, 2));
  } else if (goodhit <= .7) {
   message = "Grazing hit. ";
   dam = rng(0, dam);
  } else dam = 0;

// Find the zombie at (x, y) and hurt them, MAYBE kill them!
  if (dam > 0) {
   mon.moves -= dam * 5;
   if (u_see_mon) {
       const auto z_name = mon.desc(grammar::noun::role::direct_object, grammar::article::definite);
       if (&p == &(g->u))
           messages.add("%sYou hit the %s for %d damage.", message, z_name.c_str(), dam);
       else
           messages.add("%s%s shoots the %s.", message, p.name.c_str(), z_name.c_str());
   }
   if (mon.hurt(dam)) g->kill_mon(mon, &p);
   else hit_monster_with_flags(mon, p.weapon.curammo->item_flags); // could pre-test for no-op if there is a profiled CPU problem
   dam = 0;
  }
 }
}

void shoot_player(game *g, player &p, player *h, int &dam, double goodhit)
{
 const it_gun* const firing = dynamic_cast<const it_gun*>(p.weapon.type);
 body_part hit;
 int side = rng(0, 1);
 if (goodhit < .05) {
  hit = bp_eyes;
  dam = rng(3 * dam, 5 * dam);
  p.practice(firing->skill_used, 5);
 } else if (goodhit < .1) {
  if (one_in(6)) hit = bp_eyes;
  else if (one_in(4)) hit = bp_mouth;
  else hit = bp_head;
  dam = rng(2 * dam, 5 * dam);
  p.practice(firing->skill_used, 5);
 } else if (goodhit < .2) {
  hit = bp_torso;
  dam = rng(dam, 2 * dam);
  p.practice(firing->skill_used, 2);
 } else if (goodhit < .4) {
  if (one_in(3)) hit = bp_torso;
  else if (one_in(2)) hit = bp_arms;
  else hit = bp_legs;
  dam = rng(int(dam * .9), int(dam * 1.5));
  p.practice(firing->skill_used, rng(0, 1));
 } else if (goodhit < .5) {
  hit = one_in(2) ? bp_arms : bp_legs;
  dam = rng(dam / 2, dam);
 } else dam = 0;

 if (dam > 0) {
  h->moves -= rng(0, dam);
  if (h == &(g->u))
   messages.add("%s shoots your %s for %d damage!", p.name.c_str(), body_part_name(hit, side), dam);
  else {
   if (&p == &(g->u))
    messages.add("You shoot %s's %s.", h->name.c_str(), body_part_name(hit, side));
   else if (g->u.see(h->pos))
    messages.add("%s shoots %s's %s.",
               (g->u.see(p.pos) ? p.name.c_str() : "Someone"),
               h->name.c_str(), body_part_name(hit, side));
  }
  h->hit(g, hit, side, 0, dam);
 }
}
