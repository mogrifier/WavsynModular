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
	float audio1 = 0.f;
	float audio2 = 0.f;
	bool skip = false;
	int skipCount = 0;
	const int MAXSKIP = 25;

	//for buffers and FFT
	static const int SIZE = 1024;
	static const int LIMIT = 256;
	float circularBuffer[SIZE];
	alignas(16) float linearBuffer[SIZE] = {};
	alignas(16) float fftOutput[SIZE] = {};
	alignas(16) float freqBuffer[SIZE * 2];
	alignas(16) float empty[768] = {0.f};
	int index = 0;
	int count = 0;
	int circularIndex = 0;
	bool circularBufferFull = false;
	float original = 0.f;
	alignas(16) float antiAliased[SIZE] = {};

	Smitty(){
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SHAPE_PARAM, 0.f, 2.f, 0.f, "Add harmonics");
		configInput(SHAPECV_INPUT, "Odd FM tones with square or saw LFO");
		configInput(VOCT_INPUT, "V/OCT");
		configOutput(AUDIO1_OUTPUT, "audio 1");
		configOutput(AUDIO2_OUTPUT, "audio 2");
		//init circularBuffer
		for (int i = 0; i < SIZE; i++){
			circularBuffer[i] = 0.1f;
		}

	}


	void process(const ProcessArgs& args) override {

		dsp::RealFFT outFFT(SIZE);
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
			skip = true;
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

		audio1 = 5.f * sin(y1);
		audio2 = 5.f * sin(yq);


		original = audio1;
		//fill array
		circularBuffer[index] = audio1;
		index++;
		//this creates a circular circularBuffer that just keeps filling with latest data
		if (index >= SIZE) {
			index = 0;
			circularBufferFull = true;
		}

		//circular buffer has two parts. Their order needs to be flipped to put in time linear order
		//current index is the start of the new array  since it was already incremented (it has the oldest data)
		if (circularBufferFull) {
			//convert circularBuffer to linear ordered buffer and pass to FFT
			std::memcpy(linearBuffer, &circularBuffer[index], (SIZE - (index)) * sizeof(float));
			if (index > 0){
				//copy in buffer from 0
				std::memcpy(linearBuffer + index, &circularBuffer[0], index * sizeof(float));
			}

			//compute the real, ordered FFT
			outFFT.rfft(linearBuffer, fftOutput);
			//access the first few bins and create sine waves; add them up to recreate the sample needed

			//bandlimit the signal by setting everything above a certain point to zero, use memcpy to zero it.
			//std::memcpy(fftOutput + SIZE/2, empty, SIZE / 2 * sizeof(float));
			//bandlimit the data
			for (int i = LIMIT; i < SIZE; i++){
				fftOutput[i] = 0;
			}		
			//compute inverse FFT to get one sample. Presumably without any aliasing components

			audio1 = computeIFFTSampleIgnoringComplex(fftOutput, freq, args.sampleRate);
			//outFFT.irfft(fftOutput, antiAliased);
			//audio1 = antiAliased[0];

			
		}

		//output will be original sample for first 1024 at startup; after that will be the antialiased output
		outputs[AUDIO1_OUTPUT].setVoltage(audio1);
		//outputs are different and complimentary- best in stereo!
		outputs[AUDIO2_OUTPUT].setVoltage(audio2);

		oldFreq = freq;
	}



// Function to compute a single time-domain sample from FFT data ignoring complex values
float computeIFFTSampleIgnoringComplex(float * fftOutput, float f, float sr ) {

    float sample = 0.f;
    float angle;

//whole point is to use the harmonics based on the current frequency- not every single frequency bin!
//bin center freq is current sample rate/SIZE. Grab close values?? WTF
	float cf = sr / SIZE;
	//what is nearest bin to freq??
	int bin = (int)f/cf;
	int harmonics[6] = {10, 18, 26, 34, 42, 50};
    for (int k = 0; k < 6; k++) {
        angle = 2 * M_PI * k  / SIZE;
        sample += fftOutput[harmonics[k]] * std::sin(angle);
    }

	//compute for a single frequency to establish truth- index 20
	//angle = 2 * M_PI * 0  / SIZE;
    //sample = fftOutput[0] * std::cos(angle);

    return sample / 6;
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