// ========================= OCBDecoder.h =========================
#pragma once

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
    int get_gts_tag_rise_received() const { return gts_tag_rise_received; }
    int get_gts_tag_fall_received() const { return gts_tag_fall_received; }
    int get_tag_id_rise() const { return tag_id_rise; }
    int get_tag_id_fall() const { return tag_id_fall; }
    int get_hit_time_rise() const { return hit_time_rise; }
    int get_hit_time_fall() const { return hit_time_fall; }

    // Setters
    void set_gts_tag_rise(int tag) { gts_tag_rise = tag; }
    void set_gts_tag_fall(int tag) { gts_tag_fall = tag; }
    void set_gts_tag_rise_received(int tag) { gts_tag_rise_received = tag; }
    void set_gts_tag_fall_received(int tag) { gts_tag_fall_received = tag; }
    void set_tag_id_rise(int tag) { tag_id_rise = tag; }
    void set_tag_id_fall(int tag) { tag_id_fall = tag; }
    void set_hit_time_rise(int time) { hit_time_rise = time; }
    void set_hit_time_fall(int time) { hit_time_fall = time; }

private:
    int board_id     = -1;
    int channel_id   = -1;
    int hit_id       = -1;

    // current GTS tags when rising and falling edges are received
    int gts_tag_rise_received      = -1;
    int gts_tag_fall_received      = -1;
    // correct GTS tags of rising and falling edges
    int gts_tag_rise = -1;
    int gts_tag_fall = -1;
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
    int get_gts_tag_lg_received() const { return gts_tag_lg_received; }
    int get_gts_tag_hg_received() const { return gts_tag_hg_received; }
    int get_gts_tag_lg()     const { return gts_tag_lg; }
    int get_gts_tag_hg()     const { return gts_tag_hg; } 
    int get_tag_id_lg()      const { return tag_id_lg; }
    int get_tag_id_hg()      const { return tag_id_hg; }
    int get_amplitude_lg()   const { return amplitude_lg; }
    int get_amplitude_hg()   const { return amplitude_hg; }

    // Setters
    void set_gts_tag_lg(int tag) { gts_tag_lg = tag; }
    void set_gts_tag_hg(int tag) { gts_tag_hg = tag; }
    void set_gts_tag_lg_received(int tag) { gts_tag_lg_received = tag; }
    void set_gts_tag_hg_received(int tag) { gts_tag_hg_received = tag; }
    void set_tag_id_lg(int tag) { tag_id_lg = tag; }
    void set_tag_id_hg(int tag) { tag_id_hg = tag; }
    void set_amplitude_lg(int amp) { amplitude_lg = amp; }
    void set_amplitude_hg(int amp) { amplitude_hg = amp; } 

private:
    int board_id     = -1;
    int channel_id   = -1;
    int hit_id       = -1;

    // current GTS tags when amplitude lg and hg are received
    int gts_tag_lg_received      = -1;
    int gts_tag_hg_received      = -1;
    // correct GTS tags of amplitude lg and hg 
    int gts_tag_lg      = -1;
    int gts_tag_hg      = -1;
    // tag IDs of amplitude lg and hg: allow matching with correct GTS tags 
    int tag_id_lg      = -1;
    int tag_id_hg      = -1;

    int amplitude_lg = -1;
    int amplitude_hg = -1;
};

class FEBDataPacket {
public:

    FEBDataPacket(const std::vector<uint32_t>& words, bool debug = false);
    // Pointer/count constructor to avoid an intermediate vector copy when
    // decoding FEB data directly from an existing buffer.
    FEBDataPacket(const uint32_t* words, size_t nwords, bool debug = false);

    void addError(unsigned int err);
    int find_matching_gts_tag(uint32_t tag_id, std::vector<uint32_t>& gts_tags) const;

    // Getters
    const std::vector<HitTimeData>& get_hit_times() const { return _hit_times; }
    const std::vector<HitAmplitudeData>& get_hit_amplitudes() const { return _hit_amplitudes; }
    bool isCorrupted() const { return is_corrupted; }
    bool hasMissingGTS() const { return has_missing_gts; }
    
