#include "midi.hpp"
#include "pch.hpp"
namespace midi {
	using namespace winrt;
	using namespace winrt::Microsoft::Windows::Devices::Midi2;                  // SDK Core
	using namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;     // For diagnostics loopback endpoints
	using namespace winrt::Microsoft::Windows::Devices::Midi2::Messages;        // For message_t utilities and strong types
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
			// TODO: MIDI2 Spec
			if (ump.PacketType() == MidiPacketType::UniversalMidiPacket32) {
				auto ump32 = ump.as<MidiMessage32>().Word0();
				uint8_t status, lo, hi;
				hi = ump32 & 0xFF; ump32 >>= 8;
				lo = ump32 & 0xFF; ump32 >>= 8;
				status = ump32 & 0xFF; 
				auto message = midi1_packet(status, lo, hi);
				ctx->messages.push(message);
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
			port.Open();
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
			result.clear();
			for (auto const& endpoint : endpoints)
				result.push_back(inputDevice_t{
					.index = i++,
					.name = to_string(endpoint.Name()) + " (" + to_string(endpoint.EndpointDeviceId()) + ")",
					.id = to_string(endpoint.EndpointDeviceId())
				});
		}
		inline virtual std::string getMidiErrorMessage() { return "Unknown Error (WinMIDI2)"; }
	};

	struct outputContext_WinMIDI2 : public outputContext {
	private:
		MidiSession session{ nullptr };
		MidiEndpointConnection port{ nullptr };		
		uint32_t index = 0;
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return port != nullptr; }
		inline outputContext_WinMIDI2() {};
		inline outputContext_WinMIDI2(outputDevice_t const& device) : index(device.index) {
			session = MidiSession::Create(L"Keyboard WinMIDI2 Session (Output)");
			port = session.CreateEndpointConnection(to_hstring(device.id));			
			port.Open();
		}
		inline ~outputContext_WinMIDI2() { 
			if (session)
				session.Close();
		}
		/****/
		inline virtual void sendMessage(message_t const& message) {
			if (!getStatus()) return;
			auto packet = midi1_packet(message);
			auto result = port.SendSingleMessagePacket(MidiMessageBuilder::BuildSystemMessage(
				0,
				MidiGroup(2),
				packet.status,
				packet.lo,
				packet.hi
			));
		}
		inline virtual std::string getMidiErrorMessage() { return "Unknown Error (WinRT)"; }
		inline virtual void getMidiOutDevices(midiOutputDevices_t& result) {
			auto endpoints = MidiEndpointDeviceInformation::FindAll(
				MidiEndpointDeviceInformationSortOrder::DeviceInstanceId,
				MidiEndpointDeviceInformationFilters::AllStandardEndpoints
			);
			uint32_t i = 0;
			result.clear();
			for (auto const& endpoint : endpoints)
				result.push_back(outputDevice_t{
					.index = i++,
					.name = to_string(endpoint.Name()) + " (" + to_string(endpoint.EndpointDeviceId()) + ")",
					.id = to_string(endpoint.EndpointDeviceId())
				});
		}
	};
}