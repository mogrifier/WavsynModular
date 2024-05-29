#include "plugin.hpp"


struct Pleats : Module {
	enum ParamId {
		PITCH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		PITCH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SINE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Pleats() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configInput(PITCH_INPUT, "");
		configOutput(SINE_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct PleatsWidget : ModuleWidget {
	PleatsWidget(Pleats* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pleats.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 38.655)), module, Pleats::PITCH_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 70.07)), module, Pleats::PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 101.304)), module, Pleats::SINE_OUTPUT));
	}
};


Model* modelPleats = createModel<Pleats, PleatsWidget>("Pleats");