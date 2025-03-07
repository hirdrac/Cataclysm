#include "player.h"
#include "game.h"
#include "iuse.h"
#include "vehicle.h"
#include "skill.h"
#include "keypress.h"
#include "options.h"
#include "rng.h"
#include "stl_typetraits.h"
#include "stl_limits.h"
#include "line.h"
#include "monattack_spores.hpp"
#include "recent_msg.h"
#include "saveload.h"
#include "zero.h"
#include "posix_time.h"

#include <array>
#include <math.h>
#include <sstream>
#include <cstdarg>

using namespace cataclysm;

#define MIN_ADDICTION_LEVEL 3 // Minimum intensity before effects are seen

void addict_effect(player& u, addiction& add)   // \todo adapt for NPCs
{
    const auto delta = addict_stat_effects(add);
    int in = add.intensity;

    switch (add.type) {
    case ADD_CIG:
        if (in > 20 || one_in((500 - 20 * in))) {
            messages.add("You %s a cigarette.", rng(0, 6) < in ? "need" : "could use");
            u.cancel_activity_query("You have a nicotine craving.");
            u.add_morale(MORALE_CRAVING_NICOTINE, -15, -50);
            if (one_in(800 - 50 * in)) u.fatigue++;
            if (one_in(400 - 20 * in)) u.stim--;
        }
        break;

    case ADD_THC:
        if (in > 20 || one_in((500 - 20 * in))) {
            messages.add("You %s some weed.", rng(0, 6) < in ? "need" : "could use");
            u.cancel_activity_query("You have a marijuana craving.");
            u.add_morale(MORALE_CRAVING_THC, -15, -50);
            if (one_in(800 - 50 * in)) u.fatigue++;
            if (one_in(400 - 20 * in)) u.stim--;
        }
        break;

    case ADD_CAFFEINE:
        u.moves -= 2;
        if (in > 20 || one_in((500 - 20 * in))) {
            messages.add("You want some caffeine.");
            u.cancel_activity_query("You have a caffeine craving.");
            u.add_morale(MORALE_CRAVING_CAFFEINE, -5, -30);
            if (rng(0, 10) < in) u.stim--;
            if (rng(8, 400) < in) {
                messages.add("Your hands start shaking... you need it bad!");
                u.add_disease(DI_SHAKES, MINUTES(2));
            }
        }
        break;

    case ADD_ALCOHOL:
        if (rng(40, 1200) <= in * 10) u.health--;
        if (one_in(20) && rng(0, 20) < in) {
            messages.add("You could use a drink.");
            u.cancel_activity_query("You have an alcohol craving.");
            u.add_morale(MORALE_CRAVING_ALCOHOL, -35, -120);
        }
        else if (rng(8, 300) < in) {
            messages.add("Your hands start shaking... you need a drink bad!");
            u.cancel_activity_query("You have an alcohol craving.");
            u.add_morale(MORALE_CRAVING_ALCOHOL, -35, -120);
            u.add_disease(DI_SHAKES, MINUTES(5));
        }
        else if (!u.has_disease(DI_HALLU) && rng(10, 1600) < in)
            u.add_disease(DI_HALLU, HOURS(6));
        break;

    case ADD_SLEEP:
        // No effects here--just in player::can_sleep()
        // EXCEPT!  Prolong this addiction longer than usual.
        if (one_in(2) && add.sated < 0) add.sated++;
        break;

    case ADD_PKILLER:
        if ((in >= 25 || int(messages.turn) % (100 - in * 4) == 0) && u.pkill > 0) u.pkill--;	// Tolerance increases!
        if (35 <= u.pkill) {  // No further effects if we're doped up.
            add.sated = 0;
            return; // bypass stat processing
        }

        if (u.pain < in * 3) u.pain++;
        if (in >= 40 || one_in((1200 - 30 * in))) u.health--;
        // XXX \todo would like to not burn RNG gratuitously
        if (one_in(20) && dice(2, 20) < in) {
            messages.add("Your hands start shaking... you need some painkillers.");
            u.cancel_activity_query("You have an opiate craving.");
            u.add_morale(MORALE_CRAVING_OPIATE, -40, -200);
            u.add_disease(DI_SHAKES, MINUTES(2) + in * TURNS(5));
        }
        else if (one_in(20) && dice(2, 30) < in) {
            messages.add("You feel anxious.  You need your painkillers!");
            u.add_morale(MORALE_CRAVING_OPIATE, -30, -200);
            u.cancel_activity_query("You have a craving.");
        }
        else if (one_in(50) && dice(3, 50) < in) {
            messages.add("You throw up heavily!");
            u.cancel_activity_query("Throwing up.");
            u.vomit();
        }
        break;

    case ADD_SPEED: {
        u.moves -= clamped_ub<30>(in * 5);
        if (u.stim > -100 && (in >= 20 || int(messages.turn) % (100 - in * 5) == 0)) u.stim--;
        if (rng(0, 150) <= in) u.health--;
        // XXX \todo would like to not burn RNG gratuitously
        if (dice(2, 100) < in) {
            messages.add("You feel depressed.  Speed would help.");
            u.cancel_activity_query("You have a speed craving.");
            u.add_morale(MORALE_CRAVING_SPEED, -25, -200);
        }
        else if (one_in(10) && dice(2, 80) < in) {
            messages.add("Your hands start shaking... you need a pick-me-up.");
            u.cancel_activity_query("You have a speed craving.");
            u.add_morale(MORALE_CRAVING_SPEED, -25, -200);
            u.add_disease(DI_SHAKES, in * MINUTES(2));
        }
        else if (one_in(50) && dice(2, 100) < in) {
            messages.add("You stop suddenly, feeling bewildered.");
            u.cancel_activity();
            u.moves -= 300;
        }
        else if (!u.has_disease(DI_HALLU) && one_in(20) && 8 + dice(2, 80) < in)
            u.add_disease(DI_HALLU, HOURS(6));
    } break;

    case ADD_COKE:
        if (in >= 30 || one_in((900 - 30 * in))) {
            messages.add("You feel like you need a bump.");
            u.cancel_activity_query("You have a craving for cocaine.");
            u.add_morale(MORALE_CRAVING_COCAINE, -20, -250);
        }
        if (dice(2, 80) <= in) {
            messages.add("You feel like you need a bump.");
            u.cancel_activity_query("You have a craving for cocaine.");
            u.add_morale(MORALE_CRAVING_COCAINE, -20, -250);
            u.stim -= 3;
        }
        break;
    }

    u.str_cur += delta.Str;
    u.dex_cur += delta.Dex;
    u.int_cur += delta.Int;
    u.per_cur += delta.Per;
}

// Permanent disease capped at 3 days
#define MIN_DISEASE_AGE DAYS (-3)

stat_delta dis_stat_effects(const player& p, const disease& dis)
{
    stat_delta ret = { 0,0,0,0 };
    switch (dis.type) {
    case DI_GLARE:
        ret.Per = -1;
        break;

    case DI_COLD:
        ret.Dex -= dis.duration / MINUTES(8);
        break;

    case DI_COLD_FACE:
        ret.Per -= dis.duration / MINUTES(8);
        break;

    case DI_COLD_HANDS:
        ret.Dex -= 1 + dis.duration / MINUTES(4);
        break;

    case DI_HOT:
        ret.Int = -1;
        break;

    case DI_HEATSTROKE:
        ret.Str = -2;
        ret.Per = -1;
        ret.Int = -2;
        break;

    case DI_FBFACE:
        ret.Per = -2;
        break;

    case DI_FBHANDS:
        ret.Dex = -4;
        break;

    case DI_FBFEET:
        ret.Str = -1;
        break;

    case DI_COMMON_COLD:
        if (p.has_disease(DI_TOOK_FLUMED)) {
            ret.Str = -1;
            ret.Int = -1;
        }
        else {
            ret.Str = -3;
            ret.Dex = -1;
            ret.Int = -2;
            ret.Per = -1;
        }
        break;

    case DI_FLU:
        if (p.has_disease(DI_TOOK_FLUMED)) {
            ret.Str = -2;
            ret.Int = -1;
        }
        else {
            ret.Str = -4;
            ret.Dex = -2;
            ret.Int = -2;
            ret.Per = -1;
        }
        break;

    case DI_SMOKE:
        ret.Str = -1;
        ret.Int = -1;
        break;

    case DI_TEARGAS:
        ret.Str = -2;
        ret.Dex = -2;
        ret.Int = -1;
        ret.Per = -4;
        break;

    case DI_BOOMERED:
        ret.Per = -5;
        break;

    case DI_SAP:
        ret.Dex = -3;
        break;

    case DI_FUNGUS:
        ret.Str = -1;
        ret.Dex = -1;
        break;

    case DI_SLIMED:
        ret.Dex = -2;
        break;

    case DI_DRUNK:
        // We get 600 turns, or one hour, of DI_DRUNK for each drink we have (on avg)
        // So, the duration of DI_DRUNK is a good indicator of how much alcohol is in
        //  our system.
        ret.Per -= int(dis.duration / MINUTES(100));
        ret.Dex -= int(dis.duration / MINUTES(100));
        ret.Int -= int(dis.duration / MINUTES(70));
        ret.Str -= int(dis.duration / MINUTES(150));
        if (dis.duration <= HOURS(1)) ret.Str += 1;
        break;

    case DI_CIG:
        if (dis.duration >= HOURS(1)) {	// Smoked too much
            ret.Str = -1;
            ret.Dex = -1;
        }
        else {
            ret.Dex = 1;
            ret.Int = 1;
            ret.Per = 1;
        }
        break;

    case DI_HIGH:
        ret.Int = -1;
        ret.Per = -1;
        break;

    case DI_THC:
        ret.Int = -1;
        ret.Per = -1;
        break;

    case DI_POISON:
        ret.Per = -1;
        ret.Dex = -1;
        if (!p.has_trait(PF_POISRESIST)) ret.Str = -2;
        break;

    case DI_BADPOISON:
        ret.Per = -2;
        ret.Dex = -2;
        ret.Str = p.has_trait(PF_POISRESIST) ? -1 : -3;
        break;

    case DI_FOODPOISON:
        ret.Str = p.has_trait(PF_POISRESIST) ? -1 : -3;
        ret.Per = -1;
        ret.Dex = -1;
        break;

    case DI_SHAKES:
        ret.Dex = -4;
        ret.Str = -1;
        break;

    case DI_WEBBED:
        ret.Str = -2;
        ret.Dex = -4;
        break;

    case DI_RAT:
        ret.Int -= dis.duration / MINUTES(2);
        ret.Str -= dis.duration / MINUTES(5);
        ret.Per -= dis.duration / (MINUTES(5) / 2);
        break;

    case DI_FORMICATION:
        ret.Int = -2;
        ret.Str = -1;
        break;

    case DI_HALLU:
        // This assumes that we were given DI_HALLU with a 3600 (6-hour) lifespan
        if (HOURS(4) > dis.duration) {	// Full symptoms
            ret.Per = -2;
            ret.Int = -1;
            ret.Dex = -2;
            ret.Str = -1;
        }
        break;

    case DI_ADRENALINE:
        if (MINUTES(15) < dis.duration) {	// 5 minutes positive effects
            ret.Str = 5;
            ret.Dex = 3;
            ret.Int = -8;
            ret.Per = 1;
        }
        else if (MINUTES(15) > dis.duration) {	// 15 minutes come-down
            ret.Str = -2;
            ret.Dex = -1;
            ret.Int = -1;
            ret.Per = -1;
        }
        break;

    case DI_ASTHMA:
        ret.Str = -2;
        ret.Dex = -3;
        break;

    case DI_METH:
        if (dis.duration > HOURS(1)) {
            ret.Str = 2;
            ret.Dex = 2;
            ret.Int = 3;
            ret.Per = 3;
        }
        else {
            ret.Str = -3;
            ret.Dex = -2;
            ret.Int = -1;
        }
        break;

    case DI_EVIL: {
        bool lesser = false; // Worn or wielded; diminished effects
        if (const auto art_tool = p.weapon.is_artifact_tool()) {
            lesser = any(art_tool->effects_carried, AEP_EVIL) || any(art_tool->effects_wielded, AEP_EVIL);
        }
        if (!lesser) {
            for (const auto& it : p.worn) {
                if (const auto art_armor = it.is_artifact_armor()) {
                    lesser = any(art_armor->effects_worn, AEP_EVIL);
                    if (lesser) break;
                }
            }
        }

        if (lesser) { // Only minor effects, some even good!
            ret.Str += (dis.duration > MINUTES(450) ? 10 : dis.duration / MINUTES(45));
            if (dis.duration < HOURS(1))
                ret.Dex++;
            else
                ret.Dex -= (dis.duration > HOURS(6) ? 10 : (dis.duration - HOURS(1)) / MINUTES(30));
            ret.Int -= (dis.duration > MINUTES(300) ? 10 : (dis.duration - MINUTES(50)) / MINUTES(25));
            ret.Per -= (dis.duration > MINUTES(480) ? 10 : (dis.duration - MINUTES(80)) / MINUTES(40));
        }
        else { // Major effects, all bad.
            ret.Str -= (dis.duration > MINUTES(500) ? 10 : dis.duration / MINUTES(50));
            ret.Dex -= (dis.duration > HOURS(1) ? 10 : dis.duration / HOURS(1));
            ret.Int -= (dis.duration > MINUTES(450) ? 10 : dis.duration / MINUTES(45));
            ret.Per -= (dis.duration > MINUTES(400) ? 10 : dis.duration / MINUTES(40));
        }
    } break;
    }
    return ret;
}

void player::subjective_message(const std::string& msg) const { if (!msg.empty()) messages.add(msg); }

bool player::see_phantasm()
{
    auto g = game::active();
    const point pt(pos + rng(within_rldist<10>));
    if (!g->mon(pt)) {
        g->spawn(monster(mtype::types[mon_hallu_zom + rng(0, 3)], pt));
        return true;
    }
    return false;
}

std::vector<item>* player::use_stack_at(const point& pt) const
{
    std::vector<item>* ret = nullptr;
    auto g = game::active();
    decltype(auto) stack = g->m.i_at(pt);
    if (!stack.empty()) ret = &stack;

    const auto v = g->m._veh_at(pt);
    vehicle* const veh = v ? v->first : nullptr; // backward compatibility
    if (veh) {
        int veh_part = v ? v->second : 0;
        veh_part = veh->part_with_feature(veh_part, vpf_cargo, false);
        if (veh_part >= 0 && !veh->parts[veh_part].items.empty() && query_yn("Get items from %s?", veh->part_info(veh_part).name))
            ret = &veh->parts[veh_part].items;
    }
    return ret;
}

static void incoming_nether_portal(GPS_loc dest)
{
    const auto g = game::active();
    if (0 >= dest.move_cost()) dest.ter() = t_rubble;
    g->spawn(monster(mtype::types[(mongroup::moncats[mcat_nether])[rng(0, mongroup::moncats[mcat_nether].size() - 1)]], dest));
    // \todo technically should also cancel activities for all NPCs that see it
    if (g->u.see(dest)) {
        g->u.cancel_activity_query("A monster appears nearby!");
        messages.add("A portal opens nearby, and a monster crawls through!");
    }
}

void dis_effect(game* g, player& p, disease& dis)
{
    const auto delta = dis_stat_effects(p, dis);
    p.str_cur += delta.Str;
    p.dex_cur += delta.Dex;
    p.int_cur += delta.Int;
    p.per_cur += delta.Per;

    static std::function<GPS_loc()> within_four = [&]() { return p.GPSpos + rng(within_rldist<4>); };
    static std::function<bool(const GPS_loc&)> ok = [&](const GPS_loc& test) { return !g->mob_at(test); };

    int bonus;
    switch (dis.type) {
    case DI_WET:
        p.add_morale(MORALE_WET, -1, -50);
        break;

    case DI_COLD_FACE:
        if (dis.duration >= MINUTES(20) || (dis.duration >= MINUTES(10) && one_in(MINUTES(30) - dis.duration)))
            p.add_disease(DI_FBFACE, MINUTES(5));
        break;

    case DI_COLD_HANDS:
        if (dis.duration >= MINUTES(20) || (dis.duration >= MINUTES(10) && one_in(MINUTES(30) - dis.duration)))
            p.add_disease(DI_FBHANDS, MINUTES(5));
        break;

    case DI_COLD_FEET:
        if (dis.duration >= MINUTES(20) || (dis.duration >= MINUTES(10) && one_in(MINUTES(30) - dis.duration)))
            p.add_disease(DI_FBFEET, MINUTES(5));
        break;

    case DI_HOT:
        if (rng(0, MINUTES(50)) < dis.duration) p.add_disease(DI_HEATSTROKE, TURNS(2));
        break;

    case DI_COMMON_COLD:
        if (int(messages.turn) % 300 == 0) p.thirst++;
        if (one_in(300)) {
            p.moves -= 80;
            if (!p.is_npc()) {
                messages.add("You cough noisily.");
                g->sound(p.pos, 12, "");
            }
            else
                g->sound(p.pos, 12, "loud coughing");
        }
        break;

    case DI_FLU:
        if (int(messages.turn) % 300 == 0) p.thirst++;
        if (one_in(300)) {
            p.moves -= 80;
            if (!p.is_npc()) {
                messages.add("You cough noisily.");
                g->sound(p.pos, 12, "");
            }
            else
                g->sound(p.pos, 12, "loud coughing");
        }
        if (one_in(3600) || (p.has_trait(PF_WEAKSTOMACH) && one_in(3000)) || (p.has_trait(PF_NAUSEA) && one_in(2400))) {
            if (!p.has_disease(DI_TOOK_FLUMED) || one_in(2)) p.vomit();
        }
        break;

    case DI_SMOKE:
        if (one_in(5)) {
            if (!p.is_npc()) {
                messages.add("You cough heavily.");
                g->sound(p.pos, 12, "");
            }
            else
                g->sound(p.pos, 12, "a hacking cough.");
            p.moves -= 4 * (mobile::mp_turn / 5);
            p.hurt(bp_torso, 0, 1 - (rng(0, 1) * rng(0, 1)));
        }
        break;

    case DI_TEARGAS:
        if (one_in(3)) {
            if (!p.is_npc()) {
                messages.add("You cough heavily.");
                g->sound(p.pos, 12, "");
            }
            else
                g->sound(p.pos, 12, "a hacking cough");
            p.moves -= mobile::mp_turn;
            p.hurt(bp_torso, 0, rng(0, 3) * rng(0, 1));
        }
        break;

    case DI_ONFIRE:
        p.hurtall(3);
        for (int i = 0; i < p.worn.size(); i++) {
            if (p.worn[i].made_of(VEGGY) || p.worn[i].made_of(PAPER)) {  // \todo V0.2.3+ want to handle wood here but having it auto-destruct doesn't seem reasonable
                EraseAt(p.worn, i);
                i--;
            }
            else if ((p.worn[i].made_of(COTTON) || p.worn[i].made_of(WOOL)) && one_in(10)) {
                EraseAt(p.worn, i);
                i--;
            }
            else if (p.worn[i].made_of(PLASTIC) && one_in(50)) {	// \todo V0.2.1+ thermoplastic might melt on the way which also causes damage
                EraseAt(p.worn, i);
                i--;
            }
        }
        break;

    case DI_SPORES:
        if (one_in(30)) p.add_disease(DI_FUNGUS, -1);
        break;

    case DI_FUNGUS:
        bonus = p.has_trait(PF_POISRESIST) ? 100 : 0;
        p.moves -= mobile::mp_turn / 10;
        if (dis.duration > HOURS(-1)) {	// First hour symptoms
            if (one_in(160 + bonus)) {
                if (!p.is_npc()) {
                    messages.add("You cough heavily.");
                    g->sound(p.pos, 12, "");
                }
                else
                    g->sound(p.pos, 12, "a hacking cough");
                p.pain++;
            }
            if (one_in(100 + bonus)) {
                if (!p.is_npc()) messages.add("You feel nauseous.");
            }
            if (one_in(100 + bonus)) {
                if (!p.is_npc()) messages.add("You smell and taste mushrooms.");
            }
        }
        else if (dis.duration > HOURS(-6)) {	// One to six hours
            if (one_in(600 + bonus * 3)) {
                if (!p.is_npc()) messages.add("You spasm suddenly!");
                p.moves -= mobile::mp_turn;
                p.hurt(bp_torso, 0, 5);
            }
            if ((p.has_trait(PF_WEAKSTOMACH) && one_in(1600 + bonus * 8)) ||
                (p.has_trait(PF_NAUSEA) && one_in(800 + bonus * 6)) ||
                one_in(2000 + bonus * 10)) {
                if (!p.is_npc()) messages.add("You vomit a thick, gray goop.");
                else if (g->u.see(p.pos)) messages.add("%s vomits a thick, gray goop.", p.name.c_str());
                p.moves -= 2 * mobile::mp_turn;
                p.hunger += 50;
                p.thirst += 68;
            }
        }
        else {	// Full symptoms
            if (one_in(1000 + bonus * 8)) {
                if (!p.is_npc()) messages.add("You double over, spewing live spores from your mouth!");
                else if (g->u.see(p.pos)) messages.add("%s coughs up a stream of live spores!", p.name.c_str());
                p.moves -= 5 * mobile::mp_turn;
                monster spore(mtype::types[mon_spore]);
                for (decltype(auto) dir : Direction::vector) {
                    const auto dest(p.GPSpos + dir);
                    if (0 < dest.move_cost() && one_in(5)) spray_spores(dest, nullptr);
                }
            }
            else if (one_in(6000 + bonus * 20)) {
                if (!p.is_npc()) messages.add("Fungus stalks burst through your hands!");
                else if (g->u.see(p.pos)) messages.add("Fungus stalks burst through %s's hands!", p.name.c_str());
                p.hurt(bp_arms, 0, 60);
                p.hurt(bp_arms, 1, 60);
            }
        }
        break;

    case DI_LYING_DOWN:
        p.moves = 0;
        if (p.can_sleep(g->m)) {
            dis.duration = 1;
            if (!p.is_npc()) messages.add("You fall asleep.");
            p.add_disease(DI_SLEEP, HOURS(10));
        }
        if (dis.duration == 1 && !p.has_disease(DI_SLEEP))
            if (!p.is_npc()) messages.add("You try to sleep, but can't...");
        break;

    case DI_SLEEP:
        p.moves = 0;
        if (int(messages.turn) % 25 == 0) {
            if (p.fatigue > 0) p.fatigue -= 1 + rng(0, 1) * rng(0, 1);
            if (p.has_trait(PF_FASTHEALER))
                p.healall(rng(0, 1));
            else
                p.healall(rng(0, 1) * rng(0, 1) * rng(0, 1));
            if (p.fatigue <= 0 && p.fatigue > -20) {
                p.fatigue = -25;
                messages.add("Fully rested.");
                dis.duration = dice(3, 100);
            }
        }
        if (int(messages.turn) % 100 == 0 && !p.has_bionic(bio_recycler)) {
            // Hunger and thirst advance more slowly while we sleep.
            p.hunger--;
            p.thirst--;
        }
        if (rng(5, 80) + rng(0, 120) + rng(0, abs(p.fatigue)) +
            rng(0, abs(p.fatigue * 5)) < g->light_level() &&
            (p.fatigue < 10 || one_in(p.fatigue / 2))) {
            messages.add("The light wakes you up.");
            dis.duration = 1;
        }
        break;

    case DI_PKILL1:
        if (dis.duration <= MINUTES(7) && dis.duration % 7 == 0 && p.pkill < 15)
            p.pkill++;
        break;

    case DI_PKILL2:
        if (dis.duration % 7 == 0 &&
            (one_in(p.addiction_level(ADD_PKILLER)) ||
                one_in(p.addiction_level(ADD_PKILLER))))
            p.pkill += 2;
        break;

    case DI_PKILL3:
        if (dis.duration % 2 == 0 &&
            (one_in(p.addiction_level(ADD_PKILLER)) ||
                one_in(p.addiction_level(ADD_PKILLER))))
            p.pkill++;
        break;

    case DI_PKILL_L:
        if (dis.duration % 20 == 0 && p.pkill < 40 &&
            (one_in(p.addiction_level(ADD_PKILLER)) ||
                one_in(p.addiction_level(ADD_PKILLER))))
            p.pkill++;
        break;

    case DI_IODINE:
        if (p.radiation > 0 && one_in(16))
            p.radiation--;
        break;

    case DI_TOOK_XANAX:
        if (dis.duration % 25 == 0 && (p.stim > 0 || one_in(2)))
            p.stim--;
        break;

    case DI_DRUNK:
        // We get 600 turns, or one hour, of DI_DRUNK for each drink we have (on avg)
        // So, the duration of DI_DRUNK is a good indicator of how much alcohol is in
        //  our system.
        if (dis.duration <= HOURS(1)) p.str_cur += 1;
        if (dis.duration > MINUTES(200) + MINUTES(10) * dice(2, 100) &&
            (p.has_trait(PF_WEAKSTOMACH) || p.has_trait(PF_NAUSEA) || one_in(20)))
            p.vomit();
        if (!p.has_disease(DI_SLEEP) && dis.duration >= MINUTES(450) &&
            one_in(500 - int(dis.duration / MINUTES(8)))) {
            if (!p.is_npc()) messages.add("You pass out.");
            p.add_disease(DI_SLEEP, dis.duration / 2);
        }
        break;

    case DI_CIG:
        if (dis.duration >= HOURS(1)) {	// Smoked too much
            if (dis.duration >= HOURS(2) && (one_in(50) ||
                (p.has_trait(PF_WEAKSTOMACH) && one_in(30)) ||
                (p.has_trait(PF_NAUSEA) && one_in(20))))
                p.vomit();
        }
        // non-overdosable stimulant: too close to nicotinic acid a.k.a. niacin
        if (0 == messages.turn % MINUTES(1) && 15 > p.stim) ++p.stim;
        break;

    case DI_POISON:
        if (one_in(p.has_trait(PF_POISRESIST) ? MINUTES(90) : MINUTES(15))) {
            if (!p.is_npc()) messages.add("You're suddenly wracked with pain!");
            p.pain++;
            p.hurt(bp_torso, 0, rng(0, 2) * rng(0, 1));
        }
        break;

    case DI_BADPOISON:
        if (one_in(p.has_trait(PF_POISRESIST) ? MINUTES(50) : MINUTES(10))) {
            if (!p.is_npc()) messages.add("You're suddenly wracked with pain!");
            p.pain += 2;
            p.hurt(bp_torso, 0, rng(0, 2));
        }
        break;

    case DI_FOODPOISON:
        bonus = p.has_trait(PF_POISRESIST) ? 600 : 0;
        if (one_in(300 + bonus)) {
            if (!p.is_npc()) messages.add("You're suddenly wracked with pain and nausea!");
            p.hurt(bp_torso, 0, 1);
        }
        if ((p.has_trait(PF_WEAKSTOMACH) && one_in(300 + bonus)) ||
            (p.has_trait(PF_NAUSEA) && one_in(50 + bonus)) ||
            one_in(600 + bonus))
            p.vomit();
        break;

    case DI_DERMATIK: {
        int formication_chance = HOURS(1);
        if (dis.duration > HOURS(-4) && dis.duration < 0) formication_chance = HOURS(4) + dis.duration;
        if (one_in(formication_chance)) p.add_disease(DI_FORMICATION, HOURS(2));

        if (dis.duration < HOURS(-4) && one_in(HOURS(4))) p.vomit();

        if (dis.duration < DAYS(-1)) { // Spawn some larvae!
      // Choose how many insects; more for large characters
            int num_insects = 1;
            while (num_insects < 6 && rng(0, 10) < p.str_max) num_insects++;
            // Figure out where they may be placed
            std::vector<GPS_loc> valid_spawns;
            for (decltype(auto) delta : Direction::vector) {
                const auto loc(p.GPSpos + delta);
                if (loc.is_empty()) valid_spawns.push_back(loc);
            }
            if (valid_spawns.size() >= 1) {
                p.rem_disease(DI_DERMATIK); // No more infection!  yay.
                if (!p.is_npc()) messages.add("Insects erupt from your skin!");
                else if (g->u.see(p.pos)) messages.add("Insects erupt from %s's skin!", p.name.c_str());
                p.moves -= 6 * mobile::mp_turn;
                monster grub(mtype::types[mon_dermatik_larva]);
                while (valid_spawns.size() > 0 && num_insects > 0) {
                    num_insects--;
                    // Hurt the player
                    body_part burst = bp_torso;
                    if (one_in(3)) burst = bp_arms;
                    else if (one_in(3)) burst = bp_legs;
                    p.hurt(burst, rng(0, 1), rng(4, 8));
                    // Spawn a larva
                    int sel = rng(0, valid_spawns.size() - 1);
                    grub.spawn(valid_spawns[sel]);
                    valid_spawns.erase(valid_spawns.begin() + sel);
                    // Sometimes it's a friendly larva!
                    if (one_in(3)) grub.make_ally(p);
                    else grub.make_threat(p);
                    g->spawn(grub);
                }
            }
        }
    } break;

    case DI_RAT:
        if (rng(30, 100) < rng(0, dis.duration) && one_in(3)) p.vomit();
        if (rng(0, 100) < rng(0, dis.duration)) p.mutation_category_level[MUTCAT_RAT]++;
        if (rng(50, 500) < rng(0, dis.duration)) p.mutate();
        break;

    case DI_FORMICATION:
        if (one_in(10 + 40 * p.int_cur)) {
            if (!p.is_npc())
                messages.add("You start scratching yourself all over!");
            else if (g->u.see(p.pos))
                messages.add("%s starts scratching %s all over!", p.name.c_str(), (p.male ? "himself" : "herself"));
            p.cancel_activity();
            p.moves -= 3 * (mobile::mp_turn / 2);
            p.hurt(bp_torso, 0, 1);
        }
        break;

    case DI_HALLU:
        // This assumes that we were given DI_HALLU with a 3600 (6-hour) lifespan
        if (dis.duration > HOURS(5)) {	// First hour symptoms
            if (one_in(300)) {
                if (!p.is_npc()) messages.add("You feel a little strange.");
            }
        }
        else if (dis.duration > HOURS(4)) {	// Coming up
            if (one_in(100) || (p.has_trait(PF_WEAKSTOMACH) && one_in(100))) {
                if (!p.is_npc()) messages.add("You feel nauseous.");
                p.hunger -= 5;
            }
            if (!p.is_npc()) {
                if (one_in(200)) messages.add("Huh?  What was that?");
                else if (one_in(200)) messages.add("Oh god, what's happening?");
                else if (one_in(200)) messages.add("Of course... it's all fractals!");
            }
        }
        else if (dis.duration == HOURS(4))	// Visuals start
            p.add_disease(DI_VISUALS, HOURS(4));
        else {	// Full symptoms
            if (one_in(50)) p.see_phantasm();
        }
        break;

    case DI_ADRENALINE: // positive/negative effects are all stat adjustments
        if (dis.duration == MINUTES(15)) {	// 15 minutes come-down
            if (!p.is_npc()) messages.add("Your adrenaline rush wears off.  You feel AWFUL!");
            p.moves -= 300;
        }
        break;

    case DI_ASTHMA:
        if (dis.duration > HOURS(2)) {
            if (!p.is_npc()) messages.add("Your asthma overcomes you.  You stop breathing and die...");
            p.hurtall(500);
        }
        break;

    case DI_ATTACK_BOOST:
    case DI_DAMAGE_BOOST:
    case DI_DODGE_BOOST:
    case DI_ARMOR_BOOST:
    case DI_SPEED_BOOST:
        if (dis.intensity > 1) dis.intensity--;
        break;

    case DI_TELEGLOW:
        // Default we get around 300 duration points per teleport (possibly more
        // depending on the source).
        // TODO: Include a chance to teleport to the nether realm.
        if (dis.duration > HOURS(10)) {	// 20 teles (no decay; in practice at least 21)
            if (one_in(1000 - ((dis.duration - HOURS(10)) / 10))) {
                if (!p.is_npc()) messages.add("Glowing lights surround you, and you teleport.");
                g->teleport();
                if (one_in(10)) p.rem_disease(DI_TELEGLOW);
            }
            if (one_in(1200 - ((dis.duration - HOURS(10)) / 5)) && one_in(20)) {
                if (!p.is_npc()) messages.add("You pass out.");
                p.add_disease(DI_SLEEP, HOURS(2));
                if (one_in(6)) p.rem_disease(DI_TELEGLOW);
            }
        }
        if (dis.duration > HOURS(6)) { // 12 teles
            if (one_in(HOURS(6) + MINUTES(40) - (dis.duration - HOURS(6)) / 4)) {
                if (auto dest = LasVegasChoice(10, within_four, ok)) {
                    incoming_nether_portal(*dest);
                    if (one_in(2)) p.rem_disease(DI_TELEGLOW);
                }
            }
            if (one_in(HOURS(6)-MINUTES(10) - (dis.duration - HOURS(6)) / 4)) {
                if (!p.is_npc()) messages.add("You shudder suddenly.");
                p.mutate();
                if (one_in(4)) p.rem_disease(DI_TELEGLOW);
            }
        }
        if (dis.duration > HOURS(4)) {	// 8 teleports
            if (one_in(HOURS(16) + MINUTES(40) - dis.duration)) p.add_disease(DI_SHAKES, rng(MINUTES(4), MINUTES(8)));
            if (one_in(HOURS(20) - dis.duration)) {
                if (!p.is_npc()) messages.add("Your vision is filled with bright lights...");
                p.add_disease(DI_BLIND, rng(MINUTES(1), MINUTES(2)));
                if (one_in(8)) p.rem_disease(DI_TELEGLOW);
            }
            if (one_in(HOURS(8)+MINUTES(20)) && !p.has_disease(DI_HALLU)) {
                p.add_disease(DI_HALLU, HOURS(6));
                if (one_in(5)) p.rem_disease(DI_TELEGLOW);
            }
        }
        if (one_in(HOURS(6) + MINUTES(40))) {
            if (!p.is_npc()) messages.add("You're suddenly covered in ectoplasm.");
            p.add_disease(DI_BOOMERED, MINUTES(10));
            if (one_in(4)) p.rem_disease(DI_TELEGLOW);
        }
        if (one_in(HOURS(16)+MINUTES(40))) {
            p.add_disease(DI_FUNGUS, -1);
            p.rem_disease(DI_TELEGLOW);
        }
        break;

    case DI_ATTENTION:
        // C: Whales scale factor 100000 slightly less than one week 2021-06-27 zaimoni
        if (one_in(DAYS(7) / dis.duration) && one_in(DAYS(7) / dis.duration) && one_in(MINUTES(25))) {
            if (auto dest = LasVegasChoice(10, within_four, ok)) {
                incoming_nether_portal(*dest);
                dis.duration /= 4;
            }
        }
        break;
    }
}

