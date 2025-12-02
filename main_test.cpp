#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstdint>
#include "Word.h" // Your header with Word classes + parse_word()
#include <unordered_map>
#include <stdexcept>

// ---------- HitKey for hit grouping ----------
struct HitKey {
    uint32_t hit_id;
    uint32_t channel_id;
    bool operator==(const HitKey& other) const noexcept {
        return hit_id == other.hit_id && channel_id == other.channel_id;
    }
};

struct HitKeyHash {
    std::size_t operator()(const HitKey& k) const noexcept {
        return (static_cast<std::size_t>(k.hit_id) << 16) ^ static_cast<std::size_t>(k.channel_id);
    }
};

// ---------- HitData ----------
class HitData {
public:
    int board_id, gts_tag;
    int channel_id=-1, hit_id=-1;
    int hit_time_rise=-1, hit_time_fall=-1;
    int amplitude_lg=-1, amplitude_hg=-1;

    HitData(int board, int gts, const std::vector<Word*>& words)
        : board_id(board), gts_tag(gts)
    {
        for (Word* w : words) {
            switch (w->word_id) {
                case WordID::HIT_TIME: {
                    HitTime* ht = static_cast<HitTime*>(w);
                    validate_ids(ht->channel_id, ht->hit_id);
                    if (ht->edge==0) hit_time_rise = ht->hit_time;
                    else hit_time_fall = ht->hit_time;
                    break;
                }
                case WordID::HIT_AMPLITUDE: {
                    HitAmplitude* ha = static_cast<HitAmplitude*>(w);
                    validate_ids(ha->channel_id, ha->hit_id);
                    if (ha->amplitude_id==2) amplitude_hg = ha->amplitude_value;
                    else amplitude_lg = ha->amplitude_value;
                    break;
                }
                default: break;
            }
        }
    }

    void validate_ids(int ch, int hid) {
        if (channel_id < 0) {
            channel_id = ch;
            hit_id     = hid;
            return;
        }

        if (ch != channel_id || hid != hit_id) {
            throw std::runtime_error(
                "Inconsistent hit data: channel_id/hit_id mismatch");
        }
    }

    void print() const {
        std::cout << "Hit:\n"
                  << "  GTS tag: " << gts_tag << "\n"
                  << "  Board: " << board_id << "\n"
                  << "  Channel: " << channel_id << ", Hit: " << hit_id << "\n"
                  << "  Rise: " << hit_time_rise << ", Fall: " << hit_time_fall << "\n"
                  << "  Amp LG: " << amplitude_lg << ", Amp HG: " << amplitude_hg << "\n";
    }
};

// ---------- GTSDataPacket ----------
class GTSDataPacket {
public:
    int gts_tag, board_id;
    std::vector<HitData> hits;

    GTSDataPacket(int board, const std::vector<Word*>& words)
        : board_id(board)
    {
        if (words.size()<3) throw std::runtime_error("GTS packet too small");
        GTSHeader* hdr = static_cast<GTSHeader*>(words.front());
        GTSTrailer1* tr1 = static_cast<GTSTrailer1*>(words[words.size()-2]);
        if (hdr->gts_tag != tr1->gts_tag) throw std::runtime_error("GTS tag mismatch");
        gts_tag = hdr->gts_tag;

        // Group hits
        std::unordered_map<HitKey, std::vector<Word*>, HitKeyHash> hit_groups;
        for (Word* w : words) {
            if (w->word_id==WordID::HIT_TIME) {
                HitTime* ht = static_cast<HitTime*>(w);
                hit_groups[{ht->hit_id, ht->channel_id}].push_back(w);
            }
            else if (w->word_id==WordID::HIT_AMPLITUDE) {
                HitAmplitude* ha = static_cast<HitAmplitude*>(w);
                hit_groups[{ha->hit_id, ha->channel_id}].push_back(w);
            }
        }

        for (auto& kv : hit_groups)
            hits.emplace_back(board_id, gts_tag, kv.second);
    }
};

// ---------- FEBDataPacket ----------
class FEBDataPacket {
public:
    int board_id=-1, hold_time=-1;
    std::vector<GTSDataPacket> gts_packets;

