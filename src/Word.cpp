#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <array>
#include "Word.h"

// ----------------------
// BASE WORD CLASS
// ----------------------

static uint32_t get_bits(int value, int start, int length){
    return (value >> start) & ((1 << length) - 1);
}

WordID get_wordID(uint32_t word) {
    // Extract word ID as the most significant 4 bits (28...31) of the 32-bit word
    return WordID(get_bits(word, 28, 4));
}

Word::Word(uint32_t raw, WordID expected) : word_id(expected) {
    WordID received = get_wordID(raw);
    if (received != expected) {
        throw std::runtime_error("Wrong word ID: received " + std::to_string(received) + ", expected: " + std::to_string(expected));
    }
}

// ----------------------
// DERIVED WORD CLASSES
// ----------------------

GateHeader::GateHeader(uint32_t raw) : Word(raw, WordID::GATE_HEADER) {
            board_id = get_bits(raw, 20, 8);
            header_type = get_bits(raw, 19, 1);

            if (header_type == 0) { 
                gate_type  = get_bits(raw, 16, 3);
                gate_number = get_bits(raw, 0, 16);
            } else {
                gate_time_from_GTS = get_bits(raw, 0, 11);
            }
}

void GateHeader::print(std::ostream& os) const {
    if (header_type == 0) {
        os << "[" << std::setw(6) << word_id << "]" << " Gate Header Word - Board ID: " << board_id
           << ", Gate header: " << header_type
           << ", Gate Type: " << gate_type
           << ", Gate Number: " << gate_number << "\n";
    } else {
        os << "[" << std::setw(6) << word_id << "]" << " Gate Header Word - Board ID: " << board_id
           << ", Gate header: " << header_type
           << ", Gate time: " << gate_time_from_GTS << "\n";
    }
}

GTSHeader::GTSHeader(uint32_t raw) : Word(raw, WordID::GTS_HEADER) {
    gts_tag = get_bits(raw, 0, 28);
}

void GTSHeader::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " GTS Header Word - GTS Tag: " << gts_tag << "\n";
}

HitTime::HitTime(uint32_t raw) : Word(raw, WordID::HIT_TIME) {
    channel_id = get_bits(raw, 20, 8);
    hit_id = get_bits(raw, 17, 3);
    tag_id = get_bits(raw, 15, 2);
    edge = get_bits(raw, 14, 1);
    hit_time = get_bits(raw, 0, 13);
}

void HitTime::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " Hit Time Word - Channel ID: " << channel_id
       << ", Hit ID: " << hit_id
       << ", Tag ID: " << tag_id
       << ", Edge: " << edge
       << ", Timestamp: " << hit_time << "\n";
}

HitAmplitude::HitAmplitude(uint32_t raw) : Word(raw, WordID::HIT_AMPLITUDE) {
    channel_id = get_bits(raw, 20, 8);
    hit_id = get_bits(raw, 17, 3);
    tag_id = get_bits(raw, 15, 2);
    amplitude_id = get_bits(raw, 12, 3);
    amplitude_value = get_bits(raw, 0, 12);
}

void HitAmplitude::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " Hit Amplitude Word - Channel ID: " << channel_id
       << ", Hit ID: " << hit_id
       << ", Tag ID: " << tag_id
       << ", Amp ID: " << amplitude_id
       << ", Amplitude: " << amplitude_value << "\n";
}

GTSTrailer1::GTSTrailer1(uint32_t raw) : Word(raw, WordID::GTS_TRAILER1) {
    gts_tag = get_bits(raw, 0, 28);
}

void GTSTrailer1::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " GTS Trailer1 Word - GTS Tag: " << gts_tag << "\n";
}

GTSTrailer2::GTSTrailer2(uint32_t raw) : Word(raw, WordID::GTS_TRAILER2) {
    data = get_bits(raw, 27, 1);
    ocb_busy = get_bits(raw, 26, 1);
    feb_busy = get_bits(raw, 25, 1);
    gts_time = get_bits(raw, 0, 20);
}

void GTSTrailer2::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " GTS Trailer 2 Word - Data: " << data
       << ", OCB busy: " << ocb_busy
       << ", FEB busy: " << feb_busy 
       << ", GTS Time: " << gts_time << "\n";
}

GateTrailer::GateTrailer(uint32_t raw) : Word(raw, WordID::GATE_TRAILER) {
    board_id = get_bits(raw, 20, 8);
    gate_type = get_bits(raw, 16, 3);
    gate_number = get_bits(raw, 0, 16);
}

void GateTrailer::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " Gate Trailer Word - Board ID: " << board_id
       << ", Gate Type: " << gate_type
       << ", Gate Number: " << gate_number << "\n";
}     

GateTime::GateTime(uint32_t raw) : Word(raw, WordID::GATE_TIME) {
    gate_time = get_bits(raw, 0, 28);
}

void GateTime::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " Gate Time Word - Gate time: " << gate_time << "\n";
}

