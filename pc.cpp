#include "pc.hpp"
#include "monster.h"
#include "npc.h"
#include "mondeath.h"
#include "trap.h"
#include "submap.h"
#include "output.h"
#include "ui.h"
#include "stl_limits.h"
#include "stl_typetraits.h"
#include "options.h"
#include "recent_msg.h"
#include "rng.h"
#include "line.h"
#include "Zaimoni.STL/GDI/box.hpp"
#include "Zaimoni.STL/functional.hpp"
#include "fragment.inc/rng_box.hpp"
#include "wrap_curses.h"
#include "Zaimoni.STL/Logging.h"
#include <array>
#include <memory>
#include <stdexcept>

pc::pc()
: kills(num_monsters, 0), mostseen(0), turnssincelastmon(0), run_mode(option_table::get()[OPT_SAFEMODE] ? 1 : 0), autosafemode(option_table::get()[OPT_AUTOSAFEMODE]), target(-1), next_inv('d')
{
}

// Intentionally thin wrapper to force correct type for handler call.
void pc::consume(item& food) {
    if (const auto comest = food.is_food()) comest->consumed_by(food, *this);
    else throw std::logic_error("called player::consume on non-food");
}

bool pc::is_enemy(const player* survivor) const
{
    if (!survivor) return false;
    if (auto _npc = dynamic_cast<const npc*>(survivor)) {
        return _npc->is_enemy(this);
    }
    return false; // \todo proper multi-PC would support hostile PCs
}

void pc::subjective_message(const std::string& msg) const { if (!msg.empty()) messages.add(msg); }

bool pc::if_visible_message(std::function<std::string()> msg) const
{
    if (msg) {
        subjective_message(msg());
        return true;
    }
    return false;
}

bool pc::if_visible_message(const char* msg) const
{
    if (msg) {
        subjective_message(msg);
        return true;
    }
    return false;
}

bool pc::ask_yn(const char* msg, std::function<bool()> ai) const { return query_yn(msg); }

item pc::i_rem(char let)
{
    item tmp;
    if (weapon.invlet == let) {
        if (weapon.type->id > num_items && weapon.type->id < num_all_items) return item::null;
        tmp = std::move(weapon);
        weapon = item::null;
        return tmp;
    }
    for (int i = 0; i < worn.size(); i++) {
        if (worn[i].invlet == let) {
            tmp = std::move(worn[i]);
            EraseAt(worn, i);
            return tmp;
        }
    }
    return inv.remove_item_by_letter(let);
}

/// <returns>0: no-op; -1 charger gun; -2 artifact; 1 now null item</returns>
int pc::use_active(item& it) {
    const auto tool = it.is_tool();
    if (!tool) {
        if (!it.active) return 0;
        if (it.has_flag(IF_CHARGE)) return -1;  // process as charger gun
        it.active = false;  // restore invariant; for debugging purposes we'd use accessors and intercept on set
        return 0;
    }
    if (it.is_artifact()) return -2;
    if (!it.active) return 0;

    try {
        tool->used_by(it, *this);
    }
    catch (const std::string& e) {
        messages.add(e);
        return 0;
    }

    if (tool->turns_per_charge > 0 && int(messages.turn) % tool->turns_per_charge == 0) it.charges--;
    // separate this so we respond to bugs reasonably
    if (it.charges <= 0) {
        tool->turned_off_by(it, *this);
        if (tool->revert_to == itm_null) {
            it = item::null;
            return 1;
        }
        it.type = item::types[tool->revert_to];
    }
    return 0;
}

