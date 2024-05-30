#include "plugin.hpp"


struct Pleats : Module {

	/* XXX_LEN as a placeholder for the enum length that the SDK fills in.

	Have as many ins, outs, params, lights as you want. Put in the enum and use by name, then call configXXX.
	*/

	enum ParamId {
		LEVEL_PARAM,
		PARAMS_LEN
	};


	enum InputId {
		AUDIO_INPUT,
		INPUTS_LEN
	};
	
	enum OutputId {
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};

	bool invert = false;
	bool average = false;

	Pleats() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN);

		configParam(LEVEL_PARAM, 0.f, 1.f, .8f, "Level", "%", 0, 100);
		configInput(AUDIO_INPUT, "audio");
		configOutput(AUDIO_OUTPUT, "folded audio");

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

	// Get number of channels and number of connected inputs
	int connected = 0;

	//read info about any param, in or out by accessing the array of them by enum name (that correlates to UI)

	if (inputs[AUDIO_INPUT].isConnected())
	{
		connected++;
	}

	float gain = params[LEVEL_PARAM].getValue();

	/*
	readincoming voltage and multiple by the gain. send to out going voltage
	*/

	float out = inputs[AUDIO_INPUT].getVoltage(0);
	float delta = 0.f;

	//I believe I need knobs or CV inputs for gain, the threshold, foldAmount. they all work together. Acts like a filter.

	float threshold = 3.5f;
	float foldAmount = 3.5f;

	if (out > threshold)
	{
		delta = (out - foldAmount) * gain;
		out -= delta;
	}

	// Set output
	outputs[AUDIO_OUTPUT].setVoltage(out, 0);
	outputs[AUDIO_OUTPUT].setChannels(1);
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

		/*
		Controls. The mm2px function takes a pair of millimeter values (a point coordinate in the SVG faceplate) and converts
		it to pixels for rendering on top of the SVG. 
		
		Inkscape shows cursor position in the lower right corner- set it to millimaters!
		*/

		//level control
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 39.02)), module, Pleats::LEVEL_PARAM));
		//input and output
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 70)), module, Pleats::AUDIO_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 101.5)), module, Pleats::AUDIO_OUTPUT));
	}

	//for saving parameters there are functions that handle JSON that can be overriden (load/save0)


	void appendContextMenu(Menu* menu) override {


		//can trigger functions in the Pleats module, if there are any
		//Pleats* module = getModule<Pleats>();

		menu->addChild(new MenuSeparator);

		menu->addChild(createMenuLabel("Editor settings"));

		menu->addChild(createMenuItem("Load sample", "kick.wav",
			[=]() {
				//access system clipboard
				glfwSetClipboardString(APP->window->win, "clicked load sample");
			}
		));


	}

};

//the pointer gets a handle to the module and the moduleWidget for furth initialization by the SDK
Model* modelPleats = createModel<Pleats, PleatsWidget>("Pleats");