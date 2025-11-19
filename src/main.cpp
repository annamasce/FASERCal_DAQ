#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <iomanip>
#include <array>

enum WordID {
    GATE_HEADER = 0x0,
    GTS_HEADER = 0x1,
    HIT_TIME = 0x2,
    HIT_AMPLITUDE = 0x3,
    GTS_TRAILER1 = 0x4,
    GTS_TRAILER2 = 0x5,
    GATE_TRAILER = 0x6,
    GATE_TIME = 0x7,
    OCB_PACKET_HEADER = 0x8,
    OCB_PACKET_TRAILER = 0x9,
    HOLD_TIME = 0xB,
    EVENT_DONE = 0xC,
    FEB_DATA_PACKET_TRAILER = 0xD,
    HOUSEKEEPING = 0xE,
    SPECIAL_WORD = 0xF
};

// Simple utility to convert 4 bytes to uint32_t using little-endian interpretation
static uint32_t bytes_to_uint32(const unsigned char buf[4], bool big_endian = false) {
    if (big_endian) {
        return (uint32_t)buf[3]
         | ((uint32_t)buf[2] << 8)
         | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[0] << 24);
    }
    else {
        return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
    }
}

static uint32_t get_bits(int value, int start, int length){
    return (value >> start) & ((1 << length) - 1);
}

WordID get_wordID(uint32_t word) {
    // Extract word ID as the most significant 4 bits (28...31) of the 32-bit word
    return WordID(get_bits(word, 28, 4));
}

class Word {
    public:
        WordID word_id;

        Word(uint32_t raw, WordID expected) : word_id(expected) {
            WordID received = get_wordID(raw);
            if (received != expected) {
                throw std::runtime_error("Wrong word ID: received " + std::to_string(received) + ", expected: " + std::to_string(expected));
            }
        }

        virtual ~Word() = default;

        virtual void print() const = 0;
        
};

class GateHeader : public Word {
    public:

        static constexpr WordID word_id = WordID::GATE_HEADER;
        uint32_t header_type;
        uint32_t board_id;
        uint32_t gate_type = 0;
        uint32_t gate_number = 0;
        uint32_t gate_time_from_GTS = 0;

        GateHeader(uint32_t raw) : Word(raw, word_id) {
            board_id = get_bits(raw, 20, 8);
            header_type = get_bits(raw, 19, 1);

            if (header_type == 0) { 
                gate_type  = get_bits(raw, 16, 3);
                gate_number = get_bits(raw, 0, 16);
            } else {
                gate_time_from_GTS = get_bits(raw, 0, 11);
            }
        }

        void print() const override {
            if( header_type == 0. ) {
                std::cout << "[" << std::setw(6) << word_id << "]" << " Gate Header Word - Board ID: " << board_id
                          << ", Gate header: " << header_type
                          << ", Gate Type: " << gate_type
                          << ", Gate Number: " << gate_number << "\n";
            } else {
                    std::cout << "[" << std::setw(6) << word_id << "]" << " Gate Header Word - Board ID: " << board_id
                              << ", Gate header: " << header_type
                              << ", Gate time: " << gate_time_from_GTS << "\n";
            }
        }
       
};

class GTSHeader : public Word {
    public:
        static constexpr WordID word_id = WordID::GTS_HEADER;
        uint32_t gts_tag;

        GTSHeader(uint32_t raw) : Word(raw, word_id) {
            gts_tag = get_bits(raw, 0, 28);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " GTS Header Word - GTS Tag: " << gts_tag << "\n";
        }
};

class HitTime : public Word {
    public:
        static constexpr WordID word_id = WordID::HIT_TIME;
        uint32_t channel_id, hit_id, tag_id, edge, hit_time;

        HitTime(uint32_t raw) : Word(raw, word_id) {
            channel_id = get_bits(raw, 20, 8);
            hit_id = get_bits(raw, 17, 3);
            tag_id = get_bits(raw, 15, 2);
            edge = get_bits(raw, 14, 1);
            hit_time = get_bits(raw, 0, 13);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " Hit Time Word - Channel ID: " << channel_id
                      << ", Hit ID: " << hit_id
                      << ", Tag ID: " << tag_id
                      << ", Edge: " << edge
                      << ", Timestamp: " << hit_time << "\n";
        }
};

