#include "OCBDecoder.h"
#include <stdexcept>
// #include <map>
#include <array>

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
FEBDataPacket::FEBDataPacket(const std::vector<uint32_t>& words, bool debug) {
    m_debug = debug;

    if (words.size() < 2) {
        if (m_debug) std::cerr << "FEB data packet too small";
        is_corrupted = true;
        return;
    }

    // Check that first word corresponds to Gate header
    std::unique_ptr<Word> header_word = parse_word(words.front());
    if (header_word->word_id != WordID::GATE_HEADER) {
        if (m_debug) std::cerr << "Missing Gate Header";
        is_corrupted = true;
        return;
    }
    // Get board_id from Gate header
    auto* gate_header  = dynamic_cast<GateHeader*>(header_word.get());
    board_id = gate_header->board_id;

    // Check that last word corresponds to FEB data packet trailer
    std::unique_ptr<Word> trailer_word = parse_word(words.back());
    if (trailer_word->word_id != WordID::FEB_DATA_PACKET_TRAILER) {
        if (m_debug) std::cerr << "Missing FEB Data Packet Trailer";
        is_corrupted = true;
        return;
    }

    // Store error bits from FEB Data Packet trailer
    auto* feb_packet_trailer = dynamic_cast<FEBDataPacketTrailer*>(trailer_word.get());
    nb_decoder_errors = feb_packet_trailer->nb_decoder_errors;
    if (nb_decoder_errors > 0) has_feb_errors = true;
    if (feb_packet_trailer->rb_wr_error) addError(FEBDataPacketErrors::rb_wr_err);
    if (feb_packet_trailer->event_done_timeout) addError(FEBDataPacketErrors::event_done_timeout);
    if (feb_packet_trailer->l1_fifo_full) addError(FEBDataPacketErrors::l1_fifo_full);
    if (feb_packet_trailer->l0_fifo_full) addError(FEBDataPacketErrors::l0_fifo_full);

    // optional hold_time
    if (words.size() > 1 && parse_word(words.at(1))->word_id == WordID::HOLD_TIME){
        std::unique_ptr<Word> hold_time_word = parse_word(words.at(1));
        auto* hold_time_object  = dynamic_cast<HoldTime*>(hold_time_word.get());
        hold_time = hold_time_object->hold_time;
    }

    // Decode FEB data, i.e. get hit times and amplitudes for all channels 
    decodeFEBdata(words);
}

int FEBDataPacket::find_matching_gts_tag(uint32_t tag_id, std::vector<uint32_t>& gts_tags) const {
    // Find latest GTS tag whose 2 LS bits match those of tag_id
    constexpr int TAG_MASK = 0x3;
    for (auto gts_tag = gts_tags.rbegin(); gts_tag != gts_tags.rend(); ++gts_tag) {
        if ((*gts_tag & TAG_MASK) == (tag_id & TAG_MASK)) {
            return *gts_tag;
        }
    }
    return -1; // no matching GTS tag found
}

void FEBDataPacket::addError(unsigned int err){
    has_feb_errors = true;
    feb_errors.push_back(err);
}