// 2019-02-21: C:Whales: only the player may disarm traps \todo allow NPCs
void pc::disarm(GPS_loc loc)
{
    decltype(auto) tr_id = loc.trap_at();
    const trap* const tr = trap::traps[tr_id];
    assert(tr->disarm_legal());
    const int diff = tr->difficulty;
    int roll = rng(sklevel[sk_traps], 4 * sklevel[sk_traps]);
    while ((rng(5, 20) < per_cur || rng(1, 20) < dex_cur) && roll < 50) roll++;

    if (roll < cataclysm::rational_scaled<4, 5>(diff)) {
        const bool will_get_xp = (diff - roll <= 6);
        messages.add(will_get_xp ? "You barely fail to disarm the trap, and you set it off!"
            : "You fail to disarm the trap, and you set it off!");
        tr->trigger(*this, loc);

        // Give xp for failing, but not if we failed terribly (in which
        // case the trap may not be disarmable).
        if (will_get_xp) practice(sk_traps, 2 * diff);
        return;
    }

    // Learning is exciting and worth emphasis.  Skill must be no more than 80% of difficulty to learn.
    const bool will_get_xp = (diff > cataclysm::rational_scaled<5, 4>(sklevel[sk_traps]));

    if (roll >= diff) {
        messages.add(will_get_xp ? "You disarm the trap!" : "You disarm the trap.");
        for (const auto item_id : tr->disarm_components) {
            if (item_id != itm_null) {
                if (auto it = submap::for_drop(loc.ter(), item::types[item_id], 0)) loc.add(std::move(*it));
            }
        }
        tr_id = tr_null;
    }
    else {
        messages.add(will_get_xp ? "You fail to disarm the trap!" : "You fail to disarm the trap.");
    }
    if (will_get_xp) practice(sk_traps, cataclysm::rational_scaled<3, 2>(diff - sklevel[sk_traps]));
}

std::string pc::pronoun(role r) const
{
    switch (r)
    {
    case noun::role::possessive: return "your";
    default: return "you";
    }
}

char pc::inc_invlet(char src)
{
    switch (src) {
        case 'z': return 'A';
        case 'Z': return 'a';
        default: return ++src;
    };
}

char pc::dec_invlet(char src)
{
    switch (src) {
        case 'a': return 'Z';
        case 'A': return 'z';
        default: return --src;
    };
}

// cf. ...::from_invlet; ignores weapon and worn armors
std::optional<player::item_spec_const> pc::has_in_inventory(char let) const
{
    if (KEY_ESCAPE == let) return std::nullopt;
    int index = inv.index_by_letter(let);
    if (0 > index) return std::nullopt;
    return std::pair(&inv[index], index);
}

bool pc::wear(char let)
{
    auto wear_this = from_invlet(let);
    if (!wear_this) {
        messages.add("You don't have item '%c'.", let);
        return false;
    }
    else if (-2 > wear_this->second) {
        messages.add("You are already wearing item '%c'.", let);
        return false;
    }

    return player::wear(*wear_this);
}

bool pc::assign_invlet(item& it) const
{
    // This while loop guarantees the inventory letter won't be a repeat. If it
    // tries all 52 letters, it fails.
    int iter = 0;
    while (has_in_inventory(next_inv)) {
        next_inv = inc_invlet(next_inv);
        if (52 <= ++iter) return false;
    }
    it.invlet = next_inv;
    return true;
}

bool pc::assign_invlet_stacking_ok(item& it) const
{
    // This while loop guarantees the inventory letter won't be a repeat. If it
    // tries all 52 letters, it fails.
    int iter = 0;
    auto dest = has_in_inventory(next_inv);
    while (dest && !dest->first->stacks_with(it)) {
        next_inv = inc_invlet(next_inv);
        dest = has_in_inventory(next_inv);
        if (52 <= ++iter) return false;
    }
    it.invlet = next_inv;
    return true;
}

void pc::reassign_item()
{
    char ch = get_invlet("Reassign item:");
    if (ch == KEY_ESCAPE) {
        messages.add("Never mind.");
        return;
    }
    auto change_from = from_invlet(ch);
    if (!change_from) {
        messages.add("You do not have that item.");
        return;
    }
    char newch = popup_getkey("%c - %s; enter new letter.", ch, change_from->first->tname().c_str());
    if ((newch < 'A' || (newch > 'Z' && newch < 'a') || newch > 'z')) {
        messages.add("%c is not a valid inventory letter.", newch);
        return;
    }
    if (const auto change_to = from_invlet(newch)) { // if new letter already exists, swap it
        change_to->first->invlet = ch;
        messages.add("%c - %s", ch, change_to->first->tname().c_str());
    }
    change_from->first->invlet = newch;
    messages.add("%c - %s", newch, change_from->first->tname().c_str());
}

