#include "OCBDecoder.h"
#include <stdexcept>
// #include <map>
#include <array>


void check_expected_word(uint32_t word, WordID expected_id) {
    std::unique_ptr<Word> word_object = parse_word(word);
    if (word_object->word_id != expected_id) {
        throw std::runtime_error("Wrong word type received: wordID = " + std::to_string(word_object->word_id) + ", expected = " + std::to_string(expected_id));
    }
}

// Human-readable descriptions for the 16 OCB trailer error bits.
static const char* OCB_ERROR_MESSAGES[16] = {
    "FEB data packet 0 error",
    "FEB data packet 1 error",
    "FEB data packet 2 error",
    "FEB data packet 3 error",
    "FEB data packet 4 error",
    "FEB data packet 5 error",
    "FEB data packet 6 error",
    "FEB data packet 7 error",
    "FEB data packet 8 error",
    "FEB data packet 9 error",
    "FEB data packet 10 error",
    "FEB data packet 11 error",
    "FEB data packet 12 error",
    "FEB data packet 13 error",
    "Gate close error",
    "Gate open timeout"
};

OCBevent::OCBevent() {
    febs.fill(nullptr);
}

// ---------------- HitData ----------------

HitData::HitData(int board, int gts, const std::vector<uint32_t>& words)
    : board_id(board), gts_tag(gts)
{
    if (words.size() != 4) {
        std::cerr << "Warning: hit data packet size != 4 (size = " << words.size() << ")\n";
    }

    for (uint32_t raw : words) {
        std::unique_ptr<Word> w = parse_word(raw);

        switch (w->word_id) {
            case WordID::HIT_TIME: {
                auto* ht = static_cast<HitTime*>(w.get());
                validate_ids(ht->channel_id, ht->hit_id);

                if (ht->edge == 0)
                    hit_time_rise = ht->hit_time;
                else
                    hit_time_fall = ht->hit_time;
                break;
            }

            case WordID::HIT_AMPLITUDE: {
                auto* ha = static_cast<HitAmplitude*>(w.get());
                validate_ids(ha->channel_id, ha->hit_id);

                if (ha->amplitude_id == 2)
                    amplitude_hg = ha->amplitude_value;
                else
                    amplitude_lg = ha->amplitude_value;
                break;
            }

            default:
                throw std::runtime_error(
                    "Invalid word encountered in hit data: WordID = " +
                    std::to_string(w->word_id));
        }
    }
}

void HitData::validate_ids(int ch, int hid) {
    if (channel_id < 0) {
        channel_id = ch;
        hit_id     = hid;
        return;
    }

    if (ch != channel_id || hid != hit_id) {
        throw std::runtime_error("Inconsistent hit data: channel_id or hit_id mismatch");
    }
}

void HitData::print() const {
    std::cout << "Hit:\n"
              << "  GTS tag:       " << gts_tag << '\n'
              << "  Board ID:      " << board_id << '\n'
              << "  Channel ID:    " << channel_id << '\n'
              << "  Hit ID:        " << hit_id << '\n'
              << "  Rise time:     " << hit_time_rise << '\n'
              << "  Fall time:     " << hit_time_fall << '\n'
              << "  Amplitude LG:  " << amplitude_lg << '\n'
              << "  Amplitude HG:  " << amplitude_hg << '\n';
}

std::ostream &operator<<(std::ostream &out, const HitTimeData &data) {
    out << "Hit time data:\n"
              << "  Board ID:      " << data.board_id << '\n'
              << "  Channel ID:    " << data.channel_id << '\n'
              << "  Hit ID:        " << data.hit_id << '\n'
              << "  GTS tag rise:  " << data.gts_tag_rise << '\n'
              << "  Tag ID rise:   " << data.tag_id_rise << '\n'
              << "  GTS tag fall:  " << data.gts_tag_fall << '\n'
              << "  Tag ID fall:   " << data.tag_id_fall << '\n'
              << "  Rise time:     " << data.hit_time_rise << '\n'
              << "  Fall time:     " << data.hit_time_fall << '\n';
    return out;
}

std::ostream &operator<<(std::ostream &out, const HitAmplitudeData &data) {
    out << "Hit amplitude data:\n"
              << "  Board ID:      " << data.board_id << '\n'
              << "  Channel ID:    " << data.channel_id << '\n'
              << "  Hit ID:        " << data.hit_id << '\n'
              << "  GTS tag lg:    " << data.gts_tag_lg << '\n'
              << "  Tag ID lg:     " << data.tag_id_lg << '\n'
              << "  GTS tag hg:    " << data.gts_tag_hg << '\n'
              << "  Tag ID hg:     " << data.tag_id_hg << '\n'
              << "  Amplitude lg:  " << data.amplitude_lg << '\n'
              << "  Amplitude hg:  " << data.amplitude_hg << '\n';
    return out;
}

