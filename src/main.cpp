#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <iomanip>

// Simple utility to convert 4 bytes to uint32_t using little-endian interpretation
static uint32_t bytes_to_u32_le(const unsigned char buf[4]) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

// Convert 4 bytes to uint32_t using big-endian interpretation
static uint32_t bytes_to_u32_be(const unsigned char buf[4]) {
    return (uint32_t)buf[3]
         | ((uint32_t)buf[2] << 8)
         | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[0] << 24);
}

static void process_hit_time_word(uint32_t word) {
    uint32_t channel_id = (word >> 20) & 0xFF; // bits 27..20
    uint32_t hit_id = (word >> 17) & 0x7; // bits 19..17
    uint32_t tag_id = (word >> 15) & 0x3; // bits 16..15
    uint32_t edge = (word >> 14) & 0x1;  // bit 14
    uint32_t ts_value = word & 0x1FFF; // bits 12..0

    std::cout << "[      ] Hit Time Word - Channel ID: " << channel_id
              << ", Hit ID: " << hit_id
              << ", Tag ID: " << tag_id
              << ", Edge: " << edge
              << ", Timestamp: " << ts_value << "\n";
}

static void process_hit_amplitude_word(uint32_t word) {
    uint32_t channel_id = (word >> 20) & 0xFF; // bits 27..20
    uint32_t hit_id = (word >> 17) & 0x7; // bits 19..17
    uint32_t tag_id = (word >> 15) & 0x3; // bits 16..15
    uint32_t amp_id = (word >> 12) & 0x7;  // bits 14..12
    uint32_t amp_value = word & 0xFFF; // bits 11..0

    std::cout << "[      ] Hit Time Word - Channel ID: " << channel_id
              << ", Hit ID: " << hit_id
              << ", Tag ID: " << tag_id
              << ", Amp ID: " << amp_id
              << ", Amplitude: " << amp_value << "\n";
}

static void process_gts_header_word(uint32_t word) {
    uint32_t gts_tag = word & 0xFFFFFFF; // bits 27..0

    std::cout << "[      ] GTS Header Word - GTS Tag: " << gts_tag << "\n";
}

static void process_gate_header_word(uint32_t word) {
    uint32_t board_id = (word >> 20) & 0xFF; // bits 27..20
    uint32_t gate_head = (word >> 19) & 0x1; // bits 19..0
    if( gate_head == 0. ) {
        uint32_t gate_type = (word >> 16) & 0x7; // bits 18..16
        uint32_t gate_nbr = word & 0xFFFF; // bits 15..0
        std::cout << "[      ] Gate Header Word - Board ID: " << board_id
                  << ", Gate header: " << gate_head
                  << ", Gate Type: " << gate_type
                  << ", Gate Number: " << gate_nbr << "\n";
    } else {
        uint32_t gate_time = word & 0x7FF; // bits 10..0
        std::cout << "[      ] Gate Header Word - Board ID: " << board_id
                  << ", Gate header: " << gate_head
                  << ", Gate time: " << gate_time << "\n";
    }
}

static void process_gts_trailer1_word(uint32_t word) {
    uint32_t gts_tag = word & 0xFFFFFFF; // bits 27..0

    std::cout << "[      ] GTS Trailer 1 Word - GTS Tag: " << gts_tag << "\n";
}

static void process_gts_trailer2_word(uint32_t word) {
    uint32_t data = (word >> 27) & 0x1; // bit 27
    uint32_t ocb = (word >> 26) & 0x1; // bit 26
    uint32_t feb = (word >> 25) & 0x1; // bit 25
    uint32_t gts_time = word & 0xFFFFF; // bits 19..0

    std::cout << "[      ] GTS Trailer 2 Word - Data: " << data
              << ", OCB: " << ocb
              << ", FEB: " << feb 
              << ", GTS Time: " << gts_time << "\n";
}

static void process_gate_trailer_word(uint32_t word) {
    uint32_t board_id = (word >> 20) & 0xFF; // bits 27..20
    uint32_t gate_type = (word >> 16) & 0x7; // bits 18..16
    uint32_t gate_nbr = word & 0xFFFF; // bits 15..0

    std::cout << "[      ] Gate Trailer Word - Board ID: " << board_id
              << ", Gate Type: " << gate_type
              << ", Gate Number: " << gate_nbr << "\n";
}

static void process_gate_time_word(uint32_t word) {
    uint32_t gate_time = word & 0xFFFFFFF; // bits 27..0
    std::cout << "[      ] Gate Time Word - Gate time: " << gate_time << "\n";
}

static void process_ocb_word(uint32_t word) {
    uint32_t data = word & 0xFFFFFFF; // bits 27..0
    std::cout << "[      ] OCB Data Packet Word - Data: " << data << "\n";
}