static const std::string CATEGORIES[8] =
{ "FIREARMS:", "AMMUNITION:", "CLOTHING:", "COMESTIBLES:",
 "TOOLS:", "BOOKS:", "WEAPONS:", "OTHER:" };

static void print_inv_statics(const pc& u, WINDOW* w_inv, std::string title, const std::vector<char>& dropped_items)
{
    // Print our header
    mvwprintw(w_inv, 0, 0, title.c_str());

    const int mid_pt = SCREEN_WIDTH / 2;
    const int three_quarters = 3 * SCREEN_WIDTH / 4;

    // Print weight
    mvwprintw(w_inv, 0, mid_pt, "Weight: ");
    const int my_w_capacity = u.weight_capacity();
    const int my_w_carried = u.weight_carried();
    wprintz(w_inv, (my_w_carried >= my_w_capacity / 4) ? c_red : c_ltgray, "%d", my_w_carried);
    wprintz(w_inv, c_ltgray, "/%d/%d", my_w_capacity / 4, my_w_capacity);

    // Print volume
    mvwprintw(w_inv, 0, three_quarters, "Volume: ");
    const int my_v_capacity = u.volume_capacity() - 2;
    const int my_v_carried = u.volume_carried();
    wprintz(w_inv, (my_v_carried > my_v_capacity) ? c_red : c_ltgray, "%d", my_v_carried);
    wprintw(w_inv, "/%d", my_v_capacity);

    // Print our weapon
    mvwaddstrz(w_inv, 2, mid_pt, c_magenta, "WEAPON:");
    if (u.is_armed()) {
        const bool dropping_weapon = cataclysm::any(dropped_items, u.weapon.invlet);
        if (dropping_weapon)
            mvwprintz(w_inv, 3, mid_pt, c_white, "%c + %s", u.weapon.invlet, u.weapname().c_str());
        else
            mvwprintz(w_inv, 3, mid_pt, u.weapon.color_in_inventory(u), "%c - %s", u.weapon.invlet, u.weapname().c_str());
    }
    else if (u.weapon.is_style())
        mvwprintz(w_inv, 3, mid_pt, c_ltgray, "%c - %s", u.weapon.invlet, u.weapname().c_str());
    else
        mvwaddstrz(w_inv, 3, mid_pt + 2, c_ltgray, u.weapname().c_str());
    // Print worn items
    if (!u.worn.empty()) {
        mvwaddstrz(w_inv, 5, mid_pt, c_magenta, "ITEMS WORN:");
        int row = 6;
        for (const item& armor : u.worn) {
            const bool dropping_armor = cataclysm::any(dropped_items, armor.invlet);
            mvwprintz(w_inv, row++, mid_pt, (dropping_armor ? c_white : c_ltgray), "%c + %s", armor.invlet, armor.tname().c_str());
        }
    }
}

static auto find_firsts(const inventory& inv)
{
    std::array<int, 8> firsts;
    firsts.fill(-1);

    for (size_t i = 0; i < inv.size(); i++) {
        if (firsts[0] == -1 && inv[i].is_gun())
            firsts[0] = i;
        else if (firsts[1] == -1 && inv[i].is_ammo())
            firsts[1] = i;
        else if (firsts[2] == -1 && inv[i].is_armor())
            firsts[2] = i;
        else if (firsts[3] == -1 &&
            (inv[i].is_food() || inv[i].is_food_container()))
            firsts[3] = i;
        else if (firsts[4] == -1 && (inv[i].is_tool() || inv[i].is_gunmod() ||
            inv[i].is_bionic()))
            firsts[4] = i;
        else if (firsts[5] == -1 && inv[i].is_book())
            firsts[5] = i;
        else if (firsts[6] == -1 && inv[i].is_weap())
            firsts[6] = i;
        else if (firsts[7] == -1 && inv[i].is_other())
            firsts[7] = i;
    }

    return firsts;
}