void FEBDataPacket::decodeFEBdata(const std::vector<uint32_t>& words) {

    std::vector<uint32_t> gts_tags;
    std::map<HitTimeKey, HitTimeData> hit_times_map;
    std::map<uint32_t, HitAmplitudeData> hit_amplitudes_map; // map channel id to hit amplitude data
    
    int global_feb_index = 0; // word index
    int nbr_feb_words = 0; // nbr of feb words (excluding GTS before event)
    int nbr_artif_feb_words = 0; // nbr of artificial FEB words

    bool open_gts_packet = false;

    for (auto& w : words) {
        std::unique_ptr<Word> base = parse_word(w);
        WordID id = base->word_id;
        bool word_before_ev = false;

        if (id == WordID::GATE_HEADER) {
            auto* gate_header = static_cast<GateHeader*>(base.get());
            if (gate_header->header_type == 0) {// Gate header A
                // if header 0 is not followed by header 1, it means that header 0 is artificially added by the OCB
                if (global_feb_index + 1 < static_cast<int>(words.size())) {
                    std::unique_ptr<Word> next_w = parse_word(words.at(global_feb_index+1));
                    if (next_w->word_id != WordID::GATE_HEADER) nbr_artif_feb_words++;
                }
            }
        }

        else if (id == WordID::EVENT_DONE) {
            auto* event_done = static_cast<EventDone*>(base.get());
            // Check word count
            if (static_cast<int>(event_done->word_count) != nbr_feb_words - nbr_artif_feb_words) {
                std::cerr << "Word count in EventDone ( " + std::to_string(event_done->word_count) 
                                            << " ) does not match # words in FEB packet ( " << std::to_string(nbr_feb_words - nbr_artif_feb_words) << " )\n";
            }
        }

        else if (id == WordID::GTS_HEADER) {
            if (open_gts_packet) {
                if (m_debug) std::cerr << "Missing GTS Trailer before new GTS Header in FEB" << board_id << "\n";
                has_missing_gts = true;
            }
            auto* gts_header = static_cast<GTSHeader*>(base.get());
            gts_tags.push_back(gts_header->gts_tag);
            open_gts_packet = true;
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

                if (!inserted) {
                    // If not inserted, means second rising edge detected before falling edge
                    // This should never happen
                    std::cerr << "Rising edge received twice for same channel_id=" << std::to_string(channel_id) << 
                        " and hit_id=" << std::to_string(hit_id) << " -> skipping...\n";
                }
                else {
                    // Fill rising time info for the hit
                    auto& h = it->second;
                    h.set_hit_time_rise(hit->hit_time);
                    h.set_tag_id_rise(hit->tag_id);
                    h.set_gts_tag_rise_received(gts_tags.empty() ? -1 : gts_tags.back());
                    h.set_gts_tag_rise(find_matching_gts_tag(hit->tag_id, gts_tags));
                }
            }
            else {
                // Falling edge
                auto it = hit_times_map.find(key);
                if (it == hit_times_map.end()) {
                    // Rising edge must be received before falling edge
                    std::cerr << "Falling edge received before rising edge for channel_id=" << std::to_string(channel_id) << 
                    " and hit_id=" << std::to_string(hit_id) << " -> skipping...\n";
                }
                else if (it->second.get_hit_time_fall() != -1) {
                    // Falling edge already received for this hit
                    std::cerr << "Falling edge received twice for same channel_id=" << std::to_string(channel_id) << 
                        " and hit_id=" << std::to_string(hit_id) << " -> skipping...\n";
                }
                else {
                    // Fill falling time info for the hit
                    auto& h = it->second;
                    h.set_hit_time_fall(hit->hit_time);
                    h.set_tag_id_fall(hit->tag_id);
                    h.set_gts_tag_fall_received(gts_tags.empty() ? -1 : gts_tags.back());
                    h.set_gts_tag_fall(find_matching_gts_tag(hit->tag_id, gts_tags));

                    // #### All this should be removed with data!!!
                    // Save hit time data
                    _hit_times.push_back(std::move(h));

                    // Remove from map -> necessary for the simulation because of bug on hit_id in the simulation
                    hit_times_map.erase(key);
                    // ####
                }
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
                    std::cerr << "High Gain Amplitude received twice for same channel: channel_id=" << std::to_string(channel_id) <<" -> Skipping...\n";
                }
                else {
                    h.set_amplitude_hg(hit->amplitude_value);
                    h.set_tag_id_hg(hit->tag_id);
                    h.set_gts_tag_hg_received(gts_tags.empty() ? -1 : gts_tags.back());
                    h.set_gts_tag_hg(find_matching_gts_tag(hit->tag_id, gts_tags));
                }
            }
            else {
                // Amplitude LG
                if (!inserted && h.get_amplitude_lg() != -1) {
                    std::cerr << "Low Gain Amplitude received twice for same channel: channel_id=" << std::to_string(channel_id) << " -> Skipping...\n";
                }
                else {
                    h.set_amplitude_lg(hit->amplitude_value);
                    h.set_tag_id_lg(hit->tag_id);
                    h.set_gts_tag_lg_received(gts_tags.empty() ? -1 : gts_tags.back());
                    h.set_gts_tag_lg(find_matching_gts_tag(hit->tag_id, gts_tags));
                }
            }
        }

        else if (id == WordID::GTS_TRAILER1) {
            if (!open_gts_packet) {
                std::cerr << "Missing GTS Header in FEB" << board_id << "\n";
                has_missing_gts = true;
            }
            // Check that GTS tag matches the last GTS header
            auto* gts_trailer1 = static_cast<GTSTrailer1*>(base.get());
            int current_gts_tag = gts_tags.empty() ? -1 : gts_tags.back();
            if (current_gts_tag != (int)gts_trailer1->gts_tag) {
                if (m_debug) std::cerr << "GTS Trailer does not match GTS header in FEB" << board_id << "\n";
                has_missing_gts = true;
                gts_tags.push_back(gts_trailer1->gts_tag); // still add it to avoid desync
            }
        }

        else if (id == WordID::GTS_TRAILER2) {
            // Get GTS time and map it to current GTS tag
            if (!gts_tags.empty()) { // If Trailer1 received before Trailer2, this should always be true
                auto* gts_trailer2 = static_cast<GTSTrailer2*>(base.get());
                uint32_t gts_time = gts_trailer2->gts_time;
                _gts_tag_map[gts_tags.back()] = gts_time;
            }
            open_gts_packet = false;
        }

        global_feb_index++;
        // Keep track of words sent before event (not counted by FEB)
        if (id == WordID::GTS_HEADER || id == WordID::GTS_TRAILER1 || id == WordID::GTS_TRAILER2 || id == WordID::HIT_TIME || id == WordID::HIT_AMPLITUDE) {
            if (gts_tags.size() <= OCBConfig::NUM_GTS_BEFORE_EVENT) word_before_ev = true;
        }
        if (!word_before_ev) nbr_feb_words++;
    }

    // Check that last GTS trailer was received
    if (open_gts_packet) {
        if (m_debug) std::cerr << "Missing GTS Trailer in FEB" << board_id << "\n";   
        has_missing_gts = true;
    }

    // Move completed hits into the vectors in FEBDataPacket
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

