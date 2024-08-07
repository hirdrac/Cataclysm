#ifndef ZERO_H
#define ZERO_H

// zero-dependency header

constexpr unsigned long long int_log10(unsigned long long x) {
	return (10 <= x) ? 1 + int_log10(x / 10) : 0;
}

// upper-bound clamp
template<class T, class U> void clamp_ub(T& x, const U& ref) { if (ref < x) x = ref; }
template<auto ref, class T> void clamp_ub(T& x) { if (ref < x) x = ref; }
template<class T> void clamp_ub(T& x, const T& ref) { if (ref < x) x = ref; }

template<class T, class U> T clamped_ub(const T& x, const U& ref) { return (ref < x) ? ref : x; }
template<auto ref, class T> T clamped_ub(const T& x) { return (ref < x) ? ref : x; }
template<class T> T clamped_ub(const T& x, const T& ref) { return (ref < x) ? ref : x; }

// lower-bound clamp
template<class T, class U> void clamp_lb(T& x, const U& ref) { if (ref > x) x = ref; }
template<auto ref, class T> void clamp_lb(T& x) { if (ref > x) x = ref; }
template<class T> void clamp_lb(T& x, const T& ref) { if (ref > x) x = ref; }

template<class T, class U> T clamped_lb(const T& x, const U& ref) { return ref > x ? ref : x; }
template<auto ref, class T> T clamped_lb(const T& x) { return ref > x ? ref : x; }
template<class T> T clamped_lb(const T& x, const T& ref) { return ref > x ? ref : x; }

// bidrectional clamp
template<class T, class U> T clamped(const T& x, const T& lb, const U& ub) { return (ub < x) ? ub : ((lb > x) ? lb : x); }
template<class T, class U> T clamped(const T& x, const U& lb, const U& ub) { return (ub < x) ? ub : ((lb > x) ? lb : x); }
template<auto lb, auto ub, class T> T clamped(const T& x) { return (ub < x) ? ub : ((lb > x) ? lb : x); }
template<class T> T clamped(const T& x, const T& lb, const T& ub) { return (ub < x) ? ub : ((lb > x) ? lb : x); }

// std::visit wrappers
template<class T>
struct to_ref
{
    to_ref() = default;
    to_ref(const to_ref& src) = delete;
    to_ref(to_ref&& src) = delete;
    to_ref& operator=(const to_ref& src) = delete;
    to_ref& operator=(to_ref&& src) = delete;
    ~to_ref() = default;

    T& operator()(T* target) const { return *target; }
};

// https://devblogs.microsoft.com/oldnewthing/20200413-00/?p=103669 [Raymond Chen/"The Old New Thing"]
template<typename T, typename...>
using unconditional_t = T;

template<typename T, T v, typename...>
inline constexpr T unconditional_v = v;

// https://artificial-mind.net/blog/2020/11/14/cpp17-consteval
template <auto V>
static constexpr const auto force_consteval = V;

// proof of caller
// https://awesomekling.github.io/Serenity-C++-patterns-The-Badge/

template<class T>
class Badge
{
	friend T;
	Badge() = default;
};


// for working around std::function requiring copy-constructability
template<class OP>
class function_relay {
	OP* _ptr;

public:
	constexpr function_relay(OP* src) noexcept : _ptr(src) {}
	constexpr function_relay(OP& src) noexcept : _ptr(&src) {}

	constexpr function_relay(const function_relay&) = default;
	constexpr function_relay(function_relay&&) = default;
	constexpr function_relay& operator=(const function_relay&) = default;
	constexpr function_relay& operator=(function_relay&&) = default;
	constexpr ~function_relay() = default;

	template<class...Args>
	constexpr auto operator()(Args&&...params) { return (*_ptr)(params...); }
};

namespace cataclysm {

template<class T>
constexpr int signum(const T& x) {
	if (0 == x) return 0;
	return (0 < x) ? 1 : -1;
}

template<class T, class U>
constexpr auto min(const T& lhs, const U& rhs) { return lhs < rhs ? lhs : rhs; }

template<class T, class U>
constexpr auto max(const T& lhs, const U& rhs) { return lhs < rhs ? rhs : lhs; }

static_assert(1 == min(1, 2));
static_assert(2 == max(1, 2));

template<class T>
constexpr int square(const T& x) {
	return x * x;
}

}

#endif
