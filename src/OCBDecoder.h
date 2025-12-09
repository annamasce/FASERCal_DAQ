// ========================= OCBDecoder.h =========================
#ifndef OCBDECODER_H
#define OCBDECODER_H

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <map>
#include <iostream>
#include "Word.h"

namespace OCBConfig {
    inline constexpr int NUM_GTS_BEFORE_EVENT = 2;
    inline constexpr int NUM_FEBS_PER_OCB = 9;
}

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
    uint32_t event_id;
    // FEBs' indices assumed 0...NUM_FEB_PER_OCB; nullptr for missing FEBs.
    std::array<std::shared_ptr<FEBDataPacket>, OCBConfig::NUM_FEBS_PER_OCB> febs;

    OCBevent();
};

class OCBDataPacket {

public:
    OCBDataPacket(const std::vector<uint32_t>& words, bool debug = false);

    uint32_t get_event_id() const { return event.event_id; }

    const FEBDataPacket& get_feb(size_t board_id) const { return *(event.febs[board_id]); }
    const FEBDataPacket& operator[](size_t board_id) const { return *(event.febs[board_id]); }
    bool hasData(size_t board_id) const { return (event.febs[board_id] != nullptr); }

    size_t get_Nfebs_in_ocb() const { return event.febs.size(); }
    uint32_t get_Nfebs_fired() const {
        size_t count = 0;
        for (const auto& feb : event.febs) {
            if (feb != nullptr) count++;
        }
        return count;
    }



    // const OCBevent& get_event() const { return event; }
    // To Do: add all functions to get access to event (OCBevent) data and remove get_event()

private:
    OCBevent event;
    void decodeOCBdata(const std::vector<uint32_t>& words, bool debug);
    // void decodeFEBdata(const std::vector<uint32_t>& words, bool debug);
};

#endif // OCBDECODER_H