// ---------------- FEBDataPacket ----------------
FEBDataPacket::FEBDataPacket(const std::vector<uint32_t>& words) {
    if (words.empty())
        throw std::runtime_error("Empty FEBDataPacket words");

    check_expected_word(words.front(), WordID::GATE_HEADER);
    check_expected_word(words.back(),  WordID::FEB_DATA_PACKET_TRAILER);

    std::unique_ptr<Word> header_word = parse_word(words.front());
    std::unique_ptr<Word> trailer_word = parse_word(words.back());
    auto* gate_header  = dynamic_cast<GateHeader*>(header_word.get());
    auto* feb_packet_trailer = dynamic_cast<FEBDataPacketTrailer*>(trailer_word.get());

    board_id = gate_header->board_id;
    // decode FEB packet trailer info
    nb_decoder_errors = feb_packet_trailer->nb_decoder_errors;
    artificial_trl2 = feb_packet_trailer->artificial_trl2;
    event_done_timeout = feb_packet_trailer->event_done_timeout;
    d1_fifo_full = feb_packet_trailer->d1_fifo_full;
    d0_fifo_full = feb_packet_trailer->d0_fifo_full;
    rb_cnt_error = feb_packet_trailer->rb_cnt_error;

    // optional hold_time
    if (words.size() > 1 && parse_word(words.at(1))->word_id == WordID::HOLD_TIME){
        std::unique_ptr<Word> hold_time_word = parse_word(words.at(1));
        auto* hold_time_object  = dynamic_cast<HoldTime*>(hold_time_word.get());
        hold_time = hold_time_object->hold_time;
    }

    // Decode FEB data, i.e. get hit times and amplitudes for all channels 
    decodeFEBdata(words);
}

