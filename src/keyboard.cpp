#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"
#include "midi.hpp"
#include "imgui/imgui_internal.h"

#include "chord.hpp"

#define CONFIG_FILENAME "config"
struct {
	int backend = 0;
	int inputChannel = 0;
	int inputDeviceIndex = 0;
	int outputDeviceIndex = 0;
	int outputChannel = 0;
	int outputProgram = 0;
	int keyboardKeymap[256]{};
	void save() {
		FILE* file = fopen(CONFIG_FILENAME, "wb");
		CHECK(file, "Failed to open file for writing");
		fwrite(this, sizeof(*this), 1, file);
		fclose(file);
	};
	void load() {
		FILE* file = fopen(CONFIG_FILENAME, "rb");
		if (file) {
			fread(this, sizeof(*this), 1, file);
			fclose(file);
		}
	};
	void reset() {
		memset(this, 0, sizeof(*this));
	};
} g_config;
/****/
typedef std::unique_ptr<midi::inputContext> midiInContext_t;
typedef std::unique_ptr<midi::outputContext> midiOutContext_t;
const char* MIDI_BACKENDS[] = {
	"WinMM (Legacy)",
#ifdef WINRT
	"WinRT"
#endif
};
template<typename... T> midiInContext_t make_midi_input_context(T const&... args) {
	switch (g_config.backend)
	{
#ifdef WINRT
	case 1:
		return std::make_unique<midi::inputContext_WinRT>(args...);
#endif
	case 0:
	default:
		return std::make_unique<midi::inputContext_WinMM>(args...);
	}
}
template<typename... T> midiOutContext_t make_midi_output_context(T const&... args) {
	switch (g_config.backend)
	{
#ifdef WINRT
	case 1:
		return std::make_unique<midi::outputContext_WinRT>(args...);
#endif
	case 0:
	default:
		return std::make_unique<midi::outputContext_WinMM>(args...);
	}
}
/****/
midiInContext_t g_midiInContext;
midiOutContext_t g_midiOutContext;
midi::midiInputDevices_t g_midiInDevices;
midi::midiOutputDevices_t g_midiOutDevices;
chord::midi_key_states_t g_keyboardState;
const uint8_t ACTIVE_INPUT_FRAMES = 10;
std::array<int, 16> g_activeInputs;
/****/
line_buffer<256, 256> g_chordNames;
/****/
void setup() {
	g_midiInContext = make_midi_input_context();
	g_midiInContext->getMidiInDevices(g_midiInDevices);
	if (g_midiInDevices.size())
		g_midiInContext = make_midi_input_context(g_midiInDevices[std::min(g_midiInDevices.size() - 1, (size_t)g_config.inputDeviceIndex)]);

	g_midiOutContext = make_midi_output_context();
	g_midiOutContext->getMidiOutDevices(g_midiOutDevices);
	if (g_midiOutDevices.size())
		g_midiOutContext = make_midi_output_context(g_midiOutDevices[std::min(g_midiOutDevices.size() - 1, (size_t)g_config.outputDeviceIndex)]);
	if (g_midiInContext->getStatus())
		g_midiOutContext->sendMessage(midi::programChangeMessage_t{ (BYTE)g_config.outputChannel, (BYTE)g_config.outputProgram });
}
void poll_input() {
	auto map_midi_to_keystroke = [&](uint8_t velocity, uint8_t key) {
		if (g_config.keyboardKeymap[key]) {
			INPUT input{};
			input.type = INPUT_KEYBOARD;
			int scan = MapVirtualKeyA(g_config.keyboardKeymap[key], MAPVK_VK_TO_VSC);
			input.ki.wScan = scan;
			input.ki.dwFlags = velocity ? 0 : KEYEVENTF_KEYUP;
			input.ki.dwFlags |= KEYEVENTF_SCANCODE;
			SendInput(1, &input, sizeof(INPUT));
		}
		};
	using namespace midi;
	if (g_midiInContext) {
		if (g_midiInContext->getStatus()) {
			while (auto pool = g_midiInContext->pollMessage()) {
				auto& message = pool.value();
				std::visit(visitor{
					[&](keyDownMessage_t& msg) {						
						if (msg.channel == g_config.inputChannel)
							g_keyboardState[msg.note] = msg.velocity;
						map_midi_to_keystroke(msg.velocity, msg.note);
					},
					[&](keyUpMessage_t& msg) {
						if (msg.channel == g_config.inputChannel)
							g_keyboardState[msg.note] = 0;
						map_midi_to_keystroke(0, msg.note);
					},
					}, message);
				if (g_midiOutContext) {
					std::visit(visitor{
						[&] (auto& msg) {
							constexpr bool channel_type = requires() { msg.channel; };
							if constexpr (channel_type) {
								g_activeInputs[msg.channel] = ACTIVE_INPUT_FRAMES;
								if (msg.channel == g_config.inputChannel)
									msg.channel = g_config.outputChannel,
									g_midiOutContext->sendMessage(msg);
							}
						},
						}, message);
				}
			}
		}
	}
}
void draw() {
	ImGui::SetNextWindowPos({ 0,0 });
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::Begin("keyboard");
	if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_None)) {
		if (ImGui::Button("Save")) g_config.save();
		ImGui::SameLine();
		if (ImGui::Button("Load")) g_config.load(), setup();
		ImGui::SameLine();
		if (ImGui::Button("Reset")) g_config.reset(), setup();
	}

	if (ImGui::CollapsingHeader("Hardware", ImGuiTreeNodeFlags_None)) {
		ImGui::Text("System");
		if (ImGui::BeginCombo("Backend", MIDI_BACKENDS[g_config.backend])) {
			for (int i = 0; i < extent_of(MIDI_BACKENDS); i++) {
				bool selected = g_config.backend == i;
				if (ImGui::Selectable(MIDI_BACKENDS[i], &selected)) {
					g_config.backend = i;
					setup();
				}
			}
			ImGui::EndCombo();
		}
		const char* channel_names[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10","11","12","13","14","15","16" };
		auto draw_button_array = [&](int& value, const auto& names, const int id = 0, const int* states = nullptr) -> bool {
			bool dirty = false;
			for (int i = 0; i < extent_of(names); i++) {
				bool active = value == i;
				int styles = 0;
				if (active)
					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)), styles++;
				if (states && states[i])
					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)), styles++;
				ImGui::PushID(id + i);
				if (ImGui::Button(names[i], ImVec2(4, 0))) value = i, dirty = true;
				ImGui::PopID();
				ImGui::PopStyleColor(styles);
				ImGui::SameLine();
			}
			return dirty;
			};
		auto draw_twiddle_button = [&](int& value, const int r_min, const int r_max, const int id = 0) {
			bool dirty = false;
			ImGui::PushID(id);
			if (ImGui::Button("<", ImVec2(4, 0))) value = std::max(r_min, value - 1), dirty = true;
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::PushID(id + 1);
			if (ImGui::Button(">", ImVec2(4, 0))) value = std::min(r_max, value + 1), dirty = true;
			ImGui::PopID();
			return dirty;
			};
		ImGui::Text("Input");
		if (!g_midiInContext->getStatus()) {
			static std::string errorMessage = g_midiInContext->getMidiErrorMessage();
			ImGui::TextColored(ImColor(255, 0, 0), "ERROR: %s", errorMessage.c_str());
		}
		if (ImGui::BeginCombo(
			"Input Device",
			(g_midiInDevices.size() && g_midiInContext ? g_midiInDevices[g_midiInContext->getIndex()].name.c_str() : "Select Device"),
			ImGuiComboFlags_PopupAlignLeft
		)) {
			for (auto& [index, name, id] : g_midiInDevices) {
				bool selected = g_midiInContext && g_midiInContext->getIndex() == index;
				if (ImGui::Selectable(name.c_str(), &selected))
					g_midiInContext = make_midi_input_context(g_midiInDevices[index]), g_config.inputDeviceIndex = index;
			}
			ImGui::EndCombo();
		}
		draw_button_array(g_config.inputChannel, channel_names, 0, g_activeInputs.data());
		ImGui::Text("Input Channel");
		ImGui::Text("Output");
		if (!g_midiOutContext->getStatus()) {
			static std::string errorMessage = g_midiOutContext->getMidiErrorMessage();
			ImGui::TextColored(ImColor(255, 0, 0), "ERROR: %s", errorMessage.c_str());
		}
		if (ImGui::BeginCombo(
			"Output Device",
			(g_midiOutDevices.size() && g_midiOutContext ? g_midiOutDevices[g_midiOutContext->getIndex()].name.c_str() : "Select Device"),
			ImGuiComboFlags_PopupAlignLeft
		)) {
			for (auto& [index, name, id] : g_midiOutDevices) {
				bool selected = g_midiOutContext && g_midiOutContext->getIndex() == index;
				if (ImGui::Selectable(name.c_str(), &selected))
					g_midiOutContext = make_midi_output_context(g_midiOutDevices[index]), g_config.outputDeviceIndex = index;
			}
			ImGui::EndCombo();
		}
		if (g_midiOutContext) {
			bool dirty = draw_button_array(g_config.outputChannel, channel_names, 16);
			ImGui::Text("Output Channel");
			if (ImGui::BeginCombo("Output Program", midi::GM_programs[g_config.outputProgram])) {
				for (int i = 0; i < extent_of(midi::GM_programs); i++) {
					bool selected = g_config.outputProgram == i;
					if (ImGui::Selectable(midi::GM_programs[i], &selected)) {
						g_config.outputProgram = i;
						dirty = true;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			dirty |= draw_twiddle_button(g_config.outputProgram, 0, extent_of(midi::GM_programs), 32);
			if (dirty) g_midiOutContext->sendMessage(midi::programChangeMessage_t{ (BYTE)g_config.outputChannel, (BYTE)g_config.outputProgram });
		}
		if (ImGui::Button("Refresh")) setup();
	}
	if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
		const ImVec2 whiteKeySize(6, 8);
		const ImVec2 blackKeySize(4, 6);
		const ImVec4 blackKeyColor(0, 0, 0, 255);
		const ImVec4 whiteKeyColor(255, 255, 255, 255);
		const ImVec4 pressedKeyColor(0.8f, 0.4f, 0.4f, 1.0f);
		const int numKeys = 88;
		const int startNote = 21;
		static int offsetKey = 16;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 cpos = ImGui::GetCursorPos();
		static int activeKey = 0;
		auto draw_layer = [&](bool isBlack) {
			int offsetX = 0;
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::BeginGroup();
			for (int i = offsetKey; i < numKeys; ++i)
			{
				const int nthWhiteKey[] = { 0,  -1 , 1, -1,  2, 3, -1,  4, -1,  5, -1,  6 };
				const int nthBlackKey[] = { -1,  0, -1,  1, -1, -1, 2,  -1, 3,  -1, 4,  -1 };
				int note = startNote + i;
				int octave = note / 12;
				int noteInOctave = note % 12;
				bool isBlackKey = nthBlackKey[noteInOctave] != -1;
				if (!isBlack && isBlackKey) continue;
				if (isBlack && !isBlackKey) continue;
				ImVec4 keyColor = isBlackKey ? blackKeyColor : whiteKeyColor;
				ImVec4 labelColor = isBlackKey ? whiteKeyColor : blackKeyColor;
				if (g_keyboardState[note] > 0)
				{
					float t = g_keyboardState[note] / 127.0f;
					keyColor = ImLerp(pressedKeyColor, blackKeyColor, t);
				}
				ImVec2 keySize = isBlackKey ? blackKeySize : whiteKeySize;
				ImGui::PushStyleColor(ImGuiCol_Button, keyColor);
				ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
				ImGui::SetCursorPosX(pos.x + offsetX);
				ImGui::PushID(note);
				if (!isBlackKey) {
					int paddingX = ImGui::GetStyle().FramePadding.x;
					draw_list->AddRectFilled(ImVec2(paddingX + pos.x + offsetX, pos.y - blackKeySize.y), ImVec2(paddingX + pos.x + offsetX + keySize.x, pos.y), ImColor(keyColor));
				}
				std::string keyName = std::string{ chord::key_table[noteInOctave] } + std::to_string(octave);
				if (g_config.keyboardKeymap[note]) {
					UINT vkCode = g_config.keyboardKeymap[note];
					UINT charCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_CHAR);
					keyName += std::string{ "\n~\n" } + std::string(1, static_cast<char>(charCode ? charCode : '#'));
				}
				if (ImGui::Button(keyName.c_str(), keySize)) {
					ImGui::PopID();
					activeKey = note;
					ImGui::OpenPopup("Key Bind");
				}
				else { ImGui::PopID(); }
				ImGui::SameLine();
				if (isBlackKey) {
					int nBlack = nthBlackKey[noteInOctave];
					if (nBlack != 1 && nBlack != 4) offsetX += whiteKeySize.x;
					else offsetX += whiteKeySize.x * 2;
				}
				else
					offsetX += whiteKeySize.x;
				ImGui::PopStyleColor(2);
			}
			ImGui::EndGroup();
			ImGui::PopStyleVar();
			};
		pos.y += blackKeySize.y;
		ImGui::SetCursorPos(pos);
		draw_layer(false);
		pos.y -= blackKeySize.y;
		pos.x += whiteKeySize.x / 2;
		ImGui::SetCursorPos(pos);
		draw_layer(true);
		ImGui::SetCursorPosX(0);
		ImGui::SetCursorPosY(cpos.y + 14);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		int numDisplay = ImGui::GetContentRegionAvailWidth() / whiteKeySize.x;
		ImGui::SliderInt("##Start", &offsetKey, 0, numKeys - numDisplay);
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		static int frameActive = 0;
		if (ImGui::BeginPopupModal("Key Bind", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Mapping MIDI %d (%s)!", activeKey, chord::key_table[activeKey % 12]);
			ImGui::Separator();
			ImGui::Text("Waiting for key press...");
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			const std::vector<int> VK_SCANS{
				/* 0-9 */ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
				/* A-Z */ 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
				/* F1-F12 */ VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
				/* ALT,CTRL */ VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU,
				/* SPACE, PAGES */ VK_SPACE, VK_PRIOR, VK_NEXT, VK_END, VK_HOME, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
				/* ESC, etc */ VK_ESCAPE, VK_RETURN, VK_BACK, VK_DELETE, VK_INSERT, VK_SNAPSHOT, VK_CAPITAL, VK_SCROLL, VK_PAUSE, VK_TAB, VK_LWIN, VK_RWIN
			};
			for (int k : VK_SCANS) {
				if ((GetAsyncKeyState(k) & (1 << 31)) && k != VK_CAPITAL) {
					g_config.keyboardKeymap[activeKey] = k;
					ImGui::CloseCurrentPopup();
					break;
				}
			}
			ImGui::EndPopup();
		}
	}
	if (ImGui::CollapsingHeader("Chords", ImGuiTreeNodeFlags_DefaultOpen)) {
		g_chordNames.resize(chord::format<char[256]>(g_keyboardState, g_chordNames.span()));
		for (auto& line : g_chordNames) {
			ImGui::TextUnformatted(line);
		}
	}
	ImGui::End();
}
void refresh() {
	for (auto& frame : g_activeInputs) frame--, frame = std::max(0, frame);
}
void cleanup() {
	if (g_midiInContext) g_midiInContext.reset();
}
int main() {
#ifdef WINRT
	winrt::init_apartment();
#endif
	SetConsoleOutputCP(65001);
#ifdef ENABLE_UI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto screen = ImTui_ImplNcurses_Init(true);
	ImTui_ImplText_Init();
	ImGui::GetStyle().ScrollbarSize = 1;
	ImGui::GetStyle().GrabMinSize = 1.0f;
	g_config.load();
	setup();
	while (true) {
		ImTui_ImplNcurses_NewFrame();
		ImTui_ImplText_NewFrame();
		ImGui::NewFrame();
		refresh();
		poll_input();
		draw();
		ImGui::Render();
		ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
		ImTui_ImplNcurses_DrawScreen();
	}
	cleanup();
	ImTui_ImplText_Shutdown();
	ImTui_ImplNcurses_Shutdown();
#else
	g_config.load();
	setup();
	while (true) {
		refresh();
		poll_input();
}
	cleanup();
#endif // ENABLE_UI


	return 0;
}
