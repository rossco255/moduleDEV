#include "Template.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"


struct SequentialSwitch : Module {
	enum ParamIds {
		CHANNELS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUT, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHT, 8),
		NUM_LIGHTS
	};

	SchmittTrigger clockTrigger;
	SchmittTrigger resetTrigger;
	int channel = 0;
	SlewLimiter channelFilter[8];

	SequentialSwitch() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int i = 0; i < 8; i++) {
			channelFilter[i].rise = 0.01f;
			channelFilter[i].fall = 0.01f;
		}
	}

	void step() override {
		// Determine current channel
		if (clockTrigger.process(inputs[CLOCK_INPUT].value / 2.f)) {
			channel++;
		}
		if (resetTrigger.process(inputs[RESET_INPUT].value / 2.f)) {
			channel = 0;
		}
		int channels = 8 - (int) params[CHANNELS_PARAM].value;
		channel %= channels;

		// Filter channels
		for (int i = 0; i < 8; i++) {
			channelFilter[i].process(channel == i ? 1.0f : 0.0f);
		}

		// Set outputs

			float out = inputs[IN_INPUT + 0].value;
			for (int i = 0; i < 8; i++) {
				outputs[OUT_OUTPUT + i].value = channelFilter[i].out * out;
			}
		
		

		// Set lights
		for (int i = 0; i < 8; i++) {
			lights[CHANNEL_LIGHT + i].setBrightness(channelFilter[i].out);
		}
	}
};


struct SequentialSwitchWidget : ModuleWidget {
	SequentialSwitchWidget(SequentialSwitch *module);
};

SequentialSwitchWidget::SequentialSwitchWidget(SequentialSwitch *module) : ModuleWidget(module) {
	setPanel(SVG::load(assetPlugin(plugin, "res/SequentialSwitch.svg")));

	addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


	addInput(Port::create<PJ301MPort>(mm2px(Vec(6.133, 7.5)), Port::INPUT, module, SequentialSwitch::CLOCK_INPUT));
	addInput(Port::create<PJ301MPort>(mm2px(Vec(1.034, 19.5)), Port::INPUT, module, SequentialSwitch::RESET_INPUT));
	addInput(Port::create<PJ301MPort>(mm2px(Vec(6.133, 31.5)), Port::INPUT, module, SequentialSwitch::IN_INPUT + 0));

	addOutput(Port::create<PJ301MPort>(mm2px(Vec(1.034, 44.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 0));
	addOutput(Port::create<PJ301MPort>(mm2px(Vec(6.133, 54.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 1));
	addOutput(Port::create<PJ301MPort>(mm2px(Vec(1.034, 64.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 2));
	addOutput(Port::create<PJ301MPort>(mm2px(Vec(6.133, 74.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 3));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(1.034, 84.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 4));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(6.133, 94.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 5));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(1.034, 104.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 6));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(6.133, 114.5)), Port::OUTPUT, module, SequentialSwitch::OUT_OUTPUT + 7));

	addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(8.3, 44)), module, SequentialSwitch::CHANNEL_LIGHT + 0));
	addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(13.3, 54)), module, SequentialSwitch::CHANNEL_LIGHT + 1));
	addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(8.3, 64)), module, SequentialSwitch::CHANNEL_LIGHT + 2));
	addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(13.3, 74)), module, SequentialSwitch::CHANNEL_LIGHT + 3));
    addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(8.3, 84)), module, SequentialSwitch::CHANNEL_LIGHT + 4));
    addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(13.3, 94)), module, SequentialSwitch::CHANNEL_LIGHT + 5));
    addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(8.3, 104)), module, SequentialSwitch::CHANNEL_LIGHT + 6));
    addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(mm2px(Vec(13.3, 114)), module, SequentialSwitch::CHANNEL_LIGHT + 7));
}


Model *modelSequentialSwitch = Model::create<SequentialSwitch, SequentialSwitchWidget>("Template", "SequentialSwitch", "Sequential Switch", UTILITY_TAG);
