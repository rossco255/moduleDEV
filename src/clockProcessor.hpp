//
//  clockProcessor.hpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "clockGenerator.hpp"

namespace pulseStreamGen {
    class clockProcessor{
        clockProcessor(float, float, float, float);  //a knob for clkMod, pw, probGate; and the master BPM input
        
        //clkMod -- Clock modulation: division and multiplication of the master BPM
        float list_fRatio[25]; // Real clock ratios list (array). Based on preset ratios while KlokSpid module runs as clock multiplier/divider, by knob position only.
        unsigned int rateRatioKnob;
        enum ClkModModeIds {
            X1,    // work at x1.
            DIV,    // divider mode.
            MULT    // muliplier mode.
        };     // Clock modulator modes.
        unsigned int clkModulMode;
        unsigned int pulseMultCounter;      // Pulse counter for mult mode, avoids continuous pulse when not receiving (set at max divider value, minus 1). Kind of "timeout".
        unsigned int pulseDivCounter;       // Pulse counter for divider mode (set at max divider value, minus 1).
        void clkModType();
        
        //pulseGen -- Generates a pulse based on the modulated clock
        PulseGenerator sendPulse;
        bool sendingOutput; // These flags are related to "sendPulse" pulse generator (current pulse state).
        bool canPulse;
        float pulseDuration;
        float pTime;
        float GetPulsingTime(unsigned long int, float);      // This custom function returns pulse duration (ms), regardling number of samples (unsigned long int) and pulsation duration parameter (SETUP).
        void pulseGenStep();
        
        //probGate -- weighted coin toss which determines if a gate is sent to output
        unsigned int probKnob;
        SchmittTrigger gateTrigger;
        bool outcome;
        float r;
        float threshold;
        bool toss;
        void coinFlip ();
    };
}
