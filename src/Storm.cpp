//
//  Storm.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Storm.hpp"


float phase = 0.0f;
float pw = 0.5f;
float freq = 1.0f;
void setFreq(float freq_to_set)
{
    freq = freq_to_set;
};
void LFOstep(float dt){
    float deltaPhase = fminf(freq * dt, 0.5f);
    phase += deltaPhase;
    if (phase >= 1.0f)
        phase -= 1.0f;
};
float sqr() {
    float sqr = phase < pw ? 1.0f : -1.0f;
    return sqr;
};
int tempo = 0;
float frequency = 2.0f;
float clockOutputValue = 0;
void internalClkStep(float tempoParam)
{
    tempo = roundf(tempoParam);
    frequency = tempo/60.0f;
    setFreq(frequency*4);
    LFOstep(1.0 / engineGetSampleRate());
    clockOutputValue = clamp(5.0f * sqr(), 0.0f, 5.0f);
};

struct Storm : Module {
    
#define NUM_GENS 16        //how many pusle stream generators are used in a module
#define NUM_OUTS 8         //how many outputs do we have

    enum ParamIds {
        MODE_PARAM,
        MASTERCLK_PARAM,
        CLKSWITCH_PARAM,
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
        MASTERCLK_LIGHT,
        ENUMS(GEN_LIGHT, NUM_GENS),
        ENUMS(PROB_LIGHT, NUM_GENS),
        ENUMS(MUTE_LIGHT, NUM_GENS),
        ENUMS(FINAL_LIGHT, NUM_OUTS),
        NUM_LIGHTS
    };
    int row_size = sqrt(NUM_GENS);

    SchmittTrigger modeTrigger;
    int masterKnobMode = 0;
    
    SchmittTrigger clkSwitchTrigger;
    bool clkSwitchParam = true;
    
    bool extClockConnected = false;
    bool clockSynced = true;
    
    pulseStreamGen::streamGen *pulseStream[NUM_GENS];
    
    Storm() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
        for (int i = 0; i < NUM_GENS; i++)
        {
            pulseStream[i] = new pulseStreamGen::streamGen(params[DIV_PARAM + i].value, params[PW_PARAM + i].value, clockSynced);
        }
    }
    void step() override;
};


void Storm::step(){
    
    internalClkStep(params[MASTERCLK_PARAM].value);

    if (clkSwitchTrigger.process(params[CLKSWITCH_PARAM].value)){
        clkSwitchParam ^= true;
    }
    
    if (modeTrigger.process(params[MODE_PARAM].value)){
        masterKnobMode = (masterKnobMode + 1) % 4;
    }
    
    if (inputs[CLK_INPUT].active) {
        extClockConnected = true;
        clockSynced = true;
    }
    
    else if (clkSwitchParam){
        extClockConnected = false;
        clockSynced = true;
    }
    
    else{
        extClockConnected = false;
        clockSynced = false;
    }
    
    
    for(int i = 0; i < NUM_GENS; i++){
       
        if (extClockConnected){
            pulseStream[i]->pulseWidth = params[PW_PARAM + i].value;
            pulseStream[i]->knobPosition = params[DIV_PARAM + i].value;
            pulseStream[i]->activeCLK = inputs[CLK_INPUT].active;
            pulseStream[i]->clkStateChange();
            pulseStream[i]->knobFunction();
            pulseStream[i]->extBPMStep(inputs[CLK_INPUT].value);
        }
        
        else if(clkSwitchParam){
            pulseStream[i]->pulseWidth = params[PW_PARAM + i].value;
            pulseStream[i]->knobPosition = params[DIV_PARAM + i].value;
            pulseStream[i]->activeCLK = clkSwitchParam;
            pulseStream[i]->clkStateChange();
            pulseStream[i]->knobFunction();
            pulseStream[i]->extBPMStep(clockOutputValue);
        }
        else{
            pulseStream[i]->pulseWidth = params[PW_PARAM + i].value;
            pulseStream[i]->knobPosition = params[DIV_PARAM + i].value;
            pulseStream[i]->activeCLK = clkSwitchParam;
            pulseStream[i]->clkStateChange();
            pulseStream[i]->knobFunction();
            pulseStream[i]->intBPMStep();
        }
        
        pulseStream[i]->pulseGenStep();
        pulseStream[i]->coinToss(params[PROB_PARAM + i].value);
        pulseStream[i]->muteSwitchCheck(params[MUTE_PARAM + i].value);
        pulseStream[i]->streamOutput =( (pulseStream[i]->sendingOutput) && (pulseStream[i]->muteState) )? 5.0f : 0.0f;
        lights[GEN_LIGHT + i].setBrightnessSmooth(pulseStream[i]->streamOutput / 5.0f);
        lights[MUTE_LIGHT + i].value = (pulseStream[i]->muteState);
        lights[PROB_LIGHT + i].value = (params[PROB_PARAM + i].value);
    }
    
    float gateSUM[NUM_OUTS] = {};
    int outputCount = 0;
    
    
    for (int i = 0; i < NUM_GENS; i += row_size )
    {
        for (int j = i; j < (i + row_size); j++)
        {
            gateSUM[outputCount] += pulseStream[j]->streamOutput;
        }
        outputCount++;
    }
    
    for (int i = 0; i < row_size; i++){
        for (int j = i; j < (NUM_GENS - row_size + (i + 1)) ; j += row_size){
            gateSUM[outputCount] += pulseStream[j]->streamOutput;
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
        addParam(ParamWidget::create<TL1105>(Vec(100, 20), module, Storm::CLKSWITCH_PARAM, 0.0, 1.0, 0.0));
        addParam(ParamWidget::create<Rogan1PSRed>(Vec(170, 20), module, Storm::MASTERCLK_PARAM, 40.0f, 250.0f, 120.0f));
        addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(168, 18), module, Storm::MASTERCLK_LIGHT));

        
        static const float knobX[16] = {20, 80, 160, 230, 20, 90, 160, 230, 20, 90, 160, 230, 20, 90, 160, 230};
        static const float knobY[16] = {70, 70, 70, 70, 140, 140, 140, 140, 210, 210, 210, 210, 280, 280, 280, 280};
        
        for (int i = 0; i < NUM_GENS; i++){
            divParam[i] = ParamWidget::create<Rogan1PSRed>(Vec(knobX[i], knobY[i]), module, Storm::DIV_PARAM + i, 0.0, 24.0, 12.0);
            addParam(divParam[i]);
            probParam[i] = ParamWidget::create<Rogan1PSWhite>(Vec(knobX[i], knobY[i]), module, Storm::PROB_PARAM + i, 0.0, 1.0, 0.0);
            addParam(probParam[i]);
            pwParam[i] = ParamWidget::create<Rogan1PSGreenSnap>(Vec(knobX[i], knobY[i]), module, Storm::PW_PARAM + i, 0.0, 8.0, 4.0);
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


