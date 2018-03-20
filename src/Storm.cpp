//
//  Storm.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Storm.hpp"
#include "Template.hpp"
#include <dsp/digital.hpp>
#include <cstring>
#include <util/math.hpp>


struct streamGen{
    unsigned long currentStep = 0;
    unsigned long previousStep = 0;
    unsigned long nextPulseStep = 0;
    unsigned long nextExpectedStep = 0;
    unsigned long stepGap = 0;
    unsigned long stepGapPrevious = 0;
    
    bool isSync = false;
    
    bool muteState = true;
    SchmittTrigger muteTrigger;
    
  
    
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
    unsigned int pulseWidth = 0;
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
    float pulseDuration = 0.1f;
    
    //probGate -- weighted coin toss which determines if a gate is sent to output
    
    
     SchmittTrigger gateTrigger;
     bool outcome = false;
  
    
    
    
    float streamOutput = 0;
    
    streamGen(){
        
    }
    
    void muteSwitchCheck(float muteParam){
        if (muteTrigger.process(muteParam)){
            muteState ^= true;
        }
    }
    
    float GetPulsingTime(unsigned long int stepGap, float rate){
        float pTime = 0.1; // As default "degraded-mode/replied" pulse duration is set to 1ms (also can be forced to "fixed 1ms" via SETUP).
        if (stepGap == 0) {
            pTime = 0.1;
        }
        else {
            // Reference duration in number of samples (when known stepGap). Variable-length pulse duration can be defined.
            switch (pulseWidth) {
                case FIXED5MS:
                    pTime = 0.005f;    // Fixed 5 ms pulse.
                    break;
                case FIXED100MS:
                    pTime = 0.1f;    // Fixed 100 ms pulse.
                    break;
                case GATE25:
                    pTime = rate * 0.25f * (stepGap / engineGetSampleRate());    // Gate 1/4 (25%)
                    break;
                case GATE33:
                    pTime = rate * (1.0f / 3.0f) * (stepGap / engineGetSampleRate());    // Gate 1/3 (33%)
                    break;
                case SQUARE:
                    pTime = rate * 0.5f * (stepGap / engineGetSampleRate());    // Square wave (50%)
                    break;
                case GATE66:
                    pTime = rate * (2.0f / 3.0f) * (stepGap / engineGetSampleRate());    // Gate 2/3 (66%)
                    break;
                case GATE75:
                    pTime = rate * 0.75f * (stepGap / engineGetSampleRate());    // Gate 3/4 (75%)
                    break;
                case GATE95:
                    pTime = rate * 0.95f * (stepGap / engineGetSampleRate());    // Gate 95%
            }
        }
        return pTime;
    }    // This custom function returns pulse duration (ms), regardling number of samples (unsigned long int) and pulsation duration parameter (SETUP).
    
    
    void clkStateChange(){
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
    
    void knobFunction(){
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
    
    
    void extBPMStep(float CLK_INPUT){
        
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
    
    void intBPMStep(){
        
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
    
    void pulseGenStep(){
        // Using pulse generator to output to all ports.
        if (canPulse) {
            canPulse = false;
            // Sending pulse, using pulse generator.
            sendPulse.pulseTime = pulseDuration;
            sendPulse.trigger(pulseDuration);
        }
        sendingOutput = sendPulse.process(1.0 / engineGetSampleRate());
    }
    
    void coinToss(float PROB){
        if (gateTrigger.process(sendingOutput)) {
            // trigger
            float r = randomUniform();
            float threshold = clamp(PROB, 0.f, 1.f);
            bool toss = (r < threshold);
            outcome = toss;
        }
        sendingOutput = outcome ? sendingOutput : 0.0;
    }
    
    
};




#define NUM_GENS 16        //how many pusle stream generators are used in a module
#define NUM_OUTS 8         //how many outputs do we have

int row_size = sqrt(NUM_GENS);


struct Storm : Module {
    enum ParamIds {
        MODE_PARAM,
        ENUMS(DIV_PARAM, NUM_GENS),
        ENUMS(PROB_PARAM, NUM_GENS),
        ENUMS(PW_PARAM, NUM_GENS),
        ENUMS(MUTE_PARAM, NUM_GENS),
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
        ENUMS(GEN_LIGHT, NUM_GENS),
        ENUMS(PROB_LIGHT, NUM_GENS),
        ENUMS(MUTE_LIGHT, NUM_GENS),
        ENUMS(FINAL_LIGHT, NUM_OUTS),
        NUM_LIGHTS
    };
    
    SchmittTrigger modeTrigger;
    int masterKnobMode = 0;
    
    streamGen pulseStream[NUM_GENS];
    
    Storm() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
        
    }
    void step() override;
};

void Storm::step(){
    
    if (modeTrigger.process(params[MODE_PARAM].value)){
        masterKnobMode = (masterKnobMode + 1) % 4;
    }
    
    
    for(int i = 0; i < NUM_GENS; i++){
        pulseStream[i].pulseWidth = params[PW_PARAM + i].value;
        pulseStream[i].knobPosition = params[DIV_PARAM + i].value;
        pulseStream[i].activeCLK = inputs[CLK_INPUT].active;
        pulseStream[i].clkStateChange();
        pulseStream[i].knobFunction();
        if (pulseStream[i].activeCLK){
            pulseStream[i].extBPMStep(inputs[CLK_INPUT].value);
        }
        else{
            pulseStream[i].intBPMStep();
        }
        
        pulseStream[i].pulseGenStep();
        pulseStream[i].coinToss(params[PROB_PARAM + i].value);
        pulseStream[i].muteSwitchCheck(params[MUTE_PARAM + i].value);
        pulseStream[i].streamOutput =( (pulseStream[i].sendingOutput) && (pulseStream[i].muteState) )? 5.0f : 0.0f;
        lights[GEN_LIGHT + i].setBrightnessSmooth(pulseStream[i].streamOutput / 5.0f);
        lights[MUTE_LIGHT + i].value = (pulseStream[i].muteState);
        lights[PROB_LIGHT + i].value = (params[PROB_PARAM + i].value);
    }
    
    float gateSUM[NUM_OUTS] = {};
    int outputCount = 0;
    
    
    for (int i = 0; i < NUM_GENS; i += row_size )
    {
        for (int j = i; j < (i + row_size); j++)
        {
            gateSUM[outputCount] += pulseStream[j].streamOutput;
        }
        outputCount++;
    }
    
    for (int i = 0; i < row_size; i++){
        for (int j = i; j < (NUM_GENS - row_size + (i + 1)) ; j += row_size){
            gateSUM[outputCount] += pulseStream[j].streamOutput;
        }
        outputCount++;
    }
  
    for (int i = 0; i < NUM_OUTS; i++)
    {
        outputs[STREAM_OUTPUT + i].value = gateSUM[i] ? 7.0f : 0.0f;
        lights[FINAL_LIGHT + i].setBrightnessSmooth(gateSUM[i] / 5.0f);
    }
    
};

struct StormWidget : ModuleWidget {
    ParamWidget *divParam[NUM_GENS];
    ParamWidget *probParam[NUM_GENS];
    ParamWidget *pwParam[NUM_GENS];
    ParamWidget *muteParam[NUM_GENS];
    
    
    StormWidget(Storm *module) : ModuleWidget(module) {
    
        box.size = Vec(20 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        
        {
            SVGPanel *panel = new SVGPanel();
            panel->box.size = box.size;
            panel->setBackground(
                                 SVG::load(assetPlugin(plugin, "res/Storm.svg")));
            addChild(panel);
        }
        
        addParam(ParamWidget::create<TL1105>(Vec(70, 20), module, Storm::MODE_PARAM, 0.0, 1.0, 0.0));
        
        
        static const float knobX[16] = {20, 70, 120, 170, 20, 70, 120, 170, 20, 70, 120, 170, 20, 70, 120, 170};
        static const float knobY[16] = {70, 70, 70, 70, 140, 140, 140, 140, 210, 210, 210, 210, 280, 280, 280, 280};
        
        for (int i = 0; i < NUM_GENS; i++){
            divParam[i] = ParamWidget::create<Rogan1PSRed>(Vec(knobX[i], knobY[i]), module, Storm::DIV_PARAM + i, 0.0, 24.0, 12.0);
            addParam(divParam[i]);
            probParam[i] = ParamWidget::create<Rogan1PSWhite>(Vec(knobX[i], knobY[i]), module, Storm::PROB_PARAM + i, 0.0, 1.0, 0.0);
            addParam(probParam[i]);
            pwParam[i] = ParamWidget::create<Rogan1PSGreenSnap>(Vec(knobX[i], knobY[i]), module, Storm::PW_PARAM + i, 0.0, 8.0, 0.0);
            addParam(pwParam[i]);
            muteParam[i] = ParamWidget::create<CKD6>(Vec(knobX[i] + 8, knobY[i] + 8), module, Storm::MUTE_PARAM + i, 0.0f, 1.0f, 0.0f);
            addParam(muteParam[i]);
            addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(knobX[i] - 2, knobY[i] - 2), module, Storm::GEN_LIGHT + i));
            addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(knobX[i] + 40, knobY[i] - 2), module, Storm::MUTE_LIGHT + i));
            addChild(ModuleLightWidget::create<SmallLight<BlueLight>>(Vec(knobX[i] - 2, knobY[i] + 40), module, Storm::PROB_LIGHT + i));
        }
        

        addInput(Port::create<PJ301MPort>(Vec(24, 20), Port::INPUT, module, Storm::CLK_INPUT));
        
        addOutput(Port::create<PJ301MPort>(Vec(220, 70), Port::OUTPUT, module, Storm::STREAM_OUTPUT));
        addOutput(Port::create<PJ301MPort>(Vec(220, 140), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 1));
        addOutput(Port::create<PJ301MPort>(Vec(220, 210), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 2));
        addOutput(Port::create<PJ301MPort>(Vec(220, 280), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 3));

        addOutput(Port::create<PJ301MPort>(Vec(20, 330), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 4));
        addOutput(Port::create<PJ301MPort>(Vec(70, 330), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 5));
        addOutput(Port::create<PJ301MPort>(Vec(120, 330), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 6));
        addOutput(Port::create<PJ301MPort>(Vec(170, 330), Port::OUTPUT, module, Storm::STREAM_OUTPUT + 7));
        
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(218, 68), module, Storm::FINAL_LIGHT));
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(218, 138), module, Storm::FINAL_LIGHT + 1));
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(218, 208), module, Storm::FINAL_LIGHT + 2));
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(218, 278), module, Storm::FINAL_LIGHT + 3));

        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(18, 328), module, Storm::FINAL_LIGHT + 4));
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(68, 328), module, Storm::FINAL_LIGHT + 5));
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(118, 328), module, Storm::FINAL_LIGHT + 6));
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(168, 328), module, Storm::FINAL_LIGHT + 7));

    }
    
    void step() override {
        Storm *module = dynamic_cast<Storm*>(this->module);
        
        for (int i = 0; i < NUM_GENS; i++){
            divParam[i]->visible = (module->masterKnobMode == 0);
            probParam[i]->visible = (module->masterKnobMode == 1);
            pwParam[i]->visible = (module->masterKnobMode == 2);
            muteParam[i]->visible = (module->masterKnobMode == 3);
        }
        ModuleWidget::step();
    }
};

Model *modelStorm = Model::create<Storm, StormWidget>("moduleDEV", "Storm", "Storm", CLOCK_TAG, CLOCK_MODULATOR_TAG, SEQUENCER_TAG); // CLOCK_MODULATOR_TAG introduced in 0.6 API.


