#include "midi.hpp"
namespace midi {
	using namespace winrt;
	using namespace Windows::Foundation;
	using namespace Windows::Devices::Enumeration;
	using namespace Windows::Devices::Midi;
	using namespace Windows::Storage::Streams;

	struct inputContext_WinRT : public inputContext {
	private:
		uint32_t index = 0;
		MidiInPort port{ nullptr };
		static void MidiInProc(inputContext_WinRT* ctx, IMidiMessageReceivedEventArgs const& args) {
			unique_lock<mutex> lock(ctx->messageMutex);
			auto message = args.Message();
			switch (message.Type())
			{
			case MidiMessageType::NoteOn:
				ctx->messages.push(keyDownMessage_t{ message.as<MidiNoteOnMessage>().Channel(), message.as<MidiNoteOnMessage>().Note(), message.as<MidiNoteOnMessage>().Velocity() });
				break;
			case MidiMessageType::NoteOff:
				ctx->messages.push(keyUpMessage_t{ message.as<MidiNoteOffMessage>().Channel(), message.as<MidiNoteOffMessage>().Note() });
				break;
			case MidiMessageType::ProgramChange:
				ctx->messages.push(programChangeMessage_t{ message.as<MidiProgramChangeMessage>().Channel(), message.as<MidiProgramChangeMessage>().Program() });
				break;
			case MidiMessageType::PitchBendChange:
				ctx->messages.push(pitchWheelMessage_t{ message.as<MidiPitchBendChangeMessage>().Channel(), message.as<MidiPitchBendChangeMessage>().Bend() });
				break;
			case MidiMessageType::ControlChange:
				ctx->messages.push(controllerMessage_t{ message.as<MidiControlChangeMessage>().Channel(), message.as<MidiControlChangeMessage>().Controller(), message.as<MidiControlChangeMessage>().ControlValue() });
				break;
			case MidiMessageType::SystemExclusive:
			{
				auto sysex = message.as<MidiSystemExclusiveMessage>();
				ctx->messages.push(make_shared<sysexMessage_t::element_type>(
					(char*)sysex.RawData().data(), sysex.RawData().Length()
				));
				break;
			}
			default:
				break;
			}
			ctx->messageCV.notify_one();
		}
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return port != nullptr; }
		inline inputContext_WinRT() {};
		inline inputContext_WinRT(inputDevice_t const& device) : index(device.index) {
			auto co = [&]() -> IAsyncAction {
				auto task = co_await MidiInPort::FromIdAsync(to_hstring(device.id));
				if (task) {
					port = task.as<MidiInPort>();
					port.MessageReceived([&](auto&& sender, auto&& args) { MidiInProc(this, args); });
				}
				};
			co().get();
		}
		inline ~inputContext_WinRT() { if (port) port.Close(); }
		/****/
		inline virtual void getMidiInDevices(midiInputDevices_t& result) {
			auto co = [&]() -> IAsyncAction {
				auto port = MidiInPort::GetDeviceSelector();
				auto devices = co_await DeviceInformation::FindAllAsync(port);
				result.resize(devices.Size());
				uint32_t i = 0;
				for (auto&& index : devices)
				{
					result[i].name = to_string(index.Name());
					result[i].id = to_string(index.Id());
					result[i++].index = i;
				}
				};
			co().get();
		}
		inline virtual std::string getMidiErrorMessage() { return "Unknown Error (WinRT)"; }
	};

	struct outputContext_WinRT : public outputContext {
	private:
		uint32_t index = 0;
		MidiOutPort port{ nullptr };
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return port != nullptr; }
		inline outputContext_WinRT() {};
		inline outputContext_WinRT(outputDevice_t const& device) : index(device.index) {
			auto co = [&]() -> IAsyncAction {
				auto task = co_await MidiOutPort::FromIdAsync(to_hstring(device.id));
				if (task) {
					port = task.as<MidiOutPort>();
				}
				};
			co().get();
		}
		inline ~outputContext_WinRT() { if (port) port.Close(); }
		/****/
		inline virtual void sendMessage(message_t const& message) {
			if (!getStatus()) return;
			visit(visitor{
				[&](keyDownMessage_t const& msg) {
					port.SendBuffer(MidiNoteOnMessage(msg.channel, msg.note, msg.velocity).RawData());
				},
				[&](keyUpMessage_t const& msg) {
					port.SendBuffer(MidiNoteOffMessage(msg.channel, msg.note, msg.velocity).RawData());
				},
				[&](programChangeMessage_t const& msg) {
					port.SendBuffer(MidiProgramChangeMessage(msg.channel, msg.program).RawData());
				},
				[&](pitchWheelMessage_t const& msg) {
					port.SendBuffer(MidiPitchBendChangeMessage(msg.channel, msg.level).RawData());
				},
				[&](controllerMessage_t const& msg) {
					port.SendBuffer(MidiControlChangeMessage(msg.channel, msg.controller, msg.value).RawData());
				},
				[&](sysexMessage_t const& msg) {
					Buffer buffer(msg->size());
					memcpy(buffer.data(), msg->data(), msg->size());
					port.SendBuffer(MidiSystemExclusiveMessage(buffer).RawData());
				}
				}, message);
		}
		inline virtual std::string getMidiErrorMessage() { return "Unknown Error (WinRT)"; }
		inline virtual void getMidiOutDevices(midiOutputDevices_t& result) {
			auto co = [&]() -> IAsyncAction {
				auto port = MidiOutPort::GetDeviceSelector();
				auto devices = co_await DeviceInformation::FindAllAsync(port);
				result.resize(devices.Size());
				uint32_t i = 0;
				for (auto&& index : devices)
				{
					result[i].name = to_string(index.Name());
					result[i].id = to_string(index.Id());
					result[i++].index = i;
				}
				};
			co().get();
		}
	};
}