class HitAmplitude : public Word {
    public:
        static constexpr WordID word_id = WordID::HIT_AMPLITUDE;
        uint32_t channel_id, hit_id, tag_id, amplitude_id, amplitude_value;

    HitAmplitude(uint32_t raw) : Word(raw, word_id) {
        channel_id = get_bits(raw, 20, 8);
        hit_id = get_bits(raw, 17, 3);
        tag_id = get_bits(raw, 15, 2);
        amplitude_id = get_bits(raw, 12, 3);
        amplitude_value = get_bits(raw, 0, 12);
    }

    void print() const override {
        std::cout << "[" << std::setw(6) << word_id << "]" << " Hit Time Word - Channel ID: " << channel_id
                  << ", Hit ID: " << hit_id
                  << ", Tag ID: " << tag_id
                  << ", Amp ID: " << amplitude_id
                  << ", Amplitude: " << amplitude_value << "\n";
    }

};

class GTSTrailer1 : public Word {
    public:
        static constexpr WordID word_id = WordID::GTS_TRAILER1;
        uint32_t gts_tag;

        GTSTrailer1(uint32_t raw) : Word(raw, word_id) {
            gts_tag = get_bits(raw, 0, 28);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " GTS Trailer1 Word - GTS Tag: " << gts_tag << "\n";
        }
};

class GTSTrailer2 : public Word {
    public:
        static constexpr WordID word_id = WordID::GTS_TRAILER2;
        uint32_t data, ocb_busy, feb_busy, gts_time;

        GTSTrailer2(uint32_t raw) : Word(raw, word_id) {
            data = get_bits(raw, 27, 1);
            ocb_busy = get_bits(raw, 26, 1);
            feb_busy = get_bits(raw, 25, 1);
            gts_time = get_bits(raw, 0, 20);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " GTS Trailer 2 Word - Data: " << data
                      << ", OCB busy: " << ocb_busy
                      << ", FEB busy: " << feb_busy 
                      << ", GTS Time: " << gts_time << "\n";
        }
};

class GateTrailer : public Word {
    public:
        static constexpr WordID word_id = WordID::GATE_TRAILER;
        uint32_t board_id, gate_type, gate_number;

        GateTrailer(uint32_t raw) : Word(raw, word_id) {
            board_id = get_bits(raw, 20, 8);
            gate_type = get_bits(raw, 16, 3);
            gate_number = get_bits(raw, 0, 16);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " Gate Trailer Word - Board ID: " << board_id
                      << ", Gate Type: " << gate_type
                      << ", Gate Number: " << gate_number << "\n";
        }     

};

class GateTime : public Word {
    public:
        static constexpr WordID word_id = WordID::GATE_TIME;
        uint32_t gate_time;

        GateTime(uint32_t raw) : Word(raw, word_id) {
            gate_time = get_bits(raw, 0, 28);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " Gate Time Word - Gate time: " << gate_time << "\n";
        }
};

class OCBPacketHeader : public Word {
    public:
        static constexpr WordID word_id = WordID::OCB_PACKET_HEADER;
        uint32_t gate_type, gate_tag, event_number;

        OCBPacketHeader(uint32_t raw) : Word(raw, word_id) {
            gate_type = get_bits(raw, 25, 3);
            gate_tag = get_bits(raw, 23, 2);
            event_number = get_bits(raw, 0, 23);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " OCB Packet Header Word - Gate type: " << gate_type
                    << ", Gate Tag: " << gate_tag
                    << ", Event number: " << event_number << "\n";
        }
};

class OCBPacketTrailer : public Word {
    public:
        static constexpr WordID word_id = WordID::OCB_PACKET_TRAILER;
        uint32_t gate_type, gate_tag;
        std::array<bool, 15> errors;