// C:Whales: game::inv
char pc::get_invlet(std::string title)
{
    static const std::vector<char> null_vector;
    const int maxitems = VIEW - 5;	// Number of items to show at one time.
    int ch = '.';
    int start = 0, cur_it;
    sort_inv();
    inv.restack(this);
    {
    std::unique_ptr<WINDOW, curses_full_delete> w_inv(newwin(VIEW, SCREEN_WIDTH, 0, 0));
    print_inv_statics(*this, w_inv.get(), title, null_vector);
    // Gun, ammo, weapon, armor, food, tool, book, other
    const auto firsts = find_firsts(inv);

    do {
        if (ch == '<' && start > 0) { // Clear lines and shift
            for (int i = 1; i < VIEW; i++) draw_hline(w_inv.get(), i, c_black, ' ', 0, SCREEN_WIDTH / 2);
            start -= maxitems;
            if (start < 0) start = 0;
            mvwprintw(w_inv.get(), maxitems + 2, 0, "         ");
        }
        if (ch == '>' && cur_it < inv.size()) { // Clear lines and shift
            start = cur_it;
            mvwprintw(w_inv.get(), maxitems + 2, 12, "            ");
            for (int i = 1; i < VIEW; i++) draw_hline(w_inv.get(), i, c_black, ' ', 0, SCREEN_WIDTH / 2);
        }
        int cur_line = 2;
        for (cur_it = start; cur_it < start + maxitems && cur_line < 23; cur_it++) {
            // Clear the current line;
            mvwprintw(w_inv.get(), cur_line, 0, "                                    ");
            // Print category header
            for (int i = 0; i < 8; i++) {
                if (cur_it == firsts[i]) {
                    mvwaddstrz(w_inv.get(), cur_line, 0, c_magenta, CATEGORIES[i].c_str());
                    cur_line++;
                }
            }
            if (cur_it < inv.size()) {
                const item& it = inv[cur_it];
                mvwputch(w_inv.get(), cur_line, 0, c_white, it.invlet);
                mvwaddstrz(w_inv.get(), cur_line, 1, it.color_in_inventory(*this), it.tname().c_str());
                if (inv.stack_at(cur_it).size() > 1)
                    wprintw(w_inv.get(), " [%ld]", inv.stack_at(cur_it).size());
                if (it.charges > 0)
                    wprintw(w_inv.get(), " (%d)", it.charges);
                else if (it.contents.size() == 1 && it.contents[0].charges > 0)
                    wprintw(w_inv.get(), " (%d)", it.contents[0].charges);
            }
            cur_line++;
        }
        if (start > 0)
            mvwprintw(w_inv.get(), maxitems + 4, 0, "< Go Back");
        if (cur_it < inv.size())
            mvwprintw(w_inv.get(), maxitems + 4, 12, "> More items");
        wrefresh(w_inv.get());
        ch = getch();
    } while (ch == '<' || ch == '>');
    } // force destruction of std::unique_ptr
    refresh_all();
    return ch;
}

std::optional<player::item_spec> pc::choose(const char* prompt, std::function<std::optional<std::string>(const item_spec&)> fail)
{
    if (auto ret = from_invlet(get_invlet(prompt))) {
        if (auto err = fail(*ret)) {
            messages.add(*err);
            return std::nullopt;
        }
        return ret;
    }
    return std::nullopt;
}

