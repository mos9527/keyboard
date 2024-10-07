# CMakeList.txt : CMake project for MIDI2Keyboard, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MIDI2Keyboard")
set(CMAKE_CXX_STANDARD 20)

set(MINGW true)
include_directories("contrib/PDCurses")
add_subdirectory("contrib/imtui")


file(
	GLOB_RECURSE SOURCES
	"src/keyboard.cpp" 
	"contrib/PDCurses/wincon/*.c" "contrib/PDCurses/pdcurses/*.c" 
	"contrib/imtui/src/imtui-impl-ncurses.cpp"
)
# Add source to this project's executable.
add_executable (MIDI2Keyboard ${SOURCES})
add_compile_definitions(WINRT)
target_precompile_headers(MIDI2Keyboard PUBLIC "src/pch.hpp")
target_include_directories(MIDI2Keyboard PRIVATE PDCurses)
target_include_directories(MIDI2Keyboard PRIVATE "contrib/imtui/include")
target_include_directories(MIDI2Keyboard PRIVATE "contrib/imtui/third-party/imgui")
target_link_libraries(MIDI2Keyboard PRIVATE imtui imgui-for-imtui)