#ifndef OUTPUT_H
#define OUTPUT_H

#include "color.h"
#include <string>
#include <vector>
#include <utility>
#include <initializer_list>

//      LINE_NESW  - X for on, O for off
#define LINE_XOXO 4194424
#define LINE_OXOX 4194417
#define LINE_XXOO 4194413
#define LINE_OXXO 4194412
#define LINE_OOXX 4194411
#define LINE_XOOX 4194410
#define LINE_XXXO 4194420
#define LINE_XXOX 4194422
#define LINE_XOXX 4194421
#define LINE_OXXX 4194423
#define LINE_XXXX 4194414

// stderr logfile support
bool have_used_stderr_log();
const std::string& get_stderr_logname();
FILE* get_stderr_log();
// end stderr logfile support

bool reject_not_whitelisted_printf(const std::string& src);

void mvputch(int y, int x, nc_color FG, long ch);
void mvwputch(WINDOW* w, int y, int x, nc_color FG, long ch);
void mvwputch_inv(WINDOW* w, int y, int x, nc_color FG, long ch);
void mvwputch_hi(WINDOW* w, int y, int x, nc_color FG, long ch);

void wputch(WINDOW* w, nc_color FG, long ch);

void draw_hline(WINDOW* w, int y, nc_color FG, long ch, int x0 = -1, int x1 = -1);

template<class...Args>
void debuglog(const char* mes, Args...params)
{
	if (decltype(auto) err = get_stderr_log()) {
		if constexpr (0 < sizeof...(params)) {
			if (reject_not_whitelisted_printf(mes)) return;
			fprintf(err, mes, params...);
		} else {
			fputs(mes, err);
		}
		fputc('\n', err);
	}
}

void debuglog(const std::string& msg);

template<class...Args>
void mvprintz(int y, int x, nc_color FG, const char* mes, Args...params)
{
	if (reject_not_whitelisted_printf(mes)) return;
	attron(FG);
	mvprintw(y, x, mes, params...);
	attroff(FG);
}

template<class...Args>
void mvwprintz(WINDOW* w, int y, int x, nc_color FG, const char* mes, Args...params)
{
	if (reject_not_whitelisted_printf(mes)) return;
	wattron(w, FG);
	mvwprintw(w, y, x, mes, params...);
	wattroff(w, FG);
}

template<class...Args>
void printz(nc_color FG, const char* mes, Args...params)
{
	if (reject_not_whitelisted_printf(mes)) return;
	attron(FG);
	printw(mes, params...);
	attroff(FG);
}

template<class...Args>
void wprintz(WINDOW* w, nc_color FG, const char* mes, Args...params)
{
	if (reject_not_whitelisted_printf(mes)) return;
	wattron(w, FG);
	wprintw(w, mes, params...);
	wattroff(w, FG);
}

void mvwaddstrz(WINDOW* w, int y, int x, nc_color FG, const char* mes);
inline void mvwaddstrz(WINDOW* w, int y, int x, const std::pair<nc_color, const char*>& mes) { mvwaddstrz(w, y, x, mes.first, mes.second); }
void waddstrz(WINDOW* w, nc_color FG, const char* mes);
inline void waddstrz(WINDOW* w, const std::pair<nc_color, const char*>& mes) { waddstrz(w, mes.first, mes.second); }

void draw_tabs(WINDOW* w, int active_tab, const char* const labels[]);

void debugmsg(const char* mes, ...);
int  query_int(const char* mes, ...);
std::string string_input_popup(const char* mes, ...);
std::string string_input_popup(int max_length, const char* mes, ...);
char popup_getkey(const char* mes, ...);
int  menu_vec(const char* mes, const std::vector<std::string>& options);
int  menu(const char* mes, const std::initializer_list<std::string>& opts);
void popup_top(const char* mes, ...); // Displayed at the top of the screen
void popup(const char* mes, ...);
void popup_raw(const char* mes);
inline void popup(const std::string& msg) { popup_raw(msg.c_str()); }
void popup_nowait(const char* mes, ...); // Doesn't wait for spacebar
void full_screen_popup(const char* mes, ...);

nc_color hilite(nc_color c);
nc_color invert_color(nc_color c);
nc_color red_background(nc_color c);
long special_symbol(char sym);

struct _query_yn
{
	_query_yn() = default;
	_query_yn(const _query_yn& src) = delete;
	_query_yn(_query_yn&& src) = delete;
	~_query_yn() = default;
	_query_yn& operator=(const _query_yn& src) = delete;
	_query_yn& operator=(_query_yn&& src) = delete;

	template<class...Args> bool operator()(const char* msg, Args...params) {
		if (!msg || !*msg) return false;	// reject null and empty-string
		if constexpr (0 == sizeof...(Args)) {
			return _record(msg);
		} else {
			return _add(msg, params...);
		}
	}
	bool operator()(const std::string& src) { return _record(src.c_str()); }
private:
	bool _add(const char* msg, ...);
	bool _record(const char* msg);
};

extern _query_yn query_yn;

#endif
