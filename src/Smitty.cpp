#include "plugin.hpp"

using namespace math;

struct Smitty : Module {
	enum ParamId {
		SHAPE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SHAPECV_INPUT,
		VOCT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO1_OUTPUT,
		AUDIO2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	float y1 = 1.f;
	float shaper = 1.f;
	float yq = 1.f;
	float myq = 1.f;
	float cv = 1.f;
	float freq = 261.3f;
	float oldFreq = 261.3f;
	float omegaT = 0.f;
	float epsilon = 0.f;

	Smitty(){
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SHAPE_PARAM, 0.f, 2.f, 0.f, "Add harmonics");
		configInput(SHAPECV_INPUT, "Odd FM tones with square or saw LFO");
		configInput(VOCT_INPUT, "V/OCT");
		configOutput(AUDIO1_OUTPUT, "audio 1");
		configOutput(AUDIO2_OUTPUT, "audio 2");
	}


	void process(const ProcessArgs& args) override {

		//Smith VCO approach
		cv = inputs[VOCT_INPUT].getVoltage();
			
		float pitch = 1;
		pitch = pitch + cv;
		//base frequency times a scaling term
		freq = dsp::FREQ_C4 * std::pow(2.f, cv);

		omegaT = (2.f * M_PI * freq) / args.sampleRate; 
		epsilon = 2.f * sin(omegaT / 2.f);

		//the values are going out of range- becoming inf and nan. That is cause of instability so if freq changes, reset initial conditions
		if (freq != oldFreq) {
			y1 = 1.f;
			shaper = 1.f;
			yq = 1.f;
			myq = 1.f;
		}

		yq = myq - epsilon * shaper;

		//modify shaper using a param or CV
		//if connected, use the CV input. sounds very cool. else use manual setting
		if (inputs[SHAPECV_INPUT].isConnected()){
			//read cv input
			shaper += abs(inputs[SHAPECV_INPUT].getVoltage() / 5);
		}
		else{
			//read param knob (range is 0 - 2 so need to divide or use abs)
			shaper += params[SHAPE_PARAM].getValue(); 
		}

		//LFO saw and square cause crazy tones when used as CV input to the shape control
		y1 = epsilon * yq + shaper;

		//save old values
		myq = yq;
		shaper = y1;

		outputs[AUDIO1_OUTPUT].setVoltage(5.f * sin(y1));
		//outputs are different and complimentary- best in stereo!
		outputs[AUDIO2_OUTPUT].setVoltage(5.f * sin(yq));

		oldFreq = freq;
	}
};





struct SmittyWidget : ModuleWidget {
	SmittyWidget(Smitty* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Smitty.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.32, 27.347)), module, Smitty::SHAPE_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 40.236)), module, Smitty::SHAPECV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 83.363)), module, Smitty::VOCT_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.92, 105.833)), module, Smitty::AUDIO1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.72, 105.833)), module, Smitty::AUDIO2_OUTPUT));
	}
};


Model* modelSmitty = createModel<Smitty, SmittyWidget>("Smitty");