static void process_hold_time_word(uint32_t word) {
    uint32_t board_id = (word >> 20) & 0xFF; // bits 27..20
    uint32_t signal_type = (word >> 19) & 0x1; // bit 19 : 0 for start, 1 for stop
    uint32_t hold_time = word & 0x7FF; // bits 10..0
    std::cout << "[      ] HOLD Time Word - Board ID: " << board_id
              << ", Signal Type (start/stop): " << signal_type
              << ", HOLD time: " << hold_time << "\n";
}

static void process_event_done_word(uint32_t word) {
    uint32_t board_id = (word >> 20) & 0xFF; // bits 27..20
    uint32_t gate_nbr = (word >> 16) & 0xF; // bits 19..16
    uint32_t word_count = word & 0xFFFF; // bits 15..0
    std::cout << "[      ] Event Done Word - Board ID: " << board_id 
              << ", Gate Number (4LSB): " << gate_nbr
              << ", Word Count: " << word_count << "\n";
}


static void process_word(uint32_t word) {
    // Extract word ID as the top 4 bits (bitds 31..28) of the 32-bit word
    uint32_t word_id = (word >> 28) & 0xF; // top 4 bits
    // uint32_t payload = word & 0x0FFFFFFF;  // remaining 28 bits

    switch (word_id) {
        case 0x0:
            std::cout << "[ID=0x0] Gate header\n";
            process_gate_header_word(word);
            break;
        case 0x1:
            std::cout << "[ID=0x1] GTS header\n";
            process_gts_header_word(word);
            break;
        case 0x2:
            std::cout << "[ID=0x2] Hit time\n";
            process_hit_time_word(word);
            break;
        case 0x3:
            std::cout << "[ID=0x3] Hit amplitude\n";
            process_hit_amplitude_word(word);
            break;
        case 0x4:
            std::cout << "[ID=0x4] GTS trailer 1\n";
            process_gts_trailer1_word(word);
            break;
        case 0x5:
            std::cout << "[ID=0x5] GTS trailer 2\n";
            process_gts_trailer2_word(word);
            break;
        case 0x6:
            std::cout << "[ID=0x6] Gate trailer\n";
            process_gate_trailer_word(word);
            break;
        case 0x7:
            std::cout << "[ID=0x7] Gate time\n";
            process_gate_time_word(word);
            break;
        case 0x8:
            std::cout << "[ID=0x8] OCB data packet header\n";
            process_ocb_word(word);
            break;
        case 0x9:
            std::cout << "[ID=0x9] OCB data packet trailer\n";
            process_ocb_word(word);
            break;
        case 0xA:
            std::cout << "[ID=0xA] Reserved \n";
            break;
        case 0xB:
            std::cout << "[ID=0xB] HOLD time\n";
            process_hold_time_word(word);
            break;
        case 0xC:
            std::cout << "[ID=0xC] Event Done\n";
            process_event_done_word(word);
            break;
        case 0xD:
            std::cout << "[ID=0xD] FEB data packet trailer\n";
            break;
        case 0xE:
            std::cout << "[ID=0xE] Housekeeping\n";
            break;
        case 0xF:
            std::cout << "[ID=0xF] Special word ID\n";
            break;
        default:
            std::cout << "[ID=?] unknown\n";
            break;
    }
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
    size_t index = 0;
    while (in.read(reinterpret_cast<char*>(buf), 4)) {
        uint32_t word = big_endian ? bytes_to_u32_be(buf) : bytes_to_u32_le(buf);
        std::cout << "[" << std::setw(6) << index << "] 0x"
                  << std::hex << std::setfill('0') << std::setw(8) << word
                  << std::dec << std::setfill(' ') << "\n";
        // Dispatch processing based on top 4 bits (word ID)
        process_word(word);
        std::cout << "\n";
        ++index;
        // if (index > 60) break; // Limit output to first 20 words for brevity
    }

    // If there's a partial tail (file size not multiple of 4), read remaining bytes
    std::streamsize rem = in.gcount();
    if (rem > 0) {
        // shift remaining into a zero-filled buffer
        unsigned char tail[4] = {0,0,0,0};
        for (std::streamsize i = 0; i < rem; ++i) tail[i] = buf[i];
        uint32_t word = big_endian ? bytes_to_u32_be(tail) : bytes_to_u32_le(tail);
        std::cout << "[" << std::setw(6) << index << "] 0x"
                  << std::hex << std::setfill('0') << std::setw(8) << word
                  << " (partial, " << rem << " bytes)" << std::dec << std::setfill(' ') << "\n";
    // Process the final partial word as well
    process_word(word);
    }

    return 0;
}
