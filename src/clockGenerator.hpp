//
//  clockGenerator.hpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "Template.hpp"
#include <dsp/digital.hpp>
#include <cstring>
#include <util/math.hpp>

namespace pulseStreamGen {
    class clockGenerator{
        clockGenerator(float, float);    //floats are the bpm knob and ext CLK input
        
        unsigned long currentStep;
        unsigned long previousStep;
        unsigned long nextPulseStep;
        unsigned long nextExpectedStep;
        unsigned long stepGap;
        unsigned long stepGapPrevious;
        
        bool isSync;
        void isExtCLK(float);
        
        void clkStateChange ();
        void intOrExtClk ();
        
        ///intBPMGen
        float knobPosition;
        unsigned int BPM;
        unsigned int previousBPM;
        void intBPMStep ();
        
        //extBPMGen
        bool activeCLK;
        bool activeCLKPrevious;
        SchmittTrigger CLKInputPort;        // Schmitt trigger to check thresholds on CLK input connector.
        void findExtClkSpeed ();
        void overflowBlock ();
        void extBPMStep (float);
        float scaledInput;
        
    };
}
