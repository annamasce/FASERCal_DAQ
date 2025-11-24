#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <iomanip>
#include <array>
#include <map>
#include "Word.h"


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

void check_expected_word(uint32_t word, WordID expected_id) {
    std::unique_ptr<Word> word_object = parse_word(word);
    if (word_object->word_id != expected_id) {
        throw std::runtime_error("Wrong word type received: wordID = " + std::to_string(word_object->word_id) + ", expected = " + std::to_string(expected_id));
    }
}

// struct to define the key for a group of words belonging to the same hit
// NOTE: within the same GTS packet, hit ID and channel ID univoquely identify the same signal
struct HitKey {
    uint32_t hit_id;
    uint32_t channel_id;

    bool operator<(const HitKey& other) const noexcept {
        if (hit_id < other.hit_id) return true;
        if (hit_id > other.hit_id) return false;
        return channel_id < other.channel_id;
    }
};

class HitData {
    private:
        std::vector<uint32_t> _words;
    public:
        uint32_t tag_id, hit_id, channel_id, feb_id, hit_time_rise, hit_time_fall, amplitude_low, amplitude_up;
        HitData(std::vector<uint32_t> word_list) : _words(word_list) {
            // TO DO: fill hit information from word list
            // Is it possible to have more than 1 timestamp and only 1 amplitude?? What should I do in that case??
        }
};

class GTSDataPacket {
    private:
        std::vector<uint32_t> _words;
        // std::map<HitKey, std::vector<Word*> > _hit_groups;
        std::map<HitKey, std::vector<uint32_t> > _hit_groups;

        public:
            uint32_t gts_tag;

            GTSDataPacket(std::vector<uint32_t> word_list) : _words(word_list) {
                // std::cout << "### GTS data packet starts:" << std::endl;
                // for(auto& w: _words) {parse_word(w)->print();}
                if (_words.size() < 3) {
                    throw std::runtime_error("Size of GTS packet is not enough to contain header and trailers!");
                }
                check_expected_word(_words.front(), WordID::GTS_HEADER);
                check_expected_word(_words.at(_words.size() - 2), WordID::GTS_TRAILER1);
                check_expected_word(_words.back(),  WordID::GTS_TRAILER2);

                // Check that GTS tag in Header and Trailer1 are the same
                std::unique_ptr<Word> gts_header_word = parse_word(_words.front());
                auto* gts_header  = dynamic_cast<GTSHeader*>(gts_header_word.get());
                std::unique_ptr<Word> gts_trailer1_word = parse_word(_words.at(_words.size() - 2));
                auto* gts_trailer1 = dynamic_cast<GTSTrailer1*>(gts_trailer1_word.get());
                if (gts_header->gts_tag != gts_trailer1->gts_tag) {
                    throw std::runtime_error("GTS tag does not match between GTS header and trailer 1!");
                }
                
                gts_tag = gts_header->gts_tag;

                for (auto& w : _words) {
                    std::unique_ptr<Word> base = parse_word(w);
                    WordID id = base->word_id;

                    if (id == WordID::HIT_TIME || id == WordID::HIT_AMPLITUDE) {
                        HitKey key;

                        if (id == WordID::HIT_TIME) {
                            auto* hit = dynamic_cast<HitTime*>(base.get());
                            key.hit_id = hit->hit_id;
                            key.channel_id = hit->channel_id;
                        }
                        else { // HitAmplitude
                            auto* hit = dynamic_cast<HitAmplitude*>(base.get());
                            key.hit_id = hit->hit_id;
                            key.channel_id = hit->channel_id;
                        }

                        // --- Grouping: add to map ---
                        // _hit_groups[key].push_back(base.get());
                        _hit_groups[key].push_back(w);
                    }
                }

            }
            
            std::vector<uint32_t> get_words() {return _words;}

            std::map<HitKey, std::vector<uint32_t> > get_hits() {return _hit_groups;}

};