    FEBDataPacket(const std::vector<Word*>& words) {
        GateHeader* hdr = static_cast<GateHeader*>(words.front());
        board_id = hdr->board_id;
        if (words.size()>1 && words[1]->word_id==WordID::HOLD_TIME)
            hold_time = static_cast<HoldTime*>(words[1])->hold_time;

        constexpr int TAG_MASK=0x3;
        int current_tag=-1, previous_tag=-1;
        std::vector<Word*> current_gts, previous_gts;

        for (Word* w : words) {
            switch (w->word_id) {
                case WordID::GTS_HEADER:
                    current_tag = static_cast<GTSHeader*>(w)->gts_tag;
                    current_gts.clear();
                    current_gts.push_back(w);
                    break;
                case WordID::HIT_TIME:
                case WordID::HIT_AMPLITUDE: {
                    uint32_t hit_tag = (w->word_id==WordID::HIT_TIME)
                        ? static_cast<HitTime*>(w)->tag_id
                        : static_cast<HitAmplitude*>(w)->tag_id;
                    if ((hit_tag & TAG_MASK)==(current_tag & TAG_MASK)) current_gts.push_back(w);
                    else if ((hit_tag & TAG_MASK)==(previous_tag & TAG_MASK)) previous_gts.push_back(w);
                    else throw std::runtime_error("Hit tag mismatch");
                    break;
                }
                case WordID::GTS_TRAILER1: current_gts.push_back(w); break;
                case WordID::GTS_TRAILER2:
                    current_gts.push_back(w);
                    if (!previous_gts.empty()) { gts_packets.emplace_back(board_id, previous_gts); previous_gts.clear(); }
                    previous_tag=current_tag;
                    previous_gts=std::move(current_gts);
                    current_gts.clear();
                    break;
                default: break;
            }
        }
        if (!previous_gts.empty()) gts_packets.emplace_back(board_id, previous_gts);
        if (!current_gts.empty()) gts_packets.emplace_back(board_id, current_gts);
    }
};

// ---------- OCBDataPacket ----------
class OCBDataPacket {
public:
    std::vector<FEBDataPacket> feb_packets;

    OCBDataPacket(const std::vector<Word*>& words) {
        int start_index=-1;
        for (size_t i=0;i<words.size();++i) {
            if (words[i]->word_id==WordID::GATE_HEADER) start_index=i;
            if (words[i]->word_id==WordID::FEB_DATA_PACKET_TRAILER) {
                if (start_index<0) throw std::runtime_error("FEB trailer without header");
                std::vector<Word*> slice(words.begin()+start_index, words.begin()+i+1);
                feb_packets.emplace_back(slice);
                start_index=-1;
            }
        }
    }
};

// ---------- Main streaming parser ----------
static uint32_t bytes_to_uint32(const unsigned char buf[4], bool be=false) {
    return be ? (uint32_t(buf[3]) | (uint32_t(buf[2])<<8) | (uint32_t(buf[1])<<16) | (uint32_t(buf[0])<<24))
              : (uint32_t(buf[0]) | (uint32_t(buf[1])<<8) | (uint32_t(buf[2])<<16) | (uint32_t(buf[3])<<24));
}

int main(int argc, char** argv) {
    if (argc<2) { std::cerr<<"Usage: "<<argv[0]<<" <file>\n"; return 1; }
    bool big_endian = argc>2 && std::string(argv[2])=="--be";

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) { std::cerr<<"Failed to open file\n"; return 2; }

    std::vector<std::unique_ptr<Word>> ocb_buffer;
    unsigned char buf[4];

    while (in.read(reinterpret_cast<char*>(buf),4)) {
        uint32_t raw = bytes_to_uint32(buf,big_endian);
        auto word = parse_word(raw);

        // Add to OCB buffer
        ocb_buffer.push_back(std::move(word));
        WordID id = ocb_buffer.back()->word_id;

        if (id==WordID::OCB_PACKET_TRAILER) {
            // OCB packet complete
            std::vector<Word*> slice;
            slice.reserve(ocb_buffer.size());
            for (auto& w : ocb_buffer) slice.push_back(w.get());

            OCBDataPacket ocb(slice);

            // Print hits
            for (auto& feb : ocb.feb_packets) {
                for (auto& gts : feb.gts_packets) {
                    for (auto& hit : gts.hits) hit.print();
                }
            }
            ocb_buffer.clear();
        }
    }

    return 0;
}
