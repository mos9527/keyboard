#pragma once
namespace midi {
	using namespace std;
	struct inputDevice_t { uint32_t index; string name; string id; };
	using outputDevice_t = inputDevice_t;
	typedef vector<inputDevice_t> midiInputDevices_t;
	typedef vector<outputDevice_t> midiOutputDevices_t;
	/****/
	struct keyDownMessage_t { uint8_t channel, note, velocity; };
	struct keyUpMessage_t { uint8_t channel, note; };
	struct programChangeMessage_t { uint8_t channel, program;  };
	using message_t = variant<keyDownMessage_t, keyUpMessage_t, programChangeMessage_t>;
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
		inline virtual void getMidiInDevices(midiInputDevices_t&) = 0;
	};
	
	struct outputContext {
		inline virtual const uint32_t getIndex() const = 0;
		inline virtual const bool getStatus() const = 0;
		inline virtual std::string getMidiErrorMessage() = 0;
		inline virtual ~outputContext() {};
		/****/
		inline virtual void sendMessage(message_t const&) = 0;
		/****/
		inline virtual void getMidiOutDevices(midiOutputDevices_t&) = 0;
	};
	/****/
	union winMM_message {
		DWORD param;
		BYTE data[4];
	};
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
				winMM_message message{ .param = (DWORD)dwParam1 };
				uint8_t velocity = message.data[2], note = message.data[1];
				if (velocity == 0) ctx->messages.push(keyUpMessage_t{ 0, note }), ctx->messageCV.notify_one();
				else ctx->messages.push(keyDownMessage_t{ 0, note, velocity }), ctx->messageCV.notify_one();
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
		}
	};

	struct outputContext_WinMM : public outputContext {
	private:
		uint32_t index = 0;
		HMIDIOUT handle;
		MMRESULT status = -1;
	public:
		inline virtual const uint32_t getIndex() const { return index; }
		inline virtual const bool getStatus() const { return status == MMSYSERR_NOERROR; }
		inline outputContext_WinMM() {};
		inline outputContext_WinMM(outputDevice_t const& device) : index(device.index) {
			status = midiOutOpen(&handle, index, NULL, NULL, CALLBACK_NULL);			
		}
		inline ~outputContext_WinMM() {
			if (status == MMSYSERR_NOERROR) {				
				midiOutClose(handle);
			}
		}
		/****/
		inline virtual void sendMessage(message_t const& message) {
			if (!getStatus()) return;
			visit(visitor{
				[&](keyDownMessage_t const& msg) {
					winMM_message data {.data = { (BYTE)(0x90 | msg.channel), (BYTE)msg.note, (BYTE)msg.velocity} };
					midiOutShortMsg(handle, data.param);
				},
				[&](keyUpMessage_t const& msg) {
					winMM_message data {.data = { (BYTE)(0x80 | msg.channel), (BYTE)msg.note, (BYTE)0 } };
					midiOutShortMsg(handle, data.param);
				},
				[&](programChangeMessage_t const& msg) {
					winMM_message data {.data = { (BYTE)(0xC0 | msg.channel), (BYTE)msg.program, (BYTE)0 } };
					midiOutShortMsg(handle, data.param);
				}
			}, message);
		}
		inline virtual std::string getMidiErrorMessage() {
			static char buffer[1024];
			midiOutGetErrorText(status, buffer, sizeof(buffer));
			return buffer;
		}
		inline virtual void getMidiOutDevices(midiOutputDevices_t& devices) {
			devices.resize(midiOutGetNumDevs());
			uint32_t i = 0;
			for (auto& index : devices) {
				MIDIOUTCAPS caps;
				midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS));
				index.index = i++; index.name = index.id = caps.szPname;
			}
		}
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
				ctx->messages.push(keyDownMessage_t{ 0, message.as<MidiNoteOnMessage>().Note(), message.as<MidiNoteOnMessage>().Velocity() });
				break;
			case MidiMessageType::NoteOff:
				ctx->messages.push(keyUpMessage_t{ 0, message.as<MidiNoteOffMessage>().Note() });
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
					port.SendBuffer(MidiNoteOffMessage(msg.channel, msg.note, 0).RawData());
				},
				[&](programChangeMessage_t const& msg) {
					port.SendBuffer(MidiProgramChangeMessage(msg.channel, msg.program).RawData());
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
#endif // WINRT
}