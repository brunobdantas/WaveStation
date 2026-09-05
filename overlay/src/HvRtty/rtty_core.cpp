// MSHV RTTY integration - Copyright 2026
// RTTY DSP C++11 implementation; see THIRD_PARTY_NOTICES.md.
#include "rtty_core.h"
#include <cmath>
#include <cctype>
#include <algorithm>

namespace mshv_rtty {

static const char LTRS[32] = {
    0,'E','\n','A',' ','S','I','U','\r','D','R','J','N','F','C','K',
    'T','Z','L','W','H','Y','P','Q','O','B','G',0,'M','X','V',0
};
static const char FIGS[32] = {
    0,'3','\n','-',' ','\'', '8','7','\r','$','4',0,',','!',':','(',
    '5','"',')','2','#','6','0','1','9','?','&',0,'.','/',';',0
};
static const uint8_t FIGS_SHIFT = 27;
static const uint8_t LTRS_SHIFT = 31;
static const double PI2 = 6.283185307179586476925286766559;

Ita2::Ita2() : figs_(false) {}
void Ita2::reset() { figs_ = false; }
char Ita2::decode(uint8_t code) {
    code &= 31;
    if (code == FIGS_SHIFT) { figs_ = true; return 0; }
    if (code == LTRS_SHIFT) { figs_ = false; return 0; }
    return figs_ ? FIGS[code] : LTRS[code];
}
int Ita2::findCode(char c, bool figs) {
    const char *t = figs ? FIGS : LTRS;
    for (int i=0;i<32;i++) if (t[i] == c) return i;
    return -1;
}
std::vector<uint8_t> Ita2::encode(const std::string &text) {
    std::vector<uint8_t> out;
    bool figs = false;
    for (size_t n=0;n<text.size();++n) {
        char c = (char)std::toupper((unsigned char)text[n]);
        if (c=='\n') { out.push_back(8); out.push_back(2); continue; }
        int code = findCode(c, figs);
        if (code < 0) {
            int other = findCode(c, !figs);
            if (other >= 0) {
                figs = !figs;
                out.push_back(figs ? FIGS_SHIFT : LTRS_SHIFT);
                code = other;
            }
        }
        if (code < 0) code = findCode(' ', figs);
        out.push_back((uint8_t)code);
    }
    return out;
}

Decoder::Decoder(const Config &cfg) : cfg_(cfg) { reset(); }
void Decoder::reset() {
    samplesPerBit_ = cfg_.sampleRate / cfg_.baud;
    intLen_ = std::max(8, (int)std::floor(samplesPerBit_ + 0.5));
    markPhase_=spacePhase_=0.0;
    markInc_=PI2*cfg_.markHz/cfg_.sampleRate;
    spaceInc_=PI2*cfg_.spaceHz/cfg_.sampleRate;
    mi_.assign(intLen_,0.0); mq_.assign(intLen_,0.0); si_.assign(intLen_,0.0); sq_.assign(intLen_,0.0);
    miSum_=mqSum_=siSum_=sqSum_=0.0; ringPos_=0;
    lastBit_=true; noiseFloor_=1e-8; state_=Idle; counter_=0.0; data_=0; bits_=0;
    ita2_.reset(); lastDisc_=0.0; lastEnergy_=0.0;
}
char Decoder::process(double s) {
    // Soft limiter keeps audio-level differences from upsetting the discriminator.
    s = std::max(-1.5, std::min(1.5, s));
    double mc=std::cos(markPhase_), ms=std::sin(markPhase_);
    double sc=std::cos(spacePhase_), ss=std::sin(spacePhase_);
    markPhase_ += markInc_; if (markPhase_>=PI2) markPhase_-=PI2;
    spacePhase_ += spaceInc_; if (spacePhase_>=PI2) spacePhase_-=PI2;
    double nmi=s*mc, nmq=s*ms, nsi=s*sc, nsq=s*ss;
    miSum_ += nmi-mi_[ringPos_]; mqSum_ += nmq-mq_[ringPos_];
    siSum_ += nsi-si_[ringPos_]; sqSum_ += nsq-sq_[ringPos_];
    mi_[ringPos_]=nmi; mq_[ringPos_]=nmq; si_[ringPos_]=nsi; sq_[ringPos_]=nsq;
    if (++ringPos_>=intLen_) ringPos_=0;
    double mm=miSum_*miSum_+mqSum_*mqSum_;
    double sm=siSum_*siSum_+sqSum_*sqSum_;
    double total=mm+sm+1e-18;
    double disc=(mm-sm)/total;
    lastDisc_=disc; lastEnergy_=total;
    const double hyst=0.055;
    if (lastBit_ && disc < -hyst) lastBit_=false;
    else if (!lastBit_ && disc > hyst) lastBit_=true;
    bool bit=lastBit_;
    // Lower-envelope noise tracking. The floor rises extremely slowly while a
    // carrier is present, so a long idle MARK cannot teach the gate that the
    // signal itself is "noise". It falls quickly when the channel quiets.
    if (total < noiseFloor_) noiseFloor_ = noiseFloor_*0.97 + total*0.03;
    else noiseFloor_ = noiseFloor_*0.999995 + total*0.000005;
    const double absGate = 1.0e-8;
    bool present = total > std::max(absGate, noiseFloor_*3.0);

    switch(state_) {
    case Idle:
        if (!bit && present) { state_=WaitStart; counter_=samplesPerBit_*0.50; }
        break;
    case WaitStart:
        counter_-=1.0;
        if (counter_<=0.0) {
            if (!bit) { state_=Data; data_=0; bits_=0; counter_=samplesPerBit_; }
            else state_=Idle;
        }
        break;
    case Data:
        counter_-=1.0;
        if (counter_<=0.0) {
            if (bit) data_ |= (uint8_t)(1u<<bits_);
            ++bits_;
            if (bits_>=5) { state_=Stop; counter_=samplesPerBit_; }
            else counter_ += samplesPerBit_;
        }
        break;
    case Stop:
        counter_-=1.0;
        if (counter_<=0.0) {
            state_=Idle;
            if (bit) return ita2_.decode(data_);
        }
        break;
    }
    return 0;
}
std::string Decoder::push(const int *samples, int count, double scale) {
    std::string out;
    for (int i=0;i<count;i++) { char c=process(samples[i]*scale); if(c) out.push_back(c); }
    return out;
}
std::string Decoder::pushFloat(const float *samples, int count) {
    std::string out;
    for (int i=0;i<count;i++) { char c=process(samples[i]); if(c) out.push_back(c); }
    return out;
}

Encoder::Encoder(const Config &cfg) : cfg_(cfg), phase_(0.0) {}
void Encoder::appendTone(std::vector<int> &out, bool mark, double bitUnits, double amplitude) {
    double hz = mark ? cfg_.markHz : cfg_.spaceHz;
    double countExact = (cfg_.sampleRate/cfg_.baud)*bitUnits;
    int count=(int)std::floor(countExact+0.5);
    double inc=PI2*hz/cfg_.sampleRate;
    const double full=8380000.0*std::max(0.01,std::min(0.98,amplitude));
    for(int i=0;i<count;i++) {
        out.push_back((int)(full*std::sin(phase_)));
        phase_+=inc; if(phase_>=PI2) phase_-=PI2;
    }
}
std::vector<int> Encoder::generate(const std::string &text,double amplitude,double lead,double trail) {
    std::vector<int> out;
    int leadN=(int)(lead*cfg_.sampleRate);
    int trailN=(int)(trail*cfg_.sampleRate);
    double inc=PI2*cfg_.markHz/cfg_.sampleRate;
    double full=8380000.0*std::max(0.01,std::min(0.98,amplitude));
    for(int i=0;i<leadN;i++){ out.push_back((int)(full*std::sin(phase_))); phase_+=inc; if(phase_>=PI2)phase_-=PI2; }
    std::vector<uint8_t> codes=ita2_.encode(text);
    for(size_t n=0;n<codes.size();++n) {
        uint8_t c=codes[n];
        appendTone(out,false,1.0,amplitude);             // start bit
        for(int b=0;b<5;b++) appendTone(out,((c>>b)&1)!=0,1.0,amplitude);
        appendTone(out,true,cfg_.stopBits,amplitude);    // stop mark
    }
    for(int i=0;i<trailN;i++){ out.push_back((int)(full*std::sin(phase_))); phase_+=inc; if(phase_>=PI2)phase_-=PI2; }
    return out;
}

} // namespace mshv_rtty
