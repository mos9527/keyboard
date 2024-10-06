#pragma once
namespace winmidi {
	using namespace std;
	static const char* getMidiErrorMessage(MMRESULT error) {
		static char buffer[1024];
		midiInGetErrorText(error, buffer, sizeof(buffer));
		return buffer;
	}
	typedef vector<pair<uint32_t,MIDIINCAPS>> devices_t;
	struct midiMessage_t {
		uint64_t type, param1, param2;
	};
	struct midiInContext {
	private:
		uint32_t device;
		HMIDIIN handle;
		MMRESULT status = -1;
		queue<midiMessage_t> messages;
		condition_variable messageCV;
		mutex messageMutex;

		static void CALLBACK MidiInProc(
			HMIDIIN   hMidiIn,
			UINT      wMsg,
			midiInContext* ctx,
			DWORD_PTR dwParam1,
			DWORD_PTR dwParam2
		) {
			unique_lock<mutex> lock(ctx->messageMutex);
			ctx->messages.push(midiMessage_t{ wMsg, dwParam1, dwParam2 });
			ctx->messageCV.notify_one();
		};
	public:
		static const MMRESULT STATUS_OK = MMSYSERR_NOERROR;
		const uint32_t getDevice() const { return device; }
		const MMRESULT getStatus() const { return status; }
		optional<midiMessage_t> poolMessage(bool blocking=false) {
			if (getStatus() != STATUS_OK)
				return {};
			unique_lock<mutex> lock(messageMutex);
			if (blocking) messageCV.wait(lock, [this] { return !messages.empty(); });
			else if (messages.empty()) return {};
			auto message = messages.front();
			messages.pop();
			return message;
		}
		midiInContext(uint32_t device) : device(device) {
			status = midiInOpen(&handle, device, (DWORD_PTR)MidiInProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
			if (status == STATUS_OK) midiInStart(handle);
		}
		~midiInContext() {
			if (status == STATUS_OK) {
				midiInStop(handle);
				midiInClose(handle);
			}
		}
	};
	const devices_t getMidiInDevices() {
		devices_t devices(midiInGetNumDevs());
		uint32_t i = 0;
		for (auto& device : devices) {
			device.first = i;
			midiInGetDevCaps(i++, &device.second, sizeof(MIDIINCAPS));
		}
		return devices;
	} // namespace winmidi
}