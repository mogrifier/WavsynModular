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
	float b = 0.81f;  //0.9 was NOT chaotic; 0.8 might be?
	float k = 0.4f;   //0.4f
	float p = 7.7f;
	complex<double> z = complex<double>{0, 0};  
	double z2 = 0.f;
	//henon
	float henonX = 1.f;
	float henonY = 1.f;


	enum ParamId {
		RATE_PARAM,
		TUNE_PARAM,
		GATE_PARAM,
		MODE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		GATEIN_INPUT,
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
		configParam(TUNE_PARAM, -2.f, 2.f, 0.f, "Tuning +/- 2 octaves");
		configParam(GATE_PARAM, 0.f, 1.f, 0.5f, "Gate duration", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Select attractor", {"Henon", "Ikeda"});
		configInput(GATEIN_INPUT, "Gate");
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
		could have a rate control for each output

		add chaos function selector, enums, and a switch statement
		add a knob for at least one chaos parameter.

		external clock would be good. How's that work?

		I think I want my own gate output. Just send out the 10v signal of correct duration each cycle. How?

		minimum rate is one pulse every 20 seconds and runs up to 500 BPM?

		use modulus (which is analogous to euclidean distance ) to derive a value for use in creating a rhythm to the pulses. May need only real part.
		Or use modulus suqare (z2) which I already calculate.

		maybe add a voltage bias to raise the octave as desired? 1 volt per octave.

		I think I need gate in and gate out. Gate in for an external clock signal. While high, the CV out signal keeps generating.
		Gate out is when using internal rhythm. This is like when a sequencer is internally clocked. 

		May want a duration knob (0-100%) for controlling how long a given signal is held. ercent of the calculated total time (which I should cache when able)

		need this: // Switch with 3 modes (values 0 to 2), defaulting to "Hi-fi".
			configSwitch(QUALITY_PARAM, 0.f, 2.f, 0.f, "Quality", {"Hi-fi", "Mid-fi", "Lo-fi"});	

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
					henon(henonX, henonY);
					break;
				case IKEDA:v
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
		z = complex<double>{a, 0} + complex<double>{b, 0} * lastZ * pow(2.71828, complex<double>{0, 1} * ((k - p)/(1 + z2)));
		//pull the real and imaginary parts out for sending to the CV outputs. check for NaN values.
		double tuning = params[TUNE_PARAM].getValue();
		//add tuning bias
		outputs[CVOUT1_OUTPUT].setVoltage(isfinite(z.real()) ? z.real() + tuning : 0.f, 0);
		outputs[CVOUT1_OUTPUT].setChannels(1);
		outputs[CVOUT2_OUTPUT].setVoltage(isfinite(z.imag()) ? z.imag() + tuning : 0.f, 0);
		outputs[CVOUT2_OUTPUT].setChannels(1);
	}


	void henon(float lastX, float lastY){
		//set z2 to default
		z2 = 1;
		henonX = (lastY + 1) - (1.4 * pow(lastX, 2));
		henonY = 0.3f * lastX;
		//add tuning bias
		double tuning = params[TUNE_PARAM].getValue();
		outputs[CVOUT1_OUTPUT].setVoltage(isfinite(henonX) ? henonX + tuning : 0.f, 0);
		outputs[CVOUT1_OUTPUT].setChannels(1);
		outputs[CVOUT2_OUTPUT].setVoltage(isfinite(henonY) ? henonY + tuning : 0.f, 0);
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
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(31.096, 26.194)), module, Strange::TUNE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.296, 51.894)), module, Strange::GATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.772, 51.894)), module, Strange::MODE_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.869, 85.027)), module, Strange::GATEIN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.17, 85.027)), module, Strange::GATEOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(9.869, 106.627)), module, Strange::CVOUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.169, 106.627)), module, Strange::CVOUT2_OUTPUT));
	}
};


Model* modelStrange = createModel<Strange, StrangeWidget>("Strange");