// C:Whales: game::multidrop
std::vector<item> pc::multidrop()
{
    sort_inv();
    inv.restack(this);
    WINDOW* w_inv = newwin(VIEW, SCREEN_WIDTH, 0, 0);
    const int maxitems = VIEW - 5; // Number of items to show at one time.
    std::vector<int> dropping(inv.size(), 0);
    int count = 0; // The current count
    std::vector<char> weapon_and_armor; // Always single, not counted
    bool warned_about_bionic = false; // Printed add_msg re: dropping bionics
    print_inv_statics(*this, w_inv, "Multidrop:", weapon_and_armor);
    // Gun, ammo, weapon, armor, food, tool, book, other
    const auto firsts = find_firsts(inv);

    int ch = '.';
    int start = 0;
    size_t cur_it = 0;
    do {
        if (ch == '<' && start > 0) {
            for (int i = 1; i < VIEW; i++) draw_hline(w_inv, i, c_black, ' ', 0, SCREEN_WIDTH / 2);
            start -= maxitems;
            if (start < 0) start = 0;
            mvwprintw(w_inv, maxitems + 2, 0, "         ");
        }
        if (ch == '>' && cur_it < inv.size()) {
            start = cur_it;
            mvwprintw(w_inv, maxitems + 2, 12, "            ");
            for (int i = 1; i < VIEW; i++) draw_hline(w_inv, i, c_black, ' ', 0, SCREEN_WIDTH / 2);
        }
        int cur_line = 2;
        for (cur_it = start; cur_it < start + maxitems && cur_line < VIEW - 2; cur_it++) {
            // Clear the current line;
            mvwprintw(w_inv, cur_line, 0, "                                    ");
            // Print category header
            for (int i = 0; i < 8; i++) {
                if (cur_it == firsts[i]) {
                    mvwaddstrz(w_inv, cur_line, 0, c_magenta, CATEGORIES[i].c_str());
                    cur_line++;
                }
            }
            if (cur_it < inv.size()) {
                const item& it = inv[cur_it];
                mvwputch(w_inv, cur_line, 0, c_white, it.invlet);
                char icon = '-';
                if (dropping[cur_it] >= inv.stack_at(cur_it).size())
                    icon = '+';
                else if (dropping[cur_it] > 0)
                    icon = '#';
                nc_color col = (dropping[cur_it] == 0 ? c_ltgray : c_white);
                mvwprintz(w_inv, cur_line, 1, col, " %c %s", icon, it.tname().c_str());
                if (inv.stack_at(cur_it).size() > 1)
                    wprintz(w_inv, col, " [%d]", inv.stack_at(cur_it).size());
                if (it.charges > 0) wprintz(w_inv, col, " (%d)", it.charges);
                else if (it.contents.size() == 1 && it.contents[0].charges > 0)
                    wprintw(w_inv, " (%d)", it.contents[0].charges);
            }
            cur_line++;
        }
        if (start > 0) mvwprintw(w_inv, maxitems + 4, 0, "< Go Back");
        if (cur_it < inv.size()) mvwprintw(w_inv, maxitems + 4, 12, "> More items");
        wrefresh(w_inv);
        ch = getch();
        if (ch >= '0' && ch <= '9') {
            ch -= '0';
            count *= 10;
            count += ch;
        }
        else if (auto obj = from_invlet(ch)) {
            if (0 > obj->second) { // Not from inventory
                int found = false;
                for (int i = 0; i < weapon_and_armor.size() && !found; i++) {
                    if (weapon_and_armor[i] == ch) {
                        EraseAt(weapon_and_armor, i);
                        found = true;
                        print_inv_statics(*this, w_inv, "Multidrop:", weapon_and_armor);
                    }
                }
                if (!found) {
                    if (ch == weapon.invlet && is_between(num_items + 1, weapon.type->id, num_all_items - 1)) {
                        if (!warned_about_bionic)
                            messages.add("You cannot drop your %s.", weapon.tname().c_str());
                        warned_about_bionic = true;
                    }
                    else {
                        weapon_and_armor.push_back(ch);
                        print_inv_statics(*this, w_inv, "Multidrop:", weapon_and_armor);
                    }
                }
            }
            else {
                const int index = obj->second;
                const size_t ub = inv.stack_at(index).size();
                if (count == 0) {
                    dropping[index] = (0 == dropping[index]) ? ub : 0;
                }
                else {
                    dropping[index] = (count >= ub) ? ub : count;
                }
            }
            count = 0;
        }
    } while (ch != '\n' && ch != KEY_ESCAPE && ch != ' ');
    werase(w_inv);
    delwin(w_inv);
    erase();
    refresh_all();

    std::vector<item> ret;

    if (ch != '\n') return ret; // Canceled!

    int current_stack = 0;
    size_t max_size = inv.size();
    for (size_t i = 0; i < max_size; i++) {
        for (int j = 0; j < dropping[i]; j++) {
            if (current_stack >= 0) {
                if (inv.stack_at(current_stack).size() == 1) {
                    ret.push_back(inv.remove_item(current_stack));
                    current_stack--;
                }
                else
                    ret.push_back(inv.remove_item(current_stack));
            }
        }
        current_stack++;
    }

    for (int i = 0; i < weapon_and_armor.size(); i++)
        ret.push_back(i_rem(weapon_and_armor[i]));

    return ret;
}