        OCBPacketTrailer(uint32_t raw) : Word(raw, word_id) {
            gate_type = get_bits(raw, 25, 3);
            gate_tag = get_bits(raw, 23, 2);
            for (size_t i = 0; i < errors.size(); i++){
                errors[i] = get_bits(raw, i, 1);
            }
        }

        void print() const override{
            int n_errors = 0;
            for (size_t i = 0; i < errors.size(); i++) {
                if (errors[i]) n_errors += 1;
            }
            std::cout << "[" << std::setw(6) << word_id << "]" << " OCB Packet Trailer Word - Gate type: " << gate_type
                    << ", Gate Tag: " << gate_tag
                    << ", Number of errors: " << n_errors << "\n";
        }
};

class HoldTime : public Word {
    public:
        static constexpr WordID word_id = WordID::HOLD_TIME;
        uint32_t board_id, header_type, hold_time;

        HoldTime(uint32_t raw) : Word(raw, word_id) {
            board_id = get_bits(raw, 20, 8);
            header_type = get_bits(raw, 19, 1);
            hold_time = get_bits(raw, 0, 11);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " HOLD Time Word - Board ID: " << board_id
                     << ", Header Type (start/stop): " << header_type
                     << ", HOLD time: " << hold_time << "\n";
        }
};

class EventDone : public Word {
    public:
        static constexpr WordID word_id = WordID::EVENT_DONE;
        uint32_t board_id, gate_number, word_count;

        EventDone(uint32_t raw) : Word(raw, word_id) {
            board_id = get_bits(raw, 20, 8);
            gate_number = get_bits(raw, 16, 4);
            word_count = get_bits(raw, 0, 16);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << "Event Done Word - Board ID: " << board_id 
                      << ", Gate Number (4LSB): " << gate_number
                      << ", Word Count: " << word_count << "\n";
        }
};

class FEBDataPacketTrailer : public Word {
    public:
        static constexpr WordID word_id = WordID::FEB_DATA_PACKET_TRAILER;
        uint_fast32_t board_id, nb_decoder_errors;
        bool event_done_timeout, d1_fifo_full, d0_fifo_full;

        FEBDataPacketTrailer(uint32_t raw) : Word(raw, word_id) {
            board_id = get_bits(raw, 20, 8);
            event_done_timeout = get_bits(raw, 18, 1);
            d1_fifo_full = get_bits(raw, 17, 1);
            d0_fifo_full = get_bits(raw, 16, 1);
            nb_decoder_errors = get_bits(raw, 0, 15);
        }

        void print() const override{
            std::cout << "[" << std::setw(6) << word_id << "]" << " FEB Data Packet Trailer - Board ID: " << board_id << "\n";
        }
};

// Defining WordFactory as a pointer to a function uint32_t -> std::unique_ptr<Word>
using WordFactory = std::unique_ptr<Word>(*)(uint32_t);

template <class T>
std::unique_ptr<Word> construct(uint32_t raw) {
    return std::make_unique<T>(raw);
}

std::unordered_map<WordID, WordFactory> WORD_CLASSES = {
    { WordID::GATE_HEADER,        &construct<GateHeader> },
    { WordID::GATE_TIME,          &construct<GateTime> },
    { WordID::HOLD_TIME,          &construct<HoldTime> },
    { WordID::GTS_HEADER,         &construct<GTSHeader> },
    { WordID::HIT_TIME,           &construct<HitTime> },
    { WordID::HIT_AMPLITUDE,      &construct<HitAmplitude> },
    { WordID::GTS_TRAILER1,       &construct<GTSTrailer1> },
    { WordID::GTS_TRAILER2,       &construct<GTSTrailer2> },
    { WordID::GATE_TRAILER,       &construct<GateTrailer> },
    { WordID::EVENT_DONE,         &construct<EventDone> },
    { WordID::OCB_PACKET_HEADER,  &construct<OCBPacketHeader> },
    { WordID::OCB_PACKET_TRAILER, &construct<OCBPacketTrailer> },
    { WordID::FEB_DATA_PACKET_TRAILER, &construct<FEBDataPacketTrailer> }
};

