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

namespace pulseStreamGen {
    
    struct streamGen{
        
        streamGen(float, float, bool);

        unsigned long currentStep;
        unsigned long previousStep;
        unsigned long nextPulseStep;
        unsigned long nextExpectedStep;
        unsigned long stepGap;
        unsigned long stepGapPrevious;
        
        bool isSync;
        
        bool muteState;
        SchmittTrigger muteTrigger;
        
        
        
        float knobPosition;
        unsigned int BPM;
        unsigned int previousBPM;
        
        bool activeCLK;
        bool activeCLKPrevious;
        SchmittTrigger CLKInputPort;
        
        
        //float list_fRatio[25];
        float list_fRatio[25] = {64.0f, 32.0f, 16.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f, 1.0f/9.0f, 0.1f, 0.0625f, 0.03125f, 0.015625f};
        unsigned int rateRatioKnob;
        //enum ClkModModeIds;
        enum ClkModModeIds {
            X1,    // work at x1.
            DIV,    // divider mode.
            MULT    // muliplier mode.
        }; 
        unsigned int clkModulMode;
        unsigned int pulseMultCounter;
        unsigned int pulseDivCounter;

        float pTime;
        PulseGenerator sendPulse;
        bool sendingOutput;
        bool canPulse;
        unsigned int pulseWidth;
        //enum PulseDurations;
        enum PulseDurations {
            FIXED1MS,
            FIXED5MS,       //fixed 5ms.
            FIXED100MS,    // Fixed 100 ms.
            GATE25,    // Gate 1/4 (25%).
            GATE33,    // Gate 1/3 (33%).
            SQUARE,    // Square waveform.
            GATE66,    // Gate 2/3 (66%).
            GATE75,    // Gate 3/4 (75%).
            GATE95,    // Gate 95%.
        };
        float pulseDuration;
        
        
        
        SchmittTrigger gateTrigger;
        bool outcome;
        
        
        float streamOutput;
        
        
        void muteSwitchCheck(float);
        
        
        float GetPulsingTime(unsigned long int, float);
        
        void clkStateChange( );
    
    
        void knobFunction( );
        
        void extBPMStep(float);
        
        void intBPMStep( );
        
        void pulseGenStep( );
        
        void coinToss(float);
        
        
    };
}