void pc::use(char let)
{
    const auto src = from_invlet(let);
    if (!src) {
        messages.add("You do not have that item.");
        return;
    }

    item* used = src->first; // backward compatibility
    last_item = itype_id(used->type->id);

    if (const auto tool = used->is_tool()) {
        if (auto err = tool->cannot_use(*used, *this)) {
            messages.add(*err);
            return;
        }

        try {
            tool->used_by(*used, *this);
        } catch (const std::string& e) {
            messages.add(e);
            return;
        }
        used->charges -= tool->charges_per_use;

        if (0 == used->invlet) remove_discard(*src);
        return;
    }

    if (const auto mod = used->is_gunmod()) {
        if (sklevel[sk_gun] == 0) {
            messages.add("You need to be at least level 1 in the firearms skill before you can modify guns.");
            return;
        }
        char gunlet = get_invlet("Select gun to modify:");
        const auto src_gun = from_invlet(gunlet);
        if (!src_gun) {
            messages.add("You do not have that item.");
            return;
        }

        item& gun = *(src_gun->first); // backward compatibility
        const auto guntype = gun.is_gun();
        if (!guntype) {
            messages.add("That %s is not a gun.", gun.tname().c_str());
            return;
        }

        switch (guntype->skill_used)
        {
        case sk_pistol:
            if (!mod->used_on_pistol) {
                messages.add("That %s cannot be attached to a handgun.", used->tname().c_str());
                return;
            }
            break;
        case sk_shotgun:
            if (!mod->used_on_shotgun) {
                messages.add("That %s cannot be attached to a shotgun.", used->tname().c_str());
                return;
            }
            break;
        case sk_smg:
            if (!mod->used_on_smg) {
                messages.add("That %s cannot be attached to a submachine gun.", used->tname().c_str());
                return;
            }
            break;
        case sk_rifle:
            if (!mod->used_on_rifle) {
                messages.add("That %s cannot be attached to a rifle.", used->tname().c_str());
                return;
            }
            break;
        default: // sk_archery, sk_launcher
            messages.add("You cannot mod your %s.", gun.tname().c_str());
            return;
        }

        if (mod->acceptable_ammo_types != 0 &&
            !(mfb(guntype->ammo) & mod->acceptable_ammo_types)) {
            messages.add("That %s cannot be used on a %s gun.", used->tname().c_str(), ammo_name(guntype->ammo).c_str());
            return;
        } else if (gun.contents.size() >= 4) {
            messages.add("Your %s already has 4 mods installed!  To remove the mods, press 'U' while wielding the unloaded gun.", gun.tname().c_str());
            return;
        }
        if ((mod->id == itm_clip || mod->id == itm_clip2) && gun.clip_size() <= 2) {
            messages.add("You can not extend the ammo capacity of your %s.", gun.tname().c_str());
            return;
        }
        for (const auto& it : gun.contents) {
            if (it.type->id == used->type->id) {
                messages.add("Your %s already has a %s.", gun.tname().c_str(), used->tname().c_str());
                return;
            } else if (mod->newtype != AT_NULL &&
                (dynamic_cast<const it_gunmod*>(it.type))->newtype != AT_NULL) {
                messages.add("Your %s's caliber has already been modified.", gun.tname().c_str());
                return;
            } else if ((mod->id == itm_barrel_big || mod->id == itm_barrel_small) &&
                (it.type->id == itm_barrel_big ||
                    it.type->id == itm_barrel_small)) {
                messages.add("Your %s already has a barrel replacement.", gun.tname().c_str());
                return;
            } else if ((mod->id == itm_clip || mod->id == itm_clip2) &&
                (it.type->id == itm_clip ||
                    it.type->id == itm_clip2)) {
                messages.add("Your %s already has its clip size extended.", gun.tname().c_str());
                return;
            }
        }
        messages.add("You attach the %s to your %s.", used->tname().c_str(), gun.tname().c_str());
        gun.contents.push_back(std::move(*used));
        remove_discard(*src);
        return;
    }

    if (const auto bionic = used->is_bionic()) {
        if (install_bionics(bionic)) remove_discard(*src);
        return;
    }

    if (used->is_food() || used->is_food_container()) {
        eat(*src);
        return;
    }

    if (used->is_book()) {
        read(let);
        return;
    }

    if (used->is_armor()) {
        wear(let);
        return;
    }

    messages.add("You can't do anything interesting with your %s.", used->tname().c_str());
}

