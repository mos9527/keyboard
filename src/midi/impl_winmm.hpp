#include "midi.hpp"
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
				ctx->messages.push(make_shared<sysexMessage_t::element_type>(
					*(char**)hdr->lpData, hdr->dwBytesRecorded
				));
				break;
			}
			case MIM_DATA: {
				winMM_message message{ .param = (DWORD)dwParam1 };
				uint8_t hi = message.data[2], lo = message.data[1], status = message.data[0];
				uint8_t msg = status >> 4, channel = status & 0xF;
				switch (msg)
				{
				case 0x8:
					ctx->messages.push(keyUpMessage_t{ channel, lo, hi }); break;
				case 0x9:
					ctx->messages.push(keyDownMessage_t{ channel, lo, hi }); break;
				case 0xB:
					ctx->messages.push(controllerMessage_t{ channel, lo, hi }); break;
				case 0xC:
					ctx->messages.push(programChangeMessage_t{ channel, lo }); break;
				case 0xE:
				{
					pitchWheelMessage_t msg{ .channel = channel };
					msg.level = (hi & 0b01111111); msg.level <<= 7; msg.level |= (lo & 0b01111111);
					ctx->messages.push(msg);
					break;
				}
				default:
					break;
				}
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
					winMM_message data {.data = { (BYTE)(0x80 | msg.channel), (BYTE)msg.note, (BYTE)msg.velocity } };
					midiOutShortMsg(handle, data.param);
				},
				[&](programChangeMessage_t const& msg) {
					winMM_message data {.data = { (BYTE)(0xC0 | msg.channel), (BYTE)msg.program, (BYTE)0 } };
					midiOutShortMsg(handle, data.param);
				},
				[&](pitchWheelMessage_t const& msg) {
					winMM_message data {.data = { (BYTE)(0xE0 | msg.channel), (BYTE)(msg.level & 0x7F), (BYTE)(msg.level >> 7) } };
					midiOutShortMsg(handle, data.param);
				},
				[&](controllerMessage_t const& msg) {
					winMM_message data {.data = { (BYTE)(0xB0 | msg.channel), (BYTE)msg.controller, (BYTE)msg.value } };
					midiOutShortMsg(handle, data.param);
				},
				[&](sysexMessage_t const& msg) {
					MIDIHDR hdr{};
					hdr.lpData = msg->data();
					hdr.dwBufferLength = hdr.dwBytesRecorded = msg->size();
					midiOutLongMsg(handle, &hdr, sizeof(MIDIHDR));
				},
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
}