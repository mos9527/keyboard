#include "midi.hpp"
#include "pch.hpp"
namespace midi {
	using namespace winrt;
	using namespace winrt::Microsoft::Windows::Devices::Midi2;                  // SDK Core
	using namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;     // For diagnostics loopback endpoints
	using namespace winrt::Microsoft::Windows::Devices::Midi2::Messages;        // For message utilities and strong types
	using namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization;  // for code to check if the service is installed/running

	struct inputContext_WinMIDI2 : public inputContext {
	private:
		MidiSession session{ nullptr };
		MidiEndpointConnection port{ nullptr };
		event_token port_revoke{};
		uint32_t index = 0;		
		static void MidiInProc(inputContext_WinMIDI2* ctx, winrt::Microsoft::Windows::Devices::Midi2::IMidiMessageReceivedEventArgs const& args) {
			unique_lock<mutex> lock(ctx->messageMutex);
			auto ump = args.GetMessagePacket();
			if (ump.PacketType() == MidiPacketType::UniversalMidiPacket32) {
				auto ump32 = ump.as<MidiMessage32>();
				ump32.Word0();
			}
			ctx->messageCV.notify_one();
		}
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return port != nullptr; }
		inline inputContext_WinMIDI2() {};
		inline inputContext_WinMIDI2(inputDevice_t const& device) : index(device.index) {
			session = MidiSession::Create(L"Keyboard WinMIDI2 Session (Input)");
			port = session.CreateEndpointConnection(to_hstring(device.id));
			port_revoke = port.MessageReceived([&](auto&& sender, auto&& args) { MidiInProc(this, args); });
		}
		inline ~inputContext_WinMIDI2() {
			if (port)
				port.MessageReceived(port_revoke);			
			if (session)
				session.Close();
		}
		/****/
		inline virtual void getMidiInDevices(midiInputDevices_t& result) {
			auto endpoints = MidiEndpointDeviceInformation::FindAll(
				MidiEndpointDeviceInformationSortOrder::DeviceInstanceId,
				MidiEndpointDeviceInformationFilters::AllStandardEndpoints
			);
			uint32_t i = 0;
			for (auto const& endpoint : endpoints)
				result.push_back(inputDevice_t{
					.index = i++,
					.name = to_string(endpoint.Name()),
					.id = to_string(endpoint.EndpointDeviceId())
				});
		}
		inline virtual std::string getMidiErrorMessage() { return "Unknown Error (WinMIDI2)"; }
	};

	struct outputContext_WinMIDI2 : public outputContext {
	private:
		MidiSession session{ nullptr };
		MidiEndpointConnection port{ nullptr };
		event_token port_revoke{};
		uint32_t index = 0;
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return port != nullptr; }
		inline outputContext_WinMIDI2() {};
		inline outputContext_WinMIDI2(outputDevice_t const& device) : index(device.index) {
			session = MidiSession::Create(L"Keyboard WinMIDI2 Session (Output)");
			port = session.CreateEndpointConnection(to_hstring(device.id));			
		}
		inline ~outputContext_WinMIDI2() { 
			if (port)
				port.MessageReceived(port_revoke);
			if (session)
				session.Close();
		}
		/****/
		inline virtual void sendMessage(message_t const& message) {
			if (!getStatus()) return;
			visit(visitor{
				[&](keyDownMessage_t const& msg) {
					port.SendSingleMessagePacket(MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
						MidiClock::Now(),
						MidiGroup(1),
						Midi1ChannelVoiceMessageStatus::NoteOn,
						MidiChannel(msg.channel),
						msg.note,
						msg.velocity
					));
				},
				[&](keyUpMessage_t const& msg) {
					port.SendSingleMessagePacket(MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
						MidiClock::Now(),
						MidiGroup(1),
						Midi1ChannelVoiceMessageStatus::NoteOff,
						MidiChannel(msg.channel),
						msg.note,
						msg.velocity
					));
				}
			}, message);
		}
		inline virtual std::string getMidiErrorMessage() { return "Unknown Error (WinRT)"; }
		inline virtual void getMidiOutDevices(midiOutputDevices_t& result) {
			auto endpoints = MidiEndpointDeviceInformation::FindAll(
				MidiEndpointDeviceInformationSortOrder::DeviceInstanceId,
				MidiEndpointDeviceInformationFilters::AllStandardEndpoints
			);
			uint32_t i = 0;
			for (auto const& endpoint : endpoints)
				result.push_back(inputDevice_t{
					.index = i++,
					.name = to_string(endpoint.Name()),
					.id = to_string(endpoint.EndpointDeviceId())
				});
		}
	};
}