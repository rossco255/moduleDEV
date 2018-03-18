//
//  Drizzle.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Drizzle.hpp"
#include "Template.hpp"
#include <dsp/digital.hpp>
#include <cstring>
#include <util/math.hpp>


#define NUM_GENS 4        //how many pusle stream generators are used in a module
#define NUM_OUTS 1         //how many outputs do we have

struct Drizzle : Module {
    enum ParamIds {
        ENUMS(DIV_PARAM, NUM_GENS),
        ENUMS(PW_PARAM, NUM_GENS),
        ENUMS(PROB_PARAM, NUM_GENS),
        NUM_PARAMS
    };
    enum InputIds {
        CLK_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(STREAM_OUTPUT, NUM_OUTS),
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(FINAL_LIGHT, NUM_OUTS),
        NUM_LIGHTS
    };
    
    Drizzle() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
        
    }
    
    
    
    
    unsigned long currentStep = 0;
    unsigned long previousStep = 0;
    unsigned long nextPulseStep = 0;
    unsigned long nextExpectedStep = 0;
    unsigned long stepGap = 0;
    unsigned long stepGapPrevious = 0;
    
    bool isSync = false;
    
    ///intBPMGen
    float knobPosition = 12.0f;
    unsigned int BPM = 120;
    unsigned int previousBPM = 120;
    
    //extBPMGen
    bool activeCLK = false;
    bool activeCLKPrevious = false;
    SchmittTrigger CLKInputPort; // Schmitt trigger to check thresholds on CLK input connector.
    
    
    //clkMod -- Clock modulation: division and multiplication of the master BPM
    float list_fRatio[25] = {64.0f, 32.0f, 16.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f, 1.0f/9.0f, 0.1f, 0.0625f, 0.03125f, 0.015625f}; // Real clock ratios list (array). Based on preset ratios while KlokSpid module runs as clock multiplier/divider, by knob position only.
    unsigned int rateRatioKnob = 12;
    enum ClkModModeIds {
        X1,    // work at x1.
        DIV,    // divider mode.
        MULT    // muliplier mode.
    };     // Clock modulator modes.
    unsigned int clkModulMode = X1;
    unsigned int pulseMultCounter = 0;      // Pulse counter for mult mode, avoids continuous pulse when not receiving (set at max divider value, minus 1). Kind of "timeout".
    unsigned int pulseDivCounter = 63;       // Pulse counter for divider mode (set at max divider value, minus 1).
    
    
    //pulseGen -- Generates a pulse based on the modulated clock
    PulseGenerator sendPulse;
    bool sendingOutput = false; // These flags are related to "sendPulse" pulse generator (current pulse state).
    bool canPulse = false;
    float pulseDuration = 0.001f;
    float GetPulsingTime(unsigned long int, float){
        float pTime = 0.001; // As default "degraded-mode/replied" pulse duration is set to 1ms (also can be forced to "fixed 1ms" via SETUP).
        return pTime;
    };      // This custom function returns pulse duration (ms), regardling number of samples (unsigned long int) and pulsation duration parameter (SETUP).
    
    //probGate -- weighted coin toss which determines if a gate is sent to output
    unsigned int probKnob = 1;
    SchmittTrigger gateTrigger;
    bool outcome = false;
    float r;
    float threshold;
    bool toss;
    
    /*
    
    void clkStateChange (){
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
    
    void intOrExtClk (){
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
            BPM = (knobPosition * 20);
            if (BPM < 30)
                BPM = 30; // Minimum BPM is 30.
        }
    }
    
    void overflowBlock(){
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
    void findExtClkSpeed(){
        
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
        
    }
    
    void clkModType(){
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
    
    
    void extBPMStep(){
        
        currentStep++;
        overflowBlock();
        
        // Using Schmitt trigger (SchmittTrigger is provided by dsp/digital.hpp) to detect thresholds from CLK input connector. Calibration: +1.7V (rising edge), low +0.2V (falling edge).
        
        if (CLKInputPort.process(rescale(CLK_INPUT, 0.2f, 1.7f, 0.0f, 1.0f))) {
            // CLK input is receiving a compliant trigger voltage (rising edge): lit and "afterglow" CLK (red) LED.
            findExtClkSpeed();
            clkModType();
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
    
    void intBPMStep(){
        
        if (previousBPM == BPM) {
            // Incrementing step counter...
            currentStep++;
            overflowBlock();
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
            else{
                currentStep = 0;
                nextPulseStep = 0;
            }
        }
        previousBPM = BPM;
    }
    
    void pulseGenStep(){
        // Using pulse generator to output to all ports.
        if (canPulse) {
            canPulse = false;
            // Sending pulse, using pulse generator.
            sendPulse.pulseTime = pulseDuration;
            sendPulse.trigger(pulseDuration);
        }
        sendingOutput = sendPulse.process(1.0 / engineGetSampleRate());
        outputs[STREAM_OUTPUT].value = sendingOutput ? 5.0f : 0.0f;
    }
    
    */
    
    void step() override{
        
        knobPosition = params[DIV_PARAM].value;

        
        activeCLK = inputs[CLK_INPUT].active;

        
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
            BPM = (knobPosition * 20);
            if (BPM < 30)
                BPM = 30; // Minimum BPM is 30.
        }
        
        
        
        if (activeCLK){

            if ((currentStep > 4000000000) && (previousStep > 4000000000)) {
                if (nextExpectedStep > currentStep) {
                    nextExpectedStep -= 3999500000;
                    if (nextPulseStep > currentStep)
                        nextPulseStep -= 3999500000;
                }
                currentStep -= 3999500000;
                previousStep -= 3999500000;
            
            
            
            currentStep++;
            
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
        else{
        
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
        if (canPulse) {
            canPulse = false;
            // Sending pulse, using pulse generator.
            sendPulse.pulseTime = pulseDuration;
            sendPulse.trigger(pulseDuration);
        }
        
        sendingOutput = sendPulse.process(1.0 / engineGetSampleRate());
        outputs[STREAM_OUTPUT].value = sendingOutput ? 5.0f : 0.0f;
    }
    }
};

struct DrizzleWidget : ModuleWidget {
    DrizzleWidget(Drizzle *module) : ModuleWidget(module) {
        
        box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        
        {
            SVGPanel *panel = new SVGPanel();
            panel->box.size = box.size;
            panel->setBackground(
                                 SVG::load(assetPlugin(plugin, "res/RotatingClockDivider2.svg")));
            addChild(panel);
        }
        // Ratio/BPM knob: 12.0 is the default (x1) centered knob position/setting, 25 possible settings for ratio or BPM.
        addParam(ParamWidget::create<RoundBlackKnob>(Vec(20, 106), module, Drizzle::DIV_PARAM, 0.0, 24.0, 12.0));
        // Input ports (golden jacks).
        addInput(Port::create<PJ301MPort>(Vec(24, 215), Port::INPUT, module, Drizzle::CLK_INPUT));
        // Output ports (golden jacks).
        addOutput(Port::create<PJ301MPort>(Vec(24, 262), Port::OUTPUT, module, Drizzle::STREAM_OUTPUT));
        
    }
};

Model *modelDrizzle = Model::create<Drizzle, DrizzleWidget>("moduleDEV", "Drizzle", "Drizzle", CLOCK_TAG, CLOCK_MODULATOR_TAG, SEQUENCER_TAG); // CLOCK_MODULATOR_TAG introduced in 0.6 API.
