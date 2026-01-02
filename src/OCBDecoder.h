// ========================= OCBDecoder.h =========================
#ifndef OCBDECODER_H
#define OCBDECODER_H

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <map>
#include <cstdint>
#include <iostream>
#include "Word.h"
#include <iomanip>

namespace OCBConfig {
    inline constexpr int NUM_GTS_BEFORE_EVENT = 2;
    inline constexpr int NUM_FEBS_PER_OCB = 9;
}

// Hit key used to uniquely identify words belonging to the same HitTimeData (within same FEB and GTS)
struct HitTimeKey {
    uint32_t channel_id;
    uint32_t hit_id;

    bool operator<(const HitTimeKey& other) const noexcept {
        if (channel_id < other.channel_id) return true;
        if (channel_id > other.channel_id) return false;
        return hit_id < other.hit_id;
    }
};

class HitTimeData {
public:

    // Construct from board id, channel id and hit id
    HitTimeData(int board, int ch, int hid)
        : board_id(board), channel_id(ch), hit_id(hid) {};

    friend std::ostream &operator<<(std::ostream &out, const HitTimeData&data);

    // Getters
    int get_board_id()    const { return board_id; }
    int get_channel_id()  const { return channel_id; }
    int get_hit_id()      const { return hit_id; }
    int get_gts_tag_rise() const { return gts_tag_rise; }
    int get_gts_tag_fall() const { return gts_tag_fall; }
    int get_tag_id_rise() const { return tag_id_rise; }
    int get_tag_id_fall() const { return tag_id_fall; }
    int get_hit_time_rise() const { return hit_time_rise; }
    int get_hit_time_fall() const { return hit_time_fall; }

    // Setters
    void set_gts_tag_rise(int tag) { gts_tag_rise = tag; }
    void set_gts_tag_fall(int tag) { gts_tag_fall = tag; }
    void set_tag_id_rise(int tag) { tag_id_rise = tag; }
    void set_tag_id_fall(int tag) { tag_id_fall = tag; }
    void set_hit_time_rise(int time) { hit_time_rise = time; }
    void set_hit_time_fall(int time) { hit_time_fall = time; }

private:
    int board_id     = -1;
    int channel_id   = -1;
    int hit_id       = -1;

    // current GTS tags when rising and falling edges are received
    int gts_tag_rise      = -1;
    int gts_tag_fall      = -1;
    // tag IDs of rising and falling edges: allow matching with correct GTS tags 
    int tag_id_rise      = -1;
    int tag_id_fall      = -1;

    int hit_time_rise = -1;
    int hit_time_fall = -1;
};

class HitAmplitudeData {
public:

    // Construct from board id, channel id and hit id
    HitAmplitudeData(int board, int ch, int hid)
        : board_id(board), channel_id(ch), hit_id(hid) {};

    void print() const;

    friend std::ostream &operator<<(std::ostream &out, const HitAmplitudeData &data);

    // Getters
    int get_board_id()    const { return board_id; }
    int get_channel_id()  const { return channel_id; }
    int get_hit_id()      const { return hit_id; }
    int get_gts_tag_lg()     const { return gts_tag_lg; }
    int get_gts_tag_hg()     const { return gts_tag_hg; } 
    int get_tag_id_lg()      const { return tag_id_lg; }
    int get_tag_id_hg()      const { return tag_id_hg; }
    int get_amplitude_lg()   const { return amplitude_lg; }
    int get_amplitude_hg()   const { return amplitude_hg; }

    // Setters
    void set_gts_tag_lg(int tag) { gts_tag_lg = tag; }
    void set_gts_tag_hg(int tag) { gts_tag_hg = tag; }
    void set_tag_id_lg(int tag) { tag_id_lg = tag; }
    void set_tag_id_hg(int tag) { tag_id_hg = tag; }
    void set_amplitude_lg(int amp) { amplitude_lg = amp; }
    void set_amplitude_hg(int amp) { amplitude_hg = amp; } 

private:
    int board_id     = -1;
    int channel_id   = -1;
    int hit_id       = -1;

    // current GTS tags when amplitude lg and hg are received
    int gts_tag_lg      = -1;
    int gts_tag_hg      = -1;
    // tag IDs of amplitude lg and hg: allow matching with correct GTS tags 
    int tag_id_lg      = -1;
    int tag_id_hg      = -1;

    int amplitude_lg = -1;
    int amplitude_hg = -1;
};

class HitData {
public:

    // Construct from board id, GTS tag, channel id and hit id
    HitData(int board, int gts, int ch, int hid)
        : board_id(board), gts_tag(gts), channel_id(ch), hit_id(hid) {};

    // Construct from a list of raw 32-bit words that belong to the same hit
    HitData(int board, int gts, const std::vector<uint32_t>& words);

    void print() const;

    // Getters
    int get_board_id()    const { return board_id; }
    int get_gts_tag()     const { return gts_tag; } 
    int get_channel_id()  const { return channel_id; }
    int get_hit_id()      const { return hit_id; }
    int get_hit_time_rise() const { return hit_time_rise; }
    int get_hit_time_fall() const { return hit_time_fall; }
    int get_amplitude_lg()  const { return amplitude_lg; }
    int get_amplitude_hg()  const { return amplitude_hg; }

    // Setters
    void set_hit_time_rise(int time) { hit_time_rise = time; }
    void set_hit_time_fall(int time) { hit_time_fall = time; }
    void set_amplitude_lg(int amp)   { amplitude_lg  = amp; }
    void set_amplitude_hg(int amp)   { amplitude_hg  = amp; }

private:
    int board_id     = -1;
    int gts_tag      = -1;

    int channel_id   = -1;
    int hit_id       = -1;

    int hit_time_rise = -1;
    int hit_time_fall = -1;
    int amplitude_lg  = -1;
    int amplitude_hg  = -1;

    void validate_ids(int ch, int hid);
};

class FEBDataPacket {
private:
    // std::vector<HitData> _hits;
    std::vector<HitTimeData> _hit_times;
    std::vector<HitAmplitudeData> _hit_amplitudes;
    std::map<uint32_t, uint32_t> _gts_tag_map; // map GTS tag to GTS time in FEB data packet
    bool artificial_trl2 = false;
    bool event_done_timeout = false;
    bool d1_fifo_full = false;
    bool d0_fifo_full = false;
    bool rb_cnt_error = false;
    int nb_decoder_errors = 0;

public:
    int board_id = -1;
    int hold_time = -1;

    FEBDataPacket(const std::vector<uint32_t>& words);

    // const std::vector<HitData>& get_hits() const { return _hits; }
    const std::vector<HitTimeData>& get_hit_times() const { return _hit_times; }
    const std::vector<HitAmplitudeData>& get_hit_amplitudes() const { return _hit_amplitudes; }

private:
    void decodeFEBdata(const std::vector<uint32_t>& words);
    // void extract_hits_from_gts(int gts_tag, const std::vector<uint32_t>& block);
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

    // Access decoded OCB trailer error bits (16 flags). Call
    // `decode_ocb_errors()` to print human-readable messages for any set bits.
    const std::array<bool,16>& get_ocb_errors() const { return ocb_errors; }

    // Print messages for any OCB errors stored in this packet's `ocb_errors`.
    void decode_ocb_errors() const;

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

    friend std::ostream &operator<<(std::ostream &out, const OCBDataPacket &event);

private:
    OCBevent event;
    void decodeOCBdata(const std::vector<uint32_t>& words, bool debug);
    // Error bits extracted from the OCB packet trailer (16 bits)
    std::array<bool,16> ocb_errors{};
};

#endif // OCBDECODER_H


