//
//  pulseStreamGen.hpp
//
//
//  Created by Ross Cameron on 17/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Template.hpp"
#include <dsp/digital.hpp>
#include <cstring>

namespace pulseStreamGen{
    
    //// STEP-RELATED (REALTIME) COUNTERS/GAPS.
    // Step related variables: used to determine the frequency of source signal, and when KlokSpid must sends relevant pulses to output(s).
    unsigned long currentStep;
    unsigned long previousStep;
    unsigned long nextPulseStep;
    unsigned long nextExpectedStep;
    unsigned long stepGap;
    unsigned long stepGapPrevious;
    
    bool isSync;
    
    struct intBPMGen{
        float knobPosition;
        unsigned int BPM;
        unsigned int previousBPM;
        void intBPMStep ();
    };
    struct extBPMGen{
        bool activeCLK;
        bool activeCLKPrevious;
        SchmittTrigger CLKInputPort;        // Schmitt trigger to check thresholds on CLK input connector.
        void findExtClkSpeed ();
        void overflowBlock ();
        void extBPMStep (float);
        float scaledInput;
    };
    struct clkMod{
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
    };
    struct pulseGen{
        PulseGenerator sendPulse;
        bool sendingOutput; // These flags are related to "sendPulse" pulse generator (current pulse state).
        bool canPulse;
        float pulseDuration;
        float pTime;
        float GetPulsingTime(unsigned long int, float);      // This custom function returns pulse duration (ms), regardling number of samples (unsigned long int) and pulsation duration parameter (SETUP).
        void pulseGenStep();
    };
    struct probGate{
        unsigned int probKnob;
        SchmittTrigger gateTrigger;
        bool outcome;
        float r;
        float threshold;
        bool toss;
        void coinFlip ();
    };
    
    void clkStateChange ();
    void intOrExtClk ();
    
    
    class clockGen{
        intBPMGen;
        extBPMGen;
    };
    class singlePulseStreamGen{
        clkMod;
        pulseGen;
        probGate;
    };

    intBPMGen internalBPM;
    extBPMGen externalBPM;
    clkMod  clockModulator;
    pulseGen pulseGeneration;
    probGate probabilityGate;
}
