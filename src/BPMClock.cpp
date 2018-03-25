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
    void BPMClock::step(float tempoParam)
    {        
        tempo = tempoParam;
        frequency = tempo/60.0f;

            LFOstep(1.0 / engineGetSampleRate());
            clockOutputValue = clamp(10.0f * sqr(), 0.0f, 10.0f);
    }
}