#include <fstream>

#if NONINLINE_EXPLICIT_INSTANTIATION
template<> item discard<item>::x = item();
#endif

// start prototype for morale.cpp
const std::string morale_point::data[NUM_MORALE_TYPES] = {
	"This is a bug",
	"Enjoyed %i",
	"Music",
	"Marloss Bliss",
	"Good Feeling",

	"Nicotine Craving",
	"Caffeine Craving",
	"Alcohol Craving",
	"Opiate Craving",
	"Speed Craving",
	"Cocaine Craving",

	"Disliked %i",
	"Ate Meat",
	"Wet",
	"Bad Feeling",
	"Killed Innocent",
	"Killed Friend",
	"Killed Mother",

	"Moodswing",
	"Read %i",
	"Heard Disturbing Scream"
};

std::string morale_point::name() const
{
	std::string ret(data[type]);
	std::string item_name(item_type ? item_type->name : "");
	size_t it;
	while ((it = ret.find("%i")) != std::string::npos) ret.replace(it, 2, item_name);
	return ret;
}
// end prototype for morale.cpp

// start prototype for disease.cpp
int disease::speed_boost() const
{
	switch (type) {
	case DI_COLD:		return 0 - duration/TURNS(5);
    case DI_COLD_LEGS:
        if (duration >= TURNS(2)) return duration > MINUTES(6) ? MINUTES(6) / TURNS(2) : duration / TURNS(2);
        return 0;
    case DI_HEATSTROKE:	return -15;
	case DI_INFECTION:	return -80;
	case DI_SAP:		return -25;
	case DI_SPORES:	return -15;
	case DI_SLIMED:	return -25;
	case DI_BADPOISON:	return -10;
	case DI_FOODPOISON:	return -20;
	case DI_WEBBED:	return -25;
	case DI_ADRENALINE:	return (duration > MINUTES(15) ? 40 : -10);
	case DI_ASTHMA:	return 0 - duration/TURNS(5);
	case DI_METH:		return (duration > MINUTES(60) ? 50 : -40);
	default:		return 0;
	}
}

const char* disease::name() const
{
	switch (type) {
	case DI_GLARE:		return "Glare";
	case DI_COLD:		return "Cold";
	case DI_COLD_FACE:	return "Cold face";
	case DI_COLD_HANDS:	return "Cold hands";
	case DI_COLD_LEGS:	return "Cold legs";
	case DI_COLD_FEET:	return "Cold feet";
	case DI_HOT:		return "Hot";
	case DI_HEATSTROKE:	return "Heatstroke";
	case DI_FBFACE:	return "Frostbite - Face";
	case DI_FBHANDS:	return "Frostbite - Hands";
	case DI_FBFEET:	return "Frostbite - Feet";
	case DI_COMMON_COLD:	return "Common Cold";
	case DI_FLU:		return "Influenza";
	case DI_SMOKE:		return "Smoke";
	case DI_TEARGAS:	return "Tear gas";
	case DI_ONFIRE:	return "On Fire";
	case DI_BOOMERED:	return "Boomered";
	case DI_SAP:		return "Sap-coated";
	case DI_SPORES:	return "Spores";
	case DI_SLIMED:	return "Slimed";
	case DI_DEAF:		return "Deaf";
	case DI_BLIND:		return "Blind";
	case DI_STUNNED:	return "Stunned";
	case DI_DOWNED:	return "Downed";
	case DI_POISON:	return "Poisoned";
	case DI_BADPOISON:	return "Badly Poisoned";
	case DI_FOODPOISON:	return "Food Poisoning";
	case DI_SHAKES:	return "Shakes";
	case DI_FORMICATION:	return "Bugs Under Skin";
	case DI_WEBBED:	return "Webbed";
	case DI_RAT:		return "Ratting";
	case DI_DRUNK:
		if (duration > MINUTES(220)) return "Wasted";
		if (duration > MINUTES(140)) return "Trashed";
		if (duration > MINUTES(80))  return "Drunk";
		return "Tipsy";

	case DI_CIG:		return "Cigarette";
	case DI_HIGH:		return "High";
    case DI_THC:		return "Stoned";
    case DI_VISUALS:	return "Hallucinating";

	case DI_ADRENALINE:
		if (duration > MINUTES(15)) return "Adrenaline Rush";
		return "Adrenaline Comedown";

	case DI_ASTHMA:
		if (duration > MINUTES(80)) return "Heavy Asthma";
		return "Asthma";

	case DI_METH:
		if (duration > HOURS(1)) return "High on Meth";
		return "Meth Comedown";

	case DI_IN_PIT:	return "Stuck in Pit";

	case DI_ATTACK_BOOST:  return "Hit Bonus";
	case DI_DAMAGE_BOOST:  return "Damage Bonus";
	case DI_DODGE_BOOST:   return "Dodge Bonus";
	case DI_ARMOR_BOOST:   return "Armor Bonus";
	case DI_SPEED_BOOST:   return "Attack Speed Bonus";
	case DI_VIPER_COMBO:
		switch (intensity) {
		case 1: return "Snakebite Unlocked!";
		case 2: return "Viper Strike Unlocked!";
		default: return "VIPER BUG!!!!";
		}

//	case DI_NULL:		return "";
	default:		return nullptr;
	}
}

std::string disease::invariant_desc() const
{
    std::ostringstream stream;

    switch (type) {
    case DI_NULL: return "None";
    case DI_COLD: return  "Your body in general is uncomfortably cold.\n";

    case DI_COLD_FACE:
        stream << "Your face is cold.";
        if (duration >= MINUTES(10)) stream << "  It may become frostbitten.";
        stream << "\n";
        return stream.str();

    case DI_COLD_HANDS:
        stream << "Your hands are cold.";
        if (duration >= MINUTES(10)) stream << "  They may become frostbitten.";
        return stream.str();

    case DI_COLD_LEGS: return  "Your legs are cold.";

    case DI_COLD_FEET:	// XXX omitted from disease::speed_boost \todo fix
        stream << "Your feet are cold.";
        if (duration >= MINUTES(10)) stream << "  They may become frostbitten.";
        return stream.str();

    case DI_HOT: return " You are uncomfortably hot.\n\
You may start suffering heatstroke.";

    case DI_COMMON_COLD:	return "Increased thirst;  Frequent coughing\n\
Symptoms alleviated by medication (Dayquil or Nyquil).";

    case DI_FLU:		return "Increased thirst;  Frequent coughing;  Occasional vomiting\n\
Symptoms alleviated by medication (Dayquil or Nyquil).";

    case DI_SMOKE:		return "Occasionally you will cough, costing movement and creating noise.\n\
Loss of health - Torso";

    case DI_TEARGAS:	return "Occasionally you will cough, costing movement and creating noise.\n\
Loss of health - Torso";

    case DI_ONFIRE:	return "Loss of health - Entire Body\n\
Your clothing and other equipment may be consumed by the flames.";

    case DI_BOOMERED:	return "Range of Sight: 1;     All sight is tinted magenta";

    case DI_SPORES:	return "You can feel the tiny spores sinking directly into your flesh.";

    case DI_DEAF:		return "Sounds will not be reported.  You cannot talk with NPCs.";
    case DI_BLIND:		return "Range of Sight: 0";
    case DI_STUNNED:	return "Your movement is randomized.";
    case DI_DOWNED:	return "You're knocked to the ground.  You have to get up before you can move.";
    case DI_POISON:	return "Occasional pain and/or damage.";
    case DI_BADPOISON:	return "Frequent pain and/or damage.";
    case DI_FOODPOISON:	return "Your stomach is extremely upset, and you keep having pangs of pain and nausea.";

    case DI_FORMICATION:	return "You stop to scratch yourself frequently; high intelligence helps you resist\n\
this urge.";

    case DI_RAT: return "You feel nauseated and rat-like.";

    case DI_CIG:
        if (duration >= HOURS(1)) return "You smoked too much.";
        return std::string();

    case DI_VISUALS: return "You can't trust everything that you see.";
    case DI_IN_PIT: return "You're stuck in a pit.  Sight distance is limited and you have to climb out.";

    case DI_ATTACK_BOOST:
        stream << "To-hit bonus + " << intensity;
        return stream.str();

    case DI_DAMAGE_BOOST:
        stream << "Damage bonus + " << intensity;
        return stream.str();

    case DI_DODGE_BOOST:
        stream << "Dodge bonus + " << intensity;
        return stream.str();

    case DI_ARMOR_BOOST:
        stream << "Armor bonus + " << intensity;
        return stream.str();

    case DI_SPEED_BOOST:
        stream << "Attack speed + " << intensity;
        return stream.str();

    case DI_VIPER_COMBO:
        switch (intensity) {
        case 1: return "Your next strike will be a Snakebite, using your hand in a cone shape.  This\n\
will deal piercing damage.";
        case 2: return "Your next strike will be a Viper Strike.  It requires both arms to be in good\n\
condition, and deals massive damage.";
        }
        [[fallthrough]];

    default: return std::string();
    }
}

std::string describe(const disease& dis, const player& p)
{
    const auto stat_delta = dis_stat_effects(p, dis);

    bool live_str = false;
    std::ostringstream stat_desc;
    if (const auto speed_delta = dis.speed_boost()) {
        stat_desc << "Speed " << speed_delta << "%";
        live_str = true;
    }
    if (stat_delta.Str) {
        if (live_str) stat_desc << "; ";
        stat_desc << "Strength " << stat_delta.Str;
        live_str = true;
    }
    if (stat_delta.Dex) {
        if (live_str) stat_desc << "; ";
        stat_desc << "Dexterity " << stat_delta.Dex;
        live_str = true;
    }
    if (stat_delta.Int) {
        if (live_str) stat_desc << "; ";
        stat_desc << "Intelligence " << stat_delta.Int;
        live_str = true;
    }
    if (stat_delta.Per) {
        if (live_str) stat_desc << "; ";
        stat_desc << "Perception " << stat_delta.Per;
        live_str = true;
    }

    const std::string invar_desc = dis.invariant_desc();
    if (invar_desc.empty()) {
        if (live_str) return stat_desc.str();
        return "Who knows?  This is probably a bug.";
    }
    if (live_str) return invar_desc + "\n" + stat_desc.str();
    return invar_desc;
}
// end prototype for disease.cpp

static const char* const JSON_transcode_activity[] = {
	"RELOAD",
	"READ",
	"WAIT",
	"CRAFT",
	"BUTCHER",
	"BUILD",
	"VEHICLE",
	"REFILL_VEHICLE",
	"TRAIN"
};

static const char* const JSON_transcode_disease[] = {
	"GLARE",
	"WET",
	"COLD",
	"COLD_FACE",
	"COLD_HANDS",
	"COLD_LEGS",
	"COLD_FEET",
	"HOT",
	"HEATSTROKE",
	"FBFACE",
	"FBHANDS",
	"FBFEET",
	"INFECTION",
	"COMMON_COLD",
	"FLU",
	"SMOKE",
	"ONFIRE",
	"TEARGAS",
	"BOOMERED",
	"SAP",
	"SPORES",
	"FUNGUS",
	"SLIMED",
	"DEAF",
	"BLIND",
	"LYING_DOWN",
	"SLEEP",
	"POISON",
	"BADPOISON",
	"FOODPOISON",
	"SHAKES",
	"DERMATIK",
	"FORMICATION",
	"WEBBED",
	"RAT",
	"PKILL1",
	"PKILL2",
	"PKILL3",
	"PKILL_L",
	"DRUNK",
	"CIG",
	"HIGH",
	"HALLU",
	"VISUALS",
	"IODINE",
	"TOOK_XANAX",
	"TOOK_PROZAC",
	"TOOK_FLUMED",
	"TOOK_VITAMINS",
	"ADRENALINE",
	"ASTHMA",
	"METH",
    "STONED",
    "BEARTRAP",
	"IN_PIT",
	"STUNNED",
	"DOWNED",
	"ATTACK_BOOST",
	"DAMAGE_BOOST",
	"DODGE_BOOST",
	"ARMOR_BOOST",
	"SPEED_BOOST",
	"VIPER_COMBO",
	"AMIGARA",
	"TELEGLOW",
	"ATTENTION",
	"EVIL",
	"ASKED_TO_FOLLOW",
	"ASKED_TO_LEAD",
	"ASKED_FOR_ITEM",
	"CATCH_UP"
};

static const char* const JSON_transcode_hp_parts[] = {
	"head",
	"torso",
	"arm_l",
	"arm_r",
	"leg_l",
	"leg_r"
};

static const char* const JSON_transcode_morale[] = {
	"FOOD_GOOD",
	"MUSIC",
	"MARLOSS",
	"FEELING_GOOD",
	"CRAVING_NICOTINE",
	"CRAVING_CAFFEINE",
	"CRAVING_ALCOHOL",
	"CRAVING_OPIATE",
	"CRAVING_SPEED",
	"CRAVING_COCAINE",
    "CRAVING_MARIJUANA",
    "FOOD_BAD",
	"VEGETARIAN",
	"WET",
	"FEELING_BAD",
	"KILLED_INNOCENT",
	"KILLED_FRIEND",
	"KILLED_MONSTER",
	"MOODSWING",
	"BOOK",
	"SCREAM"
};

static const char* const JSON_transcode_pl_flags[] = {
	"FLEET",
	"PARKOUR",
	"QUICK",
	"OPTIMISTIC",
	"FASTHEALER",
	"LIGHTEATER",
	"PAINRESIST",
	"NIGHTVISION",
	"POISRESIST",
	"FASTREADER",
	"TOUGH",
	"THICKSKIN",
	"PACKMULE",
	"FASTLEARNER",
	"DEFT",
	"DRUNKEN",
	"GOURMAND",
	"ANIMALEMPATH",
	"TERRIFYING",
	"DISRESISTANT",
	"ADRENALINE",
	"INCONSPICUOUS",
	"MASOCHIST",
	"LIGHTSTEP",
	"HEARTLESS",
	"ANDROID",
	"ROBUST",
	"MARTIAL_ARTS",
	"",	// PF_SPLIT
	"MYOPIC",
	"HEAVYSLEEPER",
	"ASTHMA",
	"BADBACK",
	"ILLITERATE",
	"BADHEARING",
	"INSOMNIA",
	"VEGETARIAN",
	"GLASSJAW",
	"FORGETFUL",
	"LIGHTWEIGHT",
	"ADDICTIVE",
	"TRIGGERHAPPY",
	"SMELLY",
	"CHEMIMBALANCE",
	"SCHIZOPHRENIC",
	"JITTERY",
	"HOARDER",
	"SAVANT",
	"MOODSWINGS",
	"WEAKSTOMACH",
	"WOOLALLERGY",
	"HPIGNORANT",
	"TRUTHTELLER",
	"UGLY",
	"",	// PF_MAX
	"SKIN_ROUGH",
	"NIGHTVISION2",
	"NIGHTVISION3",
	"INFRARED",
	"FASTHEALER2",
	"REGEN",
	"FANGS",
	"MEMBRANE",
	"GILLS",
	"SCALES",
	"THICK_SCALES",
	"SLEEK_SCALES",
	"LIGHT_BONES",
    "HOLLOW_BONES",
    "FEATHERS",
	"LIGHTFUR",
	"FUR",
	"CHITIN",
	"CHITIN2",
	"CHITIN3",
	"SPINES",
	"QUILLS",
	"PLANTSKIN",
	"BARK",
	"THORNS",
	"LEAVES",
	"NAILS",
	"CLAWS",
	"TALONS",
	"RADIOGENIC",
	"MARLOSS",
	"PHEROMONE_INSECT",
	"PHEROMONE_MAMMAL",
	"DISIMMUNE",
	"POISONOUS",
	"SLIME_HANDS",
	"COMPOUND_EYES",
	"PADDED_FEET",
	"HOOVES",
	"SAPROVORE",
	"RUMINANT",
	"HORNS",
	"HORNS_CURLED",
	"HORNS_POINTED",
	"ANTENNAE",
	"FLEET2",
	"TAIL_STUB",
	"TAIL_FIN",
	"TAIL_LONG",
	"TAIL_FLUFFY",
	"TAIL_STING",
	"TAIL_CLUB",
	"PAINREC1",
	"PAINREC2",
	"PAINREC3",
	"WINGS_BIRD",
	"WINGS_INSECT",
	"MOUTH_TENTACLES",
	"MANDIBLES",
	"CANINE_EARS",
	"WEB_WALKER",
	"WEB_WEAVER",
	"WHISKERS",
	"STR_UP",
	"STR_UP_2",
	"STR_UP_3",
	"STR_UP_4",
	"DEX_UP",
	"DEX_UP_2",
	"DEX_UP_3",
	"DEX_UP_4",
	"INT_UP",
	"INT_UP_2",
	"INT_UP_3",
	"INT_UP_4",
	"PER_UP",
	"PER_UP_2",
	"PER_UP_3",
	"PER_UP_4",
	"HEADBUMPS",
	"ANTLERS",
	"SLIT_NOSTRILS",
	"FORKED_TONGUE",
	"EYEBULGE",
	"MOUTH_FLAPS",
	"WINGS_STUB",
	"WINGS_BAT",
	"PALE",
	"SPOTS",
	"SMELLY2",
	"DEFORMED",
	"DEFORMED2",
	"DEFORMED3",
	"NAUSEA",
	"VOMITOUS",
	"HUNGER",
	"THIRST",
	"ROT1",
	"ROT2",
	"ROT3",
	"ALBINO",
	"SORES",
	"TROGLO",
	"TROGLO2",
	"TROGLO3",
	"WEBBED",
	"BEAK",
	"UNSTABLE",
	"RADIOACTIVE1",
	"RADIOACTIVE2",
	"RADIOACTIVE3",
	"SLIMY",
	"HERBIVORE",
	"CARNIVORE",
	"PONDEROUS1",
	"PONDEROUS2",
	"PONDEROUS3",
	"SUNLIGHT_DEPENDENT",
	"COLDBLOOD",
	"COLDBLOOD2",
	"COLDBLOOD3",
	"GROWL",
	"SNARL",
	"SHOUT1",
	"SHOUT2",
	"SHOUT3",
	"ARM_TENTACLES",
	"ARM_TENTACLES_4",
	"ARM_TENTACLES_8",
	"SHELL",
	"LEG_TENTACLES"
};

DEFINE_JSON_ENUM_SUPPORT_TYPICAL(activity_type, JSON_transcode_activity)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(dis_type, JSON_transcode_disease)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(hp_part, JSON_transcode_hp_parts)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(morale_type, JSON_transcode_morale)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(pl_flag, JSON_transcode_pl_flags)

player::player()
: mobile(GPS_loc(tripoint(0, 0, 0), point(-1,-1)), 100), pos(-1,-1), in_vehicle(false), active_mission(-1), male(true),
  str_cur(8),dex_cur(8),int_cur(8),per_cur(8),str_max(8),dex_max(8),int_max(8),per_max(8),
  power_level(0),max_power_level(0),hunger(0),thirst(0),fatigue(0),health(0),
  underwater(false),oxygen(0),recoil(0),driving_recoil(0),scent(500),
  stim(0),pain(0),pkill(0),radiation(0),cash(0),xp_pool(0),inv_sorted(true),
  last_item(itm_null),style_selected(itm_null),weapon(item::null),dodges_left(1),blocks_left(1)
{
 for (int i = 0; i < num_skill_types; i++) {
  sklevel[i] = 0;
  skexercise[i] = 0;
 }
 for (int i = 0; i < PF_MAX2; i++)
  my_traits[i] = false;
 for (int i = 0; i < PF_MAX2; i++)
  my_mutations[i] = false;

 mutation_category_level[0] = 5; // Weigh us towards no category for a bit
 for (int i = 1; i < NUM_MUTATION_CATEGORIES; i++)
  mutation_category_level[i] = 0;
}

