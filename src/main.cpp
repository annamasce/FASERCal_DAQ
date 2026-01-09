#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include "OCBDecoder.h"


static uint32_t bytes_to_uint32(const unsigned char buf[4]) {
    // if (big_endian) {
    //     return (uint32_t)buf[3]
    //      | ((uint32_t)buf[2] << 8)
    //      | ((uint32_t)buf[1] << 16)
    //      | ((uint32_t)buf[0] << 24);
    // }
    // else {
    return (uint32_t)buf[0]
        | ((uint32_t)buf[1] << 8)
        | ((uint32_t)buf[2] << 16)
        | ((uint32_t)buf[3] << 24);
    // }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <binary-file>\n";
        return 1;
    }

    const char* path = argv[1];
    // bool big_endian = false;
    // if (argc >= 3 && std::string(argv[2]) == "--be") big_endian = true;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open file: " << path << "\n";
        return 2;
    }

    unsigned char buf[4];
    std::vector<uint32_t> word_list;

    // First loop just to print all words once
    int word_count = 0;
    while (in.read(reinterpret_cast<char*>(buf), 4)) {
        uint32_t word = bytes_to_uint32(buf);
        if (parse_word(word)->word_id == WordID::FEB_DATA_PACKET_TRAILER && word_count > 0 && word_count < 200) {
            // Remove some words just for testing
            std::cout << "Found FEB Data Packet Trailer word, skipping\n";
            continue;
        }
        word_list.push_back(word);
        word_count++;
    }

    // Iterate OCB packets inside file
    int index = 0;
    int start_index = -1;
    std::vector<OCBDataPacket> ocb_packets;

    for (auto& w : word_list) {
        if (parse_word(w)->word_id == WordID::OCB_PACKET_HEADER) {
            start_index = index;
        }
        if (parse_word(w)->word_id == WordID::OCB_PACKET_TRAILER) {
            if (start_index < 0) {
                throw std::runtime_error("OCB Packet Trailer received without corresponding Header");
            }

            std::vector<uint32_t> ocb_packet_word_list;
            for (int k = start_index; k < index+1; ++k) {
                std::cout << *parse_word(word_list[k]);
                ocb_packet_word_list.push_back(word_list[k]);
            }

            // construct OCBDataPacket from pointer and size in bytes
            OCBDataPacket ev = OCBDataPacket(ocb_packet_word_list.data(), ocb_packet_word_list.size() * 4, true);
            std::cout << ev;
            ocb_packets.push_back(std::move(ev));

            start_index = -1;
        }

        // if (ocb_packets.size() >= 1) {
        //         break;  // For testing, process only first OCB packets
        //     }

        ++index;
    }

    std::cout << "Number of OCB packets: " << (int) ocb_packets.size() << std::endl;

    return 0;
}
