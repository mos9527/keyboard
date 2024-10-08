#pragma once
#ifdef __cplusplus
#include <array>
#include <algorithm>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <mmeapi.h>
#pragma comment(lib, "winmm.lib")
#include <iostream>
#include <span>
#include <source_location>
#include <string>
#include <variant>
#ifdef WINRT
#pragma comment(lib, "windowsapp")
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Devices.Midi.h"
#include "winrt/Windows.Devices.Enumeration.h"
#endif
#define PRED(X) [](auto const& lhs, auto const& rhs) {return X;}
#define PAIR2(T) std::pair<T,T>
inline void __check(bool condition, const std::string& message = "", const std::source_location& location = std::source_location::current()) {
	if (!condition) {
		std::cerr << location.file_name() << ":" << location.line() << ",at " << location.function_name() << std::endl;
		if (message.size()) std::cerr << "ERROR: " << message << std::endl;
		std::abort();
	}
}
#define CHECK(EXPR, ...) __check(!!(EXPR), __VA_ARGS__)
// C++ Weekly - Ep 440 - Revisiting Visitors for std::visit - https://www.youtube.com/watch?v=et1fjd8X1ho
template<typename... T> struct visitor : T... {
	using T::operator()...;
};
template<size_t Rows, size_t Cols, typename Elem = char> struct line_buffer {
	using column_type = Elem[Cols];
	using row_type = std::array<column_type, Rows>;
	row_type data{};
	size_t size{};
	/***/
	std::span<column_type> span() { return { data.begin(), data.end() }; }
	row_type::iterator begin() { return data.begin(); }
	row_type::iterator end() { return data.begin() + size; }
	void resize(size_t size) { CHECK(size <= Rows); this->size = size; }
};
#endif