#include "plugin.hpp"


struct Pleats : Module {

	/* XXX_LEN as a placeholder for the enum length that the SDK fills in.

	Have as many ins, outs, params, lights as you want. Put in the enum and use by name, then call configXXX.
	*/

	enum ParamId {
		BIAS_PARAM,
		SET_PARAM,
		AMT_PARAM,
		PARAMS_LEN
	};


	enum InputId {
		AUDIO_INPUT,
		SETCV_INPUT,
		AMTCV_INPUT,
		BIASCV_INPUT,
		INPUTS_LEN
	};
	
	enum OutputId {
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};


	//variables for controls
	int AMOUNT_DEFAULT = 6;
	float bias = 0.f;
	float threshold = 0.f;
	float foldAmount = 0.f;
	//for computing new output value from folding
	float out = 0.f;
	float delta = 0.f;

	Pleats() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN);

		//CV
		configInput(SETCV_INPUT, "Setpoint CV");
		configInput(AMTCV_INPUT, "Amount CV");
		configInput(BIASCV_INPUT, "Bias CV");

		//IO
		configInput(AUDIO_INPUT, "Audio");
		configOutput(AUDIO_OUTPUT, "Folded audio");

		configParam(SET_PARAM, 0.f, 10.f, 0.f, "Setpoint", " V");
		configParam(AMT_PARAM, 0.f, 1.f, 0.5f, "Amount", "%", 0, 100);
		configParam(BIAS_PARAM, 0.f, 5.f, 0.f, "Bias", " V");
	}

	/*
	Called oonce per sample, so very quickly. Must store sample buffer somewhere to do something useful. Hmm.
	*/
	void process(const ProcessArgs& args) override {
		/*
		args is just metedata about sample rate, which sample this is. 
	
		Single voltage value comes in, per channel (input)
		concept is take in audio.
		process it
		route to the output
		have some parameters for real-time control over CV plus knobs for static setting if no CV used
		*/

		//read info about any param, in or out by accessing the array of them by enum name (that correlates to UI)

		out = inputs[AUDIO_INPUT].getVoltage(0);
		delta = 0.f;

		//I need knobs or CV inputs for bias, the threshold, foldAmount. they all work together. Acts like a filter.
		//read the knob or the CV input liek so: inputs[x].isConnected() ? inputs[x].getVoltage() : 0.0f

		bias = inputs[BIASCV_INPUT].isConnected() ? inputs[BIASCV_INPUT].getVoltage() : params[BIAS_PARAM].getValue();
		threshold = inputs[SETCV_INPUT].isConnected() ? inputs[SETCV_INPUT].getVoltage() : params[SET_PARAM].getValue();

		if (inputs[AMTCV_INPUT].isConnected())
		{
			//scale to a percentage
			foldAmount = abs((inputs[AMTCV_INPUT].getVoltage() / 10)) * AMOUNT_DEFAULT;
		}
		else
		{
			foldAmount = params[AMT_PARAM].getValue() * AMOUNT_DEFAULT;
		}

		//apply to positive half of cycle
		if (out + bias > threshold)
		{
			delta = (out - foldAmount);
			if (delta < out)
			{
				out -= delta;
			}
			else
			{
				out = 0.1;
			}	
		}

		//what about negative half

		//output <= 0 cause no sound output. D'oh.

		// Set output
		outputs[AUDIO_OUTPUT].setVoltage(out, 0);
		outputs[AUDIO_OUTPUT].setChannels(1);
	}
};


struct PleatsWidget : ModuleWidget {
	PleatsWidget(Pleats* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pleats.svg")));

		//add cute rack screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		/*
		Controls. The mm2px function takes a pair of millimeter values (a point coordinate in the SVG faceplate) and converts
		it to pixels for rendering on top of the SVG. 
		
		Use tool to read svg and generate module temple code from it.
		*/
		//level control
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.131, 27.347)), module, Pleats::SET_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.736, 27.347)), module, Pleats::AMT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(33.812, 27.347)), module, Pleats::BIAS_PARAM));
		//cv controls
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.131, 40.236)), module, Pleats::SETCV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.736, 40.236)), module, Pleats::AMTCV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(33.812, 40.236)), module, Pleats::BIASCV_INPUT));
		//input and output
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.472, 83.363)), module, Pleats::AUDIO_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.472, 105.588)), module, Pleats::AUDIO_OUTPUT));



	}

	//for saving parameters there are functions that handle JSON that can be overriden (load/save0)



	//override appendContextMenu to add new things below a menu separator

};

//the pointer gets a handle to the module and the moduleWidget for furth initialization by the SDK
Model* modelPleats = createModel<Pleats, PleatsWidget>("Pleats");