#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <iomanip>
#include <array>
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

// void check_expected_word(std::unique_ptr<Word>& word, WordID expected_id) {
// // void check_expected_word(Word* &word, WordID expected_id) {
//     if (word->word_id != expected_id) {
//         throw std::runtime_error("Wrong word type received: wordID = " + std::to_string(word->word_id) + ", expected = " + std::to_string(expected_id));
//     }
// }

void check_expected_word(uint32_t word, WordID expected_id) {
    std::unique_ptr<Word> word_object = parse_word(word);
    if (word_object->word_id != expected_id) {
        throw std::runtime_error("Wrong word type received: wordID = " + std::to_string(word_object->word_id) + ", expected = " + std::to_string(expected_id));
    }
}

// class OCBDataPacket {
//     private:
//     std::vector<std::unique_ptr<Word>> _words;
//     std::vector<std::vector<std::unique_ptr<Word>>> feb_packets;

//     public:
//         uint32_t gate_type, gate_tag, event_number;
        
//         OCBDataPacket(std::vector<std::unique_ptr<Word>>&& word_list) : _words(std::move(word_list)) {

//             check_expected_word(_words.front(), WordID::OCB_PACKET_HEADER);
//             check_expected_word(_words.back(),  WordID::OCB_PACKET_TRAILER);

//             auto* ocb_packet_header  = dynamic_cast<OCBPacketHeader*>(_words.front().get());
//             auto* ocb_packet_trailer = dynamic_cast<OCBPacketTrailer*>(_words.back().get());

//             if (ocb_packet_header->gate_type != ocb_packet_trailer->gate_type) {
//                 throw std::runtime_error("Different gate type in OCB packet header and trailer!");
//             }
//             if (ocb_packet_header->gate_tag != ocb_packet_trailer->gate_tag) {
//                 throw std::runtime_error("Different gate tag in OCB packet header and trailer!");
//             }
//             gate_type = ocb_packet_header->gate_type;
//             gate_tag = ocb_packet_header->gate_tag;
//             event_number = ocb_packet_header->event_number;

//             // std::cout << "Words in OCB packet, event number: " << event_number << std::endl;
//             int index = 0;
//             int start_index = -1;
//             for (auto &w : _words){
//                 if (w->word_id == WordID::GATE_HEADER) {
//                     start_index = index;
//                 }
//                 if (w->word_id == WordID::FEB_DATA_PACKET_TRAILER) {
//                     if (start_index < 0) {
//                         throw std::runtime_error("FEB Data Packet Trailer received without corresponding Gate Header");
//                     }
//                     std::vector<std::unique_ptr<Word>> feb_packet_word_list;
//                     // std::vector<Word*> ocb_packet_word_list;
//                     for (int k = start_index; k < index+1; ++k)
//                         feb_packet_word_list.push_back(std::move(w));
//                     feb_packets.push_back(std::move(feb_packet_word_list));
//                 }
//                 index++;
//             }


//         }
// };

class OCBDataPacket {
    private:
    std::vector<uint32_t> _words;
    std::vector<std::vector<uint32_t>> _feb_packets;

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
                    std::cout << "FEB Data Packet with following words: " << std::endl;
                    // std::vector<Word*> ocb_packet_word_list;
                    for (int k = start_index; k < index+1; ++k) {
                        parse_word(_words[k])->print();
                        feb_packet_word_list.push_back(_words[k]);
                    }
                    _feb_packets.push_back(feb_packet_word_list);
                }
                index++;
            }



        }
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
        }
        index++;
    }

    std::cout << "Number of OCB packets: " << (int) ocb_packets.size() << std::endl;


    return 0;
}
