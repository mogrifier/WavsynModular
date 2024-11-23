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

	//for buffers and FFT- should run 2048 since no real change in performance and can get better waveform apporx, if doing fft at all
	static const int SIZE = 2048;
	static const int LIMIT = 200;
	alignas(16) float circularBuffer[SIZE]{0};
	alignas(16) float linearBuffer[SIZE] = {};
	alignas(16) float fftOutput[SIZE * 2] = {};
	int index = 0;
	int count = 0;
	int circularIndex = 0;
	bool circularBufferFull = false;
	float original = 0.f;
	alignas(16) float antiAliased[SIZE] = {};
	float oversample = 0.f;
	const int CUTOFF = 8000;
	float lastSample = 0.f;

	Smitty(){
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SHAPE_PARAM, 0.f, 2.f, 0.f, "Add harmonics");
		configInput(SHAPECV_INPUT, "Odd FM tones with square or saw LFO");
		configInput(VOCT_INPUT, "V/OCT");
		configOutput(AUDIO1_OUTPUT, "audio 1");
		configOutput(AUDIO2_OUTPUT, "audio 2");
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
			//yq = 1.f;
			//myq = 1.f;
			//skip = true;

			epsilon = 0.f;
		}

		yq = myq - epsilon * shaper;

		//modify shaper using a param or CV
		//if connected, use the CV input. sounds very cool. else use manual setting
		if (inputs[SHAPECV_INPUT].isConnected()){
			//read cv input
			//shaper += abs(inputs[SHAPECV_INPUT].getVoltage() / 5);
			//modify with knob
			//float modifier = params[SHAPE_PARAM].getValue() / 2;
			shaper += abs(inputs[SHAPECV_INPUT].getVoltage() / 5) * ( params[SHAPE_PARAM].getValue() / 2);
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
		//fill array but perform oversampling (interpolate an extra sample every cycle)
		circularBuffer[index++] = audio1;

		/**
		oversample = (circularBuffer[index] + original) / 2;
		circularBuffer[index + 1 ] = oversample;
		circularBuffer[index + 2] = audio1;
		//increment index by two
		index+=2;
		*/



		//this creates a circular circularBuffer that just keeps filling with latest data
		if (index >= SIZE) {
			index = 0;
			circularBufferFull = true;
		}

		//circular buffer has two parts. Their order needs to be flipped to put in time linear order
		//current index is the start of the new array  since it was already incremented (it has the oldest data)
		if (circularBufferFull) {

			//convert circularBuffer to linear ordered buffer and pass to FFT. Uses two step copy

			//from index to end becomes the beginning
			//std::memcpy(linearBuffer, &circularBuffer[index], (SIZE - index) * sizeof(float));
			//from beggining to index becomes the end
			//std::memcpy(linearBuffer + (SIZE - index), &circularBuffer[0], index * sizeof(float));

			//remove the DC offset
			//removeDCOffset(linearBuffer);

			removeDCOffset(circularBuffer);

			//compute the ordered FFT. output include real and complex data
			outFFT.rfft(circularBuffer, fftOutput);

			attenuate(fftOutput);

			/**
			 * attenuate and recreate do not work together. attenuate is for use with ifft, not recreate.
			 */

			//compute inverse FFT to get one sample. Presumably without any aliasing components

			//recreating the sound using inverse FFT with oversampling makes a HUGE difference in a good way
			outFFT.irfft(fftOutput, antiAliased);

			audio1 = antiAliased[0]/SIZE;
			//audio1 = recreate();

			//filter based on last sample. low pass filter. CURRENTLY BROKE. CAUSE UNKNOWN. has to do with buffer fill.
			//audio1 = rcFilter(audio1, lastSample, CUTOFF, args.sampleRate);
		}

		

		//output will be original sample for first 1024 at startup; after that will be the antialiased output
		outputs[AUDIO1_OUTPUT].setVoltage(audio1);
		//outputs are different and complimentary- best in stereo!
		outputs[AUDIO2_OUTPUT].setVoltage(audio2);

		oldFreq = freq;

		lastSample = audio1;
	}


void dumpBuffer(float * buffer, int size){
	//debug output
	for (int i = 0; i < size; i++){
		DEBUG("%f", buffer[i]);
	}
}

// Function to compute a single time-domain sample from FFT data ignoring complex values


//filter- not quite right. I need to understand concept of accumulator. Should just be a running total. But that would get too big.
//1 - exp(2piFc/Fs) = k
// output = output * (1 - k) + input    where input and output are the samples in question. This is an RC filter. 
//the accumulator stores the output samples. I just need the last one.
/*
This filter does work. Not huge, but it works.

*/
float rcFilter(float in, float lastOut, float fc, float fs) {

	float k = 1 - exp(-2 * M_PI * fc / fs);
	return lastOut * (1 - k) + in;
}

/**
 * designed to drop high frequencies. works on fft output array.
 */
void attenuate(float * data){ 			
	//bandlimit the signal by setting everything above a certain point to zero
	for (int i = 2; i < SIZE * 2; i+=2){
		//calculate bin width in Hz
		float f = (48000 / SIZE) * i;
		//attenuate signals
		if (f >= 2000 ) {

			//if (fftOutput[2 * i] > 0){
				//real
				data[2 * i] = 0; //fftOutput[2 * i] * 0.1f;
			//}
			//if (fftOutput[2 * i + 1] > 0){
				//complex
				data[2 * i + 1] = 0; //fftOutput[2 * i + 1] * 0.1f;
			//}
		}	

		//remove the low frequency data with removing first two data points
		for (int i = 2; i < 5; i++) {

			//if (fftOutput[2 * i] > 0){
				//real
				data[2 * i] = 0; //fftOutput[2 * i] * 0.1f;
			//}
			//if (fftOutput[2 * i + 1] > 0){
				//complex
				data[2 * i + 1] = 0; //fftOutput[2 * i + 1] * 0.1f;
			//}
		}	
		
	}
}

/*
recreate original signal from fft data
*/
float recreate() {

    float sample = 0.f;
    float angle;
	//cosine of real component; sine of imaginary component
    for (int k = 10; k < 110; k+=2) {
        angle = 2 * M_PI * k  / SIZE;
		sample += (fftOutput[k] * cos(angle)+ fftOutput[k + 1] * sin(angle)) - fftOutput[0];
		//horrible to take magnitude pow(pow(fftOutput[k] * cos(angle), 2) + pow(fftOutput[k + 1] * sin(angle), 2), 0.5f);
    }

    return sample / 100;
}


void removeDCOffset(float * buffer) {
    float sum = 0.f;
    for (int i = 0; i < SIZE; ++i) {
        sum += buffer[i];
    }
    float mean = sum / SIZE;
    for (int i = 0; i < SIZE; ++i) {
        buffer[i] -= mean;
    }
}



/*
f is input frequency
sr is sample rate
*/
float computeIFFTSample(float * fftOutput, float f, float sr ) {

    float sample = 0.f;
    float angle;

//whole point is to use the harmonics based on the current frequency- not every single frequency bin!
//bin center freq is current sample rate/SIZE. Grab close values?? WTF
	float cf = sr / SIZE;
	//what is nearest bin to freq??
	//start at k (bin near frequency) and include the next count partials. Still not proper harmonics but may get
    for (int k = 0; k < LIMIT; k++) {
        angle = 2 * M_PI * f  / SIZE;
		//if (fftOutput[k] > 0) {
			//elimiate negative partials
        	sample += fftOutput[(int)(f*k/cf)] * std::sin(angle);
		//}
    }

    return sample / 1024;
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