void pc::complete_butcher()
{
    static const int pelts_from_corpse[mtype::MS_MAX] = { 1, 3, 6, 10, 18 };
    int index = activity.index;

    decltype(auto) items = GPSpos.items_at();
    item& it = items[index];
    const mtype* const corpse = it.corpse;
    int age = it.bday;
    EraseAt(items, index);  // reference it dies here

    int factor = butcher_factor().value();
    int pelts = pelts_from_corpse[corpse->size];
    double skill_shift = 0.;
    int pieces = corpse->chunk_count();
    if (sklevel[sk_survival] < 3)
        skill_shift -= rng(0, 8 - sklevel[sk_survival]);
    else
        skill_shift += rng(0, sklevel[sk_survival]);
    if (dex_cur < 8)
        skill_shift -= rng(0, 8 - dex_cur) / 4;
    else
        skill_shift += rng(0, dex_cur - 8) / 4;
    if (str_cur < 4) skill_shift -= rng(0, 5 * (4 - str_cur)) / 4;
    if (factor > 0) skill_shift -= rng(0, factor / 5);

    practice(sk_survival, clamped_ub<20>(4 + pieces));

    pieces += int(skill_shift);
    if (skill_shift < 5) pelts += (skill_shift - 5);	// Lose some pelts

    if ((corpse->has_flag(MF_FUR) || corpse->has_flag(MF_LEATHER)) && 0 < pelts) {
        messages.add("You manage to skin the %s!", corpse->name.c_str());
        for (int i = 0; i < pelts; i++) {
            const itype* pelt;
            if (corpse->has_flag(MF_FUR) && corpse->has_flag(MF_LEATHER)) {
                pelt = item::types[one_in(2) ? itm_fur : itm_leather];
            }
            else {
                pelt = item::types[corpse->has_flag(MF_FUR) ? itm_fur : itm_leather];
            }
            GPSpos.add(submap::for_drop(GPSpos.ter(), pelt, age).value());
        }
    }
    if (pieces <= 0) messages.add("Your clumsy butchering destroys the meat!");
    else {
        const itype* const meat = corpse->chunk_material();	// assumed non-null: precondition
        for (int i = 0; i < pieces; i++) GPSpos.add(submap::for_drop(GPSpos.ter(), meat, age).value());
        messages.add("You butcher the corpse.");
    }
}

// \todo lift to more logical location when needed
static std::optional<std::variant<const it_macguffin*, const it_book*> > is_readable(const item& it) {
    if (const auto mac = it.is_macguffin()) {
        if (mac->readable) return mac;
    }
    if (const auto book = it.is_book()) return book;
    return std::nullopt;
}

void pc::read(char ch)
{
    if (const auto err = cannot_read()) {
        subjective_message(*err);
        return;
    }

    // Find the object
    auto used = from_invlet(ch);

    if (!used) {
        messages.add("You do not have that item.");
        return;
    }

    const auto read_this = is_readable(*used->first);
    if (!read_this) {
        messages.add("Your %s is not good reading material.", used->first->tname().c_str());
        return;
    }

    if (const auto err = cannot_read(*read_this)) {
        subjective_message(*err);
        return;
    }

    static auto read_macguffin = [&](const it_macguffin* mac) {
        // Some macguffins can be read, but they aren't treated like books.
        mac->used_by(*used->first, *this);
    };

    static auto read_book = [&](const it_book* book) {
        // Base read_speed() is 1000 move points (1 minute per tmp->time)
        int time = book->time * read_speed();
        activity = player_activity(ACT_READ, time, used->second);
        moves = 0;
    };

    std::visit(zaimoni::handler<void, const it_macguffin*, const it_book*>(read_macguffin, read_book), *read_this);
}

