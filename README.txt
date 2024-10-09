keyboard
---
Simple TUI chords/notes visualization tool.
* windows only (for now)
* WinMM/WinRT API support (BLE MIDI devices are supported w/ the latter)
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

