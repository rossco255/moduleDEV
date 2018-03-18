//
//  Storm.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 17/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Storm.hpp"

StormModule::StormModule()
:Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    MasterClockGenerator;
    processor;
}

void StormModule::step(){
    // Current knob position.
    float knobPosition = params[BPM_PARAM].value;
    
    // Current state of CLK port.
    bool MasterClockGenerator->activeCLK = inputs[CLK_INPUT].active;
    
    
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




struct StormWidget : ModuleWidget {
    StormWidget(StormModule *Module);
};

StormWidget :: StormWidget(
StormModule *module)
: ModuleWidget(module) {
    box.size = Vec(44 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    
    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(
        //////////INSERT SVG HERE
                             );
        addChild(panel);
    }
    //////////ADD WIDGETS
    
}

Model *modelStorm =
    Model::create<StormModule, StormWidget>(
        "rackDEV", "Storm", "Storm",
        SEQUENCER_TAG, CLOCK_TAG);
