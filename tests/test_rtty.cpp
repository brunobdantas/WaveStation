#include "../overlay/src/HvRtty/rtty_core.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

static bool run(const std::string &msg, double amp, bool addNoise) {
    mshv_rtty::Config c; c.sampleRate=12000; c.baud=45.45; c.markHz=2125; c.spaceHz=2295; c.stopBits=1.5;
    mshv_rtty::Encoder e(c); std::vector<int> w=e.generate(msg,amp,0.30,0.20);
    if(addNoise){
        unsigned x=1;
        for(size_t i=0;i<w.size();++i){ x=x*1664525u+1013904223u; int n=((int)(x>>16)&0xffff)-32768; w[i]+=n*18; }
    }
    mshv_rtty::Decoder d(c); std::string got;
    for(size_t p=0;p<w.size();p+=317){ int n=(int)std::min<size_t>(317,w.size()-p); got += d.push(&w[p],n); }
    std::cout << "amp=" << amp << " noise=" << addNoise << " decoded=[" << got << "]\n";
    return got.find("PU2BRU")!=std::string::npos && got.find("599")!=std::string::npos;
}
int main(){
    bool a=run("CQ TEST PU2BRU 599 001",0.72,false);
    bool b=run("CQ TEST PU2BRU 599 001",0.15,false);
    bool c=run("CQ TEST PU2BRU 599 001",0.50,true);
    return (a&&b&&c)?0:1;
}
