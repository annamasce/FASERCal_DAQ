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
    while (in.read(reinterpret_cast<char*>(buf), 4)) {
        uint32_t word = bytes_to_uint32(buf);
        word_list.push_back(word);
        // parse_word(word)->print();
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
                parse_word(word_list[k])->print();
                ocb_packet_word_list.push_back(word_list[k]);
            }

            OCBDataPacket ocb(ocb_packet_word_list, /*debug=*/false);
            ocb_packets.push_back(std::move(ocb));

            // Print contents
            const OCBDataPacket& ev = ocb_packets.back();
            std::cout << "OCB event " << ev.get_event_id() << " loaded.\n";
            for (size_t feb = 0; feb < ev.get_Nfebs_in_ocb(); ++feb) {
                if (!ev.hasData(feb)) continue;
                std::cout << " FEB " << feb << " present.\nHit times: " << ev[feb].get_hit_times().size() << "\n";
                for (const auto& hit : ev[feb].get_hit_times()) {
                    hit.print();
                }
                std::cout << "Hit amplitudes: " << ev[feb].get_hit_times().size() << "\n";
                for (const auto& hit : ev[feb].get_hit_amplitudes()) {
                    hit.print();
                }
            }

            start_index = -1;
        }

        if (ocb_packets.size() > 1) {
                break;  // For testing, process only first OCB packets
            }

        ++index;
    }

    std::cout << "Number of OCB packets: " << (int) ocb_packets.size() << std::endl;

    return 0;
}
