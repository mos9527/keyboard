#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"
#include "winmidi.hpp"
#include "imgui/imgui_internal.h"

#include "chord.hpp"

winmidi::devices_t g_devices;
std::unique_ptr<winmidi::midiInContext> g_midiInContext;
chord::midi_key_states_t g_keyboardState;
int g_keyboardKeymap[256]{};
HANDLE g_stdIn, g_stdOut;

line_buffer<256,256> g_chordNames;

void enter() {
    g_devices = winmidi::getMidiInDevices();
    g_midiInContext = std::make_unique<winmidi::midiInContext>(0);
    g_stdIn = GetStdHandle(STD_INPUT_HANDLE);
    g_stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
}
void handle_midi_key(uint8_t velocity, uint8_t key) {    
    g_keyboardState[key] = velocity;    
    if (g_keyboardKeymap[key]) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        int scan = MapVirtualKeyA(g_keyboardKeymap[key], MAPVK_VK_TO_VSC);
        input.ki.wScan = scan;
        input.ki.dwFlags = velocity ? 0 : KEYEVENTF_KEYUP;
        input.ki.dwFlags |= KEYEVENTF_SCANCODE;
        SendInput(1, &input, sizeof(INPUT));
    }
}
void keyboard_widget() {
    const ImVec2 whiteKeySize(6, 8);
    const ImVec2 blackKeySize(4, 6);
    const ImVec4 blackKeyColor(0, 0, 0, 255);
    const ImVec4 whiteKeyColor(255, 255, 255, 255);
    const ImVec4 pressedKeyColor(0.8f, 0.4f, 0.4f, 1.0f);
    const int numKeys = 88;
    const int startNote = 21;
    static int offsetKey = 0;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    static int activeKey = 0;
    auto draw_layer = [&](bool isBlack) {
        int offsetX = 0;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::BeginGroup();        
        for (int i = offsetKey; i < numKeys; ++i)
        {
            const int nthWhiteKey[] = { 0,  -1 , 1, -1,  2, 3, -1,  4, -1,  5, -1,  6 };
            const int nthBlackKey[] = { -1,  0, -1,  1, -1, -1, 2,  -1, 3,  -1, 4,  -1};
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
            std::string keyName = std::string{ chord::key_table::data[noteInOctave] } + std::to_string(octave);
            if (g_keyboardKeymap[note]) {
                UINT vkCode = g_keyboardKeymap[note];
                UINT charCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_CHAR);
                keyName += std::string{ "\n~\n" } + std::string(1, static_cast<char>(charCode ? charCode : '#'));
            }
            if (ImGui::Button(keyName.c_str(), keySize)) {
                ImGui::PopID();
                activeKey = note;
                ImGui::OpenPopup("Key Bind");
            } else { ImGui::PopID(); }            
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
    ImGui::SetCursorPosY(19);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    int numDisplay = ImGui::GetContentRegionAvailWidth() / whiteKeySize.x;
    ImGui::SliderInt("##Start", &offsetKey, 0, numKeys - numDisplay);
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    static int frameActive = 0;
    if (ImGui::BeginPopupModal("Key Bind", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Mapping MIDI %d (%s)!", activeKey, chord::key_table::data[activeKey % 12]);
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
            if ((GetAsyncKeyState(k) & (1<<31)) && k != VK_CAPITAL) {
                g_keyboardKeymap[activeKey] = k;
				ImGui::CloseCurrentPopup();
				break;
            }
        }
        ImGui::EndPopup();
    }
}
void event_loop() {
    using namespace winmidi;
    ImGui::SetNextWindowPos({ 0,0 });
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("MIDI2Keyboard");
    if (g_midiInContext) {
        switch (g_midiInContext->getStatus()) {
        case midiInContext::STATUS_OK:
        {
            auto pool = g_midiInContext->poolMessage();
            if (pool) {
                auto& message = pool.value();
                switch (message.type)
                {
                case MIM_DATA:
                {
                    handle_midi_key(HIWORD(message.param1), LOWORD(message.param1) >> 8);
                    break;
                }
                default:
                    break;
                }
            }
            ImGui::TextColored(ImColor(0, 255, 0), "Connected: %s", g_devices[g_midiInContext->getDevice()].second.szPname);
            break;
        }
        default:
            ImGui::TextColored(ImColor(255, 0, 0), "ERROR: %s", getMidiErrorMessage(g_midiInContext->getStatus()));
        }
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Device", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo(
            "###",
            (g_devices.size() && g_midiInContext ? g_devices[g_midiInContext->getDevice()].second.szPname : "Select Device")
        )) {
            for (auto& [index, device] : g_devices) {
                bool selected = g_midiInContext && g_midiInContext->getDevice() == index;
                if (ImGui::Selectable(device.szPname, &selected))
                    g_midiInContext = std::make_unique<winmidi::midiInContext>(index);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();  ImGui::Spacing();  ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            g_devices = winmidi::getMidiInDevices();
            g_midiInContext.reset();
        }
    }
    if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        keyboard_widget();
    }
    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Save")) {
            FILE* file = fopen("midi2keyboard.keymap", "wb");
            if (file) {
                fwrite(g_keyboardKeymap, sizeof(g_keyboardKeymap), 1, file);
                fclose(file);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
			FILE* file = fopen("midi2keyboard.keymap", "rb");
            if (file) {
                fread(g_keyboardKeymap, sizeof(g_keyboardKeymap), 1, file);
                fclose(file);
            }
		}
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
			memset(g_keyboardKeymap, 0, sizeof(g_keyboardKeymap));
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

int main() {
    SetConsoleOutputCP(65001);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();
    ImGui::GetStyle().ScrollbarSize = 1;
    ImGui::GetStyle().GrabMinSize = 1.0f;
    enter();
    while (true) {
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        GetConsoleScreenBufferInfo(g_stdOut, &csbi);
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();
        event_loop();
        ImGui::Render();
        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }
    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();
    return 0;
}