void player::screenpos_set(point pt)
{
    set_screenpos(pos = pt);
    auto g = game::active();
    if (this == &g->u && g->update_map_would_scroll(pos)) g->update_map(pos.x,pos.y);
}

void player::screenpos_set(int x, int y) {
    set_screenpos(pos = point(x, y));
    auto g = game::active();
    if (this == &g->u && g->update_map_would_scroll(pos)) g->update_map(pos.x, pos.y);
}

void player::screenpos_add(point delta) {
    set_screenpos(pos += delta);
    auto g = game::active();
    if (this == &g->u && g->update_map_would_scroll(pos)) g->update_map(pos.x, pos.y);
}

bool player::is_enemy(const monster* z) const { return z->is_enemy(this); }

void player::pick_name()
{
    name = random_first_name(male) + " " + random_last_name();
}
 
void player::reset(const Badge<game>& auth)
{
// Reset our stats to normal levels
// Any persistent buffs/debuffs will take place in disease.h,
// player::suffer(), etc.
 str_cur = str_max;
 dex_cur = dex_max;
 int_cur = int_max;
 per_cur = per_max;
// We can dodge again!
 dodges_left = 1;
 blocks_left = 1;
// Didn't just pick something up
 last_item = itype_id(itm_null);
// Bionic buffs
 if (has_active_bionic(bio_hydraulics)) str_cur += 20;
 if (has_bionic(bio_eye_enhancer)) per_cur += 2;
 if (has_bionic(bio_carbon)) dex_cur -= 2;
 if (has_bionic(bio_armor_head)) per_cur--;
 if (has_bionic(bio_armor_arms)) dex_cur--;
 if (has_bionic(bio_metabolics) && power_level < max_power_level && hunger < 100) {
  hunger += 2;
  power_level++;
 }

// Trait / mutation buffs
 if (has_trait(PF_THICK_SCALES)) dex_cur -= 2;
 if (has_trait(PF_CHITIN2) || has_trait(PF_CHITIN3)) dex_cur--;
 if (has_trait(PF_COMPOUND_EYES) && !wearing_something_on(bp_eyes)) per_cur++;
 if (has_trait(PF_ARM_TENTACLES) || has_trait(PF_ARM_TENTACLES_4) || has_trait(PF_ARM_TENTACLES_8)) dex_cur++;
// Pain
 if (pain > pkill) {
  const int delta = pain - pkill;
  str_cur  -=     delta / 15;
  dex_cur  -=     delta / 15;
  per_cur  -=     delta / 20;
  int_cur  -= 1 + delta / 25;
 }
// Morale
 if (const int cur_morale = morale_level(); abs(cur_morale) >= 100) {
  str_cur  += cur_morale / 180;
  dex_cur  += cur_morale / 200;
  per_cur  += cur_morale / 125;
  int_cur  += cur_morale / 100;
 }
// Radiation
 if (radiation > 0) {
  str_cur  -= radiation / 80;
  dex_cur  -= radiation / 110;
  per_cur  -= radiation / 100;
  int_cur  -= radiation / 120;
 }
// Stimulants
 dex_cur += int(stim / 10);
 per_cur += int(stim /  7);
 int_cur += int(stim /  6);
 if (stim >= 30) { 
  const int delta = stim - 15;
  dex_cur -= delta /  8;
  per_cur -= delta / 12;
  int_cur -= delta / 14;
 }

// Set our scent towards the norm
 decltype(scent) norm_scent = 500;
 if (has_trait(PF_SMELLY)) norm_scent = 800;
 if (has_trait(PF_SMELLY2)) norm_scent = 1200;

 // C:Whales rounded absolute value of change up, not integer-truncate
 if (scent < norm_scent) scent += (norm_scent - scent + 1)/2;
 else if (scent > norm_scent) scent -= (scent - norm_scent + 1) / 2;

// Give us our movement points for the turn.
 moves += current_speed();

// Floor for our stats.  No stat changes should occur after this!
 if (dex_cur < 0) dex_cur = 0;
 if (str_cur < 0) str_cur = 0;
 if (per_cur < 0) per_cur = 0;
 if (int_cur < 0) int_cur = 0;

 // adjust morale based on pharmacology
 int mor = morale_level();
 const int thc = disease_level(DI_THC);
 if (thc) {
     // handwaving...should get +20 morale at 3 doses.
     const int thc_morale_ub = clamped_ub<20>(1 + (thc - 1) / (3 * 60 / 20));
     const int thc_morale = get_morale(MORALE_FEELING_GOOD);   // XXX hijack Chem Imbalance
     if (thc_morale_ub > thc_morale) {
         add_morale(MORALE_FEELING_GOOD, 1, thc_morale_ub);
         mor += 1;
     } else if ((MIN_MORALE_READ + 5) > mor) {  // B-movie: THC can rescue you from *any* amount of morale penalties enough to read or craft!
         add_morale(MORALE_FEELING_GOOD, 1);
         mor += 1;
     }
 }

 int xp_frequency = 10 - int(mor / 20);
 if (xp_frequency < 1) xp_frequency = 1;
 if (thc) {
     // but we don't want to learn when dosed.  Cf. iuse::weed.
     xp_frequency += 1 + (thc - 1) / 60;
 }

 if (int(messages.turn) % xp_frequency == 0) xp_pool++;

 if (xp_pool > 800) xp_pool = 800;
}

void player::update_morale()
{
 auto i = morale.size();
 while (0 < i) {
     auto& m = morale[--i];
     if (0 > m.bonus) m.bonus++;
     else if (0 < m.bonus) m.bonus--;

     if (m.bonus == 0) EraseAt(morale, i);
 }
}

template<bool want_details = false> static int _current_speed(const player& u, game* g, std::vector<std::pair<std::string, int> >* desc = nullptr)
{
    int newmoves = 100; // Start with 100 movement points...
   // Minus some for weight...
    int carry_penalty = 0;
    const int wgt_capacity = u.weight_capacity();
    const int wgt_carried = u.weight_carried();
    if (wgt_carried > wgt_capacity/4)
        carry_penalty = (75.0 * (wgt_carried - wgt_capacity/4)) / rational_scaled<3, 4>(wgt_capacity);

    newmoves -= carry_penalty;
    if constexpr (want_details) {
        if (carry_penalty) desc->push_back({ "Overburdened", -carry_penalty });
    }

    if (const int cur_morale = u.morale_level(); abs(cur_morale) >= 100) {
        const int delta = clamped<-10, 10>(cur_morale / 25);
        newmoves += delta;
        if constexpr (want_details) desc->push_back({ 0 < delta ? "Good mood" : "Depressed", delta });
    }

    if (u.pain > u.pkill) {
        const int delta = clamped_ub<60>(rational_scaled<7, 10>(u.pain - u.pkill));
        newmoves -= delta;
        if constexpr (want_details) desc->push_back({ "Pain", -delta });
    }

    if (u.pkill >= 10) {
        const int delta = clamped_ub<30>(u.pkill / 10);
        newmoves -= delta;
        if constexpr (want_details) desc->push_back({ "Painkillers", -delta });
    }

    if (0 != u.stim) {
        const int delta = clamped_ub<40>(u.stim);
        newmoves += delta;
        if constexpr (want_details) desc->push_back({ 0 < delta ? "Stimulants" : "Depressants", delta });
    }

    // C:Whales does not have this in the UI
    if (u.radiation >= 40) newmoves -= clamped_ub<20>(u.radiation / 40);

    if (u.thirst >= 40 + 10) {
        const int delta = (u.thirst - 40) / 10;
        newmoves -= delta;
        if constexpr (want_details) desc->push_back({ "Thirst", -delta });
    }

    if (u.hunger >= 100 + 10) {
        const int delta = (u.hunger - 100) / 10;
        newmoves -= delta;
        if constexpr (want_details) desc->push_back({ "Hunger", -delta });
    }

    if (g) {
        if (u.has_trait(PF_SUNLIGHT_DEPENDENT) && !g->is_in_sunlight(u.GPSpos)) {
            const int delta = (g->light_level() >= 12 ? 5 : 10);
            newmoves -= delta;
            if constexpr (want_details) desc->push_back({ "Out of Sunlight", -delta });
        }
        if (const int cold = u.is_cold_blooded()) {
            if (const int delta = (65 - g->temperature) / mutation_branch::cold_blooded_severity[cold - 1]; 0 < delta) {
                newmoves -= delta;
                if constexpr (want_details) desc->push_back({ "Cold-Blooded", -delta });
            }
        }
    }

    static auto describe_illness = [&](const disease& cond) {
        if (const int delta = cond.speed_boost()) {
            newmoves += delta;
            if constexpr (want_details) desc->push_back({ cond.name(), delta });
        }
        return false;
    };

    u.do_foreach(describe_illness);

    if (u.has_trait(PF_QUICK)) {
        if (const auto delta = newmoves / 10) {
            newmoves += delta;
            if constexpr (want_details) desc->push_back({ "Quick", delta });
        }
    }

    // C:Whales does not have these in the UI
    if (u.has_artifact_with(AEP_SPEED_UP)) newmoves += 20;
    if (u.has_artifact_with(AEP_SPEED_DOWN)) newmoves -= 20;

    if (newmoves < 1) newmoves = 1;

    return newmoves;
}

int player::current_speed() const { return _current_speed(*this, game::active()); }
int player::theoretical_speed() const { return _current_speed(*this, nullptr); }

int player::run_cost(int base_cost) const
{
 int movecost = base_cost;
 if (has_trait(PF_PARKOUR) && base_cost > mobile::mp_turn) {
  clamp_lb<mobile::mp_turn>(movecost /= 2);
 }
 if (40 > hp_cur[hp_leg_l]) {
     movecost += (0 >= hp_cur[hp_leg_l]) ? (mobile::mp_turn / 2) : (mobile::mp_turn / 4);
 }
 if (40 > hp_cur[hp_leg_r]) {
     movecost += (0 >= hp_cur[hp_leg_r]) ? (mobile::mp_turn / 2) : (mobile::mp_turn / 4);
 }

 if (mobile::mp_turn == base_cost) {
     if (has_trait(PF_FLEET))
         movecost = int(movecost * .85);
     if (has_trait(PF_FLEET2))
         movecost = int(movecost * .7);
 }

 const bool is_barefoot = !wearing_something_on(bp_feet);

 if (is_barefoot && has_trait(PF_PADDED_FEET))
  movecost = int(movecost * .9);
 if (has_trait(PF_LIGHT_BONES))
  movecost = int(movecost * .9);
 if (has_trait(PF_HOLLOW_BONES))
  movecost = int(movecost * .8);
 if (has_trait(PF_WINGS_INSECT))
  movecost -= 15;
 if (has_trait(PF_LEG_TENTACLES))
  movecost += 20;
 if (has_trait(PF_PONDEROUS1))
  movecost = int(movecost * 1.1);
 if (has_trait(PF_PONDEROUS2))
  movecost = int(movecost * 1.2);
 if (has_trait(PF_PONDEROUS3))
  movecost = int(movecost * 1.3);
 movecost += encumb(bp_mouth) * 5 + encumb(bp_feet) * 5 + encumb(bp_legs) * 3;
 if (is_barefoot && !has_trait(PF_PADDED_FEET) && !has_trait(PF_HOOVES))
  movecost += 15;

 return movecost;
}
 
int player::swim_speed()
{
 int ret = 440 + 2 * weight_carried() - 50 * sklevel[sk_swimming];
 if (has_trait(PF_WEBBED)) ret -= 60 + str_cur * 5;
 if (has_trait(PF_TAIL_FIN)) ret -= 100 + str_cur * 10;
 if (has_trait(PF_SLEEK_SCALES)) ret -= 100;
 if (has_trait(PF_LEG_TENTACLES)) ret -= 60;
 ret += (50 - sklevel[sk_swimming] * 2) * abs(encumb(bp_legs));
 ret += (80 - sklevel[sk_swimming] * 3) * abs(encumb(bp_torso));
 if (sklevel[sk_swimming] < 10) {
  const int scale = 10 - sklevel[sk_swimming];
  for (const auto& it : worn) ret += (it.volume() * scale) / 2;
 }
 ret -= str_cur * 6 + dex_cur * 4;
// If (ret > 500), we can not swim; so do not apply the underwater bonus.
 if (underwater && ret < 500) ret -= 50;
 if (ret < 30) ret = 30;
 return ret;
}

void player::swim(const GPS_loc& loc)
{
    DEBUG_FAIL_OR_LEAVE(!is<swimmable>(GPSpos.ter()), return);
    set_screenpos(loc);
    if (rem_disease(DI_ONFIRE)) {	// VAPORWARE: not for phosphorus or lithium ...
        messages.add("The water puts out the flames!");
    }
    int movecost = swim_speed();
    practice(sk_swimming, 1);
    if (movecost >= 5 * mobile::mp_turn) {
        if (!underwater) {
            messages.add("You sink%s!", (movecost >= 6 * mobile::mp_turn ? " like a rock" : ""));
            swimming_dive(); // Involuntarily.
        }
    }
    if (is_drowning()) {
        if (movecost < 5 * mobile::mp_turn)
            popup("You need to breathe! (Press '<' to surface.)");
        else
            popup("You need to breathe but you can't swim!  Get to dry land, quick!");
    }
    moves -= (movecost > 2 * mobile::mp_turn ? 2 * mobile::mp_turn : movecost);
    for (size_t i = 0; i < inv.size(); i++) {
        decltype(auto) it = inv[i];
        if (IRON == it.type->m1 && it.damage < 5 && one_in(8)) it.damage++; // \todo this is way too fast; also, item::damage invariant not checked for properly
    }
}

bool player::swimming_surface()
{
    if (500 > swim_speed()) {
        underwater = false;
        return true;
    };
    return false;
}

bool player::swimming_dive()
{
    if (underwater) return false;
    underwater = true;
    oxygen = 30 + 2 * str_cur;
    return true;
}

// coordinates with player::suffer, player::swim
int player::is_drowning() const
{
    if (!underwater || 5 < oxygen) return 0; // not out of breath yet
    if (has_trait(PF_GILLS)) return 0;  // breathe water: Magick.
    if (has_bionic(bio_gills) && 0 < power_level) return 0; // breathe water: Hypertechnology.
    return 0 < oxygen ? 1 : -1;
}

nc_color player::color() const
{
 if (has_disease(DI_ONFIRE)) return c_red;
 if (has_disease(DI_STUNNED)) return c_ltblue;
 if (has_disease(DI_BOOMERED)) return c_pink;
 if (underwater) return c_blue;
 if (has_active_bionic(bio_cloak) || has_artifact_with(AEP_INVISIBLE)) return c_dkgray;
 return c_white;
}

std::pair<int, nc_color> player::hp_color(hp_part part) const
{
    const int curhp = hp_cur[part];
    const int max = hp_max[part];
    if (curhp >= max) return std::pair(curhp, c_green);
    else if (curhp > max * .8) return std::pair(curhp, c_ltgreen);
    else if (curhp > max * .5) return std::pair(curhp, c_yellow);
    else if (curhp > max * .3) return std::pair(curhp, c_ltred);
    return std::pair(curhp, c_red);
}

static nc_color stat_color(int cur, int max) {
    if (0 >= cur) return c_dkgray;
    if (max / 2 > cur) return c_red;
    if (max > cur) return c_ltred;
    if (max == cur) return c_white;
    if (2 * max > 3 * cur) return c_ltgreen;
    return c_green;
}

static nc_color encumb_color(int level)
{
	if (level < 0) return c_green;
	if (level == 0) return c_ltgray;
	if (level < 4) return c_yellow;
	if (level < 7) return c_ltred;
	return c_red;
}

int player::is_cold_blooded() const
{
    if (has_trait(PF_COLDBLOOD)) return 1;
    if (has_trait(PF_COLDBLOOD2)) return 1 + (PF_COLDBLOOD2 - PF_COLDBLOOD);
    if (has_trait(PF_COLDBLOOD3)) return 1 + (PF_COLDBLOOD3 - PF_COLDBLOOD);
    return 0;
}

int player::has_light_bones() const
{
    if (has_trait(PF_LIGHT_BONES)) return 1;
    if (has_trait(PF_HOLLOW_BONES)) return 1 + (PF_HOLLOW_BONES - PF_LIGHT_BONES);
    return 0;
}

static stat_delta _troglodyte_sunburn(const player& u)
{
    stat_delta ret = {};
    const game* const g = game::active();
    if ((u.has_trait(PF_TROGLO) || u.has_trait(PF_TROGLO2)) && g->weather == WEATHER_SUNNY) {
        ret.Str--;
        ret.Dex--;
        ret.Int--;
        ret.Per--;
    }
    if (u.has_trait(PF_TROGLO2)) {
        ret.Str--;
        ret.Dex--;
        ret.Int--;
        ret.Per--;
    }
    if (u.has_trait(PF_TROGLO3)) {
        ret.Str -= 4;
        ret.Dex -= 4;
        ret.Int -= 4;
        ret.Per -= 4;
    }
    return ret;
}

