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
#include "winrt/Windows.Storage.Streams.h"
#endif
#define PRED(X) [](auto const& lhs, auto const& rhs) {return X;}
#define PAIR2(T) std::pair<T,T>
static void _assert(const wchar_t* cond_s, const wchar_t* fmt = L"", auto ...args) {
	static wchar_t _assert_msg_buffer[1024];
	int p = swprintf(_assert_msg_buffer, L"Assertion failed: %ls\n", cond_s);
	swprintf(_assert_msg_buffer + p, fmt, args...);
	MessageBoxW(NULL, _assert_msg_buffer, L"Error", MB_ICONERROR);
	exit(1);
}
#define ASSERT(cond, ...) if (!(cond)) _assert(L#cond, __VA_ARGS__);
// https://stackoverflow.com/a/22713396
template<typename T, size_t N> constexpr size_t extent_of(T(&)[N]) { return N; };
// C++ Weekly - Ep 440 - Revisiting Visitors for std::visit - https://www.youtube.com/watch?v=et1fjd8X1ho
// This also demonstrates default visitor behavior for unhandled types
template<typename Arg, typename... T> concept none_invocable = (!std::is_invocable<T, Arg&>::value && ...);
template<typename... T> struct visitor : T... {
	using T::operator()...;
	template<typename Arg> requires none_invocable<Arg, T...> auto operator()(Arg&) {
		/* nop */
	};
};
// Fixed size vector
template<typename T, size_t Size> class fixed_vector {
	std::array<T, Size> _data{};
	size_t _size{ Size };
public:	
	inline fixed_vector() = default;
	inline explicit fixed_vector(const T* data, const size_t size) {
		ASSERT(size <= Size);
		memcpy(this->data(), data, size);		
		resize(size);
	}
	
	inline T& operator[](size_t index) { return _data[index]; }
	
	inline std::array<T, Size>::iterator begin() { return _data.begin(); }
	inline std::array<T, Size>::iterator end() { return _data.begin() + _size; }
	inline std::array<T, Size>::iterator end_max() { return _data.end(); }

	inline std::span<T> span() { return { begin(), end() }; }
	inline std::span<T> span_max() { return { begin(), end_max() }; }

	inline T* data() { return _data.data(); }
	inline size_t size() { return _size; }
	inline void resize(size_t size) { ASSERT(size <= Size); _size = size; }
};
// Column major matrix
template<typename T, size_t Rows, size_t Cols> class fixed_matrix {
	using column_type = fixed_vector<T, Cols>;
	using row_type = std::array<column_type, Rows>;
	row_type _data{};
	size_t _size{ Rows };
public:
	inline column_type& operator[](size_t row) { return _data[row]; }

	inline row_type::iterator begin() { return _data.begin(); }
	inline row_type::iterator end() { return _data.begin() + _size; }
	inline row_type::iterator end_max() { return _data.end(); }

	inline std::span<column_type> span() { return { begin(), end() }; }
	inline std::span<column_type> span_max() { return { begin(), end_max() }; }

	inline size_t size() { return _size; }
	inline void resize(size_t size) { ASSERT(size <= Rows); _size = size; }
};
#endif