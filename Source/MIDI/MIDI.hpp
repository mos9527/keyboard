#pragma once
namespace midi {
	const size_t MAX_CHANNEL_COUNT = 16;
	const size_t MAX_SYSEX_MESSAGE_SIZE = 2048;

	using namespace std;
	struct inputDevice_t { uint32_t index; string name; string id; };
	using outputDevice_t = inputDevice_t;
	typedef vector<inputDevice_t> midiInputDevices_t;
	typedef vector<outputDevice_t> midiOutputDevices_t;
	/****/
	struct keyDownMessage_t { uint8_t channel, note, velocity; };
	struct keyUpMessage_t { uint8_t channel, note, velocity; };
	struct programChangeMessage_t { uint8_t channel, program; };
	struct pitchWheelMessage_t { uint8_t channel; unsigned short level; };
	struct controllerMessage_t { uint8_t channel, controller, value; };
	using sysexMessage_t = shared_ptr<fixed_vector<char, MAX_SYSEX_MESSAGE_SIZE>>;
	using message_t = variant<keyDownMessage_t, keyUpMessage_t, programChangeMessage_t, pitchWheelMessage_t, controllerMessage_t, sysexMessage_t, nullopt_t>;
	inline message_t from_midi1_packet(uint8_t status, uint8_t lo, uint8_t hi) {		
		uint8_t msg = status >> 4, channel = status & 0xF;
		switch (msg)
		{
		case 0x8:
			return keyUpMessage_t{ channel, lo, hi };
		case 0x9:
			return keyDownMessage_t{ channel, lo, hi };
		case 0xB:
			return controllerMessage_t{ channel, lo, hi };
		case 0xC:
			return programChangeMessage_t{ channel, lo };
		case 0xE:
		{
			pitchWheelMessage_t msg{ .channel = channel };
			msg.level = (hi & 0b01111111); msg.level <<= 7; msg.level |= (lo & 0b01111111);
			return msg;
		}
		default:
			break;
		}
		return nullopt;
	}
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
}
