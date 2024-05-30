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
	float bias = 0.f;
	float threshold = 0.f;
	float foldAmount = 0.f;
	//for computing new output value from folding
	float out = 0.f;
	float delta = 0.f;


	Pleats() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN);

		//configParam(BIAS_PARAM, 0.f, 2.f, 1.f, "Bias", "%", 0, 50);
		//configParam(SET_PARAM, 0.f, 4.f, 3.f, "Setpoint", "%", 0, 25);
		//configParam(AMT_PARAM, 0.f, 4.f, 3.f, "Fold Amount", "%", 0, 25);

		//CV
		configInput(SETCV_INPUT, "Setpoint CV");
		configInput(AMTCV_INPUT, "Amount CV");
		configInput(BIASCV_INPUT, "Bias CV");

		//IO
		configInput(AUDIO_INPUT, "Audio");
		configOutput(AUDIO_OUTPUT, "Folded audio");


		configParam(SET_PARAM, 0.f, 1.f, 0.f, "");
		configParam(AMT_PARAM, 0.f, 1.f, 0.f, "");
		configParam(BIAS_PARAM, 0.f, 1.f, 0.f, "");




		//is PARAMS_LEN an integer? yes, and value is the number of parameters. 
		//example: DEBUG("# of parameters %i", PARAMS_LEN);   writex to logs.txt in the plugin deployment directory


		/*try some SIMD code from the RACK SDK. returning a vector of values or not? Yes, returns a vector.
		this does in-order pairwise exponentiation. So 7^2 result is put in result.s[0]
		must declar using simd::float_4;

		float_4 a = {7.f, 1.f, 2.f, 3.f};
		float_4 b = {2.f, 2.f, 4.f, 2.f};
		float_4 result = 0.f;
		result = pow(a, b);
		//s is the vector of scalar values.
		DEBUG("power function %f", result.s[0]);

		
		example of using library (3rd party)  #include "MathLibrary.h", .h and .cpp file go in
		src directory and get compiled into the plugin dll

		float x = 7.4;
    	float y = 99;
		float value = MathLibrary::Arithmetic::Add(x, y);
		DEBUG("from math library %f", value);

		*/
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


		/*
		readincoming voltage and multiple by the gain. send to out going voltage
		*/
		out = inputs[AUDIO_INPUT].getVoltage(0);
		delta = 0.f;

		//I believe I need knobs or CV inputs for bias, the threshold, foldAmount. they all work together. Acts like a filter.
		//read the knob or the CV input liek so: inputs[TUNE_CV + i].isConnected() ? inputs[TUNE_CV + i].getVoltage() : 0.0f

		bias = inputs[BIASCV_INPUT].isConnected() ? inputs[BIASCV_INPUT].getVoltage() : params[BIAS_PARAM].getValue();
		threshold = inputs[SETCV_INPUT].isConnected() ? inputs[SETCV_INPUT].getVoltage() : params[SET_PARAM].getValue();
		foldAmount = inputs[AMTCV_INPUT].isConnected() ? inputs[AMTCV_INPUT].getVoltage() : params[AMT_PARAM].getValue();

		//apply to positive half of cycle
		if (out + bias > threshold)
		{
			delta = (out - foldAmount);
			out -= delta;
		}

		//what about negative half

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