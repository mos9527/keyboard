keyboard
---
Simple TUI chords/notes visualization tool.
* Windows only (for now)
* WinMM/WinRT/MIDI2 API support (BLE MIDI devices are supported w/ WinRT/MIDI2)
* detects chords/intervals and display their name(s).
* supports (partial) MIDI passthrough to another output device
* can map MIDI inputs to keyboard keystrokes. (dunno why you'd want to do that)

todo
---
* add option to display selected chord
* properly draw the staff

references
---
* https://github.com/juce-framework/JUCE/commit/9a38505dad7a5655edae320993f1926ae3979068
* https://github.com/stammen/winrtmidi/
* https://github.com/Microsoft/cppwinrt/issues/317
* https://kennykerrca.wordpress.com/2018/03/01/cppwinrt-understanding-async/
* https://kennykerrca.wordpress.com/2018/03/09/cppwinrt-producing-async-objects/
* https://devblogs.microsoft.com/oldnewthing/20210809-00/?p=105539
* https://www.recordingblogs.com/wiki/midi-system-exclusive-message
* https://www.recordingblogs.com/wiki/music-theory-index
* https://www.recordingblogs.com/wiki/status-byte-of-a-midi-message
* https://www.recordingblogs.com/wiki/midi-voice-messages
* https://www.recordingblogs.com/wiki/midi-controller-message
* https://midi.org/general-midi-2
* General MIDI 2 (February 6, 2007 Version 1.2a)
* https://stackoverflow.com/questions/18132987/cmake-and-msvs-nuget
* https://github.com/fredemmott/cmake-cpp-winrt-winui3/blob/master/src/CMakeLists.txt