std::unique_ptr<Word> parse_word(uint32_t word) {
    WordID id = get_wordID(word);

    auto class_constructor = WORD_CLASSES.find(id);
    if (class_constructor == WORD_CLASSES.end())
        throw std::runtime_error("Unknown WordID: " + std::to_string(id));

    std::unique_ptr<Word> word_object = class_constructor->second(word);
    word_object->print();
    return word_object;


    // switch (id) {
    //     case WordID::GATE_HEADER:
    //         std::cout << "[ID=0x0] Gate header\n";
    //         // process_gate_header_word(word);
    //         GateHeader(word).print();
    //         break;
    //     case WordID::GTS_HEADER:
    //         std::cout << "[ID=0x1] GTS header\n";
    //         GTSHeader(word).print();
    //         break;
    //     case WordID::HIT_TIME:
    //         std::cout << "[ID=0x2] Hit time\n";
    //         HitTime(word).print();
    //         break;
    //     case WordID::HIT_AMPLITUDE:
    //         std::cout << "[ID=0x3] Hit amplitude\n";
    //         HitAmplitude(word).print();
    //         break;
    //     case WordID::GTS_TRAILER1:
    //         std::cout << "[ID=0x4] GTS trailer 1\n";
    //         GTSTrailer1(word).print();
    //         break;
    //     case WordID::GTS_TRAILER2:
    //         std::cout << "[ID=0x5] GTS trailer 2\n";
    //         GTSTrailer2(word).print();
    //         break;
    //     case WordID::GATE_TRAILER:
    //         std::cout << "[ID=0x6] Gate trailer\n";
    //         GateTrailer(word).print();
    //         break;
    //     case WordID::GATE_TIME:
    //         std::cout << "[ID=0x7] Gate time\n";
    //         GateTime(word).print();
    //         break;
    //     case WordID::OCB_PACKET_HEADER:
    //         std::cout << "[ID=0x8] OCB data packet header\n";
    //         OCBPacketHeader(word).print();
    //         break;
    //     case WordID::OCB_PACKET_TRAILER:
    //         std::cout << "[ID=0x9] OCB data packet trailer\n";
    //         OCBPacketTrailer(word).print();
    //         break;
    //     case WordID::HOLD_TIME:
    //         std::cout << "[ID=0xB] HOLD time\n";
    //         HoldTime(word).print();
    //         break;

}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <binary-file> [--be]\n";
        std::cerr << "  By default reads 32-bit words as little-endian. Use --be for big-endian.\n";
        return 1;
    }

    const char* path = argv[1];
    bool big_endian = false;
    if (argc >= 3 && std::string(argv[2]) == "--be") big_endian = true;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open file: " << path << "\n";
        return 2;
    }

    unsigned char buf[4];
    // size_t index = 0;
    std::vector<std::unique_ptr<Word>> word_list;
    while (in.read(reinterpret_cast<char*>(buf), 4)) {
        uint32_t word = bytes_to_uint32(buf, big_endian);
        // std::cout << "[" << std::setw(6) << index << "] 0x"
        //           << std::hex << std::setfill('0') << std::setw(8) << word
        //           << std::dec << std::setfill(' ') << "\n";
        // Dispatch processing based on top 4 bits (word ID)
        word_list.push_back(parse_word(word));
        // std::cout << "\n";
        // ++index;
    }

    for (auto const& w : word_list) {
        w->print();
    }

    // // If there's a partial tail (file size not multiple of 4), read remaining bytes
    // std::streamsize rem = in.gcount();
    // if (rem > 0) {
    //     // shift remaining into a zero-filled buffer
    //     unsigned char tail[4] = {0,0,0,0};
    //     for (std::streamsize i = 0; i < rem; ++i) tail[i] = buf[i];
    //     uint32_t word = bytes_to_uint32(tail, big_endian);
    //     std::cout << "[" << std::setw(6) << index << "] 0x"
    //               << std::hex << std::setfill('0') << std::setw(8) << word
    //               << " (partial, " << rem << " bytes)" << std::dec << std::setfill(' ') << "\n";
    // // Process the final partial word as well
    // parse_word(word);
    // }



    return 0;
}