void FEBDataPacket::decodeFEBdata(const std::vector<uint32_t>& words) {

    std::vector<uint32_t> gts_tags;
    std::map<HitTimeKey, HitTimeData> hit_times_map;
    std::map<uint32_t, HitAmplitudeData> hit_amplitudes_map; // map channel id to hit amplitude data
    // constexpr int TAG_MASK = 0x3;

    for (auto& w : words) {
        std::unique_ptr<Word> base = parse_word(w);
        WordID id = base->word_id;

        if (id == WordID::GTS_HEADER) {
            auto* gts_header = static_cast<GTSHeader*>(base.get());
            gts_tags.push_back(gts_header->gts_tag);
        }
        else if (id == WordID::HIT_TIME) {
            // Parse hit word and get hit time information
            auto* hit = static_cast<HitTime*>(base.get());
            uint32_t channel_id = hit->channel_id;
            uint32_t hit_id = hit->hit_id;
            HitTimeKey key{channel_id, hit_id};
            if (hit->edge == 0) {
                // Rising edge
                auto [it, inserted] = hit_times_map.try_emplace(key, board_id, channel_id, hit_id);

                // If not inserted, means second rising edge detected before falling edge
                if (!inserted) {
                    throw std::runtime_error(
                        "Rising edge received twice for same hit (channel_id=" +
                        std::to_string(channel_id) +
                        ", hit_id=" +
                        std::to_string(hit_id) + ")"
                    );
                }
                // Fill rising time info for the hit
                auto& h = it->second;
                h.set_hit_time_rise(hit->hit_time);
                h.set_tag_id_rise(hit->tag_id);
                h.set_gts_tag_rise(gts_tags.back());
            }
            else {
                // Falling edge
                auto it = hit_times_map.find(key);
                if (it == hit_times_map.end()) {
                    // Rising edge must be received before falling edge
                    throw std::runtime_error(
                        "Falling edge received before rising edge for hit (channel_id=" +
                        std::to_string(channel_id) +
                        ", hit_id=" +
                        std::to_string(hit_id) + ")"
                    );
                }

                // Fill falling time info for the hit
                auto& h = it->second;
                h.set_hit_time_fall(hit->hit_time);
                h.set_tag_id_fall(hit->tag_id);
                h.set_gts_tag_fall(gts_tags.back());

                // Save hit time data
                _hit_times.push_back(std::move(h));
                // Remove from map
                hit_times_map.erase(key);

            }
        }

        else if (id == WordID::HIT_AMPLITUDE) {
            // Parse hit word and get hit amplitude information
            auto* hit = static_cast<HitAmplitude*>(base.get());
            uint32_t channel_id = hit->channel_id;
            uint32_t hit_id = hit->hit_id;
            auto [it, inserted] = hit_amplitudes_map.try_emplace(channel_id, board_id, channel_id, hit_id);
            auto& h = it->second;
            if (hit->amplitude_id == 2) {
                // Amplitude HG
                if (!inserted && h.get_amplitude_hg() != -1) {
                    throw std::runtime_error(
                        "High Gain Amplitude received twice for same channel (channel_id=" + std::to_string(channel_id) + ")"
                    );
                }
                h.set_amplitude_hg(hit->amplitude_value);
                h.set_tag_id_hg(hit->tag_id);
                h.set_gts_tag_hg(gts_tags.back());
            }
            else {
                // Amplitude LG
                if (!inserted && h.get_amplitude_lg() != -1) {
                    throw std::runtime_error(
                        "Low Gain Amplitude received twice for same channel (channel_id=" + std::to_string(channel_id) + ")"
                    );
                }
                h.set_amplitude_lg(hit->amplitude_value);
                h.set_tag_id_lg(hit->tag_id);
                h.set_gts_tag_lg(gts_tags.back());
            }
        }

        else if (id == WordID::GTS_TRAILER1) {
            if (gts_tags.empty()) {
                throw std::runtime_error("GTS Trailer1 received without corresponding GTS Header!");
            }
            // Check that GTS tag in trailer matches current GTS header
            auto* gts_trailer1 = static_cast<GTSTrailer1*>(base.get());
            if (gts_trailer1->gts_tag != gts_tags.back()) {
                throw std::runtime_error("GTS tag in Trailer1 different from current GTS Header!");
            }
        }

        else if (id == WordID::GTS_TRAILER2) {
            // Get GTS time and map it to current GTS tag
            if (gts_tags.empty()) {
                throw std::runtime_error("GTS Trailer2 received without corresponding GTS Header!");
            }
            auto* gts_trailer2 = static_cast<GTSTrailer2*>(base.get());
            uint32_t gts_time = gts_trailer2->gts_time;
            _gts_tag_map[gts_tags.back()] = gts_time;
        }
    }

    // Move completed hits into the vector in FEBDataPacket
    _hit_times.reserve(hit_times_map.size());
    for (auto& [key, hit] : hit_times_map) {
        _hit_times.push_back(std::move(hit));
    }

    _hit_amplitudes.reserve(hit_amplitudes_map.size());
    for (auto& [key, hit] : hit_amplitudes_map) {
        _hit_amplitudes.push_back(std::move(hit));
    }
}

// ---------------- OCBDataPacket ----------------

OCBDataPacket::OCBDataPacket(const std::vector<uint32_t>& words, bool debug) {
    decodeOCBdata(words, debug);
}

// Print one line per set error bit stored in the OCBDataPacket::ocb_errors member.
void OCBDataPacket::decode_ocb_errors() const {
    int n = 0;
    for (size_t i = 0; i < ocb_errors.size(); ++i) {
        if (ocb_errors[i]) {
            std::cerr << "OCB trailer error bit " << i << ": " << OCB_ERROR_MESSAGES[i] << "\n";
            ++n;
        }
    }
    (void)n; // silence unused warning in case caller doesn't care
}

