//
//  BPMClock.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 24/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "BPMClock.hpp"

namespace internalClock {

    BPMClock::BPMClock () {
        phase = 0.0f;
        pw = 0.5f;
        freq = 1.0f;
        tempo = 0;
        frequency = 2.0f;
        clockOutputValue = 0;
    }
    void BPMClock::setFreq(float freq_to_set)
    {
        freq = freq_to_set;
    }
    void BPMClock::LFOstep(float dt) {
        float deltaPhase = fminf(freq * dt, 0.5f);
        phase += deltaPhase;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }
    float BPMClock::sqr() {
        float sqr = phase < pw ? 1.0f : -1.0f;
        return sqr;
    }
    void BPMClock::internalClkStep(float tempoParam)
    {        
        tempo = roundf(tempoParam);
        frequency = tempo/60.0f;
        setFreq(frequency*4);
            LFOstep(1.0 / engineGetSampleRate());
            clockOutputValue = clamp(5.0f * sqr(), 0.0f, 5.0f);
    }
}






