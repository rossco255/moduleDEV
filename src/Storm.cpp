//
//  Storm.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 17/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Storm.hpp"

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
