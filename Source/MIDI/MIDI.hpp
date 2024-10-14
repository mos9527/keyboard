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
	struct noteOnMessage { uint8_t channel, note, velocity; };
	struct noteOffMessage { uint8_t channel, note, velocity; };
	struct pitchBendMessage { uint8_t channel; unsigned short level; };
	struct programChangeMessage { uint8_t channel, program; };
	struct controlChangeMessage { uint8_t channel, controller, value; };
	using sysExMessage = shared_ptr<fixed_vector<char, MAX_SYSEX_MESSAGE_SIZE>>;
	using message_t = variant<noteOnMessage, noteOffMessage, programChangeMessage, pitchBendMessage, controlChangeMessage, sysExMessage, nullopt_t>;	
	struct midi1_packet { 
		uint8_t status, lo, hi;
		midi1_packet() = default;
		explicit midi1_packet(uint8_t status, uint8_t lo, uint8_t hi) : status(status), lo(lo), hi(hi) {};
		explicit midi1_packet(message_t const& msg) {
			visit(visitor{
				[&](noteOnMessage const& msg) {
					status = (BYTE)(0x90 | msg.channel), lo = msg.note, hi = msg.velocity;					
				},
				[&](noteOffMessage const& msg) {
					status = (BYTE)(0x80 | msg.channel), lo = msg.note, hi = msg.velocity;					
				},
				[&](programChangeMessage const& msg) {
					status = (BYTE)(0xC0 | msg.channel), lo = msg.program, hi = 0;					
				},
				[&](pitchBendMessage const& msg) {
					status = (BYTE)(0xE0 | msg.channel), lo = msg.level & 0x7F, hi = msg.level >> 7;					
				},
				[&](controlChangeMessage const& msg) {
					status = (BYTE)(0x80 | msg.channel), lo = msg.controller, hi = msg.value;					
				},
			}, msg);
		}
		inline operator message_t() {
			uint8_t msg = status >> 4, channel = status & 0xF;
			switch (msg)
			{
			case 0x8:
				return noteOffMessage{ channel, lo, hi };
			case 0x9:
				return noteOnMessage{ channel, lo, hi };
			case 0xB:
				return controlChangeMessage{ channel, lo, hi };
			case 0xC:
				return programChangeMessage{ channel, lo };
			case 0xE:
			{
				pitchBendMessage msg{ .channel = channel };
				msg.level = (hi & 0b01111111); msg.level <<= 7; msg.level |= (lo & 0b01111111);
				return msg;
			}
			default:
				break;
			}
			return nullopt;
		}
	};	
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
