#include "plugin.hpp"


struct Trip : Module {
	enum ParamId {
		OCTAVE_PARAM,
		MODULO_PARAM,
		SKIP_PARAM,
		REVERSAL_PARAM,
		LENGTH_PARAM,
		MODE_PARAM,
		VOLTS1_PARAM,
		VOLTS2_PARAM,
		VOLTS3_PARAM,
		VOLTS4_PARAM,
		VOLTS5_PARAM,
		VOLTS6_PARAM,
		VOLTS7_PARAM,
		VOLTS8_PARAM,
		SPACE1_PARAM,
		SPACE2_PARAM,
		SPACE3_PARAM,
		SPACE4_PARAM,
		SPACE5_PARAM,
		SPACE6_PARAM,
		SPACE7_PARAM,
		SPACE8_PARAM,
		GATE1_PARAM,
		GATE2_PARAM,
		GATE3_PARAM,
		GATE4_PARAM,
		GATE5_PARAM,
		GATE6_PARAM,
		GATE7_PARAM,
		GATE8_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ALLCVOUT_OUTPUT,
		GATEOUT_OUTPUT,
		TRIGGER_OUTPUT,
		CV1_OUTPUT,
		CV2_OUTPUT,
		CV3_OUTPUT,
		CV4_OUTPUT,
		CV5_OUTPUT,
		CV6_OUTPUT,
		CV7_OUTPUT,
		CV8_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Trip() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(OCTAVE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MODULO_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SKIP_PARAM, 0.f, 1.f, 0.f, "");
		configParam(REVERSAL_PARAM, 0.f, 1.f, 0.f, "");
		configParam(LENGTH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MODE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS3_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS4_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS5_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS6_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS7_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VOLTS8_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE3_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE4_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE5_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE6_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE7_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPACE8_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE3_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE4_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE5_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE6_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE7_PARAM, 0.f, 1.f, 0.f, "");
		configParam(GATE8_PARAM, 0.f, 1.f, 0.f, "");
		configInput(RESET_INPUT, "");
		configOutput(ALLCVOUT_OUTPUT, "");
		configOutput(GATEOUT_OUTPUT, "");
		configOutput(TRIGGER_OUTPUT, "");
		configOutput(CV1_OUTPUT, "");
		configOutput(CV2_OUTPUT, "");
		configOutput(CV3_OUTPUT, "");
		configOutput(CV4_OUTPUT, "");
		configOutput(CV5_OUTPUT, "");
		configOutput(CV6_OUTPUT, "");
		configOutput(CV7_OUTPUT, "");
		configOutput(CV8_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct TripWidget : ModuleWidget {
	TripWidget(Trip* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Trip.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(12.18, 22.307)), module, Trip::OCTAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.406, 22.307)), module, Trip::MODULO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(60.632, 22.307)), module, Trip::SKIP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(84.859, 22.307)), module, Trip::REVERSAL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(109.085, 22.307)), module, Trip::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(21.336, 43.524)), module, Trip::MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 59.928)), module, Trip::VOLTS1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 59.928)), module, Trip::VOLTS2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 59.928)), module, Trip::VOLTS3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 59.928)), module, Trip::VOLTS4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 59.928)), module, Trip::VOLTS5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 59.928)), module, Trip::VOLTS6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 59.928)), module, Trip::VOLTS7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 59.928)), module, Trip::VOLTS8_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 77.344)), module, Trip::SPACE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 77.344)), module, Trip::SPACE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 77.344)), module, Trip::SPACE3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 77.344)), module, Trip::SPACE4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 77.344)), module, Trip::SPACE5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 77.344)), module, Trip::SPACE6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 77.344)), module, Trip::SPACE7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 77.344)), module, Trip::SPACE8_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 94.76)), module, Trip::GATE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 94.76)), module, Trip::GATE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 94.76)), module, Trip::GATE3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 94.76)), module, Trip::GATE4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 94.76)), module, Trip::GATE5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 94.76)), module, Trip::GATE6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 94.76)), module, Trip::GATE7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 94.76)), module, Trip::GATE8_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.333, 41.473)), module, Trip::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(76.005, 41.473)), module, Trip::ALLCVOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(93.997, 41.473)), module, Trip::GATEOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(111.989, 41.473)), module, Trip::TRIGGER_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.712, 112.107)), module, Trip::CV1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.079, 112.107)), module, Trip::CV2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(50.447, 112.107)), module, Trip::CV3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.814, 112.107)), module, Trip::CV4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.181, 112.107)), module, Trip::CV5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(87.549, 112.107)), module, Trip::CV6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(99.916, 112.107)), module, Trip::CV7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(112.283, 112.107)), module, Trip::CV8_OUTPUT));
	}
};


Model* modelTrip = createModel<Trip, TripWidget>("Trip");