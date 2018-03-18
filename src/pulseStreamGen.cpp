//
//  pulseStreamGen.cpp
//
//
//  Created by Ross Cameron on 17/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//
#include "Template.hpp"
#include "pulseStreamGen.hpp"
#include <dsp/digital.hpp>
#include <cstring>

namespace pulseStreamGen {
    
    clockGen(float BPNKnob, float ClkInput);
    singlePulseStreamGen(float modKnob, float pwKnob, float probKnob, float BPM);
    
    currentStep = 0;
    previousStep = 0;
    nextPulseStep = 0;
    nextExpectedStep = 0;
    stepGap = 0;
    stepGapPrevious = 0;
    
    isSync = false;    // Assuming clock generator isn't synchronized (sync'd) with source clock on initialization.
    
    intBPMGen :: knobPosition = 12.0f;
    intBPMGen :: BPM = 120;
    intBPMGen :: previousBPM = 120;
    
    extBPMGen :: activeCLK = false;
    extBPMGen :: activeCLKPrevious = false;
  
    clkMod :: list_fRatio = {64.0f, 32.0f, 16.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f, 1.0f/9.0f, 0.1f, 0.0625f, 0.03125f, 0.015625f};
    clkMod :: rateRatioKnob = 12; // Assuming knob is, by default, centered (12.0f), also for ratio.
    clkMod :: clkModulMode = X1; //initialise without multiplication or division
    clkMod :: pulseMultCounter = 0;
    clkMod :: pulseDivCounter = 63;
    
    pulseGen :: sendingOutput = false;
    pulseGen :: canPulse = false;
    pulseGen :: pulseDuration = 0.001f;
    pulseGen :: GetPulsingTime(unsigned long int stepGap, float rate) {
            float pTime = 0.001; // As default "degraded-mode/replied" pulse duration is set to 1ms (also can be forced to "fixed 1ms" via SETUP).
            return pTime;
        }
    
    
    void clkStateChange (){
        if (externalBPM.activeCLK != externalBPM.activeCLKPrevious) {
            // Its state was changed (added or removed a patch cable to/away CLK port)?
            // New state will become "previous" state.
            externalBPM.activeCLKPrevious = externalBPM.activeCLK;
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
    
    void intOrExtClk (){
        if (externalBPM.activeCLK) {        //////////////this is wonky and should be streamlined
            // Ratio is controled by knob.
            clockModulator.rateRatioKnob = round(internalBPM.knobPosition);
            // Related multiplier/divider mode.
            clockModulator.clkModulMode = clockModulator.DIV;       ///needs fixed
            if (clockModulator.rateRatioKnob == 12)
                clockModulator.clkModulMode = clockModulator.X1;
            else if (clockModulator.rateRatioKnob > 12)
                clockModulator.clkModulMode = clockModulator.MULT;
        }
        else {
            // BPM is set by knob.
            internalBPM.BPM = (internalBPM.knobPosition * 20);
            if (internalBPM.BPM < 30)
                internalBPM.BPM = 30; // Minimum BPM is 30.
        }
    }
    
    extBPMGen :: overflowBlock(){
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
    extBPMGen :: findExtClkSpeed(){
        
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
    
    clkMod :: clkModType(){
        switch (clkModulMode) {
            case X1:
                // Ratio is x1, following source clock, the easiest scenario! (always sync'd).
                pulseGeneration.canPulse = true;
                break;
            case DIV:
                // Divider mode scenario.
                if (pulseDivCounter == 0) {
                    pulseDivCounter = int(list_fRatio[rateRatioKnob] - 1); // Ratio is controlled by knob.
                    pulseGeneration.canPulse = true;
                }
                else {
                    pulseDivCounter--;
                    pulseGeneration.canPulse = false;
                }
                break;
            case MULT:
                // Multiplier mode scenario: pulsing only when source frequency is established.
                if (isSync) {
                    // Next step for pulsing in multiplier mode.
                    
                    // Ratio is controlled by knob.
                    nextPulseStep = currentStep + round(stepGap * list_fRatio[rateRatioKnob]);
                    pulseMultCounter = round(1.0f / list_fRatio[rateRatioKnob]) - 1;
                    
                    pulseGeneration.canPulse = true;
                }
        }
    }
    
    extBPMGen :: extBPMStep(float inputClock){
        float scaledInput = rescale(inputClock, 0.2f, 1.7f, 0.0f, 1.0f)
        
        currentStep++;
        overflowBlock();
        
        // Using Schmitt trigger (SchmittTrigger is provided by dsp/digital.hpp) to detect thresholds from CLK input connector. Calibration: +1.7V (rising edge), low +0.2V (falling edge).
        
        if (CLKInputPort.process(scaledInput)) {
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
    
    intBPMGen :: intBPMStep(){
        if (previousBPM == BPM) {
            // Incrementing step counter...
            currentStep++;
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
        }
        previousBPM = BPM;
    }
    
    pulseGen :: pulseGenStep(){
        // Using pulse generator to output to all ports.
        if (canPulse) {
            canPulse = false;
            // Sending pulse, using pulse generator.
            sendPulse.pulseTime = pulseDuration;
            sendPulse.trigger(pulseDuration);
        }
        sendingOutput = sendPulse.process(1.0 / engineGetSampleRate());
    }
    
    probGate :: coinFlip(){
        if (gateTrigger.process(pulseGeneration.sendingOutput)) {
            // trigger
            r = randomUniform();
            threshold = clamp(probKnob, 0.f, 1.f);
            toss = (r < threshold);
            outcome = toss;
        }
    }
    
    
    /*
    
    using namespace pulseStreamGen
    
    void step() override{
        // Current knob position.
        knobPosition = params[intbpmPARAM_KNOB].value;
        
        // Current state of CLK port.
        activeCLK = inputs[INPUT_CLOCK].active;

        
        // KlokSpid is working as multiplier/divider module (when CLK input port is connected - aka "active").
        
        clkStateChange();
        intOrExtClk();
        if (activeCLK){
            extBPMStep();
        }
        else{
            intBPMGen :: intBPMStep();
        }

        pulseGen.pulseGenStep();
        sendingOutput = outcome ? 1.0 : 0.0;
        outputs[OUTPUT_1].value = sendingOutput ? 5.0f : 0.0f;
    }
     
     */
}
