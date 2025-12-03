#include "OCBDecoder.h"
#include <stdexcept>

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
        throw std::runtime_error("Inconsistent hit data: channel_id/hit_id mismatch");
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

// ---------------- FEBDataPacket ----------------
FEBDataPacket::FEBDataPacket(const std::vector<uint32_t>& words) {
    if (words.empty())
        throw std::runtime_error("Empty FEBDataPacket words");

    // Gate header should be first
    if (parse_word(words.front())->word_id != WordID::GATE_HEADER)
        throw std::runtime_error("FEB packet does not start with GATE_HEADER");

    std::unique_ptr<Word> gate_header_word = parse_word(words.front());
    auto* gate_header_object  = dynamic_cast<GateHeader*>(gate_header_word.get());
    board_id = gate_header_object->board_id;

    // optional hold_time
    if (words.size() > 1 && parse_word(words.at(1))->word_id == WordID::HOLD_TIME){
        std::unique_ptr<Word> hold_time_word = parse_word(words.at(1));
        auto* hold_time_object  = dynamic_cast<HoldTime*>(hold_time_word.get());
        hold_time = hold_time_object->hold_time;
    }

    decodeFEBdata(words);
}

void FEBDataPacket::decodeFEBdata(const std::vector<uint32_t>& words) {
    int previous_gts_tag = -1;
    int current_gts_tag = -1;

    // Need to keep track of previous GTS tag since hit words from one GTS can be received in the following GTS packet
    std::vector<uint32_t> current_gts_packet;
    std::vector<uint32_t> previous_gts_packet;
    constexpr int TAG_MASK = 0x3;

    for (auto& w : words) {
        std::unique_ptr<Word> base = parse_word(w);
        WordID id = base->word_id;

        if (id == WordID::GTS_HEADER) {
            auto* gts_header = dynamic_cast<GTSHeader*>(base.get());
            current_gts_tag = gts_header->gts_tag;
            current_gts_packet.clear();
            current_gts_packet.push_back(w);
        }

        else if (id == WordID::HIT_TIME || id == WordID::HIT_AMPLITUDE) {
            // Extract HIT tag from time and amplitude hit word 
            uint32_t hit_tag = (id == WordID::HIT_TIME)
                    ? dynamic_cast<HitTime*>(base.get())->tag_id
                    : dynamic_cast<HitAmplitude*>(base.get())->tag_id;

            // Match HIT tag with current or previous GTS tag
            if ((hit_tag & TAG_MASK) == (current_gts_tag & TAG_MASK)) {
                current_gts_packet.push_back(w);
            }
            else if ((hit_tag & TAG_MASK) == (previous_gts_tag & TAG_MASK)) {
                previous_gts_packet.push_back(w);
            }
            else {
                throw std::runtime_error("Hit tag does not match current or previous GTS tag!");
            }
        }

        else if (id == WordID::GTS_TRAILER1) {
            auto* gts_trailer1 = dynamic_cast<GTSTrailer1*>(base.get());
            if ((int) gts_trailer1->gts_tag != current_gts_tag) {
                throw std::runtime_error("GTS tag in Trailer1 different from current GTS Header!");
            }
            current_gts_packet.push_back(w);
        }

        else if (id == WordID::GTS_TRAILER2) {
            if (current_gts_packet.empty()) {
                throw std::runtime_error("GTS Trailer received without corresponding GTS Header!");
            }
            current_gts_packet.push_back(w);

            if (!previous_gts_packet.empty()) {
                // Add hits from previous GTS to the FEB data packet
                extract_hits_from_gts(previous_gts_tag, previous_gts_packet);
                previous_gts_packet.clear();
            }

            previous_gts_tag = current_gts_tag;
            previous_gts_packet = std::move(current_gts_packet);
            current_gts_packet.clear();
        }

        else current_gts_packet.push_back(w);
    }

    if (!previous_gts_packet.empty()) {
        extract_hits_from_gts(previous_gts_tag, previous_gts_packet);
        previous_gts_packet.clear();
    }

    if (!current_gts_packet.empty()) {
        // This can happen when GTS_TRAILER2 is not received after GTS_HEADER.. Should we keep it?
        extract_hits_from_gts(current_gts_tag, current_gts_packet);
        current_gts_packet.clear();
    }
}

void FEBDataPacket::extract_hits_from_gts(int gts_tag, const std::vector<uint32_t>& block) {
    std::map<HitKey, std::vector<uint32_t> > hit_groups;

    for (auto& w : block) {
        std::unique_ptr<Word> base = parse_word(w);
        WordID id = base->word_id;

        if (id == WordID::HIT_TIME) {
            auto* hit = dynamic_cast<HitTime*>(base.get());
            // Words belonging to same hit data identified by same channel_id (channel in board) and hit_id (counter within same channel)
            HitKey key{hit->hit_id, hit->channel_id};
            hit_groups[key].push_back(w);
        }
        else if (id == WordID::HIT_AMPLITUDE) {
            auto* hit = dynamic_cast<HitAmplitude*>(base.get());
            HitKey key{hit->hit_id, hit->channel_id};
            hit_groups[key].push_back(w);
        }
    }

    for (const auto& [key, group] : hit_groups) {
        _hits.emplace_back(board_id, gts_tag, group);
    }
}

// ---------------- OCBDataPacket ----------------

void check_expected_word(uint32_t word, WordID expected_id) {
    std::unique_ptr<Word> word_object = parse_word(word);
    if (word_object->word_id != expected_id) {
        throw std::runtime_error("Wrong word type received: wordID = " + std::to_string(word_object->word_id) + ", expected = " + std::to_string(expected_id));
    }
}

OCBDataPacket::OCBDataPacket(const std::vector<uint32_t>& words, bool debug) {
    decodeOCBdata(words, debug);
}

void OCBDataPacket::decodeOCBdata(const std::vector<uint32_t>& words, bool debug) {
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

    event.event_number = ocb_packet_header->event_number;

    int index = 0;
    int start_index = -1;
    for (auto& w : words) {
        WordID wid = parse_word(w)->word_id;
        if (wid == WordID::GATE_HEADER) {
            start_index = index;
        }
        if (wid == WordID::FEB_DATA_PACKET_TRAILER) {
            if (start_index < 0) {
                throw std::runtime_error("FEB Data Packet Trailer received without corresponding Gate Header");
            }

            std::vector<uint32_t> feb_packet_word_list;
            for (int k = start_index; k < index+1; ++k) {
                feb_packet_word_list.push_back(words[k]);
            }

            // Get FEB board id from its gate header
            std::unique_ptr<Word> gate_header_word = parse_word(feb_packet_word_list.front());
            auto* gate_header_object  = dynamic_cast<GateHeader*>(gate_header_word.get());
            int feb_id = gate_header_object->board_id;

            if (feb_id < 0 || feb_id >= (int)event.febs.size()) {
                std::cerr << "Warning: encountered FEB with invalid board id " << feb_id << ", skipping\n";
            } 
            else if (event.febs[feb_id] != nullptr) {
                std::cerr << "Warning: FEB data packet for board " << feb_id << " already received\n";
            }
            else {
                event.febs[feb_id] = std::make_shared<FEBDataPacket>(feb_packet_word_list);
            }

            start_index = -1;
        }
        ++index;
    }
}