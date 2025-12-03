// ========================= OCBDecoder.h =========================
#ifndef OCBDECODER_H
#define OCBDECODER_H

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <map>
#include <iostream>
#include "Word.h" // Assumes your existing Word.h defines parse_word(), WordID and concrete Word subclasses

// Forward declarations
class FEBDataPacket;

// Hit key used to uniquely identify words belonging to the same Hit (within same FEB and GTS)
struct HitKey {
    uint32_t channel_id;
    uint32_t hit_id;

    bool operator<(const HitKey& other) const noexcept {
        if (channel_id < other.channel_id) return true;
        if (channel_id > other.channel_id) return false;
        return hit_id < other.hit_id;
    }
};

class HitData {
public:
    int board_id     = -1;
    int gts_tag      = -1;

    int channel_id   = -1;
    int hit_id       = -1;

    int hit_time_rise = -1;
    int hit_time_fall = -1;
    int amplitude_lg  = -1;
    int amplitude_hg  = -1;

    // Construct from a list of raw 32-bit words that belong to the same hit
    HitData(int board, int gts, const std::vector<uint32_t>& words);

    void print() const;

private:
    void validate_ids(int ch, int hid);
};

class FEBDataPacket {
private:
    std::vector<HitData> _hits;

public:
    int board_id = -1;
    int hold_time = -1;

    FEBDataPacket(const std::vector<uint32_t>& words);

    const std::vector<HitData>& get_hits() const { return _hits; }

private:
    void decodeFEBdata(const std::vector<uint32_t>& words);
    void extract_hits_from_gts(int gts_tag, const std::vector<uint32_t>& block);
};

struct OCBevent {
    int event_number = -1;
    // 9 FEBs (indices assumed 0..8). nullptr for missing FEBs.
    std::array<std::shared_ptr<FEBDataPacket>, 9> febs;

    OCBevent();
};

class OCBDataPacket {
private:
    OCBevent event;

public:
    OCBDataPacket(const std::vector<uint32_t>& words, bool debug = false);

    const OCBevent& get_event() const { return event; }

private:
    void decodeOCBdata(const std::vector<uint32_t>& words, bool debug);
    // void decodeFEBdata(const std::vector<uint32_t>& words, bool debug);
};

#endif // OCBDECODER_H


