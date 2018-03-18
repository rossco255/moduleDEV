//
//  clockProcessor.cpp
//  rackdevstuff
//
//  Created by Ross Cameron on 18/03/2018.
//  Copyright © 2018 Ross Cameron. All rights reserved.
//

#include "clockProcessor.hpp"

namespace pulseStreamGen {
    clockProcessor :: clockProcessor(float BPMIn, float clkModKnob, float pwKnob, float probGateKnob);
    
    clockProcessor :: list_fRatio = {64.0f, 32.0f, 16.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f, 1.0f/9.0f, 0.1f, 0.0625f, 0.03125f, 0.015625f};
    clockProcessor :: rateRatioKnob = 12; // Assuming knob is, by default, centered (12.0f), also for ratio.
    clockProcessor :: clkModulMode = X1; //initialise without multiplication or division
    clockProcessor :: pulseMultCounter = 0;
    clockProcessor :: pulseDivCounter = 63;
    
    clockProcessor :: sendingOutput = false;
    clockProcessor :: canPulse = false;
    clockProcessor :: pulseDuration = 0.001f;
    
    
    float clockProcessor :: GetPulsingTime(unsigned long int stepGap, float rate) {
        float pTime = 0.001; // As default "degraded-mode/replied" pulse duration is set to 1ms (also can be forced to "fixed 1ms" via SETUP).
        return pTime;
    }
    
    void clockProcessor :: clkModType(){
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
                if (clockGenerator::isSync) {
                    // Next step for pulsing in multiplier mode.
                    
                    // Ratio is controlled by knob.
                    nextPulseStep = currentStep + round(clockGenerator::stepGap * list_fRatio[rateRatioKnob]);
                    pulseMultCounter = round(1.0f / list_fRatio[rateRatioKnob]) - 1;
                    
                    canPulse = true;
                }
        }
    }
    
    void clockProcessor:: pulseGenStep(){
        // Using pulse generator to output to all ports.
        if (canPulse) {
            canPulse = false;
            // Sending pulse, using pulse generator.
            sendPulse.pulseTime = pulseDuration;
            sendPulse.trigger(pulseDuration);
        }
        sendingOutput = sendPulse.process(1.0 / engineGetSampleRate());
    }
    
}
