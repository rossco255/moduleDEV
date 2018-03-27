//
//  pulseStreamGen.cpp
//
//
//  Created by Ross Cameron on 17/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "StreamGen.hpp"


namespace pulseStreamGen {
    
    streamGen::streamGen (float divValue, float pwValue, bool clkInputActive){
        currentStep = 0;
        previousStep = 0;
        nextPulseStep = 0;
        nextExpectedStep = 0;
        stepGap = 0;
        stepGapPrevious = 0;
        
        isSync = false;
        
        muteState = true;
 
        ///intBPMGen
        knobPosition = 12.0f;
        BPM = 120;
        previousBPM = 120;
        
        //extBPMGen
        activeCLK = false;
        activeCLKPrevious = false;
        
        //clkMod -- Clock modulation: division and multiplication of the master BPM
         // Real clock ratios list (array). Based on preset ratios while KlokSpid module runs as clock multiplier/divider, by knob position only.
        rateRatioKnob = 12;
        // Clock modulator modes.
        clkModulMode = X1;
        pulseMultCounter = 0;      // Pulse counter for mult mode, avoids continuous pulse when not receiving (set at max divider value, minus 1). Kind of "timeout".
        pulseDivCounter = 63;       // Pulse counter for divider mode (set at max divider value, minus 1).
        
        
        //pulseGen -- Generates a pulse based on the modulated clock
        sendingOutput = false; // These flags are related to "sendPulse" pulse generator (current pulse state).
        canPulse = false;
        pulseWidth = 0;
      
        pulseDuration = 0.1f;
     
        outcome = false;
        
        streamOutput = 0;
        
        ///input values from the knobs + ext clock
        pulseWidth = pwValue;
        knobPosition = divValue;
        activeCLK = clkInputActive;
    }

    void streamGen::muteSwitchCheck(float muteParam){
        if (muteTrigger.process(muteParam)){
            muteState ^= true;
        }
    }
    
