#include "midi.hpp"
#include "pch.hpp"
namespace midi {
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
			case MIM_LONGDATA:
			{
				MIDIHDR* hdr = (MIDIHDR*)dwParam1;
				ctx->messages.push(make_shared<sysExMessage::element_type>(
					*(char**)hdr->lpData, hdr->dwBytesRecorded
				));
				break;
			}
			case MIM_DATA: {
				winMM_message message{ .param = (DWORD)dwParam1 };
				uint8_t hi = message.data[2], lo = message.data[1], status = message.data[0];				
				auto msg = midi1_packet(status, lo, hi);
				ctx->messages.push(msg);
				ctx->messageCV.notify_one();
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
			midiInGetErrorTextA(status, buffer, sizeof(buffer));
			return buffer;
		}
		inline virtual void getMidiInDevices(midiInputDevices_t& devices) {
			devices.resize(midiInGetNumDevs());
			uint32_t i = 0;
			for (auto& index : devices) {
				MIDIINCAPSA caps;
				midiInGetDevCapsA(i, &caps, sizeof(MIDIINCAPSA));
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
			auto packet = midi1_packet(message);
			winMM_message data{ .data = { packet.status, packet.lo, packet.hi } };
			midiOutShortMsg(handle, data.param);
		}
		inline virtual std::string getMidiErrorMessage() {
			static char buffer[1024];
			midiOutGetErrorTextA(status, buffer, sizeof(buffer));
			return buffer;
		}
		inline virtual void getMidiOutDevices(midiOutputDevices_t& devices) {
			devices.resize(midiOutGetNumDevs());
			uint32_t i = 0;
			for (auto& index : devices) {
				MIDIOUTCAPSA caps;
				midiOutGetDevCapsA(i, &caps, sizeof(MIDIOUTCAPSA));
				index.index = i++; index.name = index.id = caps.szPname;
			}
		}
	};
}