class FEBDataPacket {
    private:
        std::vector<uint32_t> _words;
        std::vector<GTSDataPacket> _gts_packets;
    public:
        int hold_time = -1;
        FEBDataPacket(std::vector<uint32_t> word_list) : _words(word_list) {

            check_expected_word(_words.front(), WordID::GATE_HEADER);
            check_expected_word(_words.back(),  WordID::FEB_DATA_PACKET_TRAILER);
            if (parse_word(_words.at(1))->word_id == WordID::HOLD_TIME){
                std::unique_ptr<Word> hold_time_word = parse_word(_words.at(1));
                auto* hold_time_object  = dynamic_cast<HoldTime*>(hold_time_word.get());
                hold_time = hold_time_object->hold_time;
            }

            int previous_gts_tag = -1; 
            int current_gts_tag = -1;
            
            std::vector<uint32_t> current_gts_packet;
            std::vector<uint32_t> previous_gts_packet;
            constexpr int TAG_MASK = 0x3;

            for (auto& w : _words) {
                std::unique_ptr<Word> base = parse_word(w);
                WordID id = base->word_id;

                // Filling gts packets with corresponding hits using the hit tag_id (2LSB must be equal to gts_tag)
                // NOTE: need to store previous gts_id and packet because hit data can be received after GTS trailer
                if (id == WordID::GTS_HEADER) {
                    // Parse header
                    auto* gts_header = dynamic_cast<GTSHeader*>(base.get());
                    current_gts_tag = gts_header->gts_tag;

                    // Start new packet with header as first word
                    current_gts_packet.push_back(w);
                }

                else if (id == WordID::HIT_TIME || id == WordID::HIT_AMPLITUDE) {
                    uint32_t hit_tag = (id == WordID::HIT_TIME)
                            ? dynamic_cast<HitTime*>(base.get())->tag_id
                            : dynamic_cast<HitAmplitude*>(base.get())->tag_id;
                    
                    if ((hit_tag & TAG_MASK) == (current_gts_tag & TAG_MASK)) {
                        current_gts_packet.push_back(w);
                    }
                    else if ((hit_tag & TAG_MASK) == (previous_gts_tag & TAG_MASK)) {
                        previous_gts_packet.push_back(w);
                    }
                    else {
                        throw std::runtime_error("Hit tag does not match current or previous GTS tag!");
                    }
                }

                // --- GTS TRAILER 1 --------------------------------------------------------
                else if (id == WordID::GTS_TRAILER1) {
                    auto* gts_trailer1 = dynamic_cast<GTSTrailer1*>(base.get());
                    if ((int) gts_trailer1->gts_tag != current_gts_tag) {
                        throw std::runtime_error("GTS tag in Trailer1 different from Current GTS Header!");
                    }
                    current_gts_packet.push_back(w);
                }

                // --- GTS TRAILER 2 --------------------------------------------------------
                else if (id == WordID::GTS_TRAILER2) {
                    if (current_gts_packet.empty()) {
                        throw std::runtime_error("GTS Trailer received without corresponding GTS Header!");
                    }
                    current_gts_packet.push_back(w);

                    if (!previous_gts_packet.empty()) {
                        // Close out previous packet
                        _gts_packets.push_back(GTSDataPacket(previous_gts_packet));
                        previous_gts_packet.clear();
                    }

                    // Current becomes previous
                    previous_gts_tag = current_gts_tag;
                    previous_gts_packet = std::move(current_gts_packet);

                    current_gts_packet.clear();
                }
            }

            if (!previous_gts_packet.empty()) {
                // Close out last GTS packet
                _gts_packets.push_back(GTSDataPacket(previous_gts_packet));
                previous_gts_packet.clear();
            }

            if (!current_gts_packet.empty()) {
                // Close out current GTS packet even if there is no Trailer2 ??
                _gts_packets.push_back(GTSDataPacket(current_gts_packet));
                current_gts_packet.clear();
            }
        }

        std::vector<GTSDataPacket> get_gts_packets() {return _gts_packets;}

};

class OCBDataPacket {
    private:
        std::vector<uint32_t> _words;
        std::vector<FEBDataPacket> _feb_packets;

    public:
        uint32_t gate_type, gate_tag, event_number;
        