void OCBDataPacket::decodeOCBdata(const std::vector<uint32_t>& words, bool debug) {
    // throw std::runtime_error("Testing error handling in OCBDataPacket::decodeOCBdata");
    if (words.size() < 2) throw std::runtime_error("OCB packet too small");

    check_expected_word(words.front(), WordID::OCB_PACKET_HEADER);
    check_expected_word(words.back(),  WordID::OCB_PACKET_TRAILER);

    std::unique_ptr<Word> header_word = parse_word(words.front());
    std::unique_ptr<Word> trailer_word = parse_word(words.back());
    auto* ocb_packet_header  = dynamic_cast<OCBPacketHeader*>(header_word.get());
    auto* ocb_packet_trailer = dynamic_cast<OCBPacketTrailer*>(trailer_word.get());

    if (ocb_packet_header->gate_type != ocb_packet_trailer->gate_type) {
        throw std::runtime_error("Different gate type in OCB packet header and trailer!");
    }
    if (ocb_packet_header->gate_tag != ocb_packet_trailer->gate_tag) {
        throw std::runtime_error("Different gate tag in OCB packet header and trailer!");
    }

    event.event_id = ocb_packet_header->event_number;
    // Store trailer error bits in this packet and report any set errors
    ocb_errors = ocb_packet_trailer->errors;
    decode_ocb_errors();

    // Check word count and construct FEB data packets
    int global_index = 0;
    int gate_header_index = -1;
    int feb_id = -1;
    int nbr_feb_words = 0;
    int nbr_gts = 0;
    for (auto& w : words) {
        std::unique_ptr<Word> parsed_w = parse_word(w);

        switch (parsed_w->word_id) {
            case WordID::GATE_HEADER: {
                auto* gate_header = static_cast<GateHeader*>(parsed_w.get());
                if (gate_header->header_type != 0) nbr_feb_words++;
                else { 
                    // reset FEB word counter
                    nbr_feb_words = 0;
                    nbr_gts = 0;
                    // Store index of current gate header and board id
                    gate_header_index = global_index;
                    feb_id = gate_header->board_id;
                    // word count should be increased only if header 0 is followed by header 1, otherwise it means header 0 is artificially added by the OCB
                    if (global_index + 1 < (int)words.size()) {
                        std::unique_ptr<Word> next_w = parse_word(words.at(global_index+1));
                        if (next_w->word_id == WordID::GATE_HEADER) nbr_feb_words++;
                    }
                }
                break;
            }

            case WordID::GATE_TIME:
            case WordID::HOLD_TIME: {
                nbr_feb_words++;
                break;
            }

            case WordID::GTS_HEADER: {
                nbr_gts++;
                if (nbr_gts > OCBConfig::NUM_GTS_BEFORE_EVENT) nbr_feb_words++;
                break;
            }

            // increment FEB word count only if number of GTS headers received is above GTS_BEFORE_EVENT
            case WordID::GTS_TRAILER1:
            case WordID::GTS_TRAILER2:
            case WordID::HIT_TIME:
            case WordID::HIT_AMPLITUDE: {
                if (nbr_gts > OCBConfig::NUM_GTS_BEFORE_EVENT) nbr_feb_words++;
                break;
            }

            case WordID::EVENT_DONE: {
                auto* event_done = static_cast<EventDone*>(parsed_w.get());
                // Check word count
                if ((int)event_done->word_count != nbr_feb_words) {
                    std::cerr << "Word count in EventDone ( " + std::to_string(event_done->word_count) 
                                             << " ) does not match # words in FEB packet ( " << std::to_string(nbr_feb_words) << " )\n";
                }
                break;
            }

            case WordID::FEB_DATA_PACKET_TRAILER: {
                nbr_feb_words++;
                if (gate_header_index < 0) {
                    throw std::runtime_error("FEB Data Packet Trailer received without corresponding Gate Header");
                }

                if (feb_id < 0 || feb_id >= (int)event.febs.size()) {
                    std::cerr << "Warning: encountered FEB with invalid board id " << feb_id << ", skipping\n";
                } 
                else if (event.febs[feb_id] != nullptr) {
                    std::cerr << "Warning: FEB data packet for board " << feb_id << " already received\n";
                }
                else {
                    std::vector<uint32_t> feb_packet_word_list;
                    for (int k = gate_header_index; k < global_index+1; ++k) {
                        feb_packet_word_list.push_back(words[k]);
                    }
                    event.febs[feb_id] = std::make_shared<FEBDataPacket>(feb_packet_word_list);
                }

                // Reset FEB index
                gate_header_index = -1;
                break;
            }
            
            default: {
                std::cerr << "Warning: encountered word id not belonging to FEB data packet: " << parsed_w->word_id << "\n";
            }

        }
        ++global_index;
    }
}

std::ostream &operator<<(std::ostream &out, const OCBDataPacket &event) {
        try {
            out
            << std::setfill('#')<<std::setw(16)<<" Event ID: "<<std::setfill(' ')<<std::setw(12)<<event.get_event_id()<<std::endl;

            for (size_t board_id = 0; board_id < OCBConfig::NUM_FEBS_PER_OCB; board_id++){
                if (event.hasData(board_id)) {
                    auto feb_packet = event[board_id];
                    out << "FEB " << board_id << " has " << feb_packet.get_hit_times().size() << " decoded time hits, and " 
                    << feb_packet.get_hit_amplitudes().size() << " decoded amplitude hits." << std::endl;
                    for (const auto& hit_time : feb_packet.get_hit_times()) {
                        out << hit_time;
                    }
                    for (const auto& hit_amplitude : feb_packet.get_hit_amplitudes()) {
                        out << hit_amplitude;
                    }
                }
            }
        } catch (const std::runtime_error& e) {
            std::cerr << "Runtime error: " << e.what() << '\n';
        }

        return out;
}
