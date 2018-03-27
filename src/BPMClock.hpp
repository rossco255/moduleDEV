//
//  BPMClock.hpp
//  rackdevstuff
//
//  Created by Ross Cameron on 24/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//


//**************************************************************************************
//
//BPM Clock module for VCV Rack by Alfredo Santamaria - AS - https://github.com/AScustomWorks/AS
//
//Based on code taken from Master Clock Module VCV Module Strum 2017 https://github.com/Strum/Strums_Mental_VCV_Modules
//**************************************************************************************

#include "Template.hpp"
#include "dsp/digital.hpp"
#include <sstream>
#include <iomanip>

namespace internalClock {
    struct BPMClock {
        BPMClock ( );
        float phase;
        float pw;
        float freq;
        void setFreq( float );
        void LFOstep ( float );
        float sqr( );
        int tempo;
        float frequency;
        float clockOutputValue;
        void internalClkStep(float);
    };
}