OCBPacketHeader::OCBPacketHeader(uint32_t raw) : Word(raw, WordID::OCB_PACKET_HEADER) {
    gate_type = get_bits(raw, 25, 3);
    gate_tag = get_bits(raw, 23, 2);
    event_number = get_bits(raw, 0, 23);
}

void OCBPacketHeader::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " OCB Packet Header Word - Gate type: " << gate_type
       << ", Gate Tag: " << gate_tag
       << ", Event number: " << event_number << "\n";
}

OCBPacketTrailer::OCBPacketTrailer(uint32_t raw) : Word(raw, WordID::OCB_PACKET_TRAILER) {
    gate_type = get_bits(raw, 25, 3);
    gate_tag = get_bits(raw, 23, 2);
    for (size_t i = 0; i < errors.size(); i++){
        errors[i] = get_bits(raw, i, 1);
    }
}

void OCBPacketTrailer::print(std::ostream& os) const {
    int n_errors = 0;
    for (size_t i = 0; i < errors.size(); i++) {
        if (errors[i]) n_errors += 1;
    }
    os << "[" << std::setw(6) << word_id << "]" << " OCB Packet Trailer Word - Gate type: " << gate_type
       << ", Gate Tag: " << gate_tag
       << ", Number of errors: " << n_errors << "\n";
}

HoldTime::HoldTime(uint32_t raw) : Word(raw, WordID::HOLD_TIME) {
    board_id = get_bits(raw, 20, 8);
    header_type = get_bits(raw, 19, 1);
    hold_time = get_bits(raw, 0, 11);
}

void HoldTime::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " HOLD Time Word - Board ID: " << board_id
       << ", Header Type (start/stop): " << header_type
       << ", HOLD time: " << hold_time << "\n";
}

EventDone::EventDone(uint32_t raw) : Word(raw, WordID::EVENT_DONE) {
    board_id = get_bits(raw, 20, 8);
    gate_number = get_bits(raw, 16, 4);
    word_count = get_bits(raw, 0, 16);
}

void EventDone::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << "Event Done Word - Board ID: " << board_id 
       << ", Gate Number (4LSB): " << gate_number
       << ", Word Count: " << word_count << "\n";
}

FEBDataPacketTrailer::FEBDataPacketTrailer(uint32_t raw) : Word(raw, WordID::FEB_DATA_PACKET_TRAILER) {
    board_id = get_bits(raw, 20, 8);
    artificial_trl2 = get_bits(raw, 19, 1);
    event_done_timeout = get_bits(raw, 18, 1);
    d1_fifo_full = get_bits(raw, 17, 1);
    d0_fifo_full = get_bits(raw, 16, 1);
    rb_cnt_error = get_bits(raw, 15, 1);
    nb_decoder_errors = get_bits(raw, 0, 15);
}

void FEBDataPacketTrailer::print(std::ostream& os) const {
    os << "[" << std::setw(6) << word_id << "]" << " FEB Data Packet Trailer - Board ID: " << board_id << "\n";
}
// ----------------------
// FACTORY
// ----------------------

// Defining WordFactory as a pointer to a function uint32_t -> std::unique_ptr<Word>
using WordFactory = std::unique_ptr<Word>(*)(uint32_t);

template <class T>
std::unique_ptr<Word> construct(uint32_t raw) {
    return std::make_unique<T>(raw);
}

static std::unordered_map<WordID, WordFactory> WORD_CLASSES = {
    { WordID::GATE_HEADER,        &construct<GateHeader> },
    { WordID::GATE_TIME,          &construct<GateTime> },
    { WordID::HOLD_TIME,          &construct<HoldTime> },
    { WordID::GTS_HEADER,         &construct<GTSHeader> },
    { WordID::HIT_TIME,           &construct<HitTime> },
    { WordID::HIT_AMPLITUDE,      &construct<HitAmplitude> },
    { WordID::GTS_TRAILER1,       &construct<GTSTrailer1> },
    { WordID::GTS_TRAILER2,       &construct<GTSTrailer2> },
    { WordID::GATE_TRAILER,       &construct<GateTrailer> },
    { WordID::EVENT_DONE,         &construct<EventDone> },
    { WordID::OCB_PACKET_HEADER,  &construct<OCBPacketHeader> },
    { WordID::OCB_PACKET_TRAILER, &construct<OCBPacketTrailer> },
    { WordID::FEB_DATA_PACKET_TRAILER, &construct<FEBDataPacketTrailer> }
};

std::unique_ptr<Word> parse_word(uint32_t word) {
    WordID id = get_wordID(word);

    auto class_constructor = WORD_CLASSES.find(id);
    if (class_constructor == WORD_CLASSES.end())
        throw std::runtime_error("Unknown WordID: " + std::to_string(id));

    std::unique_ptr<Word> word_object = class_constructor->second(word);
    // word_object->print();
    return word_object;
}    