#include "plugin.hpp"
#include <complex>
using namespace std;
using namespace math;

struct Strange : Module {
	enum ParamId {
		RATE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		CVOUT1_OUTPUT,
		CVOUT2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Strange() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(RATE_PARAM, 0.001f, 0.99f, 0.5f, "Rate of output change");
		configOutput(CVOUT1_OUTPUT, "");
		configOutput(CVOUT2_OUTPUT, "");
	}

	//variables for controls
	float rate = 0.f;
	int count = 0;
	const int SECOND = 44100;
	//for computing new output value from attractor
	float out1 = 0.f;
	float out2 = 0.f;
	//maybe these should be knobs for seeding values!!
	float a = 0.85f;
	float b = 0.81f;  //0.9 was NOT chaotic; 0.8 might be?
	float k = 0.4f;
	float p = 7.7f;
	complex<double> z = complex<double>{1, 0};  
	bool start = true;


	void process(const ProcessArgs& args) override {

		count++;
		rate = params[RATE_PARAM].getValue();

		/*
		allow rate to go from super slow to super fast (don't just use sampleRate since that limits speed) . Do some kind of log scaling of rate knob.
		Becomes an AWG function at audio rates.
		could have a rate control for each output

		add chaos function selector, enums, and a switch statement

		*/

		if (count < ((1.0f - rate) * args.sampleRate))
		{
			return;
		}

		if (start){
			//runs once (could enable a reset for it) to initialize process
			start = false;
			z = ikeda(complex<double>{0, 0});
		}
		else{
			z = ikeda(z);
		}

		//pull the real and imaginary parts out for sending to the CV outputs
		outputs[CVOUT1_OUTPUT].setVoltage(z.real(), 0);
		outputs[CVOUT1_OUTPUT].setChannels(1);
		outputs[CVOUT2_OUTPUT].setVoltage(z.imag(), 0);
		outputs[CVOUT2_OUTPUT].setChannels(1);

		//DEBUG("# of parameters %i", PARAMS_LEN);

		//reset count
		count = 0;
	}

	complex<double> ikeda(complex<double> lastZ) {
		//compute the next z based on the input value
		double z2 = pow(lastZ.real(), 2) + pow(lastZ.imag(), 2);
		//complex<double> output = complex<double>{a, 0}  
		//	+ complex<double>{b, 0}  *pow(lastZ, (complex<double>{0, 1} * ((k - p)/(1 + z2))));

		//I broke the formula into parts
		//complex<double> exp = complex<double>{0, 1} * ((k - p)/(1 + z2));
		//complex<double> power = pow(2.71828, exp);
		complex<double> output = complex<double>{a, 0} + complex<double>{b, 0} * lastZ * pow(2.71828, complex<double>{0, 1} * ((k - p)/(1 + z2)));
		return output;
	}


};

	

struct StrangeWidget : ModuleWidget {
	StrangeWidget(Strange* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Strange.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(19.844, 39.423)), module, Strange::RATE_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.472, 83.363)), module, Strange::CVOUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.472, 105.588)), module, Strange::CVOUT2_OUTPUT));
	}
};


Model* modelStrange = createModel<Strange, StrangeWidget>("Strange");