void player::disp_info(game *g)
{
 int line;
 std::vector<std::string> effect_name;
 std::vector<std::string> effect_text;
 for(const auto& cond : illness) {
  const auto name = cond.name();
  if (!name) continue;
  effect_name.push_back(name);
  effect_text.push_back(describe(cond, *this));
 }
 if (const int cur_morale = morale_level(), abs_morale = abs(cur_morale); abs_morale >= 100) {
  bool pos = (cur_morale > 0);
  effect_name.push_back(pos ? "Elated" : "Depressed");
  const char* const sgn_text = (pos ? " +" : " ");
  std::ostringstream morale_text;
  if (200 <= abs_morale) morale_text << "Dexterity" << sgn_text << cur_morale / 200 << "   ";
  if (180 <= abs_morale) morale_text << "Strength" << sgn_text << cur_morale / 180 << "   ";
  if (125 <= abs_morale) morale_text << "Perception" << sgn_text << cur_morale / 125 << "   ";
  morale_text << "Intelligence" << sgn_text << cur_morale / 100 << "   ";
  effect_text.push_back(morale_text.str());
 }
 if (pain > pkill) {
  const int pain_delta = pain - pkill;
  effect_name.push_back("Pain");
  std::ostringstream pain_text;
  if (const auto malus = pain_delta / 15; 1 <= malus) pain_text << "Strength -" << malus << "   Dexterity -" << malus << "   ";
  if (const auto malus = pain_delta / 20; 1 <= malus) pain_text << "Perception -" << malus << "   ";
  pain_text << "Intelligence -" << 1 + pain_delta / 25;
  effect_text.push_back(pain_text.str());
 }
 if (stim > 0) {
  int dexbonus = int(stim / 10);
  int perbonus = int(stim /  7);
  int intbonus = int(stim /  6);
  if (abs(stim) >= 30) { 
   dexbonus -= int(abs(stim - 15) /  8);
   perbonus -= int(abs(stim - 15) / 12);
   intbonus -= int(abs(stim - 15) / 14);
  }
  
  if (dexbonus < 0)
   effect_name.push_back("Stimulant Overdose");
  else
   effect_name.push_back("Stimulant");
  std::ostringstream stim_text;
  stim_text << "Speed +" << stim << "   Intelligence " <<
               (intbonus > 0 ? "+ " : "") << intbonus << "   Perception " <<
               (perbonus > 0 ? "+ " : "") << perbonus << "   Dexterity "  <<
               (dexbonus > 0 ? "+ " : "") << dexbonus;
  effect_text.push_back(stim_text.str());
 } else if (stim < 0) {
  effect_name.push_back("Depressants");
  std::ostringstream stim_text;
  int dexpen = int(stim / 10);
  int perpen = int(stim /  7);
  int intpen = int(stim /  6);
// Since dexpen etc. are always less than 0, no need for + signs
  stim_text << "Speed " << stim << "   Intelligence " << intpen <<
               "   Perception " << perpen << "   Dexterity " << dexpen;
  effect_text.push_back(stim_text.str());
 }

 static auto sun_severity = [](int delta) {
     if (-1 <= delta) return "The sunlight irritates you.\n";
     if (-2 <= delta) return "The sunlight irritates you badly.\n";
     return "The sunlight irritates you terribly.\n";
 };

 if (g->is_in_sunlight(GPSpos)) {
     const auto trog_stats = _troglodyte_sunburn(*this);
     if (0 > trog_stats.Str) { // Not really.  Historical coincidence that all four coordinates are equal
         std::string trog_text(sun_severity(trog_stats.Str));
         trog_text += "Strength " + std::to_string(trog_stats.Str);
         if (trog_stats.Dex) trog_text += "; Dexterity " + std::to_string(trog_stats.Dex);
         if (trog_stats.Int) trog_text += "; Intelligence " + std::to_string(trog_stats.Int);
         if (trog_stats.Per) trog_text += "; Perception " + std::to_string(trog_stats.Per);
         effect_name.push_back("In Sunlight");
         effect_text.push_back(std::move(trog_text));
     }
 }

 for (int i = 0; i < addictions.size(); i++) {
  if (addictions[i].sated < 0 &&
      addictions[i].intensity >= MIN_ADDICTION_LEVEL) {
   effect_name.push_back(addiction_name(addictions[i]));
   effect_text.push_back(addiction_text(addictions[i]));
  }
 }

 enum tabs {
     stats = 1,
     traits,
     encumbrance,
     effects,
     skills
 };

 // XXX yes inherited widths don't add up properly
 WINDOW* w_grid    = newwin(VIEW, SCREEN_WIDTH,  0,  0);

 // intent is 3x2 subpanels of "roughly equal width"
 const int subpanel_width = (SCREEN_WIDTH-2)/3; // C:Whales 26

 // non-scaling row is top row
 // bottom row expands to fill up available screen space
 WINDOW* w_cells[] = {
    newwin(9, subpanel_width,  2,  0),
    newwin(VIEW - 16, subpanel_width, 12,  0),
    newwin(9, subpanel_width,  2, subpanel_width + 1),
    newwin(VIEW - 16, subpanel_width, 12, subpanel_width + 1),
    newwin(9, subpanel_width,  2, 2 * subpanel_width + 2),
    newwin(VIEW - 16, subpanel_width, 12, 2 * subpanel_width + 2)
 };

 WINDOW* w_stats   = w_cells[stats-1]; // minimum height 7
 WINDOW* w_encumb  = w_cells[encumbrance - 1]; // minimum height 9
 WINDOW* w_traits  = w_cells[traits - 1]; // would like to scale
 WINDOW* w_effects = w_cells[effects - 1]; // would like to scale
 WINDOW* w_skills  = w_cells[skills - 1]; // would like to scale
 WINDOW* w_speed   = w_cells[skills]; // would like to scale
 WINDOW* w_info    = newwin( 3, SCREEN_WIDTH, VIEW - 3,  0);

 // This is UI; do not micro-optimize by wiring as constexpr 2020-11-17 zaimoni
 const int traits_hgt = getmaxy(w_traits) - 2;
 const int effects_hgt = getmaxy(w_effects) - 2;

// Print name and header
 mvwprintw(w_grid, 0, 0, "%s - %s", name.c_str(), (male ? "Male" : "Female"));
 mvwaddstrz(w_grid, 0, SCREEN_WIDTH/2 - 1, c_ltred, "| Press TAB to cycle, ESC or q to return.");
// Main line grid
 draw_hline(w_grid,  1, c_ltgray, LINE_OXOX);
 draw_hline(w_grid, 11, c_ltgray, LINE_OXOX);
 draw_hline(w_grid, VIEW - 4, c_ltgray, LINE_OXOX);
 for (int i = 2; i < VIEW - 4; i++) {
   mvwputch(w_grid, i, subpanel_width, c_ltgray, LINE_XOXO);
   mvwputch(w_grid, i, 2 * subpanel_width + 1, c_ltgray, LINE_XOXO);
 }
 mvwputch(w_grid,  1, subpanel_width, c_ltgray, LINE_OXXX);
 mvwputch(w_grid,  1, 2 * subpanel_width + 1, c_ltgray, LINE_OXXX);
 mvwputch(w_grid, VIEW - 4, subpanel_width, c_ltgray, LINE_XXOX);
 mvwputch(w_grid, VIEW - 4, 2 * subpanel_width + 1, c_ltgray, LINE_XXOX);
 mvwputch(w_grid, 11, subpanel_width, c_ltgray, LINE_XXXX);
 mvwputch(w_grid, 11, 2 * subpanel_width + 1, c_ltgray, LINE_XXXX);
 wrefresh(w_grid);	// w_grid should stay static.

// First!  Default STATS screen.
 static constexpr const char* stat_labels[] = {
     "Strength:",
     "Dexterity:",
     "Intelligence:",
     "Perception:"
 };

 // \todo? Technically the stats and encumbrance subpanels should use available width.
 // However, they have other issues (the text labels need central configuration, and are one of
 // the easier translation targets.)

 mvwaddstrz(w_stats, 0, 10, c_ltgray, "STATS");
 mvwaddstrz(w_stats, 2,  2, c_ltgray, "Strength:");
 mvwprintz(w_stats, 2, 20 - int_log10(str_max), c_ltgray, "(%d)", str_max);
 mvwaddstrz(w_stats, 3, 2, c_ltgray, "Dexterity:");
 mvwprintz(w_stats, 3, 20 - int_log10(dex_max), c_ltgray, "(%d)", dex_max);
 mvwaddstrz(w_stats, 4, 2, c_ltgray, "Intelligence:");
 mvwprintz(w_stats, 4, 20 - int_log10(int_max), c_ltgray, "(%d)", int_max);
 mvwaddstrz(w_stats, 5, 2, c_ltgray, "Perception:");
 mvwprintz(w_stats, 5, 20 - int_log10(per_max), c_ltgray, "(%d)", per_max);

 mvwprintz(w_stats,  2, 17-int_log10(str_cur), stat_color(str_cur, str_max), "%d", str_cur);
 mvwprintz(w_stats,  3, 17-int_log10(dex_cur), stat_color(dex_cur, dex_max), "%d", dex_cur);
 mvwprintz(w_stats,  4, 17-int_log10(int_cur), stat_color(int_cur, int_max), "%d", int_cur);
 mvwprintz(w_stats,  5, 17-int_log10(per_cur), stat_color(per_cur, per_max), "%d", per_cur);

 wrefresh(w_stats);

// C++20: compute labels from body_part_name
 static constexpr const char* enc_labels[] = {
   "Head",
   "Eyes",
   "Mouth",
   "Torso",
   "Hands",
   "Legs",
   "Feet"
 };

// Next, draw encumberment.
 mvwaddstrz(w_encumb, 0, 6, c_ltgray, "ENCUMBRANCE");
 mvwaddstrz(w_encumb, 2, 2, c_ltgray, "Head................");
 int enc = encumb(bp_head);    // problematic if this can be negative (need better function)
 mvwprintz(w_encumb, 2, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 mvwaddstrz(w_encumb, 3, 2, c_ltgray, "Eyes................");
 enc = encumb(bp_eyes);
 mvwprintz(w_encumb, 3, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 mvwaddstrz(w_encumb, 4, 2, c_ltgray, "Mouth...............");
 enc = encumb(bp_mouth);
 mvwprintz(w_encumb, 4, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 mvwaddstrz(w_encumb, 5, 2, c_ltgray, "Torso...............");
 enc = encumb(bp_torso);
 mvwprintz(w_encumb, 5, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 mvwaddstrz(w_encumb, 6, 2, c_ltgray, "Hands...............");
 enc = encumb(bp_hands);
 mvwprintz(w_encumb, 6, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 mvwaddstrz(w_encumb, 7, 2, c_ltgray, "Legs................");
 enc = encumb(bp_legs);
 mvwprintz(w_encumb, 7, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 mvwaddstrz(w_encumb, 8, 2, c_ltgray, "Feet................");
 enc = encumb(bp_feet);
 mvwprintz(w_encumb, 8, 21 - int_log10(abs(enc)) - (0 > enc), encumb_color(enc), "%d", enc);
 wrefresh(w_encumb);

// Next, draw traits.
 line = 2;
 mvwaddstrz(w_traits, 0, 9, c_ltgray, "TRAITS");

 static auto traits_vector = [&]() {
     std::vector<pl_flag> ret;
     for (int i = 0; i < PF_MAX2; i++) {
         if (my_traits[i]) {
             ret.push_back(pl_flag(i));
             if (line < 9) {
                 const auto& tr = mutation_branch::traits[i];
                 mvwaddstrz(w_traits, line, 1, (0 < tr.points ? c_ltgreen : c_ltred), tr.name.c_str());
                 line++;
             }
         }
     }
     return ret;
 };

 const std::vector<pl_flag> traitslist = traits_vector();
 const size_t traits_ub = traitslist.size();

 wrefresh(w_traits);

// Next, draw effects.
 line = 2;
 mvwaddstrz(w_effects, 0, 8, c_ltgray, "EFFECTS");
 for (int i = 0; i < effect_name.size() && line < 9; i++) {
  mvwaddstrz(w_effects, line, 1, c_ltgray, effect_name[i].c_str());
  line++;
 }
 wrefresh(w_effects);

// Next, draw skills.
 line = 2;
 std::vector <skill> skillslist;
 mvwaddstrz(w_skills, 0, 11, c_ltgray, "SKILLS");
 for (int i = 1; i < num_skill_types; i++) {
  if (sklevel[i] > 0) {
   skillslist.push_back(skill(i));
   if (line < 9) {
    mvwprintz(w_skills, line, 1, c_ltblue, "%s:", skill_name(skill(i)));
    mvwprintz(w_skills, line, 19, c_ltblue, "%d", sklevel[i]);
    mvwprintz(w_skills, line, 21, c_ltblue, "(%s%d%%)",
        (skexercise[i] < 10 && skexercise[i] >= 0 ? " " : ""),
        (skexercise[i] < 0 ? 0 : skexercise[i]));
    line++;
   }
  }
 }
 wrefresh(w_skills);

// Finally, draw speed.
 std::vector<std::pair<std::string, int> > speed_modifiers;

 mvwaddstrz(w_speed, 0, 11, c_ltgray, "SPEED");
 mvwaddstrz(w_speed, 1,  1, c_ltgray, "Base Move Cost:");
 mvwaddstrz(w_speed, 2,  1, c_ltgray, "Current Speed:");
 int newmoves = _current_speed<true>(*this, g, &speed_modifiers);
 int pen = 0;
 line = 3;
 for (decltype(auto) x : speed_modifiers) {
     auto display = (0 < x.second) ? std::pair(c_green, '+') : std::pair(c_red, '-');
     mvwaddstrz(w_speed, line, 1, display.first, x.first.c_str());
     mvwputch(w_speed, line, 23 - int_log10(abs(x.second)), display.first, abs(x.second));
     mvwputch(w_speed, line, 20, display.first, display.second);
     ++line;
 }

 int runcost = run_cost(mobile::mp_turn);
 nc_color col = (runcost <= mobile::mp_turn ? c_green : c_red);
 mvwprintz(w_speed, 1, 23 - int_log10(runcost), col, "%d", runcost);
 col = (newmoves >= mobile::mp_turn ? c_green : c_red);
 mvwprintz(w_speed, 2, 23 - int_log10(newmoves), col, "%d", newmoves);
 wrefresh(w_speed);

 refresh();
 int curtab = 1;
 int min, max;
 line = 0;
 bool done = false;

 nc_color status = c_white;

 static constexpr const char* stat_desc[] = {
     "\
Strength affects your melee damage, the amount of weight you can carry, your\n\
total HP, your resistance to many diseases, and the effectiveness of actions\n\
which require brute force.",
     "\
Dexterity affects your chance to hit in melee combat, helps you steady your\n\
gun for ranged combat, and enhances many actions that require finesse.",
     "\
Intelligence is less important in most situations, but it is vital for more\n\
complex tasks like electronics crafting. It also affects how much skill you\n\
can pick up from reading a book.",
"\
Perception is the most important stat for ranged combat. It's also used for\n\
detecting traps and other things of interest."
 };

 static_assert(std::end(stat_labels)-std::begin(stat_labels) == std::end(stat_desc) - std::begin(stat_desc));

// Initial printing is DONE.  Now we give the player a chance to scroll around
// and "hover" over different items for more info.
 do {
  werase(w_info);
  switch (curtab) {
  case stats:	// Stats tab
   mvwaddstrz(w_stats, 0, 10, h_ltgray, "STATS");
   mvwaddstrz(w_stats, 2 + line, 2, h_ltgray, stat_labels[line]);
   mvwaddstrz(w_info, 0, 0, c_magenta, stat_desc[line]);
   wrefresh(w_stats);
   wrefresh(w_info);
   switch (input()) {
    case 'j':
     mvwaddstrz(w_stats, 2 + line, 2, c_ltgray, stat_labels[line]);
     if (std::end(stat_labels) - std::begin(stat_labels) <= ++line) line = 0;
     mvwaddstrz(w_stats, 2 + line, 2, h_ltgray, stat_labels[line]);
     break;
    case 'k':
     mvwaddstrz(w_stats, 2 + line, 2, c_ltgray, stat_labels[line]);
     if (0 > --line) line = std::end(stat_labels) - std::begin(stat_labels) - 1;
     mvwaddstrz(w_stats, 2 + line, 2, h_ltgray, stat_labels[line]);
     break;
    case '\t':
     mvwaddstrz(w_stats, 2 + line, 2, c_ltgray, stat_labels[line]);
     mvwaddstrz(w_stats, 0, 10, c_ltgray, "STATS");
     line = 0;
     curtab++;
     break;
    case 'q':
    case KEY_ESCAPE:
     done = true;
   }
   if (!done) wrefresh(w_stats);
   break;
  case encumbrance:	// Encumberment tab
   mvwaddstrz(w_encumb, 0, 6, h_ltgray, "ENCUMBRANCE");
   mvwaddstrz(w_encumb, 2 + line, 2, h_ltgray, enc_labels[line]);
   if (line == 0) {
    mvwaddstrz(w_info, 0, 0, c_magenta, "Head encumbrance has no effect; it simply limits how much you can put on.");
   } else if (line == 1) {
    const int enc_eyes = encumb(bp_eyes);
    mvwprintz(w_info, 0, 0, c_magenta, "Perception -%d when checking traps or firing ranged weapons;\n\
Perception -%.1f when throwing items", enc_eyes, enc_eyes / 2.0);
   } else if (line == 2) {
    mvwprintz(w_info, 0, 0, c_magenta, "Running costs +%d movement points", encumb(bp_mouth) * 5);
   } else if (line == 3) {
    const int enc_torso = encumb(bp_torso);
    mvwprintz(w_info, 0, 0, c_magenta, "Melee skill -%d;      Dodge skill -%d;\n\
Swimming costs +%d movement points;\n\
Melee attacks cost +%d movement points", enc_torso, enc_torso,
enc_torso * (80 - sklevel[sk_swimming] * 3), enc_torso * 20);
   } else if (line == 4) {
    const int enc_hands = encumb(bp_hands);
    mvwprintz(w_info, 0, 0, c_magenta, "Reloading costs +%d movement points;\n\
Dexterity -%d when throwing items", enc_hands * 30, enc_hands);
   } else if (line == 5) {
    const int enc_legs = encumb(bp_legs);
    const char* const sign = (enc_legs >= 0 ? "+" : "");
    const char* const osign = (enc_legs < 0 ? "+" : "-");
    mvwprintz(w_info, 0, 0, c_magenta, "\
Running costs %s%d movement points;  Swimming costs %s%d movement points;\n\
Dodge skill %s%.1f", sign, enc_legs * 3,
                     sign, enc_legs *(50 - sklevel[sk_swimming]),
                     osign, enc_legs / 2.0);
   } else if (line == 6) {
    const int enc_feet = encumb(bp_feet);
    mvwprintz(w_info, 0, 0, c_magenta, "Running costs %s%d movement points", (enc_feet >= 0 ? "+" : ""), enc_feet * 5);
   }
   wrefresh(w_encumb);
   wrefresh(w_info);
   switch (input()) {
    case 'j':
     mvwaddstrz(w_encumb, 2 + line, 2, c_ltgray, enc_labels[line]);
     if (std::end(enc_labels) - std::begin(enc_labels) <= ++line) line = 0;
     mvwaddstrz(w_encumb, 2 + line, 2, h_ltgray, enc_labels[line]);
     break;
    case 'k':
     mvwaddstrz(w_encumb, 2 + line, 2, c_ltgray, enc_labels[line]);
     if (0 > --line) line = std::end(enc_labels) - std::begin(enc_labels) - 1;
     mvwaddstrz(w_encumb, 2 + line, 2, h_ltgray, enc_labels[line]);
     break;
    case '\t':
     mvwaddstrz(w_encumb, 2 + line, 2, c_ltgray, enc_labels[line]);
     mvwaddstrz(w_encumb, 0, 6, c_ltgray, "ENCUMBRANCE");
     wrefresh(w_encumb);
     line = 0;
     curtab++;
     break;
    case 'q':
    case KEY_ESCAPE:
     done = true;
   }
   if (!done) wrefresh(w_encumb);
   break;
  case traits:	// Traits tab
   mvwaddstrz(w_traits, 0, 9, h_ltgray, "TRAITS");
   if (line <= 2) {
    min = 0;
    max = cataclysm::max(traits_hgt, traits_ub);
   } else if (line >= traits_ub - traits_hgt/2) {
    min = (traits_ub <= traits_hgt ? 0 : traits_ub - traits_hgt);
    max = traits_ub;
   } else {
    min = line - 3;
    max = clamped_ub(line + (traits_hgt - traits_hgt / 2), traits_ub);
   }
   for (int i = min; i < max; i++) {
    mvwprintz(w_traits, 2 + i - min, 1, c_ltgray, "                         ");
	if (traitslist[i] >= PF_MAX2) continue;	// XXX out of bounds dereference \todo better recovery strategy
	const auto& tr = mutation_branch::traits[traitslist[i]];
	status = (tr.points > 0 ? c_ltgreen : c_ltred);
    mvwaddstrz(w_traits, 2 + i - min, 1, (i == line) ? hilite(status) : status, tr.name.c_str());
   }
   if (line >= 0 && line < traits_ub)
    mvwaddstrz(w_info, 0, 0, c_magenta, mutation_branch::traits[traitslist[line]].description.c_str());
   wrefresh(w_traits);
   wrefresh(w_info);
   switch (input()) {
    case 'j':
     if (line < traits_ub - 1) line++;
     break;
    case 'k':
     if (line > 0) line--;
     break;
    case '\t':
     mvwaddstrz(w_traits, 0, 9, c_ltgray, "TRAITS");
     for (int i = 0; i < traits_ub && i < 7; i++) {
      draw_hline(w_traits, i + 2, c_black, 'x', 1);
	  if (traitslist[i] >= PF_MAX2) continue;	// XXX out of bounds dereference \todo better recovery strategy
	  const auto& tr = mutation_branch::traits[traitslist[i]];
      mvwaddstrz(w_traits, i + 2, 1, (0 < tr.points ? c_ltgreen : c_ltred), tr.name.c_str());
     }
     wrefresh(w_traits);
     line = 0;
     curtab++;
     break;
    case 'q':
    case KEY_ESCAPE:
     done = true;
   }
   break;

  case effects:	// Effects tab
   mvwaddstrz(w_effects, 0, 8, h_ltgray, "EFFECTS");
   if (line <= 2) {
    min = 0;
    max = cataclysm::max(effects_hgt, effect_name.size());
   } else if (line >= effect_name.size() - effects_hgt/2) {
    min = (effect_name.size() <= effects_hgt ? 0 : effect_name.size() - effects_hgt);
    max = effect_name.size();
   } else {
    min = line - 2;
    max = cataclysm::min(line + (effects_hgt - effects_hgt / 2), effect_name.size());
   }
   for (int i = min; i < max; i++) {
    mvwaddstrz(w_effects, 2 + i - min, 1, (i == line) ? h_ltgray : c_ltgray, effect_name[i].c_str());
   }
   if (line >= 0 && line < effect_text.size())
    mvwaddstrz(w_info, 0, 0, c_magenta, effect_text[line].c_str());
   wrefresh(w_effects);
   wrefresh(w_info);
   switch (input()) {
    case 'j':
     if (line < effect_name.size() - 1) line++;
     break;
    case 'k':
     if (line > 0) line--;
     break;
    case '\t':
     mvwaddstrz(w_effects, 0, 8, c_ltgray, "EFFECTS");
     for (int i = 0; i < effect_name.size() && i < effects_hgt; i++)
      mvwaddstrz(w_effects, i + 2, 1, c_ltgray, effect_name[i].c_str());
     wrefresh(w_effects);
     line = 0;
     curtab++;
     break;
    case 'q':
    case KEY_ESCAPE:
     done = true;
   }
   break;

  case skills:	// Skills tab
   mvwaddstrz(w_skills, 0, 11, h_ltgray, "SKILLS");
   if (line <= 2) {
    min = 0;
    max = 7;
    if (skillslist.size() < max) max = skillslist.size();
   } else if (line >= skillslist.size() - 3) {
    min = (skillslist.size() < 8 ? 0 : skillslist.size() - 7);
    max = skillslist.size();
   } else {
    min = line - 3;
    max = line + 4;
    if (skillslist.size() < max) max = skillslist.size();
    if (min < 0) min = 0;
   }
   for (int i = min; i < max; i++) {
    if (i == line) {
	 status = (skexercise[skillslist[i]] >= 100) ? h_pink : h_ltblue;
    } else {
	 status = (skexercise[skillslist[i]] < 0) ? c_ltred : c_ltblue;
    }
    draw_hline(w_skills, 2 + i - min, c_ltgray, ' ');
    mvwprintz(w_skills, 2 + i - min, 1, status, "%s:", skill_name(skill(skillslist[i])));
    mvwprintz(w_skills, 2 + i - min, 19, status, "%d", sklevel[skillslist[i]]);
    if (skexercise[i] >= 100) {
     mvwprintz(w_skills, 2 + i - min, 21, status, "(%d%%)", skexercise[skillslist[i]]);
    } else {
     mvwprintz(w_skills, 2 + i - min, 21, status, "(%s%d%%)",
               (skexercise[skillslist[i]] < 10 && skexercise[skillslist[i]] >= 0 ? " " : ""),
               (skexercise[skillslist[i]] <  0 ? 0 : skexercise[skillslist[i]]));
    }
   }
   werase(w_info);
   if (line >= 0 && line < skillslist.size())
    mvwaddstrz(w_info, 0, 0, c_magenta, skill_description(skill(skillslist[line])));
   wrefresh(w_skills);
   wrefresh(w_info);
   switch (input()) {
    case 'j':
     if (line < skillslist.size() - 1) line++;
     break;
    case 'k':
     if (line > 0) line--;
     break;
    case '\t':
     mvwaddstrz(w_skills, 0, 11, c_ltgray, "SKILLS");
     // C++20: view?
     for (int i = 0; i < skillslist.size() && i < 7; i++) {
	  status = (skexercise[skillslist[i]] < 0) ? c_ltred : c_ltblue;
      mvwprintz(w_skills, i + 2,  1, status, "%s:", skill_name(skill(skillslist[i])));
      mvwprintz(w_skills, i + 2, 19, status, "%d (%s%d%%)",
                sklevel[skillslist[i]],
                (skexercise[skillslist[i]] < 10 &&
                 skexercise[skillslist[i]] >= 0 ? " " : ""),
                (skexercise[skillslist[i]] <  0 ? 0 :
                 skexercise[skillslist[i]]));
     }
     wrefresh(w_skills);
     line = 0;
     curtab = 1;
     break;
    case 'q':
    case 'Q':
    case KEY_ESCAPE:
     done = true;
   }
  }
 } while (!done);
 
 werase(w_info);
 werase(w_grid);
 werase(w_stats);
 werase(w_encumb);
 werase(w_traits);
 werase(w_effects);
 werase(w_skills);
 werase(w_speed);
 werase(w_info);

 delwin(w_info);
 delwin(w_grid);
 delwin(w_stats);
 delwin(w_encumb);
 delwin(w_traits);
 delwin(w_effects);
 delwin(w_skills);
 delwin(w_speed);
 erase();
}

void player::disp_morale()
{
 WINDOW *w = newwin(VIEW - 3, SCREEN_WIDTH, 0, 0);
 wborder(w, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
            LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );

 static constexpr const int col_offset = sizeof("Morale Modifiers:") + 2; // C++20: constexpr strlen?

 mvwaddstrz(w, 1,  1, c_white, "Morale Modifiers:");
 mvwaddstrz(w, 2,  1, c_ltgray, "Name");
 mvwaddstrz(w, 2, col_offset, c_ltgray, "Value");

 static auto draw_morale_row = [&](int y, const char* label, int val) {
     int bpos = col_offset + 4 - int_log10(abs(val));
     const nc_color morale_color = (val < 0) ? (--bpos ,c_red) : c_green;

     mvwaddstrz(w, y, 1, morale_color, label);
     mvwprintz(w, y, bpos, morale_color, "%d", val);
 };

 for (int i = 0; i < morale.size(); i++) {
  if (VIEW - 5 <= i + 3) break; // don't overwrite our total line, or overflow the window
  draw_morale_row(i + 3, morale[i].name().c_str(), morale[i].bonus);
 }

 draw_morale_row(VIEW - 5, "Total:", morale_level());

 wrefresh(w);
 getch();
 werase(w);
 delwin(w);
}

static constexpr std::optional<nc_color> _recoil_color(unsigned int adj_recoil)
{
    if (adj_recoil >= 36) return c_red;
    if (adj_recoil >= 20) return c_ltred;
    if (adj_recoil >= 4) return c_yellow;
    if (adj_recoil > 0) return c_ltgray;
    return std::nullopt;
}

static_assert(c_red == _recoil_color(36));
static_assert(c_ltred == _recoil_color(35));
static_assert(c_ltred == _recoil_color(20));
static_assert(c_yellow == _recoil_color(19));
static_assert(c_yellow == _recoil_color(4));
static_assert(c_ltgray == _recoil_color(3));
static_assert(c_ltgray == _recoil_color(1));
static_assert(!_recoil_color(0));

// threshold table.  Won't be compile-time constant after translation tables built out.
static constexpr const std::pair<std::pair<nc_color, const char*>, std::pair<int, bool> > _hunger_lookup[] = {
    {std::pair(c_red, "Starving!"), std::pair(2800, true)},
    {std::pair(c_ltred, "Near starving"), std::pair(1400, true)},
    {std::pair(c_ltred, "Famished"), std::pair(300, true)},
    {std::pair(c_yellow, "Very hungry"), std::pair(100, true)},
    {std::pair(c_yellow, "Hungry"), std::pair(40, true)},
    {std::pair(c_green, "Full"), std::pair(0, false)},
};

static constexpr std::optional<std::pair<nc_color, const char*> > _hunger_text(int hunger)
{
    for (decltype(auto) x : _hunger_lookup) if (x.second.second ? (hunger > x.second.first) : (hunger < x.second.first)) return x.first;
    return std::nullopt;
}

static_assert(std::pair(c_red, "Starving!") == _hunger_text(2801));
static_assert(std::pair(c_ltred, "Near starving") == _hunger_text(2800));
static_assert(std::pair(c_ltred, "Near starving") == _hunger_text(1401));
static_assert(std::pair(c_ltred, "Famished") == _hunger_text(1400));
static_assert(std::pair(c_ltred, "Famished") == _hunger_text(301));
static_assert(std::pair(c_yellow, "Very hungry") == _hunger_text(300));
static_assert(std::pair(c_yellow, "Very hungry") == _hunger_text(101));
static_assert(std::pair(c_yellow, "Hungry") == _hunger_text(100));
static_assert(std::pair(c_yellow, "Hungry") == _hunger_text(41));
static_assert(!_hunger_text(40));
static_assert(!_hunger_text(0));
static_assert(std::pair(c_green, "Full") == _hunger_text(-1));

// threshold table.  Won't be compile-time constant after translation tables built out.
static constexpr const std::pair<std::pair<nc_color, const char*>, std::pair<int, bool> > _thirst_lookup[] = {
    {std::pair(c_ltred, "Parched"), std::pair(520, true)},
    {std::pair(c_ltred, "Dehydrated"), std::pair(240, true)},
    {std::pair(c_yellow, "Very thirsty"), std::pair(80, true)},
    {std::pair(c_yellow, "Thirsty"), std::pair(40, true)},
    {std::pair(c_green, "Slaked"), std::pair(0, false)},
};

static constexpr std::optional<std::pair<nc_color, const char*> > _thirst_text(int thirst)
{
    for (decltype(auto) x : _thirst_lookup) if (x.second.second ? (thirst > x.second.first) : (thirst < x.second.first)) return x.first;
    return std::nullopt;
}

static_assert(std::pair(c_ltred, "Parched") == _thirst_text(521));
static_assert(std::pair(c_ltred, "Dehydrated") == _thirst_text(520));
static_assert(std::pair(c_ltred, "Dehydrated") == _thirst_text(241));
static_assert(std::pair(c_yellow, "Very thirsty") == _thirst_text(240));
static_assert(std::pair(c_yellow, "Very thirsty") == _thirst_text(81));
static_assert(std::pair(c_yellow, "Thirsty") == _thirst_text(80));
static_assert(std::pair(c_yellow, "Thirsty") == _thirst_text(41));
static_assert(!_thirst_text(40));
static_assert(!_thirst_text(0));
static_assert(std::pair(c_green, "Slaked") == _thirst_text(-1));

// threshold table, hard-coded ... > x.second .  Won't be compile-time constant after translation tables built out.
static constexpr const std::pair<std::pair<nc_color, const char*>, int> _fatigue_lookup[] = {
    {std::pair(c_red, "Exhausted"), 575},
    {std::pair(c_ltred, "Dead tired"), 383},
    {std::pair(c_yellow, "Tired"), 191}
};

static constexpr std::optional<std::pair<nc_color, const char*> > _fatigue_text(int fatigue)
{
    for (decltype(auto) x : _fatigue_lookup) if (fatigue > x.second) return x.first;
    return std::nullopt;
}

static_assert(std::pair(c_red, "Exhausted") == _fatigue_text(576));
static_assert(std::pair(c_ltred, "Dead tired") == _fatigue_text(575));
static_assert(std::pair(c_ltred, "Dead tired") == _fatigue_text(384));
static_assert(std::pair(c_yellow, "Tired") == _fatigue_text(383));
static_assert(std::pair(c_yellow, "Tired") == _fatigue_text(192));
static_assert(!_fatigue_text(191));
static_assert(!_fatigue_text(0));

static constexpr nc_color _xp_color(int xp)
{
    if (100 <= xp) return c_white;
    if (  0 <  xp) return c_ltgray;
    return c_dkgray;
}

static_assert(c_white  == _xp_color(101));
static_assert(c_white  == _xp_color(100));
static_assert(c_ltgray == _xp_color( 99));
static_assert(c_ltgray == _xp_color(  1));
static_assert(c_dkgray == _xp_color(  0));
static_assert(c_dkgray == _xp_color( -1));

static constexpr auto _morale_emoticon(int morale_cur)
{
    if ( 100 <= morale_cur) return std::pair(c_green, ":D");
    if (  10 <= morale_cur) return std::pair(c_green, ":)");
    if ( -10 <  morale_cur) return std::pair(c_white, ":|");
    if (-100 <  morale_cur) return std::pair(c_red, ":(" );
    return std::pair(c_red, "D:");
}

static_assert(std::pair(c_green, ":D") == _morale_emoticon(101));
static_assert(std::pair(c_green, ":D") == _morale_emoticon(100));
static_assert(std::pair(c_green, ":)") == _morale_emoticon(99));
static_assert(std::pair(c_green, ":)") == _morale_emoticon(11));
static_assert(std::pair(c_green, ":)") == _morale_emoticon(10));
static_assert(std::pair(c_white, ":|") == _morale_emoticon(9));
static_assert(std::pair(c_white, ":|") == _morale_emoticon(-9));
static_assert(std::pair(c_red, ":(") == _morale_emoticon(-10));
static_assert(std::pair(c_red, ":(") == _morale_emoticon(-11));
static_assert(std::pair(c_red, ":(") == _morale_emoticon(-99));
static_assert(std::pair(c_red, "D:") == _morale_emoticon(-100));
static_assert(std::pair(c_red, "D:") == _morale_emoticon(-101));

static constexpr nc_color _veh_strain_color(float strain)
{
    if (0.0f >= strain) return c_ltblue;
    if (0.2f >= strain) return c_yellow;
    if (0.4f >= strain) return c_ltred;
    return c_red;
}

static_assert(c_ltblue == _veh_strain_color(-0.01f));
static_assert(c_ltblue == _veh_strain_color(0.0f));
static_assert(c_yellow == _veh_strain_color(0.01f));
static_assert(c_yellow == _veh_strain_color(0.2f));
static_assert(c_ltred == _veh_strain_color(0.201f));
static_assert(c_ltred == _veh_strain_color(0.4f));
static_assert(c_red == _veh_strain_color(0.401f));

static constexpr nc_color _RGW_color(int test, int ref)
{
    if (ref > test) return c_red;
    if (ref < test) return c_green;
    return c_white;
}

static_assert(c_green == _RGW_color(11, 10));
static_assert(c_white == _RGW_color(10, 10));
static_assert(c_red == _RGW_color(9, 10));

static constexpr nc_color _speed_color(int spd_cur)
{
    if (mobile::mp_turn > spd_cur) return c_red;
    if (mobile::mp_turn < spd_cur) return c_green;
    return c_white;
}

static_assert(c_green == _speed_color(mobile::mp_turn + 1));
static_assert(c_white == _speed_color(mobile::mp_turn));
static_assert(c_red   == _speed_color(mobile::mp_turn - 1));

// 2020-08-04: unclear whether this should be in game class or not.
// incoming window is game::w_status, canonical height 4 canonical width 55
void player::disp_status(WINDOW *w, game *g)
{
 mvwprintz(w, 1, 0, c_ltgray, "Weapon: %s", weapname().c_str());
 if (weapon.is_gun()) {
     if (const auto recoil_clr = _recoil_color(recoil + driving_recoil)) {
         mvwprintz(w, 1, 30, *recoil_clr, "Recoil");
     }
 }

 // C++ 20: extract offset changes from string lookup tables (likely using views, to be translation-friendly)
 if (const auto text = _hunger_text(hunger)) mvwaddstrz(w, 2, 0, *text);
 if (const auto text = _thirst_text(thirst)) mvwaddstrz(w, 2, 15, *text);
 if (const auto text = _fatigue_text(fatigue)) mvwaddstrz(w, 2, 30, *text);

 mvwaddstrz(w, 2, 41, c_white, "XP: ");
 mvwprintz(w, 2, 45, _xp_color(xp_pool), "%d", xp_pool);

 if (pain > pkill) {
     const int pain_delta = pain - pkill;
     const nc_color col_pain = (60 <= pain_delta) ? c_red : ((40 <= pain_delta) ? c_ltred : c_yellow);
     mvwprintz(w, 3, 0, col_pain, "Pain: %d", pain_delta);
 }

 const auto morale_text = _morale_emoticon(morale_level());
 mvwaddstrz(w, 3, 10, morale_text.first, morale_text.second);

 const auto v = GPSpos.veh_at();
 const vehicle* const veh = v ? v->first : nullptr; // backward compatibility

 if (in_vehicle && veh) {
  veh->print_fuel_indicator (w, 3, 49);
  nc_color col_indf1 = c_ltgray; // \todo use or hard-code

  nc_color col_vel = _veh_strain_color(veh->strain());

  bool has_turrets = false;
  for (decltype(auto) _part : veh->parts) {
      if (_part.has_flag(vpf_turret)) {
          has_turrets = true;
          break;
      }
  }

  if (has_turrets) {
   mvwaddstrz(w, 3, 25, col_indf1, "Gun:");
   mvwaddstrz(w, 3, 29, veh->turret_mode ? c_ltred : c_ltblue,
                       veh->turret_mode ? "auto" : "off ");
  }

  const bool use_metric_system = option_table::get()[OPT_USE_METRIC_SYS];
  if (veh->cruise_on) {
   if(use_metric_system) {
    mvwaddstrz(w, 3, 33, col_indf1, "{Km/h....>....}");
    mvwprintz(w, 3, 38, col_vel, "%4d", int(veh->velocity / vehicle::km_1));
    mvwprintz(w, 3, 43, c_ltgreen, "%4d", int(veh->cruise_velocity / vehicle::km_1));
   } else {
    mvwaddstrz(w, 3, 34, col_indf1, "{mph....>....}");
    mvwprintz(w, 3, 38, col_vel, "%4d", veh->velocity / vehicle::mph_1);
    mvwprintz(w, 3, 43, c_ltgreen, "%4d", veh->cruise_velocity / vehicle::mph_1);
   }
  } else {
   if(use_metric_system) {
    mvwaddstrz(w, 3, 33, col_indf1, "  {Km/h....}  ");
    mvwprintz(w, 3, 40, col_vel, "%4d", int(veh->velocity / vehicle::km_1));
   } else {
    mvwaddstrz(w, 3, 34, col_indf1, "  {mph....}  ");
    mvwprintz(w, 3, 40, col_vel, "%4d", veh->velocity / vehicle::mph_1);
   }
  }

  if (veh->velocity != 0) {
   nc_color col_indc = veh->skidding? c_red : c_green;
   int dfm = veh->face.dir() - veh->move.dir();
   mvwputch(w, 3, 21, col_indc, dfm < 0? 'L' : '.');
   wputch(w, col_indc, dfm == 0? '0' : '.');
   wputch(w, col_indc, dfm > 0? 'R' : '.');
  }
 } else {  // Not in vehicle
  nc_color col_str = _RGW_color(str_cur, str_max);
  nc_color col_dex = _RGW_color(dex_cur, dex_max);
  nc_color col_int = _RGW_color(int_cur, int_max);
  nc_color col_per = _RGW_color(per_cur, per_max);

  // C:Whales computed color w/o environmental effects, but did not display those effects either
  nc_color col_spd = _speed_color(theoretical_speed());
  int spd_cur = current_speed();

  mvwprintz(w, 3, 13, col_str, "Str %s%d", str_cur >= 10 ? "" : " ", str_cur);
  mvwprintz(w, 3, 20, col_dex, "Dex %s%d", dex_cur >= 10 ? "" : " ", dex_cur);
  mvwprintz(w, 3, 27, col_int, "Int %s%d", int_cur >= 10 ? "" : " ", int_cur);
  mvwprintz(w, 3, 34, col_per, "Per %s%d", per_cur >= 10 ? "" : " ", per_cur);
  mvwprintz(w, 3, 41, col_spd, "Spd %s%d", spd_cur >= 10 ? "" : " ", spd_cur);
 }
}

bool player::has_trait(int flag) const
{
 if (flag == PF_NULL) return true;
 return my_traits[flag];
}

bool player::has_mutation(int flag) const
{
 return (flag == PF_NULL) ? true : my_mutations[flag];
}

void player::toggle_trait(int flag)
{
 my_traits[flag] = !my_traits[flag];
 my_mutations[flag] = !my_mutations[flag];
}

const char* player::interpret_trait(const std::pair<pl_flag, const char*>* origin, ptrdiff_t ub) const
{
    while (0 < ub--) {
        if (has_trait(origin[ub].first)) return origin[ub].second;
    }
    return nullptr;
}


bool player::has_bionic(bionic_id b) const
{
 for (const auto& bionic : my_bionics) if (bionic.id == b) return true;
 return false;
}

bool player::has_active_bionic(bionic_id b) const
{
 for (const auto& bionic : my_bionics) if (bionic.id == b) return bionic.powered;
 return false;
}

void player::add_bionic(bionic_id b)
{
 for(const auto& bio : my_bionics) if (bio.id == b) return;	// No duplicates!

 char newinv = my_bionics.empty() ? 'a' : my_bionics.back().invlet+1;
 my_bionics.push_back(bionic(b, newinv));
}

void player::charge_power(int amount)
{
 power_level += amount;
 if (power_level > max_power_level)
  power_level = max_power_level;
 if (power_level < 0)
  power_level = 0;
}

unsigned int player::sight_range(int light_level) const
{
 // critical vision impairments.
 if (has_disease(DI_BLIND)) return 0;
 if (has_disease(DI_BOOMERED)) return 1;
 if (has_disease(DI_IN_PIT)) return 1;

 if (    underwater
     && !has_bionic(bio_membrane)
     && !has_trait(PF_MEMBRANE)
     && !is_wearing(itm_goggles_swim))
     return 1;

 int ret = light_level;
 if (12 > ret) { // night vision relevant
     // the nightvision traits are configured to be mutually exclusive.
     if (   ((is_wearing(itm_goggles_nv) && has_active_item(itm_UPS_on))
         ||   has_active_bionic(bio_night_vision))
         ||   has_trait(PF_NIGHTVISION3))
         ret = 12;
     // Theoretically can push sight range over 12 i.e. be superior to 3rd level nightvision.
     // Does do that because the highest light level other than sunlight, is full moon which is 9.  2021-01-25 zaimoni
     else if (has_trait(PF_NIGHTVISION2)) ret += 4;
     else if (has_trait(PF_NIGHTVISION))  ret += 1;
 }
 // check for near-sightedness
 // \todo? what about presbyopia and far-sightedness?
 // \todo? what about surgical correction for cataracts?  This can grant either near-sightedness or far-sightedness
 if (ret > 4 && has_trait(PF_MYOPIC) && !is_wearing(itm_glasses_eye) &&
     !is_wearing(itm_glasses_monocle))
  ret = 4;
 return ret;
}

/// includes aerial vibrations, not just ground vibrations -- flying creatures noticed as well
unsigned int player::seismic_range() const { return has_trait(PF_ANTENNAE) ? 3 : 0; }
unsigned int player::sight_range() const { return sight_range(game::active()->light_level(GPSpos)); }

unsigned int player::overmap_sight_range() const
{
 auto sight = sight_range();
 // low-light interferes with overmap sight range
 if (4*SEE >= sight) return (SEE > sight) ? 0 : SEE/2;
 // technology overrides
 if (has_amount(itm_binoculars, 1)) return 20;
 return 10;	// baseline
}

int player::clairvoyance() const
{
 if (has_artifact_with(AEP_CLAIRVOYANCE)) return 3;
 return 0;
}

bool player::see(const monster& mon) const
{
    int dist = rl_dist(GPSpos, mon.GPSpos);
    if (dist <= seismic_range()) return true;
    if (mon.has_flag(MF_DIGS) && !has_active_bionic(bio_ground_sonar) && dist > 1)
        return false;	// Can't see digging monsters until we're right next to them
    const auto range = sight_range();
    if (const auto clairvoyant = clairvoyance()) {
        if (dist <= clamped_lb(range, clairvoyant)) return true;
    }
    return (bool)game::active()->m.sees(pos, mon.pos, range);
}

std::optional<int> player::see(const player& u) const
{
    int dist = rl_dist(GPSpos, u.GPSpos);
    if (dist <= seismic_range()) return true;
    if (u.has_active_bionic(bio_cloak) || u.has_artifact_with(AEP_INVISIBLE)) return std::nullopt;
    const auto range = sight_range();
    if (const auto clairvoyant = clairvoyance()) {
        if (dist <= clamped_lb(range, clairvoyant)) return true;
    }
    return game::active()->m.sees(pos, u.pos, range);
}

std::optional<int> player::see(const GPS_loc& loc) const
{
    if (loc == GPSpos) return 0; // always aware of own location
    // \todo? primary implementation, rather than forward
    if (auto pt = game::active()->toScreen(loc)) return see(*pt);
    return std::nullopt;
}

std::optional<int> player::see(const point& pt) const
{
    const auto range = sight_range();
    if (const auto c_range = clairvoyance()) {
        if (rl_dist(pos, pt) <= clamped_ub(range, c_range)) return 0; // clairvoyant default
    }
    return game::active()->m.sees(pos, pt, range);
}

bool player::has_two_arms() const
{
 if (has_bionic(bio_blaster) || hp_cur[hp_arm_l] < 10 || hp_cur[hp_arm_r] < 10)
  return false;
 return true;
}

namespace {

// do not want to expose player forward declaration to mobile subclass
struct CanSee
{
    const player& viewpoint;
    int dist;

    CanSee(const player& v, int dist) noexcept : viewpoint(v),dist(dist) {}
    CanSee(const CanSee& src) = delete;
    CanSee(CanSee&& src) = delete;
    CanSee& operator=(const CanSee& src) = delete;
    CanSee& operator=(CanSee&& src) = delete;
    ~CanSee() = default;

    bool operator()(monster* m) {
        if (m->has_flag(MF_DIGS) && !viewpoint.has_active_bionic(bio_ground_sonar) && 1 < dist)
            return false; // Can't see digging monsters until we're right next to them
        return true;
    }
    bool operator()(player* p) {
        if (p->has_active_bionic(bio_cloak) || p->has_artifact_with(AEP_INVISIBLE)) return false;
        return true;
    }
};

}

std::optional<std::vector<GPS_loc> > player::see(const std::variant<monster*, npc*, pc*>& dest, int range) const
{
    auto loc = std::visit(to_ref<mobile>(), dest).GPSpos;
    int dist = rl_dist(loc, GPSpos);
    if (0 == dist) return std::vector<GPS_loc>();   // self-targeting...assume friendly-fire legal, for now
    if (dist > seismic_range()) {
        if (!std::visit(CanSee(*this, dist), dest)) return std::nullopt;
        const auto range = sight_range();
        if (const auto c_range = clairvoyance()) {
            if (dist > clamped_ub(range, c_range)) return std::nullopt;
        }
    }
    return GPSpos.sees(loc, -1);
}

bool player::avoid_trap(const trap* const tr) const
{
 int myroll = dice(3, dex_cur + sklevel[sk_dodge] * 1.5);
 int traproll;
 if (per_cur - encumb(bp_eyes) >= tr->visibility)
  traproll = dice(3, tr->avoidance);
 else
  traproll = dice(6, tr->avoidance);
 if (has_trait(PF_LIGHTSTEP)) myroll += dice(2, 6);
 return myroll >= traproll;
}

void player::pause()
{
 moves = 0;
 if (recoil > 0) {
  const int dampen_recoil = str_cur + 2 * sklevel[sk_gun];
  if (dampen_recoil >= recoil) recoil = 0;
  else {
      recoil -= dampen_recoil;
      recoil /= 2;
  }
 }

// Meditation boost for Toad Style
 if (weapon.type->id == itm_style_toad && activity.type == ACT_NULL) {
  int arm_amount = 1 + (int_cur - 6) / 3 + (per_cur - 6) / 3;
  int arm_max = (int_cur + per_cur) / 2;
  if (arm_amount > 3) arm_amount = 3;
  if (arm_max > 20) arm_max = 20;
  add_disease(DI_ARMOR_BOOST, 2, arm_amount, arm_max);
 }
}

unsigned int player::aiming_range(const item& aimed) const	// return value can be negative at this time
{
    auto ret = aimed.range();

    // We delegate preventing negative ranges to Socrates' Daimon.
    if (aimed.has_flag(IF_STR8_DRAW)) {
        if (4 > str_cur) return 0;
        else if (8 > str_cur) ret -= 2 * (8 - str_cur);
    } else if (aimed.has_flag(IF_STR10_DRAW)) {
        if (5 > str_cur) return 0;
        else if (10 > str_cur) ret -= 2 * (10 - str_cur);
    }

    return ret;
}

unsigned int player::throw_range(const item& thrown) const
{
    if (thrown.weight() > str_cur * 15) return 0;
    int ret = int((str_cur * 8) / (thrown.weight() > 0 ? thrown.weight() : 10));
    ret -= int(thrown.volume() / 10);
    if (ret < 1) return 1;
    // Cap at one and a half of our strength, plus skill
    int ub = str_cur;
    rational_scale<3, 2>(ub);
    ub += sklevel[sk_throw];
    if (ret > ub) return ub;
    return ret;
}

int player::ranged_dex_mod(bool real_life) const
{
 int dex = (real_life ? dex_cur : dex_max);
 if (dex == 8) return 0;
 const int dex_delta = 8 - dex;
 if (dex > 8) return (real_life ? (0 - rng(0, -dex_delta)) : dex_delta);

 int deviation = 0;
 if (dex < 6) deviation = 2 * dex_delta;
 else deviation = 3 * dex_delta / 2;

 return (real_life ? rng(0, deviation) : deviation);
}

int player::ranged_per_mod(bool real_life) const
{
 int per = (real_life ? per_cur : per_max);
 if (per == 8) return 0;
 if (16 < per) per = 16;
 const int per_delta = 8 - per;

 int deviation = 0;
 if (8 < per) {
  deviation = 3 * (0 - (per > 16 ? 8 : per - 8));
  if (real_life && one_in(per)) deviation = 0 - rng(0, abs(deviation));
 } else {
  if (per < 4) deviation = 5 * (8 - per);
  else if (per < 6) deviation = 2.5 * (8 - per);
  else /* if (per < 8) */ deviation = 2 * (8 - per);
  if (real_life) deviation = rng(0, deviation);
 }
 return deviation;
}

int player::throw_dex_mod(bool real_life) const
{
 int dex = (real_life ? dex_cur : dex_max);
 if (dex == 8 || dex == 9) return 0;
 if (dex >= 10) return (real_life ? 0 - rng(0, dex - 9) : 9 - dex);
 
 int deviation = 8 - dex;
 if (dex < 4) deviation *= 4;
 else if (dex < 6) deviation *= 3;
 else deviation *= 2;

 return (real_life ? rng(0, deviation) : deviation);
}

int player::comprehension_percent(skill s, bool real_life) const
{
 double intel = (double)(real_life ? int_cur : int_max);
 if (intel == 0.) intel = 1.;
 double percent = 80.; // double temporarily, since we divide a lot
 int learned = (real_life ? sklevel[s] : 4);
 if (learned > intel / 2) percent /= 1 + ((learned - intel / 2) / (intel / 3));
 else if (!real_life && intel > 8) percent += 125 - 1000 / intel;

 if (has_trait(PF_FASTLEARNER)) percent += 50.;

 if (const int thc = disease_level(DI_THC); 3*60 < thc) {
     // B-movie: no effect as long as painkill not overdosed (at normal weight).  cf iuse::weed
     percent /= 1.0 + (thc - 3*60) / 400.0;   // needs empirical tuning.  For now, just be annoying.
 }
 return (int)(percent);
}

int player::read_speed(bool real_life) const
{
 int intel = (real_life ? int_cur : int_max);
 int ret = 1000 - 50 * (intel - 8);
 if (has_trait(PF_FASTREADER)) rational_scale<4,5>(ret);
 if (const int thc = disease_level(DI_THC)) {  // don't overwhelm fast reader...at least, if we haven't had enough to max out pain kill
     // cf iuse::weed.  Neutral point for canceling fast reader handwaved as 4 full doses, at least at normal weight
     ret += (ret / 16) * (1 + (thc - 1) / 60);
 }
 if (ret < 100) ret = 100;
 return (real_life ? ret : ret / 10);
}

int player::talk_skill() const
{
 int ret = int_cur + per_cur + sklevel[sk_speech] * 3;
 if (has_trait(PF_DEFORMED)) ret -= 4;
 else if (has_trait(PF_DEFORMED2)) ret -= 6;

 return ret;
}

int player::intimidation() const
{
 int ret = str_cur * 2;
 if (weapon.is_gun()) ret += 10;
 if (weapon.damage_bash() >= 12 || weapon.damage_cut() >= 12) ret += 5;
 if (has_trait(PF_DEFORMED2)) ret += 3;
 if (stim > 20) ret += 2;
 if (has_disease(DI_DRUNK)) ret -= 4;

 return ret;
}

double player::barter_price_adjustment() const
{
    switch(const int sk = sklevel[sk_barter]) {
    case 0:  return 1.5;
    case 1:  return 1.4;
    case 2:  return 1.2;
    case 3:  return 1.0;
    case 4:  return 0.8;
    case 5:  return 0.6;
    case 6:  return 0.5;
    default: return int(100 * (.3 + 1.0 / sk)) / 100.0;
    }
}

void player::hit(game *g, body_part bphurt, int side, int dam, int cut)
{
    if (!rude_awakening() && has_disease(DI_LYING_DOWN))
        rem_disease(DI_LYING_DOWN);

 absorb(bphurt, dam, cut);

 dam += cut;
 if (dam <= 0) return;

 rem_disease(DI_SPEED_BOOST);
 if (dam >= 6) rem_disease(DI_ARMOR_BOOST);

 cancel_activity_query("You were hurt!");

 if (has_artifact_with(AEP_SNAKES) && dam >= 6) {
  int snakes = dam / 6;
  std::vector<GPS_loc> valid;
  for (decltype(auto) delta : Direction::vector) {
      const auto loc(GPSpos + delta);
      if (loc.is_empty()) valid.push_back(loc);
  }
  clamp_ub(snakes, valid.size());
  if (0 < snakes) {
      // \todo adapt message for NPCs
      if (!is_npc()) messages.add(1 == snakes ? "A snake sprouts from your body!" : "Some snakes sprout from your body!");
      monster snake(mtype::types[mon_shadow_snake]);
      snake.friendly = -1;
      for (int i = 0; i < snakes; i++) {
          int index = rng(0, valid.size() - 1);
          snake.spawn(valid[index]);
          valid.erase(valid.begin() + index);
          g->spawn(snake);
      }
  }
 }
  
 int painadd = 0;
 if (has_trait(PF_PAINRESIST))
  painadd = (sqrt(double(cut)) + dam + cut) / (rng(4, 6));
 else
  painadd = (sqrt(double(cut)) + dam + cut) / 4;
 pain += painadd;

 switch (bphurt) {
 case bp_eyes:
  pain++;
  if (dam > 5 || cut > 0) {
   const int minblind = clamped_lb<1>((dam + cut) / 10);
   const int maxblind = clamped_ub<5>((dam + cut) / 4);
   add_disease(DI_BLIND, rng(minblind, maxblind));
  }
  [[fallthrough]];

 case bp_mouth: // Fall through to head damage
 case bp_head: 
  pain++;
  if (0 > (hp_cur[hp_head] -= dam)) hp_cur[hp_head] = 0;
 break;
 case bp_torso:
  recoil += dam / 5;
  if (0 > (hp_cur[hp_torso] -= dam)) hp_cur[hp_torso] = 0;
 break;
 case bp_hands: // Fall through to arms
 case bp_arms:
  if (side == 1 || side == 3 || weapon.is_two_handed(*this)) recoil += dam / 3;
  if (side == 0 || side == 3) {
   if (0 > (hp_cur[hp_arm_l] -= dam)) hp_cur[hp_arm_l] = 0;
  }
  if (side == 1 || side == 3) {
   if (0 > (hp_cur[hp_arm_r] -= dam)) hp_cur[hp_arm_r] = 0;
  }
 break;
 case bp_feet: // Fall through to legs
 case bp_legs:
  if (side == 0 || side == 3) {
   if (0 > (hp_cur[hp_leg_l] -= dam)) hp_cur[hp_leg_l] = 0;
  }
  if (side == 1 || side == 3) {
   if (0 > (hp_cur[hp_leg_r] -= dam)) hp_cur[hp_leg_r] = 0;
  }
 break;
 default:
  debugmsg("Wacky body part hit!");
 }
 if (has_trait(PF_ADRENALINE) && !has_disease(DI_ADRENALINE) &&
     (hp_cur[hp_head] < 25 || hp_cur[hp_torso] < 15))
  add_disease(DI_ADRENALINE, MINUTES(20));
}

void player::hurt(body_part bphurt, int side, int dam)
{
    if (2 < rng(0, dam) && rude_awakening()) {}
    else rem_disease(DI_LYING_DOWN);

 if (dam <= 0) return;

 cancel_activity_query("You were hurt!");

 int painadd = dam / (has_trait(PF_PAINRESIST) ? 3 : 2);
 pain += painadd;

 switch (bphurt) {
 case bp_eyes:	// Fall through to head damage
 case bp_mouth:	// Fall through to head damage
 case bp_head:
  pain++;
  if (0 > (hp_cur[hp_head] -= dam)) hp_cur[hp_head] = 0;
 break;
 case bp_torso:
  if (0 > (hp_cur[hp_torso] -= dam)) hp_cur[hp_torso] = 0;
 break;
 case bp_hands:	// Fall through to arms
 case bp_arms:
  if (side == 0 || side == 3) {
   if (0 > (hp_cur[hp_arm_l] -= dam)) hp_cur[hp_arm_l] = 0;
  }
  if (side == 1 || side == 3) {
   if (0 > (hp_cur[hp_arm_r] -= dam)) hp_cur[hp_arm_r] = 0;
  }
 break;
 case bp_feet:	// Fall through to legs
 case bp_legs:
  if (side == 0 || side == 3) {
   if (0 > (hp_cur[hp_leg_l] -= dam)) hp_cur[hp_leg_l] = 0;
  }
  if (side == 1 || side == 3) {
   if (0 > (hp_cur[hp_leg_r] -= dam)) hp_cur[hp_leg_r] = 0;
  }
 break;
 default:
  debugmsg("Wacky body part hurt!");
 }
 if (has_trait(PF_ADRENALINE) && !has_disease(DI_ADRENALINE) && (hp_cur[hp_head] < 25 || hp_cur[hp_torso] < 15))
  add_disease(DI_ADRENALINE, MINUTES(20));
}

bool player::hurt(int dam) {
    hit(game::active(), bp_torso, 0, dam, 0); // as required by mobile::knock_back_from
    return 0 >= hp_cur[hp_torso] || 0 >= hp_cur[hp_head];
}

void player::heal(body_part healed, int side, int dam)
{
 hp_part healpart;
 switch (healed) {
 case bp_eyes:	// Fall through to head damage
 case bp_mouth:	// Fall through to head damage
 case bp_head:
  healpart = hp_head;
 break;
 case bp_torso:
  healpart = hp_torso;
 break;
 case bp_hands:
// Shouldn't happen, but fall through to arms
  debugmsg("Heal against hands!");
  [[fallthrough]];
 case bp_arms:
  if (side == 0)
   healpart = hp_arm_l;
  else
   healpart = hp_arm_r;
 break;
 case bp_feet:
// Shouldn't happen, but fall through to legs
  debugmsg("Heal against feet!");
  [[fallthrough]];
 case bp_legs:
  if (side == 0)
   healpart = hp_leg_l;
  else
   healpart = hp_leg_r;
 break;
 default:
  debugmsg("Wacky body part healed!");
  healpart = hp_torso;
 }
 heal(healpart, dam);
}

void player::heal(hp_part healed, int dam)
{
 hp_cur[healed] += dam;
 clamp_ub(hp_cur[healed], hp_max[healed]);
}

void player::healall(int dam)
{
 for (int i = 0; i < num_hp_parts; i++) {
  if (hp_cur[i] > 0) {
   hp_cur[i] += dam;
   clamp_ub(hp_cur[i], hp_max[i]);
  }
 }
}

void player::hurtall(int dam)
{
 for (int i = 0; i < num_hp_parts; i++) {
  int painadd = 0;
  hp_cur[i] -= dam;
  clamp_lb<0>(hp_cur[i]);

  if (has_trait(PF_PAINRESIST))
   painadd = dam / 3;
  else
   painadd = dam / 2;
  pain += painadd;
 }
}

bool player::hitall(int dam, int vary)
{
    if (!rude_awakening()) rem_disease(DI_LYING_DOWN);

 for (int i = 0; i < num_hp_parts; i++) {
  int ddam = vary? dam * rng (100 - vary, 100) / 100 : dam;
  int cut = 0;
  absorb((body_part) i, ddam, cut);
  //dam += cut;
  if (dam <= 0) continue;
  int painadd = 0;
  hp_cur[i] -= ddam;
  clamp_lb<0>(hp_cur[i]);
  if (has_trait(PF_PAINRESIST))
   painadd = dam / 3 / 4;
  else
   painadd = dam / 2 / 4;
  pain += painadd;
 }

    return 0 >= hp_cur[hp_torso] || 0 >= hp_cur[hp_head];
}

bool player::handle_knockback_into_impassable(const GPS_loc& dest)
{
    if (is<swimmable>(dest.ter())) {
        swim(dest);
        return true;
    }
    return false;
}

int player::hp_percentage() const
{
 int total_cur = 0, total_max = 0;
// Head and torso HP are weighted 3x and 2x, respectively
 total_cur = hp_cur[hp_head] * 3 + hp_cur[hp_torso] * 2;
 total_max = hp_max[hp_head] * 3 + hp_max[hp_torso] * 2;
 for (int i = hp_arm_l; i < num_hp_parts; i++) {
  total_cur += hp_cur[i];
  total_max += hp_max[i];
 }
 return (100 * total_cur) / total_max;
}

void player::get_sick()
{
 if (health > 0 && rng(0, health + 10) < health) health--;
 if (health < 0 && rng(0, 10 - health) < (0 - health)) health++;
 if (one_in(12)) health -= 1;

 if (game::debugmon) debugmsg("Health: %d", health);

 if (has_trait(PF_DISIMMUNE)) return;

 if (!has_disease(DI_FLU) && !has_disease(DI_COMMON_COLD) &&
     one_in(900 + 10 * health + (has_trait(PF_DISRESISTANT) ? 300 : 0))) {
  if (one_in(6))
   infect(DI_FLU, bp_mouth, 3, rng(40000, 80000));
  else
   infect(DI_COMMON_COLD, bp_mouth, 3, rng(20000, 60000));
 }
}

void player::infect(dis_type type, body_part vector, int strength, int duration)
{
 if (dice(strength, 3) > dice(resist(vector), 3)) add_disease(type, duration);
}

// 2nd person singular.  Would need 3rd person for npcs
const char* describe(dis_type type)
{
	switch (type) {
	case DI_GLARE: return "The sunlight's glare makes it hard to see.";
	case DI_WET: return "You're getting soaked!";
	case DI_HEATSTROKE: return "You have heatstroke!";
	case DI_FBFACE: return "Your face is frostbitten.";
	case DI_FBHANDS: return "Your hands are frostbitten.";
	case DI_FBFEET: return "Your feet are frostbitten.";
	case DI_COMMON_COLD: return "You feel a cold coming on...";
	case DI_FLU: return "You feel a flu coming on...";
	case DI_ONFIRE: return "You're on fire!";
	case DI_SMOKE: return "You inhale a lungful of thick smoke.";
	case DI_TEARGAS: return "You inhale a lungful of tear gas.";
	case DI_BOOMERED: return "You're covered in bile!";
	case DI_SAP: return "You're coated in sap!";
	case DI_SPORES: return "You're covered in tiny spores!";
	case DI_SLIMED: return "You're covered in thick goo!";
	case DI_LYING_DOWN: return "You lie down to go to sleep...";
	case DI_FORMICATION: return "There's bugs crawling under your skin!";
	case DI_WEBBED: return "You're covered in webs!";
	case DI_DRUNK:
	case DI_HIGH: return "You feel lightheaded.";
    case DI_THC: return "You feel lightheaded.";    // \todo once this is built out, be more accurate
    case DI_ADRENALINE: return "You feel a surge of adrenaline!";
	case DI_ASTHMA: return "You can't breathe... asthma attack!";
	case DI_DEAF: return "You're deafened!";
	case DI_BLIND: return "You're blinded!";
	case DI_STUNNED: return "You're stunned!";
	case DI_DOWNED: return "You're knocked to the floor!";
	case DI_AMIGARA: return "You can't look away from the fautline...";
	default: return nullptr;
	}
}

static constexpr dis_type translate(mobile::effect src)
{
    switch (src)
    {
    case mobile::effect::DOWNED: return DI_DOWNED;
    case mobile::effect::STUNNED: return DI_STUNNED;
    case mobile::effect::DEAF: return DI_DEAF;
    case mobile::effect::BLIND: return DI_BLIND;
    case mobile::effect::POISONED: return DI_POISON;
    case mobile::effect::ONFIRE: return DI_ONFIRE;
    default: return DI_NULL;    // \todo should be hard error
    }
}

void player::add(effect src, int duration) { return add_disease(translate(src), duration); }
bool player::has(effect src) const { return has_disease(translate(src)); }

void player::add_disease(dis_type type, int duration, int intensity, int max_intensity)
{
 if (duration == 0) return;
 for(decltype(auto) ill : illness) {
  if (type != ill.type) continue; // invariant: at most one "disease" of a given type
  ill.duration += duration;
  ill.intensity += intensity;
  if (max_intensity != -1 && ill.intensity > max_intensity) ill.intensity = max_intensity;
  return;
 }
 if (!is_npc()) {
	if (DI_ADRENALINE == type) moves += 8 * mobile::mp_turn;	// \todo V 0.2.3+: handle NPC adrenaline
	messages.add(describe(type));
 }
 illness.emplace_back(type, duration, intensity);
}

bool player::rem_disease(dis_type type)
{
    bool ret = false;
    int ub = illness.size();
    while (0 <= --ub) {
        if (type == illness[ub].type) {
            EraseAt(illness, ub);
            ret = true;
        }
    }
    return ret;
}

bool player::rem_disease(std::function<bool(disease&)> op)
{
    bool ret = false;
    int ub = illness.size();
    // do a complete scan, as that repairs the data integrity error
    // of duplicate "disease" entries of the same type
    while (0 <= --ub) {
        if (op(illness[ub])) {
            EraseAt(illness, ub);
            ret = true;
        }
    }
    return ret;
}

bool player::do_foreach(std::function<bool(disease&)> op) {
    for (decltype(auto) ill : illness) if (op(ill)) return true;
    return false;
}

bool player::do_foreach(std::function<bool(const disease&)> op) const {
    for (decltype(auto) ill : illness) if (op(ill)) return true;
    return false;
}

bool player::has_disease(dis_type type) const
{
 for (decltype(auto) ill : illness) if (ill.type == type) return true;
 return false;
}

int player::disease_level(dis_type type) const
{
 for (decltype(auto) ill : illness) if (ill.type == type) return ill.duration;
 return 0;
}

int player::disease_intensity(dis_type type) const
{
 for (decltype(auto) ill : illness) if (ill.type == type) return ill.intensity;
 return 0;
}

bool player::rude_awakening()
{
    static auto i_awake = []() { return std::string("You wake up!"); };
    static auto other_awake = [&]() { return subject() + " wakes up!"; };

    if (rem_disease(DI_SLEEP)) {
        if_visible_message(i_awake, other_awake);
        return true;
    }
    return false;
}

void player::add_addiction(add_type type, int strength)
{
 if (type == ADD_NULL) return;
 int timer = HOURS(2);  // \todo make this substance-dependent?
 if (has_trait(PF_ADDICTIVE)) {
  rational_scale<3,2>(strength);
  rational_scale<2,3>(timer);
 }
 for(auto& a : addictions) {
  if (type != a.type) continue;
  if (a.sated <   0) a.sated = timer;
  else if (a.sated < HOURS(1)) a.sated += timer;	// TODO: Make this variable?
  else a.sated += int((HOURS(5) - a.sated) / 2);	// definitely could be longer than above
  if (20 > a.intensity && (rng(0, strength) > rng(0, a.intensity * 5) || rng(0, 500) < strength))
   a.intensity++;
  return;
 }
 if (rng(0, 100) < strength) addictions.push_back(addiction(type, 1));
}

bool player::has_addiction(add_type type) const
{
 for(const auto& a : addictions) if (a.type == type && a.intensity >= MIN_ADDICTION_LEVEL) return true;
 return false;
}

#if DEAD_FUNC
void player::rem_addiction(add_type type)
{
 for (int i = 0; i < addictions.size(); i++) {
  if (addictions[i].type == type) {
   EraseAt(addictions, i);
   return;
  }
 }
}
#endif

int player::addiction_level(add_type type) const
{
 for(const auto& a : addictions) if (a.type == type) return a.intensity;
 return 0;
}

void player::die()
{
    item my_body(messages.turn);
    my_body.name = name;
    // \todo all of these drops are thin-wrapped; would prefer they work outside of reality bubble
    GPSpos.add(std::move(my_body));
    // we want all of these to be value-copy map::add_item so that the post-mortem for the player works
    for (size_t i = 0; i < inv.size(); i++) GPSpos.add(inv[i]);
    for (const auto& it : worn) GPSpos.add(it);
    if (weapon.type->id != itm_null) GPSpos.add(weapon);
}

void player::suffer(game *g)
{
 for (int i = 0; i < my_bionics.size(); i++) {
  if (my_bionics[i].powered) activate_bionic(i);
 }
 if (underwater) { // player::is_drowning must agree with this
  if (!has_trait(PF_GILLS)) oxygen--;
  if (oxygen < 0) {
   if (has_bionic(bio_gills) && power_level > 0) {
    oxygen += 5;
    power_level--;
   } else {
    messages.add("You're drowning!");
    hurt(bp_torso, 0, rng(1, 4));
   }
  }
 }
 // time out illnesses and other temporary conditions
 int _i = illness.size();   // non-standard countdown timer
 while (0 <= --_i) {
     decltype(auto) ill = illness[_i];
     if (MIN_DISEASE_AGE > --ill.duration) ill.duration = MIN_DISEASE_AGE; // Cap permanent disease age
     else if (0 == ill.duration) EraseAt(illness, _i);
 }
 if (!has_disease(DI_SLEEP)) {
  const int timer = has_trait(PF_ADDICTIVE) ? -HOURS(6)-MINUTES(40) : -HOURS(6);
  // \todo work out why addiction processing only happens when awake
  _i = addictions.size();
  while (0 <= --_i) {
      decltype(auto) addict = addictions[_i];
      if (0 >= addict.sated && MIN_ADDICTION_LEVEL <= addict.intensity) addict_effect(*this, addict);
      if (0 < --addict.sated && !one_in(addict.intensity - 2)) addict.sated--;
      if (addict.sated < timer - (MINUTES(10) * addict.intensity)) {
          if (addict.intensity < MIN_ADDICTION_LEVEL) {
              EraseAt(addictions, _i);
              continue;
          }
          addict.intensity /= 2;
          addict.intensity--;
          addict.sated = 0;
      }
  }
  if (has_trait(PF_CHEMIMBALANCE)) {
   if (one_in(HOURS(6))) {
    messages.add("You suddenly feel sharp pain for no reason.");
    pain += 3 * rng(1, 3);
   }
   if (one_in(HOURS(6))) {
       if (int delta = 5 * rng(-1, 2)) {
           messages.add(0 < delta ? "You suddenly feel numb." : "You suddenly ache.");
           pkill += delta;
       }
   }
   if (one_in(HOURS(6))) {
    messages.add("You feel dizzy for a moment.");
    moves -= rng(10, 30);
   }
   if (one_in(HOURS(6))) {
       if (int delta = 5 * rng(-1, 3)) {
           messages.add(0 < delta ? "You suddenly feel hungry." : "You suddenly feel a little full.");
           hunger += delta;
       }
   }
   if (one_in(HOURS(6))) {
    messages.add("You suddenly feel thirsty.");
    thirst += 5 * rng(1, 3);
   }
   if (one_in(HOURS(6))) {
    messages.add("You feel fatigued all of a sudden.");
    fatigue += 10 * rng(2, 4);
   }
   if (one_in(HOURS(8))) {
    if (one_in(3)) add_morale(MORALE_FEELING_GOOD, 20, 100);
    else add_morale(MORALE_FEELING_BAD, -20, -100);
   }
  }
  if ((has_trait(PF_SCHIZOPHRENIC) || has_artifact_with(AEP_SCHIZO)) && one_in(HOURS(4))) {
   int i;
   switch(rng(0, 11)) {
    case 0:
     add_disease(DI_HALLU, HOURS(6));
     break;
    case 1:
     add_disease(DI_VISUALS, TURNS(rng(15, 60)));
     break;
    case 2:
     messages.add("From the south you hear glass breaking.");
     break;
    case 3:
     messages.add("YOU SHOULD QUIT THE GAME IMMEDIATELY.");
     add_morale(MORALE_FEELING_BAD, -50, -150);
     break;
    case 4:
     for (i = 0; i < 10; i++) {
      messages.add("XXXXXXXXXXXXXXXXXXXXXXXXXXX");
     }
     break;
    case 5:
     messages.add("You suddenly feel so numb...");
     pkill += 25;
     break;
    case 6:
     messages.add("You start to shake uncontrollably.");
     add_disease(DI_SHAKES, MINUTES(1) * rng(2, 5));
     break;
    case 7:
     for (i = 0; i < 10; i++) see_phantasm();
     break;
    case 8:
     messages.add("It's a good time to lie down and sleep.");
     add_disease(DI_LYING_DOWN, MINUTES(20));
     break;
    case 9:
     messages.add("You have the sudden urge to SCREAM!");
     g->sound(pos, 10 + 2 * str_cur, "AHHHHHHH!");
     break;
    case 10:
     messages.add(std::string(name + name + name + name + name + name + name +
                            name + name + name + name + name + name + name +
                            name + name + name + name + name + name).c_str());
     break;
    case 11:
     add_disease(DI_FORMICATION, HOURS(1));
     break;
   }
  }

  if (has_trait(PF_JITTERY) && !has_disease(DI_SHAKES)) {
   if (stim > 50 && one_in(300 - stim)) add_disease(DI_SHAKES, MINUTES(30) + stim);
   else if (hunger > 80 && one_in(500 - hunger)) add_disease(DI_SHAKES, MINUTES(40));
  }

  if (has_trait(PF_MOODSWINGS) && one_in(HOURS(6))) {
   if (rng(1, 20) > 9)	// 55% chance
    add_morale(MORALE_MOODSWING, -100, -500);
   else			// 45% chance
    add_morale(MORALE_MOODSWING, 100, 500);
  }

  if (has_trait(PF_VOMITOUS) && one_in(HOURS(7))) vomit();

  if (has_trait(PF_SHOUT1) && one_in(HOURS(6))) g->sound(pos, 10 + 2 * str_cur, "You shout loudly!");
  if (has_trait(PF_SHOUT2) && one_in(HOURS(4))) g->sound(pos, 15 + 3 * str_cur, "You scream loudly!");
  if (has_trait(PF_SHOUT3) && one_in(HOURS(3))) g->sound(pos, 20 + 4 * str_cur, "You let out a piercing howl!");
 }	// Done with while-awake-only effects

 if (has_trait(PF_ASTHMA) && one_in(3600 - stim * 50)) {
  bool auto_use = has_charges(itm_inhaler, 1);
  if (underwater) {
   oxygen /= 2;
   auto_use = false;
  }
  if (rem_disease(DI_SLEEP)) {
      messages.add("Your asthma wakes you up!");
      auto_use = false;
  }
  if (auto_use)
   use_charges(itm_inhaler, 1);
  else {
   add_disease(DI_ASTHMA, MINUTES(5) * rng(1, 4));
   cancel_activity_query("You have an asthma attack!");
  }
 }

 if (pain > 0) {
  if (has_trait(PF_PAINREC1) && one_in(HOURS(1))) pain--;
  if (has_trait(PF_PAINREC2) && one_in(MINUTES(30))) pain--;
  if (has_trait(PF_PAINREC3) && one_in(MINUTES(15))) pain--;
 }

 if (g->is_in_sunlight(GPSpos)) {
  if (has_trait(PF_LEAVES) && one_in(HOURS(1))) hunger--;

  if (has_trait(PF_ALBINO) && one_in(MINUTES(2))) {
   subjective_message("The sunlight burns your skin!");
   rude_awakening();
   hurtall(1);
  }

  const auto trog_stats = _troglodyte_sunburn(*this);
  str_cur += trog_stats.Str;
  dex_cur += trog_stats.Dex;
  int_cur += trog_stats.Int;
  per_cur += trog_stats.Per;
 }

 if (has_trait(PF_SORES)) {
  for (int i = bp_head; i < num_bp; i++) {
   const int nonstrict_lb = 5 + 4 * abs(encumb(body_part(i)));
   if (pain < nonstrict_lb) pain = nonstrict_lb;
  }
 }

 if (has_trait(PF_SLIMY)) {
  auto& fd = GPSpos.field_at();
  if (fd.type == fd_null) GPSpos.add(field(fd_slime, 1));
  else if (fd.type == fd_slime && fd.density < 3) fd.density++;
 }

 if (has_trait(PF_WEB_WEAVER) && one_in(3)) {
  auto& fd = GPSpos.field_at();
  if (fd.type == fd_null) GPSpos.add(field(fd_web, 1));
  else if (fd.type == fd_web && fd.density < 3) fd.density++;
 }

 if (has_trait(PF_RADIOGENIC) && int(messages.turn) % MINUTES(5) == 0 && radiation >= 10) {
  radiation -= 10;
  healall(1);
 }

 if (has_trait(PF_RADIOACTIVE1)) {
  auto& rad = g->m.radiation(GPSpos);
  if (rad < 10 && one_in(MINUTES(5))) rad++;
 }
 if (has_trait(PF_RADIOACTIVE2)) {
  auto& rad = g->m.radiation(GPSpos);
  if (rad < 20 && one_in(MINUTES(5)/2)) rad++;
 }
 if (has_trait(PF_RADIOACTIVE3)) {
  auto& rad = g->m.radiation(GPSpos);
  if (rad < 30 && one_in(MINUTES(1))) rad++;
 }

 if (has_trait(PF_UNSTABLE) && one_in(DAYS(2))) mutate();
 if (has_artifact_with(AEP_MUTAGENIC) && one_in(DAYS(2))) mutate();
 if (has_artifact_with(AEP_FORCE_TELEPORT) && one_in(HOURS(1))) g->teleport(this);

 const auto rad = g->m.radiation(GPSpos);
 // \todo? this is exceptionally 1950's
 const int rad_resist = is_wearing(itm_hazmat_suit) ? 20 : 8;
 if (rad_resist <= rad) {
     if (radiation < (100 * rad) / rad_resist) radiation += rng(0, rad / rad_resist);
 }

 if (rng(1, 2500) < radiation && (int(messages.turn) % MINUTES(15) == 0 || radiation > 2000)){
  mutate();
  if (radiation > 2000) radiation = 2000;
  radiation /= 2;
  radiation -= 5;
  if (radiation < 0) radiation = 0;
 }

// Negative bionics effects
 if (has_bionic(bio_dis_shock) && one_in(HOURS(2))) {
  messages.add("You suffer a painful electrical discharge!");
  pain++;
  moves -= 150;
 }
 if (has_bionic(bio_dis_acid) && one_in(HOURS(2)+MINUTES(30))) {
  messages.add("You suffer a burning acidic discharge!");
  hurtall(1);
 }
 if (has_bionic(bio_drain) && power_level > 0 && one_in(HOURS(1))) {
  messages.add("Your batteries discharge slightly.");
  power_level--;
 }
 if (has_bionic(bio_noise) && one_in(MINUTES(50))) {
  messages.add("A bionic emits a crackle of noise!");
  g->sound(pos, 60, "");
 }
 if (has_bionic(bio_power_weakness) && max_power_level > 0 && power_level >= max_power_level * .75)
  str_cur -= 3;

// Artifact effects
 if (has_artifact_with(AEP_ATTENTION)) add_disease(DI_ATTENTION, TURNS(3));

 for (decltype(auto) ill : illness) dis_effect(g, *this, ill);  // 2020-06-16: applying disease effects had gotten dropped at some point

 if (dex_cur < 0) dex_cur = 0;
 if (str_cur < 0) str_cur = 0;
 if (per_cur < 0) per_cur = 0;
 if (int_cur < 0) int_cur = 0;
}

void player::vomit()
{
    static auto me = []() { return std::string("You throw up heavily!"); };
    static auto other = [&]() {
        return name + " throws up heavily!";
    };

    if_visible_message(me, other); // \todo? sound for when not in sight at all
    hunger += rng(30, 50);
    thirst += rng(30, 50);
    moves -= mobile::mp_turn;

    static auto purge = [](disease& ill) {
        switch (ill.type) {
        case DI_PKILL1:
        case DI_PKILL2:
        case DI_PKILL3:
        case DI_SLEEP:
            return true;
        case DI_FOODPOISON:
            return 0 > (ill.duration -= MINUTES(30));
        case DI_DRUNK:
            return 0 > (ill.duration -= rng(1, 5) * MINUTES(10));
        default: return false;
        }
    };

    rem_disease(purge);
}

void player::fling(int dir, int flvel)
{
    const auto g = game::active();
    int steps = 0;
    bool is_u = this == &g->u;

    tileray tdir(dir);
    std::string sname = grammar::capitalize(subject())+" "+to_be();

    int range = flvel / 10;
    decltype(auto) loc = GPSpos;
    while (range > 0) {
        tdir.advance();
        loc = GPSpos + point(tdir.dx(), tdir.dy());
        if (!flung(flvel, loc)) break;
        set_screenpos(loc);
        range--;
        steps++;
        timespec ts = { 0, 50000000 };   // Timespec for the animation
        nanosleep(&ts, nullptr);
    }

    if (!is<swimmable>(loc.ter())) {
        // fall on ground
        int dam1 = rng(flvel / 3, flvel * 2 / 3) / 2;
        {
            dam1 = dam1 * 8 / clamped_lb<4>(dex_cur);
            if (has_trait(PF_PARKOUR)) dam1 /= 2;
        }

        if (is_u && 0 < dam1) {
            messages.add("You fall on the ground for %d damage.", dam1);
        } else {
            static auto prone = [&]() {
                return SVO_sentence(*this, "fall", "on the ground");
            };
            g->if_visible_message(prone, *this);
        }
        if (dam1 > 0) hitall(dam1, 40);
    } else {
        static auto swimming = [&]() {
            return SVO_sentence(*this, "fall", "into water");
        };
        g->if_visible_message(swimming, *this);
        // inline player::handle_knockback_into_impassable
        swim(GPSpos);
    }
}

int player::weight_carried() const
{
 int ret = 0;
 ret += weapon.weight();
 for (const auto& it : worn) ret += it.weight();
 for (size_t i = 0; i < inv.size(); i++) {
  for (const auto& it : inv.stack_at(i)) ret += it.weight();
 }
 return ret;
}

int player::volume_carried() const
{
 int ret = 0;
 for (size_t i = 0; i < inv.size(); i++) {
  for (const auto& it : inv.stack_at(i)) ret += it.volume();
 }
 return ret;
}

// internal unit is 4 oz (divide by 4 to get pounds)
int player::weight_capacity(bool real_life) const
{
 int str = (real_life ? str_cur : str_max);
 int ret = 400 + str * 35;
 if (has_trait(PF_BADBACK)) rational_scale<13,20>(ret);
 if (auto weak = has_light_bones()) ret *= mutation_branch::light_bones_carrying[weak - 1];
 if (has_artifact_with(AEP_CARRY_MORE)) ret += 200;
 return ret;
}

int player::volume_capacity() const
{
 int ret = 2;	// A small bonus (the overflow)
 for (const auto& it : worn) {
  ret += dynamic_cast<const it_armor*>(it.type)->storage;
 }
 if (has_bionic(bio_storage)) ret += 6;
 if (has_trait(PF_SHELL)) ret += 16;
 if (has_trait(PF_PACKMULE)) ret = int(ret * 1.4);
 return ret;
}

int player::morale_level() const
{
 int ret = 0;
 for (const auto& tmp : morale) ret += tmp.bonus;

 if (has_trait(PF_HOARDER)) {
  int pen = clamped_ub<70>((volume_capacity() - volume_carried()) / 2);
  if (has_disease(DI_TOOK_XANAX)) pen /= 7;
  else if (has_disease(DI_TOOK_PROZAC)) pen /= 2;
  ret -= pen;
 }

 if (has_trait(PF_MASOCHIST)) {
  int bonus = clamped_ub<25>(rational_scaled<5,2>(pain));
  if (has_disease(DI_TOOK_PROZAC)) bonus /= 3;
  ret += bonus;
 }

 if (has_trait(PF_OPTIMISTIC)) {
  if (ret < 0) {	// Up to -30 is canceled out
   clamp_ub<0>(ret += 30);
  } else		// Otherwise, we're just extra-happy
   ret += 20;
 }

 if (has_disease(DI_TOOK_PROZAC) && ret < 0) ret /= 4;

 return ret;
}

void player::add_morale(morale_type type, int bonus, int max_bonus, const itype* item_type)
{
 for (auto& tmp : morale) {
  if (tmp.type == type && tmp.item_type == item_type) {
   if (abs(tmp.bonus) < abs(max_bonus) || max_bonus == 0) {
    tmp.bonus += bonus;
    if (abs(tmp.bonus) > abs(max_bonus) && max_bonus != 0)
     tmp.bonus = max_bonus;
   }
   return;
  }
 }
 // Didn't increase an existing point, so add a new one
 morale.push_back(morale_point(type, item_type, bonus));
}

int player::get_morale(morale_type type, const itype* item_type) const
{
    for (auto& tmp : morale) {
        if (tmp.type == type && tmp.item_type == item_type) return tmp.bonus;
    }
    return 0;
}

static void _add_range(std::vector<item>& dest, const std::vector<item>& src)
{
    for (decltype(auto) it : src) dest.push_back(it);
}

void player::sort_inv()
{
 // guns ammo weaps armor food tools books other
 std::array< std::vector<item>, 8 > types;
 std::vector<item> tmp;
 for (size_t i = 0; i < inv.size(); i++) {
  const auto& tmp = inv.stack_at(i);
  if (tmp[0].is_gun()) _add_range(types[0], tmp);
  else if (tmp[0].is_ammo()) _add_range(types[1], tmp);
  else if (tmp[0].is_armor()) _add_range(types[3], tmp);
  else if (tmp[0].is_tool() || tmp[0].is_gunmod()) _add_range(types[5], tmp);
  else if (tmp[0].is_food() || tmp[0].is_food_container()) _add_range(types[4], tmp);
  else if (tmp[0].is_book()) _add_range(types[6], tmp);
  else if (tmp[0].is_weap()) _add_range(types[2], tmp);
  else _add_range(types[7], tmp);
 }
 inv.clear();
 for (const auto& stack : types) {
  for(const auto& it : stack) inv.push_back(it);
 }
 inv_sorted = true;
}

void player::i_add(item&& it)
{
 last_item = itype_id(it.type->id);
 if (it.is_food() || it.is_ammo() || it.is_gun()  || it.is_armor() || 
     it.is_book() || it.is_tool() || it.is_weap() || it.is_food_container())
  inv_sorted = false;
 if (const auto ammo = it.is_ammo()) {	// Possibly combine with other ammo
  for (size_t i = 0; i < inv.size(); i++) {
   auto& obj = inv[i];
   if (obj.type->id != it.type->id) continue;
   if (obj.charges < ammo->count) {
	obj.charges += it.charges;
    if (obj.charges > ammo->count) {
     it.charges = obj.charges - ammo->count;
	 obj.charges = ammo->count;
    } else it.charges = 0;	// requires full working copy to be valid
   }
  }
  if (it.charges > 0) inv.push_back(std::move(it));
  return;
 }
 if (const auto art_tool = it.is_artifact_tool()) {
  game::add_artifact_messages(art_tool->effects_carried);
 }
 inv.push_back(std::move(it));
}

bool player::has_active_item(itype_id id) const
{
 if (weapon.type->id == id && weapon.active) return true;
 for (size_t i = 0; i < inv.size(); i++) {
  if (inv[i].type->id == id && inv[i].active) return true;
 }
 return false;
}

int player::active_item_charges(itype_id id) const
{
 int max = 0;
 if (weapon.type->id == id && weapon.active) max = weapon.charges;
 for (size_t i = 0; i < inv.size(); i++) {
     for (const item& it : inv.stack_at(i)) if (it.type->id == id && it.active && it.charges > max) max = it.charges;
 }
 return max;
}

void player::process_active_items(game *g)
{
    switch (int code = use_active(weapon)) {
    case -2:
        g->process_artifact(&weapon, this, true);
        break;
    case -1:   // IF_CHARGE
        {
        if (weapon.charges == 8) {
            bool maintain = false;
            if (has_charges(itm_UPS_on, 4)) {
                use_charges(itm_UPS_on, 4);
                maintain = true;
            } else if (has_charges(itm_UPS_off, 4)) {
                use_charges(itm_UPS_off, 4);
                maintain = true;
            }
            if (maintain) {
                if (one_in(20)) {
                    // do not think range of this discharge is fundamentally linked to map generation's SEE 2020-09-23 zaimoni
                    messages.add("Your %s discharges!", weapon.tname().c_str());

                    const auto target2(GPSpos + rng(within_rldist<12>));
                    if (auto traj2 = GPSpos.sees(target2, 0)) {
                        g->fire(*this, *traj2, false);
                    };
                } else
                    messages.add("Your %s beeps alarmingly.", weapon.tname().c_str());
            }
        } else {
            if (has_charges(itm_UPS_on, 1 + weapon.charges)) {
                use_charges(itm_UPS_on, 1 + weapon.charges);
                weapon.poison++;
            }
            else if (has_charges(itm_UPS_off, 1 + weapon.charges)) {
                use_charges(itm_UPS_off, 1 + weapon.charges);
                weapon.poison++;
            } else {
                messages.add("Your %s spins down.", weapon.tname().c_str());
                if (weapon.poison <= 0) {
                    weapon.charges--;
                    weapon.poison = weapon.charges - 1;
                }
                else weapon.poison--;
                if (weapon.charges == 0) weapon.active = false;
            } if (weapon.poison >= weapon.charges) {
                weapon.charges++;
                weapon.poison = 0;
            }
        }
        return;
        }
    // ignore code 1: ok for weapon to be the null item
    }

 for (size_t i = 0; i < inv.size(); i++) {
  decltype(auto) _inv = inv.stack_at(i);
  int j = _inv.size();
  while(0 < j) {
      item* tmp_it = &(_inv[--j]);
      switch (int code = use_active(*tmp_it))
      {
      case -2:
          g->process_artifact(tmp_it, this, true);
          break;
          // ignore IF_CHARGE/case -1
      case 1:  // null item shall not survive in inventory
          if (1 == _inv.size()) {
              inv.destroy_stack(i); // references die
              i--;
              j = 0;
          } else EraseAt(_inv, j);
          break;
      }
  }
 }
 for (auto& it : worn) {
  if (it.is_artifact()) g->process_artifact(&it, this);
 }
}

void player::remove_weapon()
{
 weapon = item::null;
// We need to remove any boosts related to our style
 static const decltype(DI_ATTACK_BOOST) attack_boosts[] = {
     DI_ATTACK_BOOST,
     DI_DODGE_BOOST,
     DI_DAMAGE_BOOST,
     DI_SPEED_BOOST,
     DI_ARMOR_BOOST,
     DI_VIPER_COMBO
 };

 static auto decline = [&](disease& ill) { return any(attack_boosts, ill.type); };
 rem_disease(decline);
}

item player::unwield()
{
    item ret(std::move(weapon));
    remove_weapon();
    return ret;
}

void player::remove_mission_items(int mission_id)
{
 if (mission::MIN_ID > mission_id) return;
 if (weapon.is_mission_item(mission_id)) remove_weapon();
 size_t i = inv.size();
 while (0 < i) {
     decltype(auto) stack = inv.stack_at(--i);
     size_t j = stack.size();
     while (0 < j) {
         decltype(auto) it = stack[--j];
         // inv.remove_item *could* invalidate the stack reference, but if so 1 == stack.size() beforehand
         // and we're not using that reference further
         if (it.is_mission_item(mission_id)) inv.remove_item(i, j);
     }
 }
}

std::optional<player::item_spec> player::from_invlet(char let)
{
    if (KEY_ESCAPE == let) return std::nullopt;
    if (let == weapon.invlet) return std::pair(&weapon, -1);
    int worn_index = -2;
    for (decltype(auto) it : worn) {
        if (let == it.invlet) return std::pair(&it, worn_index);
        worn_index--;
    }
    int index = inv.index_by_letter(let);
    if (0 > index) return std::nullopt;
    return std::pair(&inv[index], index);
}

std::vector<player::item_spec> player::reject(std::function<std::optional<std::string>(const item_spec&)> fail)
{
    std::vector<player::item_spec> ret;
    player::item_spec stage;

    int i = inv.size();
    while (0 <= --i) {
        stage = player::item_spec(&inv[i], i);
        if (!fail(stage)) ret.push_back(stage);
    }

    stage = player::item_spec(&weapon, -1);
    if (!fail(stage)) ret.push_back(stage);

    i = -2;
    for (decltype(auto) it : worn) {
        stage = player::item_spec(&it, i);
        if (!fail(stage)) ret.push_back(stage);
        i--;
    }

    return ret;
}

std::optional<player::item_spec_const> player::from_invlet(char let) const
{
    if (KEY_ESCAPE == let) return std::nullopt;
    if (let == weapon.invlet) return std::pair(&weapon, -1);
    int worn_index = -2;
    for (decltype(auto) it : worn) {
        if (let == it.invlet) return std::pair(&it, worn_index);
        worn_index--;
    }
    int index = inv.index_by_letter(let);
    if (0 > index) return std::nullopt;
    return std::pair(&inv[index], index);
}

std::optional<player::item_spec> player::lookup(item* it)
{
    if (!it) return std::nullopt;
    if (it == &weapon) return std::pair(&weapon, -1);
    int worn_index = -2;
    for (decltype(auto) obj : worn) {
        if (&obj == it) return std::pair(&obj, worn_index);
        worn_index--;
    }
    if (auto code = inv.index_of(*it); 0 <= code) return std::pair(&inv[code], code);
    return std::nullopt;
}

item& player::i_of_type(itype_id type)
{
 if (weapon.type->id == type) return weapon;
 for(auto& it : worn) {
  if (it.type->id == type) return it;
 }
 for (size_t i = 0; i < inv.size(); i++) {
  if (inv[i].type->id == type) return inv[i];
 }
 return (discard<item>::x = item::null);
}

std::vector<item> player::inv_dump() const
{
 std::vector<item> ret;
 if (weapon.type->id != 0 && weapon.type->id < num_items) ret.push_back(weapon);
 for(const auto& it : worn) ret.push_back(it);
 for(size_t i = 0; i < inv.size(); i++) {
  for (const auto& it : inv.stack_at(i)) ret.push_back(it);
 }
 return ret;
}

item player::i_remn(int index)
{
 if (index > inv.size() || index < 0) return item::null;
 return inv.remove_item(index);
}

bool player::remove_discard(const item_spec& it)
{
    assert(it.first);
    if (-1 == it.second) {
        assert(it.first == &weapon);
        remove_weapon();
        return true;
    }
    if (0 <= it.second && inv.size() > it.second) {
        assert(it.first == &inv[it.second]);
        i_remn(it.second);
        return true;
    }
    if (-2 >= it.second && -2 - worn.size() < it.second) {
        auto worn_at = -(2 + it.second);
        assert(it.first == &worn[worn_at]);
        EraseAt(worn, worn_at);
        return true;
    }
    return false;
}


void player::use_amount(itype_id it, int quantity, bool use_container)
{
 bool used_weapon_contents = false;
 for (int i = 0; i < weapon.contents.size(); i++) {
  if (weapon.contents[0].type->id == it) {
   quantity--;
   EraseAt(weapon.contents, 0);
   i--;
   used_weapon_contents = true;
  }
 }
 if (use_container && used_weapon_contents)
  remove_weapon();

 if (weapon.type->id == it) {
  quantity--;
  remove_weapon();
 }

 inv.use_amount(it, quantity, use_container);
}

unsigned int player::use_charges(itype_id it, int quantity)
{
 const int start_qty = quantity;

 if (it == itm_toolset) {
  power_level -= quantity;
  if (power_level < 0) power_level = 0;
  return start_qty;
 }

 // check weapon first
 if (auto code = weapon.use_charges(it, quantity)) {
     if (0 > code) remove_weapon();
     if (0 < code || 0 >= quantity) return start_qty;
 }

 unsigned int delta = start_qty - quantity;
 return delta + inv.use_charges(it, quantity);
}

std::optional<int> player::butcher_factor() const
{
    std::optional<int> lowest_factor;
 for (size_t i = 0; i < inv.size(); i++) {
  for (int j = 0; j < inv.stack_at(i).size(); j++) {
   const item *cur_item = &(inv.stack_at(i)[j]);
   if (cur_item->damage_cut() >= 10 && !cur_item->has_flag(IF_SPEAR)) {
    int factor = cur_item->volume() * 5 - cur_item->weight() * 1.5 - cur_item->damage_cut();
    if (cur_item->damage_cut() <= 20) factor *= 2;
    if (!lowest_factor || factor < *lowest_factor) lowest_factor = factor;
   }
  }
 }
 if (weapon.damage_cut() >= 10 && !weapon.has_flag(IF_SPEAR)) {
  int factor = weapon.volume() * 5 - weapon.weight() * 1.5 - weapon.damage_cut();
  if (weapon.damage_cut() <= 20) factor *= 2;
  if (!lowest_factor || factor < *lowest_factor) lowest_factor = factor;
 }
 return lowest_factor;
}

item* player::pick_usb()
{
 std::vector<int> drives;
 for (size_t i = 0; i < inv.size(); i++) {
  if (inv[i].type->id == itm_usb_drive) {
   if (inv[i].contents.empty()) return &inv[i]; // No need to pick, use an empty one by default!
   drives.push_back(i);
  }
 }

 if (drives.empty()) return nullptr; // None available!
 if (1 == drives.size()) return &inv[drives[0]];	// exactly one.

 std::vector<std::string> selections;
 for (int i = 0; i < drives.size() && i < 9; i++)
  selections.push_back( inv[drives[i]].tname() );

 int select = menu_vec("Choose drive:", selections);

 return &inv[drives[ select - 1 ]];
}

bool player::is_wearing(itype_id it) const
{
 for (const auto& obj : worn) {
  if (obj.type->id == it) return true;
 }
 return false;
}

bool player::has_artifact_with(art_effect_passive effect) const
{
 if (const auto art_tool = weapon.is_artifact_tool()) {
     if (any(art_tool->effects_wielded, effect)) return true;
     if (any(art_tool->effects_carried, effect)) return true;
 }
 for (size_t i = 0; i < inv.size(); i++) {
     const auto& it = inv[i];
     if (const auto art_tool = it.is_artifact_tool()) {
         if (any(art_tool->effects_carried, effect)) return true;
     }
 }
 for (const auto& it : worn) {
     if (const auto art_armor = it.is_artifact_armor()) {
         if (any(art_armor->effects_worn, effect)) return true;
     }
 }
 return false;
}
   
int player::amount_of(itype_id it) const
{
 if (it == itm_toolset && has_bionic(bio_tools)) return 1;
 int quantity = 0;
 if (weapon.type->id == it) quantity++;
 for (const auto& obj : weapon.contents) {
  if (obj.type->id == it) quantity++;
 }
 quantity += inv.amount_of(it);
 return quantity;
}

int player::charges_of(itype_id it) const
{
 if (it == itm_toolset) return has_bionic(bio_tools) ? power_level : 0;

 int quantity = 0;
 if (weapon.type->id == it) quantity += weapon.charges;
 for (const auto& obj : weapon.contents) {
  if (obj.type->id == it) quantity += obj.charges;
 }
 quantity += inv.charges_of(it);
 return quantity;
}

bool player::has_watertight_container() const
{
 for (size_t i = 0; i < inv.size(); i++) {
     if (const auto cont = inv[i].is_container()) {
         if (inv[i].contents.empty()) {
             if ((mfb(con_wtight) | mfb(con_seals)) == (cont->flags & (mfb(con_wtight) | mfb(con_seals))))
                 return true;
         }
     }
 }
 return false;
}

bool player::has_weapon_or_armor(char let) const
{
 if (weapon.invlet == let) return true;
 for (const auto& it : worn) if (it.invlet == let) return true;
 return false;
}

bool player::has_item(item *it) const
{
 if (it == &weapon) return true;
 for (int i = 0; i < worn.size(); i++) {
  if (it == &(worn[i])) return true;
 }
 return inv.has_item(it);
}

bool player::has_mission_item(int mission_id) const
{
 if (mission::MIN_ID > mission_id) return false;
 if (weapon.is_mission_item(mission_id)) return true;
 for (size_t i = 0; i < inv.size(); i++) {
  for (decltype(auto) it : inv.stack_at(i)) if (it.is_mission_item(mission_id)) return true;
 }
 return false;
}

int player::lookup_item(char let) const
{
 if (weapon.invlet == let) return -1;

 for (size_t i = 0; i < inv.size(); i++) {
  if (inv[i].invlet == let) return i;
 }

 return -2; // -2 is for "item not found"
}

item* player::decode_item_index(const int n)
{
    if (0 <= n) return inv.size() > n ? &inv[n] : nullptr;
    else if (-2 == n) return &weapon;
    else if (-3 >= n) {
        const auto armor_n = -3 - n;
        return worn.size() > armor_n ? &worn[n] : nullptr;
    }
    return nullptr;
}

static auto parse_food(const player::item_spec & src, const player & u)
{
    const auto eating_from = src.first->is_food_container2(u);
    auto eating = eating_from ? eating_from : src.first->is_food2(u);
    item* const eaten = eating_from ? &src.first->contents[0] : src.first;

    return std::tuple(eating_from, eating, eaten);
}

std::optional<std::string> player::cannot_eat(const item_spec& src) const
{
    if (-1 > src.second) return "You need to take that off before eating it.";
    auto [eating_from, eating, eaten] = parse_food(src, *this);
    if (!eating) return std::string("You can't eat your ") + eaten->tname() + ".";

    if (const auto food_ptr = std::get_if<const it_comest*>(&(*eating))) {
        // Remember, comest points to the it_comest data
        const auto comest = *food_ptr;
        if (comest->tool != itm_null) {
            bool has = item::types[comest->tool]->count_by_charges() ? has_charges(comest->tool, 1) : has_amount(comest->tool, 1);
            if (!has) return std::string("You need a ")+ item::types[comest->tool]->name +" to consume that!";
        }

        if (const auto err = comest->cannot_consume(*src.first, *this)) return err;

        bool overeating = (!has_trait(PF_GOURMAND) && hunger < 0 && comest->nutr >= 15);
        if (overeating && !ask_yn("You're full.  Force yourself to eat?")) return std::string();

        if (has_trait(PF_CARNIVORE) && eaten->made_of(VEGGY) && comest->nutr > 0) return "You can only eat meat!";
        if (has_trait(PF_VEGETARIAN) && eaten->made_of(FLESH) && !ask_yn("Really eat that meat? (The poor animals!)")) return std::string();

        if (    eaten->rotten() && !has_trait(PF_SAPROVORE)
            && !ask_yn(std::string("This ") + eaten->tname() + " smells awful!Eat it ? "))
            return std::string();
    }

    return std::nullopt;
}

bool player::eat(const item_spec& src)
{
    if (const auto inedible = cannot_eat(src)) {
        subjective_message(*inedible);
        return false;
    }

    auto [eating_from, eating, eaten] = parse_food(src, *this);
    assert(eating);

    if (const auto food_ptr = std::get_if<const it_comest*>(&(*eating))) {
        // Remember, comest points to the it_comest data
        const auto comest = *food_ptr;

        // \todo restart migration of precondition testing into player::cannot_eat
        bool overeating = (!has_trait(PF_GOURMAND) && hunger < 0 && comest->nutr >= 15);

        last_item = itype_id(eaten->type->id); // strictly pc-only, but last_item is *only* used by the tutorial and warrants a re-implementation

        if (eaten->rotten()) {
            const bool immune = has_trait(PF_SAPROVORE);
            if (!immune) // NPCs reject at player::cannot_eat
                messages.add("Ick, this %s doesn't taste so good...", eaten->tname().c_str());

            const bool resistant = has_bionic(bio_digestion);
            if (!immune && (!resistant || one_in(3))) add_disease(DI_FOODPOISON, rng(MINUTES(6), (comest->nutr + 1) * MINUTES(6)));
            hunger -= rng(0, comest->nutr);
            thirst -= comest->quench;
            if (!immune && !resistant) health -= 3;
        }
        else {
            hunger -= comest->nutr;
            thirst -= comest->quench;
            if (has_bionic(bio_digestion)) hunger -= rng(0, comest->nutr);
            else if (!has_trait(PF_GOURMAND)) {
                if ((overeating && rng(-200, 0) > hunger)) vomit();
            }
            health += comest->healthy;
        }
        // At this point, we've definitely eaten the item, so use up some turns.
        moves -= has_trait(PF_GOURMAND) ? 3 * (mobile::mp_turn / 2) : 5 * (mobile::mp_turn / 2);
        // If it's poisonous... poison us.  TODO: More several poison effects
        if (eaten->poison >= rng(2, 4)) add_disease(DI_POISON, eaten->poison * MINUTES(10));
        if (eaten->poison > 0) add_disease(DI_FOODPOISON, eaten->poison * MINUTES(30));

        static auto me = [&]() {
            if (eaten->made_of(LIQUID)) return std::string("You drink your ")+ eaten->tname() + ".";
            else return std::string("You eat your ") + eaten->tname() + ".";
        };

        static auto other = [&]() {
            if (eaten->made_of(LIQUID)) return name + " drinks a " + eaten->tname() + ".";
            else return name + " eats a " + eaten->tname() + ".";
        };

        if_visible_message(me, other);  // \todo this is *not* correct for first aid/bandages

        if (item::types[comest->tool]->is_tool()) use_charges(comest->tool, 1); // Tools like lighters get used
        if (comest->stim > 0) {
            if (comest->stim < 10 && stim < comest->stim) {
                stim += comest->stim;
                if (stim > comest->stim) stim = comest->stim;
            }
            else if (comest->stim >= 10 && stim < comest->stim * 3) stim += comest->stim;
        }

        try {
            consume(*eaten); // to ensure correct type is seen by the handler
        } catch (const std::string e) {
            debugmsg(e.c_str());
            return false;
        }
        add_addiction(comest->add, comest->addict);

        if (eaten->made_of(FLESH)) {
            if (has_trait(PF_VEGETARIAN)) {
                subjective_message("You feel bad about eating this meat...");
                add_morale(MORALE_VEGETARIAN, -75, -400);
            }
            if (has_trait(PF_HERBIVORE) || has_trait(PF_RUMINANT)) {
                if (!one_in(3)) vomit();
                if (comest->quench >= 2) thirst += int(comest->quench / 2);
                if (comest->nutr >= 2) hunger += int(comest->nutr * .75);
            }
        }
        if (has_trait(PF_GOURMAND)) {
            if (comest->fun < -2)
                add_morale(MORALE_FOOD_BAD, comest->fun * 2, comest->fun * 4, comest);
            else if (comest->fun > 0)
                add_morale(MORALE_FOOD_GOOD, comest->fun * 3, comest->fun * 6, comest);
            if (hunger < -60 || thirst < -60) subjective_message("You can't finish it all!");
            if (hunger < -60) hunger = -60;
            if (thirst < -60) thirst = -60;
        }
        else {
            if (comest->fun < 0)
                add_morale(MORALE_FOOD_BAD, comest->fun * 2, comest->fun * 6, comest);
            else if (comest->fun > 0)
                add_morale(MORALE_FOOD_GOOD, comest->fun * 2, comest->fun * 4, comest);
            if (hunger < -20 || thirst < -20) subjective_message("You can't finish it all!");
            if (hunger < -20) hunger = -20;
            if (thirst < -20) thirst = -20;
        }
    } else if (const auto ammo = std::get_if<const it_ammo*>(&(*eating))) {
        charge_power(eaten->charges / 20); // For when bionics let you eat fuel
        eaten->charges = 0;
    } else {
        const auto fuel = std::get<const item*>(*eating);
        int charge = (fuel->volume() + fuel->weight()) / 2;
        // bypassing corpse part of item::made_of test
        if (fuel->type->m1 == LEATHER || fuel->type->m2 == LEATHER) charge /= 4;
        if (fuel->type->m1 == WOOD || fuel->type->m2 == WOOD) charge /= 2;
        charge_power(charge);
    }

    if (0 >= --eaten->charges) {
        if (eating_from) {
            EraseAt(src.first->contents, 0);
            if (-1 == src.second) messages.add("You are now wielding an empty %s.", src.first->tname().c_str());
            else {
                if (!is_npc()) messages.add("%c - an empty %s", src.first->invlet, src.first->tname().c_str());
                if (!inv.stack_at(src.second).empty()) inv.restack(this);
                inv_sorted = false;
            }
        } else {
            remove_discard(src);
        }
    }
    return true;
}

bool player::wield(int index)
{
 if (weapon.has_flag(IF_NO_UNWIELD)) {
  messages.add("You cannot unwield your %s!  Withdraw them with 'p'.", weapon.tname().c_str());
  return false;
 }
 if (index == -3) {
  bool pickstyle = (!styles.empty());
  if (weapon.is_style()) remove_weapon();
  else if (!is_armed()) {
   if (!pickstyle) {
    messages.add("You are already wielding nothing.");
    return false;
   }
  } else if (volume_carried() + weapon.volume() < volume_capacity()) {
   inv.push_back(unwield());
   inv_sorted = false;
   moves -= 20;
   recoil = 0;
   if (!pickstyle) return true;
  } else if (ask_yn("No room in inventory for your " + weapon.tname() + ".  Drop it?")) {
   GPSpos.add(unwield());
   recoil = 0;
   if (!pickstyle) return true;
  } else return false;

  if (pickstyle) {
   weapon = item(item::types[style_selected], 0 );
   weapon.invlet = ':';
   return true;
  }
 }
 if (index == -1) {
  messages.add("You're already wielding that!");
  return false;
 } else if (index == -2) {
  messages.add("You don't have that item.");
  return false;
 }

 if (inv[index].is_two_handed(*this) && !has_two_arms()) {
  messages.add("You cannot wield a %s with only one arm.", inv[index].tname().c_str());
  return false;
 }
 if (!is_armed()) {
  weapon = inv.remove_item(index);
  if (const auto art_tool = weapon.is_artifact_tool()) game::add_artifact_messages(art_tool->effects_wielded);
  moves -= 3 * (mobile::mp_turn / 10);
  last_item = itype_id(weapon.type->id);
  return true;
 } else if (volume_carried() + weapon.volume() - inv[index].volume() < volume_capacity()) {
  item tmpweap(unwield());
  weapon = inv.remove_item(index);
  inv.push_back(std::move(tmpweap));
  inv_sorted = false;
  moves -= 9 * (mobile::mp_turn / 20);
  if (const auto art_tool = weapon.is_artifact_tool()) game::add_artifact_messages(art_tool->effects_wielded);
  last_item = itype_id(weapon.type->id);
  return true;
 } else if (ask_yn("No room in inventory for your " + weapon.tname() + ".  Drop it?")) {
  GPSpos.add(unwield());
  weapon = inv.remove_item(index);
  inv_sorted = false;
  moves -= 3 * (mobile::mp_turn / 10);
  if (const auto art_tool = weapon.is_artifact_tool()) {
   game::add_artifact_messages(art_tool->effects_wielded);
  }
  last_item = itype_id(weapon.type->id);
  return true;
 }

 return false;
}

void player::pick_style() // Style selection menu
{
 std::vector<std::string> options;
 options.push_back("No style");
 for (const auto style : styles) options.push_back(item::types[style]->name);
 int selection = menu_vec("Select a style", options);
 style_selected = (2 <= selection) ? styles[selection - 2] : itm_null;
}

bool player::wear(const item_spec& wear_this)
{
    if (!wear_item(*wear_this.first)) return false;

    if (-2 == wear_this.second) weapon = item::null;
    else inv.remove_item(wear_this.second);

    return true;
}

const it_armor* player::wear_is_performable(const item& to_wear) const
{
    const auto armor = to_wear.is_armor();
    if (!armor) {
        if (!is_npc()) messages.add("Putting on a %s would be tricky.", to_wear.tname().c_str());
        return nullptr;
    }

    // Make sure we're not wearing 2 of the item already
    int count = 0;
    for (decltype(auto) it : worn) {
        if (it.type->id == to_wear.type->id) count++;
    }
    if (2 <= count) {
        if (!is_npc()) messages.add("You can't wear more than two %s at once.", to_wear.tname().c_str());
        return nullptr;
    }
    if (has_trait(PF_WOOLALLERGY) && to_wear.made_of(WOOL)) {
        if (!is_npc()) messages.add("You can't wear that, it's made of wool!");
        return nullptr;
    }
    if ((armor->covers & mfb(bp_head)) && encumb(bp_head) != 0) {
        if (!is_npc()) messages.add("You can't wear a%s helmet!", wearing_something_on(bp_head) ? "nother" : "");
        return nullptr;
    }
    if ((armor->covers & mfb(bp_hands)) && has_trait(PF_WEBBED)) {
        if (!is_npc()) messages.add("You cannot put %s over your webbed hands.", armor->name.c_str());
        return nullptr;
    }
    if ((armor->covers & mfb(bp_hands)) && has_trait(PF_TALONS)) {
        if (!is_npc()) messages.add("You cannot put %s over your talons.", armor->name.c_str());
        return nullptr;
    }
    if ((armor->covers & mfb(bp_mouth)) && has_trait(PF_BEAK)) {
        if (!is_npc()) messages.add("You cannot put a %s over your beak.", armor->name.c_str());
        return nullptr;
    }
    if ((armor->covers & mfb(bp_feet)) && has_trait(PF_HOOVES)) {
        if (!is_npc()) messages.add("You cannot wear footwear on your hooves.");
        return nullptr;
    }
    if ((armor->covers & mfb(bp_head)) && has_trait(PF_HORNS_CURLED)) {
        if (!is_npc()) messages.add("You cannot wear headgear over your horns.");
        return nullptr;
    }
    if ((armor->covers & mfb(bp_torso)) && has_trait(PF_SHELL)) {
        if (!is_npc()) messages.add("You cannot wear anything over your shell.");
        return nullptr;
    }

    static constexpr const std::pair<pl_flag, const char*> transcode[] = {
        {PF_HORNS_POINTED, "horns"},
        {PF_ANTENNAE, "antennae"},
        {PF_ANTLERS, "antlers"},
    };

    if ((armor->covers & mfb(bp_head)) && !to_wear.made_of(WOOL) &&
        !to_wear.made_of(COTTON) && !to_wear.made_of(LEATHER)) {
        if (const auto text = interpret_trait(std::begin(transcode), std::end(transcode) - std::begin(transcode))) {
            if (!is_npc()) messages.add("You cannot wear a helmet over your %s.", text);
            return nullptr;
        }
    }
    if ((armor->covers & mfb(bp_feet)) && wearing_something_on(bp_feet)) {
        if (!is_npc()) messages.add("You're already wearing footwear!");
        return nullptr;
    }
    return armor;
}

bool player::wear_item(const item& to_wear)
{
 const it_armor* const armor = wear_is_performable(to_wear);
 if (!armor) return false;

 if (!is_npc()) messages.add("You put on your %s.", to_wear.tname().c_str());
 if (const auto art_armor = to_wear.is_artifact_armor()) game::add_artifact_messages(art_armor->effects_worn);
 moves -= 7 * (mobile::mp_turn / 2); // \todo? Make this variable
 last_item = itype_id(to_wear.type->id);
 worn.push_back(to_wear);
 if (!is_npc()) {
     for (body_part i = bp_head; i < num_bp; i = body_part(i + 1)) {
         if (armor->covers & mfb(i) && encumb(i) >= 4)
             messages.add("Your %s %s very encumbered! %s",
                 body_part_name(body_part(i), 2),
                 (i == bp_head || i == bp_torso || i == bp_mouth ? "is" : "are"),
                 encumb_text(i));
     }
 }
 return true;
}

int player::can_take_off_armor(const item& it) const
{
    if (volume_capacity() - (dynamic_cast<const it_armor*>(it.type))->storage > volume_carried() + it.type->volume) return 1;
    if (ask_yn("No room in inventory for your " + it.tname() + ".  Drop it?")) return -1;
    return 0;
}

bool player::take_off(int i)
{
    auto& it = worn[i];
    switch (const auto code = can_take_off_armor(it))
    {
    case 1:
        inv.push_back(std::move(it));
        inv_sorted = false;
        EraseAt(worn, i);
        return true;
    case -1:
        GPSpos.add(std::move(it));
        EraseAt(worn, i);
        return true;
    default: return false;
    }
}

std::optional<std::string> player::cannot_read() const
{
    if (const auto veh = GPSpos.veh_at()) {
        if (veh->first->player_in_control(*this)) return "It's bad idea to read while driving.";
    }
    if (morale_level() < MIN_MORALE_READ) return "What's the point of reading?  (Your morale is too low!)";	// See morale.h
    // following does not account for reading by touch (braille, or embossed/inlaid text)
    if (has(mobile::effect::BLIND)) return "You're blind!";
    // This is likely not fully correct (more light with say cataracts, somewhat less light when dark-adapted)
    if (2 * MOONLIGHT_LEVEL > game::active()->light_level()) return "It's too dark to read!";
    return std::nullopt;
}

std::optional<std::string> player::cannot_read(const std::variant<const it_macguffin*, const it_book*>& src) const
{
    if (const auto book2 = std::get_if<const it_book*>(&src)) {
        const auto book = *book2;   // backward compatibility
        if (book->intel > 0 && has_trait(PF_ILLITERATE)) return "You're illiterate!";
        else if (book->intel > int_cur) return "This book is way too complex for you to understand.";
        else if (book->req > sklevel[book->type]) return std::string("The ") + skill_name(skill(book->type)) + "-related jargon flies over your head!";
        else if (book->level <= sklevel[book->type] && book->fun <= 0 &&
            !ask_yn(std::string("Your ") + skill_name(skill(book->type)) + " skill won't be improved.  Read anyway?"))
            return std::string();
    }
    return std::nullopt;
}

void player::try_to_sleep()
{
 switch (const auto terrain = GPSpos.ter())
 {
 case t_floor: break;
 case t_bed:
	 messages.add("This bed is a comfortable place to sleep.");
	 break;
 default:
	{
	const auto& t_data = ter_t::list[terrain];
    messages.add("It's %shard to get to sleep on this %s.", t_data.movecost <= 2 ? "a little " : "", t_data.name.c_str());
	}
 }
 add_disease(DI_LYING_DOWN, MINUTES(30));
}

bool player::can_sleep(const map& m) const
{
 int sleepy = 0;
 if (has_addiction(ADD_SLEEP)) sleepy -= 3;
 if (has_trait(PF_INSOMNIA)) sleepy -= 8;

 const auto veh = GPSpos.veh_at();
 if (veh && veh->first->part_with_feature(veh->second, vpf_seat) >= 0) sleepy += 4;
 else {
     decltype(auto) sleeping_on = GPSpos.ter();
     if (t_bed == sleeping_on) sleepy += 5;
     else if (t_floor == sleeping_on) sleepy += 1;
     else sleepy -= move_cost_of(sleeping_on);
 }
 sleepy = (192 > fatigue) ? (192-fatigue)/4 : (fatigue-192)/16;
 sleepy += rng(-8, 8);
 sleepy -= 2 * stim;
 return 0 < sleepy;
}

int player::warmth(body_part bp) const
{
 int ret = 0;
 for (const auto& it : worn) {
  const it_armor* const armor = dynamic_cast<const it_armor*>(it.type);
  if (armor->covers & mfb(bp)) ret += armor->warmth;
 }
 return ret;
}

// we would like a contrafactual version suitable for use by NPC AI and Socrates' Daimon
void player::check_warmth(int ambient_F)
{
    const auto g = game::active();
    // no attempt to model wind chill, heat index, radiant heat, etc.  \todo fix this?
    const int F_delta = (ambient_F - 65) / 10;

    // HEAD
    int temp_code = warmth(bp_head) + F_delta;
    if (temp_code <= -6) {
        subjective_message("Your head is freezing!");
        add_disease(DI_COLD, abs(temp_code * 2));// Heat loss via head is bad
        hurt(bp_head, 0, rng(0, abs(temp_code / 3)));
    } else if (temp_code <= -3) {
        subjective_message("Your head is cold.");
        add_disease(DI_COLD, abs(temp_code * 2));
    } else if (temp_code >= 8) {
        subjective_message("Your head is overheating!");
        add_disease(DI_HOT, rational_scaled<3,2>(temp_code));
    }
    // FACE -- Mouth and eyes
    temp_code = warmth(bp_eyes) + warmth(bp_mouth) + F_delta;
    if (temp_code <= -6) {
        subjective_message("Your face is freezing!");
        add_disease(DI_COLD_FACE, abs(temp_code));
        hurt(bp_head, 0, rng(0, abs(temp_code / 3)));
    } else if (temp_code <= -4) {
        subjective_message("Your face is cold.");
        add_disease(DI_COLD_FACE, abs(temp_code));
    } else if (temp_code >= 12) {
        subjective_message("Your face is overheating!");
        add_disease(DI_HOT, temp_code);
    }
    // TORSO
    temp_code = warmth(bp_torso) + F_delta;
    if (temp_code <= -8) {
        subjective_message("Your body is freezing!");
        add_disease(DI_COLD, abs(temp_code));
        hurt(bp_torso, 0, rng(0, abs(temp_code / 4)));
    } else if (temp_code <= -2) {
        subjective_message("Your body is cold.");
        add_disease(DI_COLD, abs(temp_code));
    } else if (temp_code >= 12) {
        subjective_message("Your body is too hot.");
        add_disease(DI_HOT, temp_code * 2);
    }
    // \todo: frostbite
    // HANDS
    temp_code = warmth(bp_hands) + F_delta;
    if (temp_code <= -4) {
        subjective_message("Your hands are freezing!");
        add_disease(DI_COLD_HANDS, abs(temp_code));
    } else if (temp_code >= 8) {
        subjective_message("Your hands are overheating!");
        add_disease(DI_HOT, rng(0, temp_code / 2));
    }
    // LEGS
    // \todo: frostbite
    temp_code = warmth(bp_legs) + F_delta;
    if (temp_code <= -6) {
        subjective_message("Your legs are freezing!");
        add_disease(DI_COLD_LEGS, abs(temp_code));
    } else if (temp_code <= -3) {
        subjective_message("Your legs are very cold.");
        add_disease(DI_COLD_LEGS, abs(temp_code));
    } else if (temp_code >= 8) {
        subjective_message("Your legs are overheating!");
        add_disease(DI_HOT, rng(0, temp_code));
    }
    // FEET
    // \todo: frostbite
    temp_code = warmth(bp_feet) + F_delta;
    if (temp_code <= -3) {
        subjective_message("Your feet are freezing!");
        add_disease(DI_COLD_FEET, temp_code);
    } else if (temp_code >= 12) {
        subjective_message("Your feet are overheating!");
        add_disease(DI_HOT, rng(0, temp_code));
    }
}

int player::encumb(body_part bp) const
{
 int ret = 0;
 int layers = 0;
 for (const auto& it : worn) {
  const it_armor* const armor = dynamic_cast<const it_armor*>(it.type);
  if (armor->covers & mfb(bp) ||
      (bp == bp_torso && (armor->covers & mfb(bp_arms)))) {
   ret += armor->encumber;
   if (armor->encumber >= 0 || bp != bp_torso) layers++;
  }
 }
 if (layers > 1) ret += (layers - 1) * (bp == bp_torso ? .5 : 2);// Easier to layer on torso
 if (volume_carried() > volume_capacity() - 2 && bp != bp_head) ret += 3;

// Bionics and mutation
 if ((bp == bp_head  && has_bionic(bio_armor_head))  ||
     (bp == bp_torso && has_bionic(bio_armor_torso)) ||
     (bp == bp_legs  && has_bionic(bio_armor_legs)))
  ret += 2;
 if (has_bionic(bio_stiff) && bp != bp_head && bp != bp_mouth) ret += 1;
 if (has_trait(PF_CHITIN3) && bp != bp_eyes && bp != bp_mouth) ret += 1;
 if (has_trait(PF_SLIT_NOSTRILS) && bp == bp_mouth) ret += 1;
 if (bp == bp_hands &&
     (has_trait(PF_ARM_TENTACLES) || has_trait(PF_ARM_TENTACLES_4) ||
      has_trait(PF_ARM_TENTACLES_8)))
  ret += 3;
 return ret;
}

static std::pair<int, int> armor(body_part bp, const player& p)
{
    std::pair<int, int> ret(0, 0); // bash, cut armor values

    // See, we do it backwards, which assumes the player put on their jacket after
    //  their T shirt, for example.  TODO: don't assume! ASS out of U & ME, etc.
    for (int i = p.worn.size() - 1; i >= 0; i--) {
        decltype(auto) armor = p.worn[i];
        const it_armor* const tmp = dynamic_cast<const it_armor*>(armor.type);
        if ((tmp->covers & mfb(bp))) continue;
        int arm_bash = tmp->dmg_resist;
        int arm_cut = tmp->cut_resist;
        if (tmp->storage < 20) { // e.g., trenchcoat is large enough to get in the way even if tattered
            if (5 <= armor.damage) continue;  // V0.2.7 : damage level 5+ is no protection at all
            switch (armor.damage) {
            case 1:
                arm_bash *= .8;
                arm_cut *= .9;
                break;
            case 2:
                arm_bash *= .7;
                arm_cut *= .7;
                break;
            case 3:
                arm_bash *= .5;
                arm_cut *= .4;
                break;
            case 4:
                arm_bash *= .2;
                arm_cut *= .1;
                break;
            }
        }
        ret.first += arm_bash;
        ret.second += arm_cut;
    }

    if (p.has_bionic(bio_carbon)) {
        ret.first -= 2;
        ret.second -= 4;
    }
    if (bp == bp_head && p.has_bionic(bio_armor_head)) {
        ret.first -= 3;
        ret.second -= 3;
    }
    else if (bp == bp_arms && p.has_bionic(bio_armor_arms)) {
        ret.first -= 3;
        ret.second -= 3;
    }
    else if (bp == bp_torso && p.has_bionic(bio_armor_torso)) {
        ret.first -= 3;
        ret.second -= 3;
    }
    else if (bp == bp_legs && p.has_bionic(bio_armor_legs)) {
        ret.first -= 3;
        ret.second -= 3;
    }
    if (p.has_trait(PF_THICKSKIN)) --ret.second;
    if (p.has_trait(PF_SCALES)) ret.second -= 2;
    if (p.has_trait(PF_THICK_SCALES)) ret.second -= 4;
    if (p.has_trait(PF_SLEEK_SCALES)) --ret.second;
    if (p.has_trait(PF_FEATHERS)) --ret.first;
    if (p.has_trait(PF_FUR)) --ret.first;
    if (p.has_trait(PF_CHITIN)) ret.second -= 2;
    if (p.has_trait(PF_CHITIN2)) {
        --ret.first;
        ret.second -= 4;
    }
    if (p.has_trait(PF_CHITIN3)) {
        ret.first -= 2;
        ret.second -= 8;
    }
    if (p.has_trait(PF_PLANTSKIN)) --ret.first;
    if (p.has_trait(PF_BARK)) ret.first -= 2;
    if (bp == bp_feet && p.has_trait(PF_HOOVES)) --ret.second;
    if (bp == bp_torso && p.has_trait(PF_SHELL)) {
        ret.first += 6;
        ret.second += 14;
    }
    if (const int armor_boost = p.disease_intensity(DI_ARMOR_BOOST); 0 < armor_boost) {
        ret.first += rng(0, armor_boost);
        ret.second += rng(0, armor_boost);
    }
    return ret;
}

int player::armor_bash(body_part bp) const { return armor(bp, *this).first; }
int player::armor_cut(body_part bp) const { return armor(bp, *this).second; }

void player::absorb(body_part bp, int &dam, int &cut)
{
 if (has_active_bionic(bio_ads)) {
  if (dam > 0 && power_level > 1) {
   clamp_lb<0>(dam -= rng(1, 8));
   power_level--;
  }
  if (cut > 0 && power_level > 1) {
   clamp_lb<0>(cut -= rng(0, 4));
   power_level--;
  }
  if (0 >= dam && 0 >= cut) return;
 }

 const auto damage_reduction = armor(bp, *this); // destroyed armor still protects one last time

// See, we do it backwards, which assumes the player put on their jacket after
//  their T shirt, for example.  TODO: don't assume! ASS out of U & ME, etc.
 for (int i = worn.size() - 1; i >= 0; i--) {
  const it_armor* const tmp = dynamic_cast<const it_armor*>(worn[i].type);
  static auto is_gone = [&]() {
      return grammar::capitalize(possessive()) + " " + worn[i].tname() + " is destroyed!";
  };

  if ((tmp->covers & mfb(bp)) && tmp->storage < 20) { // \todo? trenchcoats are too large to damage with weapons?!?
// Wool, leather, and cotton clothing may be damaged by CUTTING damage
   if ((worn[i].made_of(WOOL)   || worn[i].made_of(LEATHER) ||
        worn[i].made_of(COTTON) || worn[i].made_of(GLASS)   ||
        worn[i].made_of(WOOD)   || worn[i].made_of(KEVLAR)) &&
       rng(0, tmp->cut_resist * 2) < cut && !one_in(cut))
    worn[i].damage++;
// Kevlar, plastic, iron, steel, and silver may be damaged by BASHING damage
   if ((worn[i].made_of(PLASTIC) || worn[i].made_of(IRON)   ||
        worn[i].made_of(STEEL)   || worn[i].made_of(SILVER) ||
        worn[i].made_of(STONE))  &&
       rng(0, tmp->dmg_resist * 2) < dam && !one_in(dam))
    worn[i].damage++;
   if (worn[i].damage >= 5) {
    if_visible_message(is_gone, is_gone);
    EraseAt(worn, i);
   }
  }
 }

 clamp_lb<0>(dam -= damage_reduction.first);
 clamp_lb<0>(cut -= damage_reduction.second);
 if (0 >= dam /* && 0 >= cut */ ) return;

 if (auto weak = has_light_bones()) dam *= mutation_branch::light_bones_damage[weak - 1];
}
  
int player::resist(body_part bp) const
{
 int ret = 0;
 for (const auto& it : worn) {
  const it_armor* const armor = dynamic_cast<const it_armor*>(it.type);
  if (armor->covers & mfb(bp) ||
      (bp == bp_eyes && (armor->covers & mfb(bp_head)))) // Head protection works on eyes too (e.g. baseball cap)
   ret += armor->env_resist;
 }
 if (bp == bp_mouth && has_bionic(bio_purifier) && ret < 5) {
  ret += 2;
  if (ret == 6) ret = 5;
 }
 return ret;
}

bool player::wearing_something_on(body_part bp) const
{
 for (const auto& it : worn) {
  if ((dynamic_cast<const it_armor*>(it.type))->covers & mfb(bp)) return true;
 }
 return false;
}

void player::practice(skill s, int amount)
{
 skill savant = sk_null;
 int savant_level = 0, savant_exercise = 0;
 if (skexercise[s] < 0)
  amount += (amount >= -1 * skexercise[s] ? -1 * skexercise[s] : amount);
 if (has_trait(PF_SAVANT)) {
// Find our best skill
  for (int i = 1; i < num_skill_types; i++) {
   if (sklevel[i] >= savant_level) {
    savant = skill(i);
    savant_level = sklevel[i];
    savant_exercise = skexercise[i];
   } else if (sklevel[i] == savant_level && skexercise[i] > savant_exercise) {
    savant = skill(i);
    savant_exercise = skexercise[i];
   }
  }
 }
 while (amount > 0 && xp_pool >= (1 + sklevel[s])) {
  amount -= sklevel[s] + 1;
  if ((savant == sk_null || savant == s || !one_in(2)) &&
      rng(0, 100) < comprehension_percent(s)) {
   xp_pool -= (1 + sklevel[s]);
   skexercise[s]++;
  }
 }
}

// NPC AI should be trying to learn skills, before exposing them to this.
void player::update_skills()
{
    static const std::string Your_skill_in("Your skill in ");

    //  SKILL   TURNS/--
    //	1	4096
    //	2	2048
    //	3	1024
    //	4	 512
    //	5	 256
    //	6	 128
    //	7+	  64
    for (int i = 0; i < num_skill_types; i++) {
        int tmp = sklevel[i] > 7 ? 7 : sklevel[i];
        if (sklevel[i] > 0 && messages.turn % (8192 / int(pow(2, double(tmp - 1)))) == 0 &&
            (one_in(has_trait(PF_FORGETFUL) ? 3 : 4))) {
            if (has_bionic(bio_memory) && power_level > 0) {
                if (one_in(5)) power_level--;
            } else
                skexercise[i]--;
        }
        if (skexercise[i] < -100) {
            sklevel[i]--;
            subjective_message(Your_skill_in + skill_name(skill(i)) +" has reduced to "+ std::to_string(sklevel[i])  +"!");
            skexercise[i] = 0;
        } else if (skexercise[i] >= 100) {
            sklevel[i]++;
            subjective_message(Your_skill_in + skill_name(skill(i)) +" has increased to "+ std::to_string(sklevel[i]) +"!");
            skexercise[i] = 0;
        }
    }
}

void player::assign_activity(activity_type type, int moves, int index)
{
 if (backlog.type == type && backlog.index == index && query_yn("Resume task?")) {
  activity = std::move(backlog);
  backlog.clear();
 } else
  activity = player_activity(type, moves, index);
}

void player::assign_activity(activity_type type, int moves, const point& pt, int index)
{
    assert(map::in_bounds(pt));
    assert(1 >= rl_dist(pos, pt));
    GPS_loc dest = game::active()->toGPS(pt);
    if (backlog.type == type && backlog.index == index && (dest == backlog.gps_placement || pt == backlog.placement) && query_yn("Resume task?")) {
        activity = std::move(backlog);
        backlog.clear();
    } else {
        activity = player_activity(type, moves, index);
        activity.placement = pt;
        activity.gps_placement = dest;
    }
}

static bool activity_is_suspendable(activity_type type)
{
	if (type == ACT_NULL || type == ACT_RELOAD) return false;
	return true;
}

void player::cancel_activity()
{
 if (activity_is_suspendable(activity.type)) backlog = std::move(activity);
 activity.clear();
}

void player::cancel_activity_query(const char* message, ...)
{
    if (reject_not_whitelisted_printf(message)) return;
    char buff[1024];
	va_list ap;
	va_start(ap, message);
#ifdef _MSC_VER
	vsprintf_s<sizeof(buff)>(buff, message, ap);
#else
	vsnprintf(buff, sizeof(buff), message, ap);
#endif
	va_end(ap);

	bool doit = false;

	switch (activity.type) {
	case ACT_NULL: return;
	case ACT_READ:
		if (query_yn("%s Stop reading?", buff)) doit = true;
		break;
	case ACT_RELOAD:
		if (query_yn("%s Stop reloading?", buff)) doit = true;
		break;
	case ACT_CRAFT:
		if (query_yn("%s Stop crafting?", buff)) doit = true;
		break;
	case ACT_BUTCHER:
		if (query_yn("%s Stop butchering?", buff)) doit = true;
		break;
	case ACT_BUILD:
	case ACT_VEHICLE:
		if (query_yn("%s Stop construction?", buff)) doit = true;
		break;
	case ACT_TRAIN:
		if (query_yn("%s Stop training?", buff)) doit = true;
		break;
	default:
		doit = true;
	}

	if (doit) cancel_activity();
}

void player::accept(mission* const miss)
{
    assert(miss);
    try {
        (*miss->type->start)(game::active(), miss);
    } catch (const std::string& e) {
        debugmsg(e.c_str());
        return;
    }
    active_missions.push_back(miss->uid);
    active_mission = active_missions.size() - 1;
}

void player::fail(const mission& miss)
{
    auto i = active_missions.size();
    while (0 < i) {
        if (active_missions[--i] == miss.uid) {
            EraseAt(active_missions, i);
            failed_missions.push_back(miss.uid);   // 2020-01-11 respecify: cannot fail a mission that isn't accepted
        }
    }
}

void player::wrap_up(mission* const miss)
{
    assert(miss);
    auto i = active_missions.size();
    while (0 < i) {
        if (active_missions[--i] == miss->uid) {
            EraseAt(active_missions, i);
            completed_missions.push_back(miss->uid);    // respecified 2020-01-12: can't openly complete a mission we haven't taken
        }
    }

    switch (miss->type->goal) {
    case MGOAL_FIND_ITEM:
        use_amount(miss->type->item_id, 1);
        break;
    case MGOAL_FIND_ANY_ITEM:
        remove_mission_items(miss->uid);
        break;
    }
    (*miss->type->end)(game::active(), miss);
}


bool player::has_ammo(ammotype at) const
{
 bool newtype = true;
 for (size_t a = 0; a < inv.size(); a++) {
     if (const auto ammo = inv[a].is_ammo()) {
         if (at == ammo->type) return true;
     }
 }
 return false;
}

std::vector<int> player::have_ammo(ammotype at) const
{
    std::vector<int> ret;
    bool newtype = true;
    for (size_t a = 0; a < inv.size(); a++) {
        if (const auto ammo = inv[a].is_ammo()) {
            if (ammo->type != at) continue;
            bool newtype = true;
            for (const auto n : ret) {
                const auto& it = inv[n];
                if (ammo->id == it.type->id && inv[a].charges == it.charges) {
                    // They're effectively the same; don't add it to the list
                    // TODO: Bullets may become rusted, etc., so this if statement may change
                    newtype = false;
                    break;
                }
            }
            if (newtype) ret.push_back(a);
        }
    }
    return ret;
}

std::string player::weapname(bool charges) const
{
 if (!(weapon.is_tool() && dynamic_cast<const it_tool*>(weapon.type)->max_charges <= 0) && weapon.charges >= 0 && charges) {
  std::ostringstream dump;
  dump << weapon.tname().c_str() << " (" << weapon.charges << ")";
  return dump.str();
 } else if (weapon.is_null()) return "fists";

 else if (weapon.is_style()) { // Styles get bonus-bars!
  std::ostringstream dump;
  dump << weapon.tname();

  switch (weapon.type->id) {
   case itm_style_capoeira:
    if (has_disease(DI_DODGE_BOOST)) dump << " +Dodge";
    if (has_disease(DI_ATTACK_BOOST)) dump << " +Attack";
    break;

   case itm_style_ninjutsu:
   case itm_style_leopard:
    if (has_disease(DI_ATTACK_BOOST)) dump << " +Attack";
    break;

   case itm_style_crane:
    if (has_disease(DI_DODGE_BOOST)) dump << " +Dodge";
    break;

   case itm_style_dragon:
    if (has_disease(DI_DAMAGE_BOOST)) dump << " +Damage";
    break;

   case itm_style_tiger: {
    dump << " [";
    int intensity = disease_intensity(DI_DAMAGE_BOOST);
    for (int i = 1; i <= 5; i++) {
     if (intensity >= i * 2) dump << "*";
     else dump << ".";
    }
    dump << "]";
   } break;

   case itm_style_centipede: {
    dump << " [";
    int intensity = disease_intensity(DI_SPEED_BOOST);
    for (int i = 1; i <= 8; i++) {
     if (intensity >= i * 4) dump << "*";
     else dump << ".";
    }
    dump << "]";
   } break;

   case itm_style_venom_snake: {
    dump << " [";
    int intensity = disease_intensity(DI_VIPER_COMBO);
    for (int i = 1; i <= 2; i++) {
     if (intensity >= i) dump << "C";
     else dump << ".";
    }
    dump << "]";
   } break;

   case itm_style_lizard: {
    dump << " [";
    int intensity = disease_intensity(DI_ATTACK_BOOST);
    for (int i = 1; i <= 4; i++) {
     if (intensity >= i) dump << "*";
     else dump << ".";
    }
    dump << "]";
   } break;

   case itm_style_toad: {
    dump << " [";
    int intensity = disease_intensity(DI_ARMOR_BOOST);
    for (int i = 1; i <= 5; i++) {
     if (intensity >= 5 + i) dump << "!";
     else if (intensity >= i) dump << "*";
     else dump << ".";
    }
    dump << "]";
   } break;
  } // switch (weapon.type->id)
  return dump.str();
 } else
  return weapon.tname();
}

static bool file_to_string_vector(const char* src, std::vector<std::string>& dest)
{
	if (!src || !src[0]) return false;
	std::ifstream fin;
	fin.exceptions(std::ios::badbit);	// throw on hardware failure
	fin.open(src);
	if (!fin.is_open()) {
		debugmsg((std::string("Could not open ")+std::string(src)).c_str());
		return false;
	}
	std::string x;
	do {
		getline(fin, x);
		if (!x.empty()) dest.push_back(x);
	} while (!fin.eof());
	fin.close();
	return !dest.empty();
}

std::string random_first_name(bool male)
{
	static std::vector<std::string> mr;
	static std::vector<std::string> mrs;
	static bool have_tried_to_read_mr_file = false;
	static bool have_tried_to_read_mrs_file = false;

	std::vector<std::string>& first_names = male ? mr : mrs;
	if (!first_names.empty()) return first_names[rng(0, first_names.size() - 1)];
	if (male ? have_tried_to_read_mr_file : have_tried_to_read_mrs_file) return "";
	if (male) have_tried_to_read_mr_file = true;
	else have_tried_to_read_mrs_file = true;

	if (!file_to_string_vector(male ? "data/NAMES_MALE" : "data/NAMES_FEMALE", first_names)) return "";
	return first_names[rng(0, first_names.size() - 1)];
}

std::string random_last_name()
{
 static std::vector<std::string> last_names;
 static bool have_tried_to_read_file = false;

 if (!last_names.empty()) return last_names[rng(0, last_names.size() - 1)];
 if (have_tried_to_read_file) return "";
 have_tried_to_read_file = true;
 if (!file_to_string_vector("data/NAMES_LAST", last_names)) return "";
 return last_names[rng(0, last_names.size() - 1)];
}
