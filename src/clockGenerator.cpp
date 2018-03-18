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
    
    void clockGenerator::isExtCLK(bool INPUT_CLOCK_active){
        activeCLK = INPUT_CLOCK;
    }
    
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
    
    void clockGenerator:: extBPMStep(float INPUT_CLOCK){
        float scaledInput = rescale(INPUT_CLOCK, 0.2f, 1.7f, 0.0f, 1.0f)
        
        currentStep++;
        overflowBlock();
        
        // Using Schmitt trigger (SchmittTrigger is provided by dsp/digital.hpp) to detect thresholds from CLK input connector. Calibration: +1.7V (rising edge), low +0.2V (falling edge).
        
        if (CLKInputPort.process(rescale(INPUT_CLOCK, 0.2f, 1.7f, 0.0f, 1.0f))) {
            // CLK input is receiving a compliant trigger voltage (rising edge): lit and "afterglow" CLK (red) LED.
            findExtClkSpeed();
            clockModulator.clkModType();
        }
        else {
            // At this point, it's not a rising edge!
            
            // When running as multiplier, may pulse here too during low voltages on CLK input!
            if (isSync && (nextPulseStep == currentStep) && (clockModulator.clkModulMode == clockModulator.MULT)) {
                nextPulseStep = currentStep + round(stepGap * clockModulator.list_fRatio[clockModulator.rateRatioKnob]); // Ratio is controlled by knob.
                // This block is to avoid continuous pulsing if no more receiving incoming signal.
                if (clockModulator.pulseMultCounter > 0) {
                    clockModulator.pulseMultCounter--;
                    pulseGeneration.canPulse = true;
                }
                else {
                    pulseGeneration.canPulse = false;
                    isSync = false;
                }
            }
        }
    }
    
    void clockGenerator:: intBPMStep(){
        // BPM is set by knob.
        BPM = (knobPosition * 20);
        if (BPM < 30)
            BPM = 30; // Minimum BPM is 30.
        
        if (previousBPM == BPM) {
            // Incrementing step counter...
            currentStep++;
            overflowBlock();
            if (currentStep >= nextPulseStep) {
                // Current step is greater than... next step: senting immediate pulse (if unchanged BPM by knob).
                nextPulseStep = currentStep;
                pulseGeneration.canPulse = true;
            }
            
            if (pulseGeneration.canPulse) {
                // Setting pulse...
                // Define the step for next pulse. Time reference is given by (current) engine samplerate setting.
                nextPulseStep = round(60.0f * engineGetSampleRate() / BPM);
                // Define the pulse duration (fixed or variable-length).
                pulseGeneration.pulseDuration = pulseGeneration.GetPulsingTime(engineGetSampleRate(), 60.0f / BPM);
                currentStep = 0;
            }
            else{
                currentStep = 0;
                nextPulseStep = 0;
            }
        }
        previousBPM = BPM;
    }
}
