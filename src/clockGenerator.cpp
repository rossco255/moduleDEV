//
//  clockGenerator.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "clockGenerator.hpp"

namespace pulseStreamGen {
    clockGenerator :: clockGenerator(float BPMKnob, float clkInput);
    
    clockGenerator :: currentStep = 0;
    clockGenerator :: previousStep = 0;
    clockGenerator :: nextPulseStep = 0;
    clockGenerator :: nextExpectedStep = 0;
    clockGenerator :: stepGap = 0;
    clockGenerator :: stepGapPrevious = 0;
    
    clockGenerator :: isSync = false;
    
    clockGenerator :: knobPosition = 12.0f;
    clockGenerator :: BPM = 120;
    clockGenerator :: previousBPM = 120;
    
    clockGenerator :: activeCLK = false;
    clockGenerator :: activeCLKPrevious = false;
    
    void clockGenerator :: clkStateChange (){
        if (activeCLK != activeCLKPrevious) {
            // Its state was changed (added or removed a patch cable to/away CLK port)?
            // New state will become "previous" state.
            activeCLKPrevious = activeCLK;
            // Reset all steps counter and "gaps", not synchronized.
            currentStep = 0;
            previousStep = 0;
            nextPulseStep= 0;
            nextExpectedStep = 0;
            stepGap = 0;
            stepGapPrevious = 0;
            isSync = false;
            pulseGeneration.canPulse = false;       //is this making it super latent?
        }
    }
    
    void clockGenerator::intOrExtClk (){
        if (activeCLK) {        //////////////this is wonky and should be streamlined
            // Ratio is controled by knob.
            clockModulator.rateRatioKnob = round(knobPosition);
            // Related multiplier/divider mode.
            clockModulator.clkModulMode = clockModulator.DIV;       ///needs fixed
            if (clockModulator.rateRatioKnob == 12)
                clockModulator.clkModulMode = clockModulator.X1;
            else if (clockModulator.rateRatioKnob > 12)
                clockModulator.clkModulMode = clockModulator.MULT;
        }
        else {
            // BPM is set by knob.
            BPM = (knobPosition * 20);
            if (BPM < 30)
                BPM = 30; // Minimum BPM is 30.
        }
    }
    
    void clockGenerator:: overflowBlock(){
        // Folling block is designed to avoid possible variable "overflows" on 32-bit unsigned integer variables, simply by "shifting" all of them!
        // Long unsigned integers overfows at max. 32-bit value (4,294,967,296) may occur after... 6h 12min 49sec @ 192000 Hz, or 27h 3min 11sec @ 44100 Hz lol.
        if ((currentStep > 4000000000) && (previousStep > 4000000000)) {
            if (nextExpectedStep > currentStep) {
                nextExpectedStep -= 3999500000;
                if (nextPulseStep > currentStep)
                    nextPulseStep -= 3999500000;
            }
            currentStep -= 3999500000;
            previousStep -= 3999500000;
        }
    }
    
    void clockGenerator:: findExtClkSpeed(){
        
        if (previousStep == 0) {
            // No "history", it's the first pulse received on CLK input after a frequency change. Not synchronized.
            nextExpectedStep = 0;
            stepGap = 0;
            stepGapPrevious = 0;
            // stepGap at 0: the pulse duration will be 1 ms (default), or 2 ms or 5 ms (depending SETUP). Variable pulses can't be used as long as frequency remains unknown.
            
            pulseGeneration.pulseDuration = pulseGeneration.GetPulsingTime(0, clockModulator.list_fRatio[clockModulator.rateRatioKnob]);  // Ratio is controlled by knob.
            // Not synchronized.
            isSync = false;
            clockModulator.pulseDivCounter = 0; // Used for DIV mode exclusively!
            clockModulator.pulseMultCounter = 0; // Used for MULT mode exclusively!
            pulseGeneration.canPulse = (clockModulator.clkModulMode != clockModulator.MULT); // MULT needs second pulse to establish source frequency.
            previousStep = currentStep;
        }
        else {
            // It's the second pulse received on CLK input after a frequency change.
            stepGapPrevious = stepGap;
            stepGap = currentStep - previousStep;
            nextExpectedStep = currentStep + stepGap;
            // The frequency is known, we can determine the pulse duration (defined by SETUP).
            // The pulse duration also depends of clocking ratio, such "X1", multiplied or divided, and its ratio.
            
            pulseGeneration.pulseDuration = pulseGeneration.GetPulsingTime(stepGap, clockModulator.list_fRatio[clockModulator.rateRatioKnob]); // Ratio is controlled by knob.
            isSync = true;
            if (stepGap > stepGapPrevious)
                isSync = ((stepGap - stepGapPrevious) < 2);
            else if (stepGap < stepGapPrevious)
                isSync = ((stepGapPrevious - stepGap) < 2);
            if (isSync)
                pulseGeneration.canPulse = (clockModulator.clkModulMode != clockModulator.DIV);
            else pulseGeneration.canPulse = (clockModulator.clkModulMode == clockModulator.X1);
            previousStep = currentStep;
        }
        
    }
}
