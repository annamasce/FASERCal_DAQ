#pragma once
#include <cstdint>

enum WordID {
    GATE_HEADER = 0x0,
    GTS_HEADER = 0x1,
    HIT_TIME = 0x2,
    HIT_AMPLITUDE = 0x3,
    GTS_TRAILER1 = 0x4,
    GTS_TRAILER2 = 0x5,
    GATE_TRAILER = 0x6,
    GATE_TIME = 0x7,
    OCB_PACKET_HEADER = 0x8,
    OCB_PACKET_TRAILER = 0x9,
    HOLD_TIME = 0xB,
    EVENT_DONE = 0xC,
    FEB_DATA_PACKET_TRAILER = 0xD,
    HOUSEKEEPING = 0xE,
    SPECIAL_WORD = 0xF
};

WordID get_wordID(uint32_t word);

class Word {
    public:
        WordID word_id;

        Word(uint32_t raw, WordID expected);

        virtual ~Word() = default;

        virtual void print() const = 0;
        
};

class GateHeader : public Word {
    public:
        uint32_t header_type;
        uint32_t board_id;
        uint32_t gate_type = 0;
        uint32_t gate_number = 0;
        uint32_t gate_time_from_GTS = 0;

        GateHeader(uint32_t raw);

        void print() const override;
};

class GTSHeader : public Word {
    public:
        uint32_t gts_tag;

        GTSHeader(uint32_t raw);

        void print() const override;
};

class HitTime : public Word {
    public:
        uint32_t channel_id, hit_id, tag_id, edge, hit_time;

        HitTime(uint32_t raw);

        void print() const override;
};

class HitAmplitude : public Word {
    public:
        uint32_t channel_id, hit_id, tag_id, amplitude_id, amplitude_value;

    HitAmplitude(uint32_t raw);

    void print() const override;

};

class GTSTrailer1 : public Word {
    public:
        uint32_t gts_tag;

        GTSTrailer1(uint32_t raw);

        void print() const override;
};

class GTSTrailer2 : public Word {
    public:
        uint32_t data, ocb_busy, feb_busy, gts_time;

        GTSTrailer2(uint32_t raw);

        void print() const override;
};

class GateTrailer : public Word {
    public:
        uint32_t board_id, gate_type, gate_number;

        GateTrailer(uint32_t raw);

        void print() const override; 

};

class GateTime : public Word {
    public:
        uint32_t gate_time;

        GateTime(uint32_t raw);

        void print() const;
};

class OCBPacketHeader : public Word {
    public:
        uint32_t gate_type, gate_tag, event_number;

        OCBPacketHeader(uint32_t raw);

        void print() const override;
};

class OCBPacketTrailer : public Word {
    public:
        uint32_t gate_type, gate_tag;
        std::array<bool, 15> errors;

        OCBPacketTrailer(uint32_t raw);

        void print() const override;
};

class HoldTime : public Word {
    public:
        uint32_t board_id, header_type, hold_time;

        HoldTime(uint32_t raw);

        void print() const override;
};

class EventDone : public Word {
    public:
        uint32_t board_id, gate_number, word_count;

        EventDone(uint32_t raw);

        void print() const override;
};

class FEBDataPacketTrailer : public Word {
    public:
        uint_fast32_t board_id, nb_decoder_errors;
        bool event_done_timeout, d1_fifo_full, d0_fifo_full;

        FEBDataPacketTrailer(uint32_t raw);

        void print() const override;
};

std::unique_ptr<Word> parse_word(uint32_t word);

