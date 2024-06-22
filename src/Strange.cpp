#include "plugin.hpp"
#include <complex>
using namespace std;
using namespace math;

struct Strange : Module {

	//variables for controls
	float rate = 0.f;
	int count = 0;
	const int SECOND = 44100;
	//for computing new output value from attractor
	float out1 = 0.f;
	float out2 = 0.f;
	//maybe these should be knobs for seeding values!!
	float a = 0.85f;
	// replaced with SEED parameter float b = 0.81f;  //0.9 was NOT chaotic; 0.8 might be?
	float k = 0.4f;   //0.4f
	float p = 7.7f;
	complex<double> z = complex<double>{0, 0};  
	double z2 = 0.f;
	//henon
	float henonX = 1.f;
	float henonY = 1.f;


	enum ParamId {
		RATE_PARAM,
		BIAS_PARAM,
		GATE_PARAM,
		MODE_PARAM,
		SEED_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		GATEOUT_OUTPUT,
		CVOUT1_OUTPUT,
		CVOUT2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Strange() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(RATE_PARAM, -2.f, 2.f, 0.f, "Pulse Rate Factor");
		configOutput(CVOUT1_OUTPUT, "Attractor X");
		configOutput(CVOUT2_OUTPUT, "Attractor Y");
		configParam(SEED_PARAM, 0.f, 1.f, 0.5f, "Modifies the sequence");
		configParam(BIAS_PARAM, -2.f, 2.f, 0.f, "Adds +/- 2 volts to output");
		configParam(GATE_PARAM, 0.f, 1.f, 0.5f, "Gate duration", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Attractor:", {"Henon", "Ikeda"});
		configInput(CLOCK_INPUT, "External Clock");
		configOutput(GATEOUT_OUTPUT, "Gate");
	}



	void process(const ProcessArgs& args) override {

		//mode values
		const int HENON = 0;
		const int IKEDA = 1;

		count++;
		rate = params[RATE_PARAM].getValue();

		/*
		allow rate to go from super slow to super fast (don't just use sampleRate since that limits speed) . Do some kind of log scaling of rate knob.
		Becomes an AWG function at audio rates.
		add a knob for at least one chaos parameter.
		stable and unstable rhythm selector
		make Gate In work
		minimum rate is one pulse every 20 seconds and runs up to 500 BPM?

		*/

		double duration = params[GATE_PARAM].getValue();
		double ticks = (pow(10, -1*rate) * z2 * args.sampleRate/5);

		/* this logic also controls if the Gate signal is off or on. */
		if (count < (duration * ticks))
		{
			/* what is happening here is that the output "sticks" on the last value until a new value is created. */
			//keep gate on
			outputs[GATEOUT_OUTPUT].setVoltage(10.f, 0);
			outputs[GATEOUT_OUTPUT].setChannels(1);
			return;
		}
		else if ((count > (duration * ticks)) && (count < ticks))
		{
			
			//turn off gate output
			outputs[GATEOUT_OUTPUT].setVoltage(0.f, 0);
			outputs[GATEOUT_OUTPUT].setChannels(1);
			return;
		}
		else if (count >= ticks)
		{
			//read UI state to determine if using Henon or Ikeda strange attractor.
			switch ((int)params[MODE_PARAM].getValue()) {
				case HENON:
				//can get stuck so this will reset it automatically
					if (henonX == 0 && henonY == 0){
						henonX = 1.f;
						henonY = 1.f;
					}
					henon(henonX, henonY);
					break;
				case IKEDA:
					ikeda(z);
					break;
			}
			
			//DEBUG("# of parameters %i", PARAMS_LEN);
			//reset count
			count = 0;
		}
	}
	
	/* formulas from https://hypertextbook.com/chaos/strange */

	void ikeda(complex<double> lastZ) {
		//compute the next z based on the input value
		//z2 is the complex modulus squared
		z2 = pow(lastZ.real(), 2) + pow(lastZ.imag(), 2);
		//replace a constant with a SEED derived value that makes sense
		float adjuster = params[SEED_PARAM].getValue() * 0.3f + 0.7f;
		z = complex<double>{a, 0} + complex<double>{adjuster, 0} * lastZ * pow(2.71828, complex<double>{0, 1} * ((k - p)/(1 + z2)));
		//pull the real and imaginary parts out for sending to the CV outputs. check for NaN values.
		setOutput(z.real(), z.imag());
	}


	void henon(float lastX, float lastY){
		//set z2 to default
		z2 = 1;
		//replace a constant with a SEED derived value that makes sense
		float adjuster = params[SEED_PARAM].getValue() * 0.3f + 0.7f;
		henonX = (lastY + adjuster) - (1.4 * pow(lastX, 2));
		henonY = 0.3f * lastX;
		setOutput(henonX, henonY);
	}


	/* handles values from all the strange attractor functions, checks them for Nan, add in the bias value,
		and sends to the output channels*/
	void setOutput(float x, float y){
		//add  bias
		double bias = params[BIAS_PARAM].getValue();
		outputs[CVOUT1_OUTPUT].setVoltage(isfinite(x) ? x + bias : 0.f, 0);
		outputs[CVOUT1_OUTPUT].setChannels(1);
		outputs[CVOUT2_OUTPUT].setVoltage(isfinite(y) ? y + bias : 0.f, 0);
		outputs[CVOUT2_OUTPUT].setChannels(1);
	}

};

	

struct StrangeWidget : ModuleWidget {
	StrangeWidget(Strange* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Strange.svg")));

		/*
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(19.844, 39.423)), module, Strange::RATE_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.472, 83.363)), module, Strange::CVOUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.472, 105.588)), module, Strange::CVOUT2_OUTPUT));
		*/

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.296, 26.194)), module, Strange::RATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(31.096, 26.194)), module, Strange::BIAS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.296, 47.202)), module, Strange::GATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.772, 47.202)), module, Strange::MODE_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20.32, 68.685)), module, Strange::SEED_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.869, 93.902)), module, Strange::CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.17, 93.902)), module, Strange::GATEOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(9.869, 114.83)), module, Strange::CVOUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.169, 114.83)), module, Strange::CVOUT2_OUTPUT));
	}
};


Model* modelStrange = createModel<Strange, StrangeWidget>("Strange");