        OCBDataPacket(std::vector<uint32_t> word_list) : _words(word_list) {

            check_expected_word(_words.front(), WordID::OCB_PACKET_HEADER);
            check_expected_word(_words.back(),  WordID::OCB_PACKET_TRAILER);

            std::unique_ptr<Word> header_word = parse_word(_words.front());
            std::unique_ptr<Word> trailer_word = parse_word(_words.back());
            auto* ocb_packet_header  = dynamic_cast<OCBPacketHeader*>(header_word.get());
            auto* ocb_packet_trailer = dynamic_cast<OCBPacketTrailer*>(trailer_word.get());

            if (ocb_packet_header->gate_type != ocb_packet_trailer->gate_type) {
                throw std::runtime_error("Different gate type in OCB packet header and trailer!");
            }
            if (ocb_packet_header->gate_tag != ocb_packet_trailer->gate_tag) {
                throw std::runtime_error("Different gate tag in OCB packet header and trailer!");
            }
            gate_type = ocb_packet_header->gate_type;
            gate_tag = ocb_packet_header->gate_tag;
            event_number = ocb_packet_header->event_number;

            int index = 0;
            int start_index = -1;
            for (auto& w : _words){
                if (parse_word(w)->word_id == WordID::GATE_HEADER) {
                    start_index = index;
                }
                if (parse_word(w)->word_id == WordID::FEB_DATA_PACKET_TRAILER) {
                    if (start_index < 0) {
                        throw std::runtime_error("FEB Data Packet Trailer received without corresponding Gate Header");
                    }
                    std::vector<uint32_t> feb_packet_word_list;
                    for (int k = start_index; k < index+1; ++k) {
                        feb_packet_word_list.push_back(_words[k]);
                    }
                    _feb_packets.push_back(FEBDataPacket(feb_packet_word_list));
                    start_index = -1;
                }
                index++;
            }

        }

        std::vector<FEBDataPacket> get_feb_packets() {return _feb_packets;}
};


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
    // std::vector<std::unique_ptr<Word>> word_list;
    std::vector<uint32_t> word_list;
    std::vector<OCBDataPacket> ocb_packets;
    int index = 0;
    int start_index = -1;
    while (in.read(reinterpret_cast<char*>(buf), 4)) {
        uint32_t word = bytes_to_uint32(buf, big_endian);
        word_list.push_back(word);
        if (parse_word(word)->word_id == WordID::OCB_PACKET_HEADER) {
            start_index = index;
        }
        if (parse_word(word)->word_id == WordID::OCB_PACKET_TRAILER) {
            if (start_index < 0) {
                throw std::runtime_error("OCB Packet Trailer received without corresponding Header");
            }
            // std::vector<std::unique_ptr<Word>> ocb_packet_word_list;
            std::vector<uint32_t> ocb_packet_word_list;
            for (int k = start_index; k < index+1; ++k)
                ocb_packet_word_list.push_back(word_list[k]);

            ocb_packets.push_back(OCBDataPacket(ocb_packet_word_list));            
            std::vector<FEBDataPacket> feb_packets = OCBDataPacket(ocb_packet_word_list).get_feb_packets();
            std::cout << "Total number of FEB packets: " << feb_packets.size() << std::endl;
            for(auto& feb : feb_packets){
                std::cout << "### Starts FEB Packet" << std::endl;
                std::vector<GTSDataPacket> gts_packets = feb.get_gts_packets();
                std::cout << "Total number of GTS packets: " << gts_packets.size() << std::endl;
                for(auto& gts : gts_packets) {
                    std::cout << "### Starts GTS Packet with GTS Tag " << gts.gts_tag << std::endl;
                    std::map<HitKey, std::vector<uint32_t> > hits = gts.get_hits();
                    std::cout << "Total number of Hits: " << hits.size() << std::endl;
                    for (const auto& [key, group] : hits) {
                        std::cout << "### Starts HIT with channel ID: "  << key.hit_id << ", hit ID: " << key.hit_id << std::endl;
                        for(const auto& w : group){
                            parse_word(w)->print();
                        }
                    }
                }
            }
        }
        index++;
    }

    std::cout << "Number of OCB packets: " << (int) ocb_packets.size() << std::endl;


    return 0;
}
