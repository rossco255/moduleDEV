//
//  Storm.hpp
//  rackdevstuff
//
//  Created by Ross Cameron on 17/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Template.hpp"
#include "clockProcessor.hpp"

#define NUM_GENS 16        //how many pusle stream generators are used in a module
#define NUM_OUTS 4         //how many outputs do we have

struct StormModule : Module {

    enum ParamIds {
        BPM_PARAM,
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
        BPMIN_LIGHT,
        ENUMS(FINAL_LIGHT, NUM_OUTS),
        NUM_LIGHTS
    };
    
    StormModule();
    void step() override;
    
    pulseStreamGen::clockGenerator *MasterClockGenerator;
    pulseStreamGen::clockProcessor *processor[NUM_GENS];
};