bool pc::takeoff(char let)
{
    for (int i = 0; i < worn.size(); i++) {
        auto& it = worn[i];
        if (it.invlet == let) return take_off(i);
    }
    subjective_message("You are not wearing that item.");
    return false;
}

// add_footstep will create a list of locations to draw monster
// footsteps. these will be more or less accurate depending on the
// characters hearing and how close they are
void pc::add_footstep(const point& orig, int volume)
{
    if (orig == pos) return;
    else if (see(orig)) return;

    int distance = rl_dist(orig, pos);
    int err_offset;
    // \todo V 0.2.1 rethink this (effect is very loud, very close sounds don't have good precision
    if (volume / distance < 2)
        err_offset = 3;
    else if (volume / distance < 3)
        err_offset = 2;
    else
        err_offset = 1;
    if (has_bionic(bio_ears)) err_offset--;
    if (has_trait(PF_BADHEARING)) err_offset++;

    if (0 >= err_offset) {
        footsteps.push_back(orig);
        return;
    }

    const zaimoni::gdi::box<point> spread(point(-err_offset), point(err_offset));
    int tries = 0;
    do {
        const auto pt = orig + rng(spread);
        if (pt != pos && !see(pt)) {
            footsteps.push_back(pt);
            return;
        }
    } while (++tries < 10);
}

// draws footsteps that have been created by monsters moving about
void pc::draw_footsteps(void* w)
{
    auto w_terrain = reinterpret_cast<WINDOW*>(w);

    for (const point& step : footsteps) {
        const point draw_at(point(VIEW_CENTER) + step - pos);
        mvwputch(w_terrain, draw_at.y, draw_at.x, c_yellow, '?');
    }
    footsteps.clear(); // C:Whales behavior; never reaches savefile, display cleared on save/load cycling
    wrefresh(w_terrain);
    return;
}

void pc::toggle_safe_mode()
{
    if (0 == run_mode) {
        run_mode = 1;
        messages.add("Safe mode ON!");
    } else {
        turnssincelastmon = 0;
        run_mode = 0;
        if (autosafemode)
            messages.add("Safe mode OFF! (Auto safe mode still enabled!)");
        else
            messages.add("Safe mode OFF!");
    }
}

void pc::toggle_autosafe_mode()
{
    if (autosafemode) {
        messages.add("Auto safe mode OFF!");
        autosafemode = false;
    } else {
        messages.add("Auto safe mode ON");
        autosafemode = true;
    }
}

void pc::stop_on_sighting(int new_seen)
{
    if (new_seen > mostseen) {
        cancel_activity_query("Monster spotted!");
        turnssincelastmon = 0;
        if (1 == run_mode) run_mode = 2;	// Stop movement!
    } else if (autosafemode) { // Auto-safemode
        turnssincelastmon++;
        if (turnssincelastmon >= 50 && 0 == run_mode) run_mode = 1;
    }

    mostseen = new_seen;
}

void pc::ignore_enemy()
{
    if (2 == run_mode) {
        messages.add("Ignoring enemy!");
        run_mode = 1;
    }
}

std::optional<std::string> pc::move_is_unsafe() const
{
    // Monsters around and we don't wanna run
    if (2 == run_mode) return "Monster spotted--safe mode is on! (Press '!' to turn it off or ' to ignore monster.)";
    return std::nullopt;
}

void pc::record_kill(const monster& m)
{
    const mtype* const corpse = m.type;
    if (m.has_flag(MF_GUILT)) mdeath::guilt(*this, m);
    if (corpse->species != species_hallu) kills[corpse->id]++;
}

std::vector<std::pair<const mtype*, int> > pc::summarize_kills()
{
    std::vector<std::pair<const mtype*, int> > ret;

    for (int i = 0; i < num_monsters; i++) {
        if (0 < kills[i]) ret.push_back(std::pair(mtype::types[i], kills[i]));
    }

    return ret;
}

void pc::target_dead(int deceased)
{
    if (target == deceased) target = -1;
    else if (target > deceased) {
        if (0 <= deceased) target--;
    } else {
        if (-2 > deceased) target++;    // cf. TARGET_PLAYER/npcmove.cpp
    }
}
