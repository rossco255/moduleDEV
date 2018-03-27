//
//  Drizzle.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Drizzle.hpp"


struct Drizzle : Module {
#define NUM_GENS 4        //how many pusle stream generators are used in a module
#define NUM_OUTS 1         //how many outputs do we have

    enum ParamIds {
        MODE_PARAM,
        MASTERCLK_PARAM,
        CLKSWITCH_PARAM,
        ENUMS(DIV_PARAM, NUM_GENS),
        ENUMS(PROB_PARAM, NUM_GENS),
        ENUMS(PW_PARAM, NUM_GENS),
        ENUMS(MUTE_PARAM, NUM_GENS),
        ENUMS(DELAY_PARAM, NUM_GENS),
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
        ENUMS(DELAY_LIGHT, NUM_GENS),
        ENUMS(FINAL_LIGHT, NUM_OUTS),
        NUM_LIGHTS
    };
    
    SchmittTrigger modeTrigger;
    int masterKnobMode = 0;
    
    SchmittTrigger clkSwitchTrigger;
    bool clkSwitchParam = true;
    bool extClockConnected = false;
    bool clockSynced = true;

    pulseStreamGen::streamGen *pulseStream[NUM_GENS];
    internalClock::BPMClock internalClk;

    Drizzle() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
        for (int i = 0; i < NUM_GENS; i++)
        {
            pulseStream[i] = new pulseStreamGen::streamGen(params[DIV_PARAM + i].value, params[PW_PARAM + i].value, clockSynced, params[DELAY_PARAM + i].value);
        }
    }
    int row_size = sqrt(NUM_GENS);
    void step() override;
};

void Drizzle::step(){
    internalClk.internalClkStep(params[MASTERCLK_PARAM].value);
    
    if (clkSwitchTrigger.process(params[CLKSWITCH_PARAM].value)){
        clkSwitchParam ^= true;
    }
    if (modeTrigger.process(params[MODE_PARAM].value)){
        masterKnobMode = (masterKnobMode + 1) % 5;
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
            pulseStream[i]->extBPMStep(internalClk.clockOutputValue);
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
       // pulseStream[i]->pulseDelay(params[DELAY_PARAM + i].value);
        pulseStream[i]->streamOutput =( (pulseStream[i]->sendingOutput) && (pulseStream[i]->muteState) )? 5.0f : 0.0f;
        lights[GEN_LIGHT + i].setBrightnessSmooth(pulseStream[i]->streamOutput / 5.0f);
        lights[MUTE_LIGHT + i].value = (pulseStream[i]->muteState);
        lights[PROB_LIGHT + i].value = (params[PROB_PARAM + i].value);
    }
    
    float gateSUM = 0;
    for (int i = 0; i < NUM_GENS; i ++)
    {
        gateSUM += pulseStream[i]->streamOutput;
    }
    outputs[STREAM_OUTPUT].value = gateSUM ? 7.0f : 0.0f;
    lights[FINAL_LIGHT].setBrightnessSmooth(gateSUM / 5.0f);
};

struct DrizzleWidget : ModuleWidget {
    ParamWidget *divParam[NUM_GENS];
    ParamWidget *probParam[NUM_GENS];
    ParamWidget *pwParam[NUM_GENS];
    ParamWidget *muteParam[NUM_GENS];
    ParamWidget *delayParam[NUM_GENS];
    DrizzleWidget(Drizzle *module) : ModuleWidget(module) {
        box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        {
            SVGPanel *panel = new SVGPanel();
            panel->box.size = box.size;
            panel->setBackground(SVG::load(assetPlugin(plugin, "res/Drizzle.svg")));
            addChild(panel);
        }
        
        addParam(ParamWidget::create<TL1105>(Vec(50, 20), module, Drizzle::MODE_PARAM, 0.0, 1.0, 0.0));
        addParam(ParamWidget::create<TL1105>(Vec(50, 40), module, Drizzle::CLKSWITCH_PARAM, 0.0, 1.0, 0.0));
        addParam(ParamWidget::create<Rogan1PSRed>(Vec(70, 20), module, Drizzle::MASTERCLK_PARAM, 40.0, 250.0f, 120.0f));
        addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(168, 18), module, Drizzle::MASTERCLK_LIGHT));
        
        
        static const float knobX[4] = {10, 10, 10, 10};
        static const float knobY[4] = {70, 140, 210, 280};
        
        for (int i = 0; i < NUM_GENS; i++){
            divParam[i] = ParamWidget::create<Rogan1PSRed>(Vec(knobX[i], knobY[i]), module, Drizzle::DIV_PARAM + i, 0.0, 24.0, 12.0);
            addParam(divParam[i]);
            probParam[i] = ParamWidget::create<Rogan1PSWhite>(Vec(knobX[i], knobY[i]), module, Drizzle::PROB_PARAM + i, 0.0, 1.0, 0.0);
            addParam(probParam[i]);
            pwParam[i] = ParamWidget::create<Rogan1PSGreenSnap>(Vec(knobX[i], knobY[i]), module, Drizzle::PW_PARAM + i, 0.0, 8.0, 4.0);
            addParam(pwParam[i]);
            muteParam[i] = ParamWidget::create<CKD6>(Vec(knobX[i] + 8, knobY[i] + 8), module, Drizzle::MUTE_PARAM + i, 0.0f, 1.0f, 0.0f);
            addParam(muteParam[i]);
            delayParam[i] = ParamWidget::create<Rogan1PSWhite>(Vec(knobX[i], knobY[i]), module, Drizzle::DELAY_PARAM + i, 0.0, 1.0, 0.0);
            addParam(delayParam[i]);
            
            addChild(ModuleLightWidget::create<SmallLight<RedLight>>(Vec(knobX[i] - 2, knobY[i] - 2), module, Drizzle::GEN_LIGHT + i));
            addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(knobX[i] + 40, knobY[i] - 2), module, Drizzle::MUTE_LIGHT + i));
            addChild(ModuleLightWidget::create<SmallLight<BlueLight>>(Vec(knobX[i] - 2, knobY[i] + 40), module, Drizzle::PROB_LIGHT + i));
        }
        
        
        addInput(Port::create<PJ301MPort>(Vec(12, 20), Port::INPUT, module, Drizzle::CLK_INPUT));
        
        addOutput(Port::create<PJ301MPort>(Vec(12, 330), Port::OUTPUT, module, Drizzle::STREAM_OUTPUT));
     
        
        addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(10, 328), module, Drizzle::FINAL_LIGHT));
  
        
    }
    
    void step() override {
        Drizzle *module = dynamic_cast<Drizzle*>(this->module);
        
        for (int i = 0; i < NUM_GENS; i++){
            divParam[i]->visible = (module->masterKnobMode == 0);
            probParam[i]->visible = (module->masterKnobMode == 1);
            pwParam[i]->visible = (module->masterKnobMode == 2);
            muteParam[i]->visible = (module->masterKnobMode == 3);
            delayParam[i]->visible = (module->masterKnobMode == 4);
        }
        ModuleWidget::step();
    }
};

Model *modelDrizzle = Model::create<Drizzle, DrizzleWidget>("moduleDEV", "Drizzle", "Drizzle", CLOCK_TAG, CLOCK_MODULATOR_TAG, SEQUENCER_TAG); // CLOCK_MODULATOR_TAG introduced in 0.6 API.