    // Access decoded FEB trailer error bits (4 flags)
    const std::array<bool, 4>& get_feb_errors() const { return feb_errors; }
    // Print messages for any FEB errors stored in this packet's `feb_errors`.
    static std::string errorMessageForBit(std::size_t bit);
    bool has_feb_errors() const {
        for (const auto& err : feb_errors) {
            if (err) return true;
        }
        return false;
    }

private:
    int board_id = -1;
    int hold_time = -1;
    std::vector<HitTimeData> _hit_times;
    std::vector<HitAmplitudeData> _hit_amplitudes;
    std::map<uint32_t, uint32_t> _gts_tag_map; // map GTS tag to GTS time in FEB data packet
    void decodeFEBdata(const std::vector<uint32_t>& words);
    // Pointer/count overload to decode FEB data without copying
    void decodeFEBdata(const uint32_t* words, size_t nwords);
    inline static bool m_debug = false;
    // corrupted FEB data packet flag: packet too small or missing gate header or FEB trailer
    bool is_corrupted = false;
    // missing GTS header or trailers flag
    bool has_missing_gts = false;
    // Error bits extracted from the FEB packet trailer (16 bits)
    std::array<bool, 4> feb_errors{false};
    int nb_decoder_errors = 0;
};

struct OCBevent {
    uint32_t event_id;
    // FEBs' indices assumed 0...NUM_FEB_PER_OCB; nullptr for missing FEBs.
    std::array<std::shared_ptr<FEBDataPacket>, OCBConfig::NUM_FEBS_PER_OCB> febs;

    // corrupted OCB data packet flag: less than 2 words or missing header or trailer
    bool corrupted_ocb_packet = false;
    // Error bits extracted from the OCB packet trailer (16 bits)
    std::array<bool, 16> ocb_errors{false};
    // per-FEB flag for corrupted FEB data packet: missing header or trailer
    std::array<bool, OCBConfig::NUM_FEBS_PER_OCB> currupted_feb_errors{false};

    OCBevent();
};

class OCBDataPacket {

public:
    // Construct from a pointer to 32-bit words and the total size in bytes.
    // `size` is the total number of bytes in the fragment; the number of
    // 32-bit words processed is `size / 4`.
    OCBDataPacket(const uint32_t* words, size_t size, bool debug = false);

    uint32_t get_event_id() const { return event.event_id; }
    uint32_t get_size() const { return m_size; }

    // Access decoded OCB trailer error bits (16 flags)
    const std::array<bool, 16>& get_ocb_errors() const { return event.ocb_errors; }
    // Print messages for any OCB errors stored in this packet's `ocb_errors`.
    static std::string errorMessageForBit(std::size_t bit);
    bool has_ocb_errors() const {
        for (const auto& err : event.ocb_errors) {
            if (err) return true;
        }
        return false;
    }
    // Access corrupted OCB data packet flag, i.e. less than 2 words or missing header or trailer
    bool isCorrupted() const {return event.corrupted_ocb_packet; }

    // Access corrupted FEB data packet flags, i.e. missing gate header or FEB trailer
    const std::array<bool, OCBConfig::NUM_FEBS_PER_OCB>& get_corrupted_feb_errors() const { return event.currupted_feb_errors; }
    bool isCorruptedFEB(size_t board_id) const { return event.currupted_feb_errors[board_id]; }

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

    // Add error messages
    void add_corrupted_feb_error(size_t board_id) {
        if ((board_id >= 0) && (board_id < OCBConfig::NUM_FEBS_PER_OCB)) {
            event.currupted_feb_errors[board_id] = true;
        }
        else {
            if (m_debug) std::cerr << "Error: trying to add corrupted FEB error for invalid board id " << board_id << "\n";
        }
    }

    static void set_debug_on( bool debug = true ) { m_debug = debug; } //set debug mode

    friend std::ostream &operator<<(std::ostream &out, const OCBDataPacket &event);

private:
    OCBevent event;
    size_t m_size;
    inline static bool m_debug = false;
    void decodeOCBdata(const std::vector<uint32_t>& words);
    // Overload that decodes directly from a pointer+count to avoid copying
    void decodeOCBdata(const uint32_t* words, size_t nwords);
};