OCBDataPacket::OCBDataPacket(const uint32_t* words, size_t size, bool debug) {
    m_debug = debug;
    m_size = size;
    // size is in bytes; compute number of 32-bit words
    size_t nwords = size / 4;
    // decode directly from the provided pointer range to avoid copying
    decodeOCBdata(words, nwords);
}

void OCBDataPacket::decodeOCBdata(const uint32_t* words, size_t nwords) {
    if (m_debug) {
        std::cout << "Using debugging mode for OCB Data Packet decoding\n";
    }

    if (nwords < 2) {
        if (m_debug) std::cerr << "OCB data packet too small";
        event.corrupted_ocb_packet = true;
        return;
    }

    // Check that first word corresponds to OCB header
    std::unique_ptr<Word> header_word = parse_word(words[0]);
    if (header_word->word_id != WordID::OCB_PACKET_HEADER) {
        if (m_debug) std::cerr << "Missing OCB Packet Header";
        event.corrupted_ocb_packet = true;
        return;
    }
    // Get event number from OCB header
    auto* ocb_packet_header  = dynamic_cast<OCBPacketHeader*>(header_word.get());
    event.event_id = ocb_packet_header->event_number;

    std::unique_ptr<Word> trailer_word = parse_word(words[nwords-1]);
    if (trailer_word->word_id != WordID::OCB_PACKET_TRAILER) {
        if (m_debug) std::cerr << "Missing OCB Packet Trailer";
        event.corrupted_ocb_packet = true;
        return;
    }
    // Store error bits from OCB trailer
    auto* ocb_packet_trailer = dynamic_cast<OCBPacketTrailer*>(trailer_word.get());
    event.ocb_errors = ocb_packet_trailer->errors;

    // Check word count and construct FEB data packets
    int global_index = 0;
    int gate_header_index = -1;
    int feb_id = -1;

    for (size_t i = 0; i < nwords; ++i) {
        uint32_t w = words[i];
        std::unique_ptr<Word> parsed_w = parse_word(w);
        WordID id = parsed_w->word_id;

        if (id == WordID::GATE_HEADER){
            auto* gate_header = static_cast<GateHeader*>(parsed_w.get());
            if (gate_header->header_type == 0) {// Gate header A
                if (gate_header_index != -1) {
                    if (m_debug) std::cerr << "No FEB Data packet trailer received for FEB " << feb_id << " before new Gate Header\n";
                    add_corrupted_feb_error(feb_id);
                }
                gate_header_index = global_index;
                feb_id = gate_header->board_id;
            }
        }

        else if (id == WordID::FEB_DATA_PACKET_TRAILER) {
            auto* feb_trailer = static_cast<FEBDataPacketTrailer*>(parsed_w.get());
            if (gate_header_index < 0) {
                if (m_debug) std::cerr << "FEB Data Packet Trailer received for FEB " << feb_trailer->board_id << " without corresponding Gate Header\n";
                add_corrupted_feb_error(feb_trailer->board_id);
            }
            else if ((int)feb_trailer->board_id != feb_id) {
                if (m_debug) std::cerr << "FEB Data Packet Trailer received for FEB " << feb_trailer->board_id 
                            << " does not match FEB id " << feb_id << " in last Gate Header\n";
                add_corrupted_feb_error(feb_trailer->board_id);
                add_corrupted_feb_error(feb_id);
            }

            else if (feb_id < 0 || feb_id >= static_cast<int>(event.febs.size())) {
                if (m_debug) std::cerr << "Warning: encountered FEB with invalid board id " << feb_id << ", skipping\n";
            } 
            else if (event.febs[feb_id] != nullptr) {
                if (m_debug) std::cerr << "Warning: FEB data packet for board " << feb_id << " already received\n";
            }
            else { // Save FEB data packet only if not corrupted (i.e. no missing header or trailer) and valid board id
                event.febs[feb_id] = std::make_shared<FEBDataPacket>(std::vector<uint32_t>(&words[gate_header_index], &words[global_index + 1]), m_debug);
            }

            // Reset FEB data packet index
            gate_header_index = -1;
        }
        ++global_index;
    }

    // Check that last FEB data packet was closed properly
    if (gate_header_index != -1) {
        if (m_debug) std::cerr << "No FEB Data packet trailer received for FEB " << feb_id << "\n";
        add_corrupted_feb_error(feb_id);
    }
}

