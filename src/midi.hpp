#pragma once
namespace midi {
	using namespace std;
	struct inputDevice_t { uint32_t index; string name; string id; };
	typedef vector<inputDevice_t> midiInputDevices_t;
	/****/
	struct keyDownMessage_t { uint64_t note, velocity; };
	struct keyUpMessage_t { uint64_t note, velocity; };
	using message_t = variant<keyDownMessage_t, keyUpMessage_t>;
	/****/
	struct inputContext {
	public:
		std::queue<message_t> messages;
		std::condition_variable messageCV;
		std::mutex messageMutex;
		inline virtual const uint32_t getIndex() const = 0;
		inline virtual const bool getStatus() const = 0;
		inline virtual std::string getMidiErrorMessage() = 0;
		inline virtual ~inputContext() {};
		/****/
		inline virtual std::optional<message_t> pollMessage(bool blocking = false) {
			if (!getStatus())
				return {};
			std::unique_lock<std::mutex> lock(messageMutex);
			if (blocking) messageCV.wait(lock, [this] { return !messages.empty(); });
			else if (messages.empty()) return {};
			auto message = messages.front();
			messages.pop();
			return message;
		}
		/****/
		inline virtual void getMidiInDevices(midiInputDevices_t& devices) = 0;
	};
	/****/
	struct inputContext_WinMM : public inputContext {
	private:
		uint32_t index = 0;
		HMIDIIN handle;
		MMRESULT status = -1;
		static void CALLBACK MidiInProc(
			HMIDIIN   hMidiIn,
			UINT      wMsg,
			inputContext_WinMM* ctx,
			DWORD_PTR dwParam1,
			DWORD_PTR dwParam2
		) {
			unique_lock<mutex> lock(ctx->messageMutex);
			switch (wMsg)
			{
			case MIM_OPEN:
				break;
			case MIM_DATA: {
				uint8_t velocity = HIWORD(dwParam1), note = LOWORD(dwParam1) >> 8;
				if (velocity == 0) ctx->messages.push(keyUpMessage_t{ note, velocity }), ctx->messageCV.notify_one();
				else ctx->messages.push(keyDownMessage_t{ note, velocity }), ctx->messageCV.notify_one();
				break;
			}
			default:
				break;
			}
		};
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return status == MMSYSERR_NOERROR; }

		inline inputContext_WinMM() {};
		inline inputContext_WinMM(inputDevice_t const& device) : index(device.index) {
			status = midiInOpen(&handle, index, (DWORD_PTR)MidiInProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
			if (status == MMSYSERR_NOERROR) midiInStart(handle);
		}
		inline ~inputContext_WinMM() {
			if (status == MMSYSERR_NOERROR) {
				midiInStop(handle);
				midiInClose(handle);
			}
		}
		/****/
		inline virtual std::string getMidiErrorMessage() {
			static char buffer[1024];
			midiInGetErrorText(status, buffer, sizeof(buffer));
			return buffer;
		}
		inline virtual void getMidiInDevices(midiInputDevices_t& devices) {
			devices.resize(midiInGetNumDevs());
			uint32_t i = 0;
			for (auto& index : devices) {
				MIDIINCAPS caps;
				midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));
				index.index = i++; index.name = index.id = caps.szPname;
			}
		} // namespace winmidi
	};
	/****/
#ifdef WINRT
	using namespace winrt;
	using namespace Windows::Foundation;
	using namespace Windows::Devices::Enumeration;
	using namespace Windows::Devices::Midi;
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
				ctx->messages.push(keyDownMessage_t{ message.as<MidiNoteOnMessage>().Note(), message.as<MidiNoteOnMessage>().Velocity() });
				break;
			case MidiMessageType::NoteOff:
				ctx->messages.push(keyUpMessage_t{ message.as<MidiNoteOffMessage>().Note(), message.as<MidiNoteOffMessage>().Velocity() });
				break;
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
#endif // WINRT
}