    float streamGen::GetPulsingTime(unsigned long int stepGap, float rate){
        pTime = 0.1; // As default "degraded-mode/replied" pulse duration is set to 1ms (also can be forced to "fixed 1ms" via SETUP).
        if (stepGap == 0) {
            pTime = 0.1;
        }
        
        else {
            // Reference duration in number of samples (when known stepGap). Variable-length pulse duration can be defined.
            switch (pulseWidth) {
                case FIXED5MS:
                    pTime = 0.005f;    // Fixed 5 ms pulse.
                    break;
                case FIXED25MS:
                    pTime = 0.025f;    // Fixed 5 ms pulse.
                    break;
                case FIXED100MS:
                    pTime = 0.1f;    // Fixed 100 ms pulse.
                    break;
                case GATE25:
                    pTime = rate * 0.5f * (stepGap / engineGetSampleRate());    // Gate 1/4 (25%)
                    break;
                case GATE33:
                    pTime = rate * (2.0f / 3.0f) * (stepGap / engineGetSampleRate());    // Gate 1/3 (33%)
                    break;
                case SQUARE:
                    pTime = rate * 1.0f * (stepGap / engineGetSampleRate());    // Square wave (50%)
                    break;
                case GATE66:
                    pTime = rate * (4.0f / 3.0f) * (stepGap / engineGetSampleRate());    // Gate 2/3 (66%)
                    break;
                case GATE75:
                    pTime = rate * 1.5f * (stepGap / engineGetSampleRate());    // Gate 3/4 (75%)
                    break;
                case GATE95:
                    pTime = rate * 1.9f * (stepGap / engineGetSampleRate());    // Gate 95%
            }
        }
        return pTime;
    }    // This custom function returns pulse duration (ms), regardling number of samples (unsigned long int) and pulsation duration parameter (SETUP).
    
    
    void streamGen::clkStateChange(){
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
            canPulse = false;       //is this making it super latent?
        }
    }
    
    void streamGen::knobFunction(){
        if (activeCLK) {        //////////////this is wonky and should be streamlined
            // Ratio is controled by knob.
            rateRatioKnob = round(knobPosition);
            // Related multiplier/divider mode.
            clkModulMode = DIV;       ///needs fixed
            if (rateRatioKnob == 12)
                clkModulMode = X1;
            else if (rateRatioKnob > 12)
                clkModulMode = MULT;
        }
        else {
            // BPM is set by knob.
            BPM = (knobPosition * 10);
            if (BPM < 30)
                BPM = 30; // Minimum BPM is 30.
        }
    }
    
    
    void streamGen::extBPMStep(float CLK_INPUT){
        
        currentStep++;
        if ((currentStep > 4000000000) && (previousStep > 4000000000)) {
            if (nextExpectedStep > currentStep) {
                nextExpectedStep -= 3999500000;
                if (nextPulseStep > currentStep)
                    nextPulseStep -= 3999500000;
            }
            currentStep -= 3999500000;
            previousStep -= 3999500000;
        }
        
        // Using Schmitt trigger (SchmittTrigger is provided by dsp/digital.hpp) to detect thresholds from CLK input connector. Calibration: +1.7V (rising edge), low +0.2V (falling edge).
        
        if (CLKInputPort.process(rescale(CLK_INPUT, 0.2f, 1.7f, 0.0f, 1.0f))) {
            // CLK input is receiving a compliant trigger voltage (rising edge): lit and "afterglow" CLK (red) LED.
            
            
            if (previousStep == 0) {
                // No "history", it's the first pulse received on CLK input after a frequency change. Not synchronized.
                nextExpectedStep = 0;
                stepGap = 0;
                stepGapPrevious = 0;
                // stepGap at 0: the pulse duration will be 1 ms (default), or 2 ms or 5 ms (depending SETUP). Variable pulses can't be used as long as frequency remains unknown.
                
                pulseDuration = GetPulsingTime(0, list_fRatio[rateRatioKnob]);  // Ratio is controlled by knob.
                // Not synchronized.
                isSync = false;
                pulseDivCounter = 0; // Used for DIV mode exclusively!
                pulseMultCounter = 0; // Used for MULT mode exclusively!
                canPulse = (clkModulMode != MULT); // MULT needs second pulse to establish source frequency.
                previousStep = currentStep;
            }
            else {
                // It's the second pulse received on CLK input after a frequency change.
                stepGapPrevious = stepGap;
                stepGap = currentStep - previousStep;
                nextExpectedStep = currentStep + stepGap;
                // The frequency is known, we can determine the pulse duration (defined by SETUP).
                // The pulse duration also depends of clocking ratio, such "X1", multiplied or divided, and its ratio.
                
                pulseDuration = GetPulsingTime(stepGap, list_fRatio[rateRatioKnob]); // Ratio is controlled by knob.
                isSync = true;
                if (stepGap > stepGapPrevious)
                    isSync = ((stepGap - stepGapPrevious) < 2);
                else if (stepGap < stepGapPrevious)
                    isSync = ((stepGapPrevious - stepGap) < 2);
                if (isSync)
                    canPulse = (clkModulMode != DIV);
                else canPulse = (clkModulMode == X1);
                previousStep = currentStep;
            }
            
            switch (clkModulMode) {
                case X1:
                    // Ratio is x1, following source clock, the easiest scenario! (always sync'd).
                    canPulse = true;
                    break;
                case DIV:
                    // Divider mode scenario.
                    if (pulseDivCounter == 0) {
                        pulseDivCounter = int(list_fRatio[rateRatioKnob] - 1); // Ratio is controlled by knob.
                        canPulse = true;
                    }
                    else {
                        pulseDivCounter--;
                        canPulse = false;
                    }
                    break;
                case MULT:
                    // Multiplier mode scenario: pulsing only when source frequency is established.
                    if (isSync) {
                        // Next step for pulsing in multiplier mode.
                        
                        // Ratio is controlled by knob.
                        nextPulseStep = currentStep + round(stepGap * list_fRatio[rateRatioKnob]);
                        pulseMultCounter = round(1.0f / list_fRatio[rateRatioKnob]) - 1;
                        
                        canPulse = true;
                    }
            }
        }
        else {
            // At this point, it's not a rising edge!
            
            // When running as multiplier, may pulse here too during low voltages on CLK input!
            if (isSync && (nextPulseStep == currentStep) && (clkModulMode == MULT)) {
                nextPulseStep = currentStep + round(stepGap * list_fRatio[rateRatioKnob]); // Ratio is controlled by knob.
                // This block is to avoid continuous pulsing if no more receiving incoming signal.
                if (pulseMultCounter > 0) {
                    pulseMultCounter--;
                    canPulse = true;
                }
                else {
                    canPulse = false;
                    isSync = false;
                }
            }
        }
    }
    
    void streamGen::intBPMStep(){
        
        if (previousBPM == BPM) {
            // Incrementing step counter...
            currentStep++;
            if (currentStep >= nextPulseStep) {
                // Current step is greater than... next step: senting immediate pulse (if unchanged BPM by knob).
                nextPulseStep = currentStep;
                canPulse = true;
            }
            
            if (canPulse) {
                // Setting pulse...
                // Define the step for next pulse. Time reference is given by (current) engine samplerate setting.
                nextPulseStep = round(60.0f * engineGetSampleRate() / BPM);
                // Define the pulse duration (fixed or variable-length).
                pulseDuration = GetPulsingTime(engineGetSampleRate(), 60.0f / BPM);
                currentStep = 0;
            }
            
        }
        previousBPM = BPM;
    }
    
    void streamGen::pulseGenStep(){
        // Using pulse generator to output to all ports.
        if (canPulse) {
            canPulse = false;
            // Sending pulse, using pulse generator.
            sendPulse.pulseTime = pulseDuration;
            sendPulse.trigger(pulseDuration);
        }
        sendingOutput = sendPulse.process(2.0 / engineGetSampleRate());
    }
    
    void streamGen::coinToss(float PROB){
        if (gateTrigger.process(sendingOutput)) {
            // trigger
            float r = randomUniform();
            float threshold = clamp(PROB, 0.f, 1.f);
            bool toss = (r < threshold);
            outcome = toss;
        }
        sendingOutput = outcome ? sendingOutput : 0.0;
    }
    

}