std::ostream &operator<<(std::ostream &out, const OCBDataPacket &event) {
        try {
            if (event.isCorrupted()) {
                out << "Corrupted OCB data packet: missing header or trailer." << std::endl;
            }
            out
            << std::setfill('#')<<std::setw(16)<<" Event ID: "<<std::setfill(' ')<<std::setw(12)<<event.get_event_id()<<std::endl;
            if (event.has_ocb_errors()) {
                out << "OCB data packet has the following encoded errors:" << std::endl;
                const auto& ocb_errors = event.get_ocb_errors();
                for (size_t i = 0; i < ocb_errors.size(); ++i) {
                    if (ocb_errors[i]) {
                        out << "Error message: " << OCB_ERROR_MESSAGES[i] << std::endl;
                    }
                }
            }

            for (size_t board_id = 0; board_id < OCBConfig::NUM_FEBS_PER_OCB; board_id++){
                if (event.isCorruptedFEB(board_id)) {
                    out << "FEB " << board_id << " data packet is corrupted (missing header or trailer)." << std::endl;
                }
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

                    if (feb_packet.hasFEBerrors()) {
                        out << "FEB " << board_id << " has the following encoded errors: " << std::endl;
                        for (auto err : feb_packet.getFEBerrors()){
                            auto errMsg = FEBDataPacket::feb_error_messages.find(err);
                            out << "Error message: " << errMsg->second << std::endl;
                        } 
                    }
                }
            }

        } catch (const std::runtime_error& e) {
            std::cerr << "Runtime error: " << e.what() << '\n';
        }

        return out;
}
