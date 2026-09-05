// MSHV RTTY integration - Copyright 2026
// GPL-compatible derived integration for MSHV. See package THIRD_PARTY_NOTICES.md.
#ifndef MSHV_RTTY_CORE_H
#define MSHV_RTTY_CORE_H

#include <vector>
#include <string>
#include <stdint.h>

namespace mshv_rtty {

struct Config {
    double sampleRate;
    double baud;
    double markHz;
    double spaceHz;
    double stopBits;
    Config() : sampleRate(12000.0), baud(45.45), markHz(2125.0), spaceHz(2295.0), stopBits(1.5) {}
};

class Ita2 {
public:
    Ita2();
    void reset();
    char decode(uint8_t code);
    std::vector<uint8_t> encode(const std::string &text);
private:
    bool figs_;
    static int findCode(char c, bool figs);
};

class Decoder {
public:
    explicit Decoder(const Config &cfg = Config());
    void reset();
    std::string push(const int *samples, int count, double inputScale = 1.0 / 8388607.0);
    std::string pushFloat(const float *samples, int count);
    double discriminator() const { return lastDisc_; }
    double signalMetric() const { return lastEnergy_; }
private:
    enum State { Idle, WaitStart, Data, Stop };
    Config cfg_;
    double samplesPerBit_;
    int intLen_;
    double markPhase_, spacePhase_, markInc_, spaceInc_;
    std::vector<double> mi_, mq_, si_, sq_;
    double miSum_, mqSum_, siSum_, sqSum_;
    int ringPos_;
    bool lastBit_;
    double noiseFloor_;
    State state_;
    double counter_;
    uint8_t data_;
    int bits_;
    Ita2 ita2_;
    double lastDisc_, lastEnergy_;
    char process(double s);
};

class Encoder {
public:
    explicit Encoder(const Config &cfg = Config());
    std::vector<int> generate(const std::string &text, double amplitude = 0.72,
                              double leadMarkSeconds = 0.30, double trailMarkSeconds = 0.20);
private:
    Config cfg_;
    Ita2 ita2_;
    double phase_;
    void appendTone(std::vector<int> &out, bool mark, double bitUnits, double amplitude);
};

} // namespace mshv_rtty
#endif
