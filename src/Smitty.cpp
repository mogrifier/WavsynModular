#include "plugin.hpp"

using namespace math;

//use a structure for performing the FFT operations. This avoids callling the FFT constructor in the process loop.
struct FFTFilter {

	dsp::RealFFT fft;

	FFTFilter() : fft(2048) {}

	void forwardFFT(float * input, float * output){
		fft.rfft(input, output);
	}

	void reverseFFT(float * input, float * output){
		fft.irfft(input, output);
	}


};

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

	//for buffers and FFT- should run 2048 since no real change in performance and can get better waveform apporx, if doing fft at all
	static const int SIZE = 2048;
	static const int LIMIT = 200;
	alignas(16) float circularBuffer[SIZE]{};
	alignas(16) float linearBuffer[SIZE] = {};
	alignas(16) float fftOutput[SIZE * 2] = {};
	int index = 0;
	int circularIndex = 0;
	bool circularBufferFull = false;
	float original = 0.f;
	alignas(16) float antiAliased[SIZE] = {};
	float oversample = 0.f;
	const int CUTOFF = 8000;
	float lastSample = 0.f;
	static const int BUCKETS = 100;

	FFTFilter myFFT;

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
			shaper = 0.5f;
			//yq = 1.f;
			//myq = 1.f;
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
			shaper += (inputs[SHAPECV_INPUT].getVoltage() / 5) * (params[SHAPE_PARAM].getValue() / 2);
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

		//circularBuffer[index] is always latest sample and "end" of the buffer.
		circularBuffer[index++] = audio1;
		//DEBUG("audio = %f, array value = %f, index = %i", audio1, circularBuffer[index - 1], index  );

		//DEBUG("**index = %f", shaper);
		//this creates a  circularBuffer that just keeps filling with latest data
		//DEBUG("index = %i", index);
		if (index >= SIZE) {
			index = 0;
			circularBufferFull = true;
		}

		//std::memcpy(linearBuffer, &circularBuffer[0], SIZE * sizeof(float));

		if (circularBufferFull) {

			//I DO NOT linearize the buffer since it has no audible effecg if I just use the circular one
			//from index to end becomes the beginning
			std::memcpy(linearBuffer, &circularBuffer[index], (SIZE - index) * sizeof(float));
			//from beggining to index becomes the end
			std::memcpy(linearBuffer + (SIZE - index), &circularBuffer[0], index * sizeof(float));
			//remove the DC offset
			removeDCOffset(linearBuffer);
			//compute the ordered FFT. output include real and complex data
			myFFT.forwardFFT(linearBuffer, fftOutput);
			attenuate(fftOutput);


			/*
			 * attenuate and recreate do not work together. attenuate is for use with ifft, not recreate.
			 */

			//compute inverse FFT to get one sample. Presumably without any aliasing components

			//recreating the sound using inverse FFT with oversampling makes a HUGE difference in a good way

			//change DC offset
			//fftOutput[0] = fftOutput[0] * 0.8;
			myFFT.reverseFFT(fftOutput, antiAliased);


			//each inverseFFT sample MUST be divided by SIZE

			//compareFFT(fftOutput, antiAliased);

			audio1 = antiAliased[SIZE - 1]/SIZE;
			//audio1 = recreate(args.sampleTime);

			//filter based on last sample. low pass filter. CURRENTLY BROKE. CAUSE UNKNOWN. has to do with buffer fill.
			//audio1 = rcFilter(audio1, lastSample, CUTOFF, args.sampleRate);
		}

		//circular buffer is being changed in above code, so I make a copy then copy it back
		//std::memcpy(circularBuffer, &linearBuffer[0], SIZE * sizeof(float));

		//save the modified sample back to the buffer - index was already advanced so save to index -1
		circularBuffer[index - 1] = audio1;

		//output will be original sample for first SIZE samples at startup; after that will be original or attenuated
		outputs[AUDIO1_OUTPUT].setVoltage(circularBuffer[index - 1]);
		//outputs are different and complimentary- best in stereo!
		
		//DEBUG("buffer = %f", circularBuffer[index]);
				
		outputs[AUDIO2_OUTPUT].setVoltage(circularBuffer[abs(index - 1024) % SIZE]);
		//DEBUG("buffer = %f", circularBuffer[abs(index - 100) % SIZE]);

		oldFreq = freq;
	}


void dumpBuffer(float * buffer, int size){
	//debug output
	for (int i = 0; i < size; i++){
		DEBUG("index = %i value = %f", i, buffer[i]);
	}
}


bool compareArrays(const float* arr1, const float* arr2, size_t size, float epsilon = 1e-3) {
    for (size_t i = 0; i < size; ++i) {
        if (std::fabs(arr1[i] - arr2[i]) > epsilon) {
            return false;
        }
    }
    return true;
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

	for (int i = 2; i < SIZE; i+=2){
		//calculate bin width in Hz
		//float f = (48000 / SIZE) * i;
		//attenuate signals- the more you attenuate, the more inacurate the result is
		if (i >= 100 ) {

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
		for (int i = 2; i < 10; i++) {

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



void compareFFT(float * forward, float * backward){

	for (int i = 0; i < 100; i++)
	{
		DEBUG("forward = %f",forward[i]);
	}
	for (int i = 0; i < 100; i++)
	{
		DEBUG("backward = %f, backward/SIZE = %f, original = %f", backward[i], backward[i]/SIZE, linearBuffer[i]);
	}
	exit(0);
}


/*
recreate original signal from fft data
*/
float recreate(float period) {


	float totalAmp = 0.f;
    float sample = 0.f;
	float phaseData[BUCKETS]{0};
	float sampleData[BUCKETS]{0};
	float phaseInc = period/SIZE;

	//calculate harmonic freq, phase data, and the sample

//what if the phase data is in the fft data?? first value is amplitude, second is phase. No phase calculation needed.
	int count = 0;

	for (int i = 2; i < BUCKETS * 2; i+=2){
		//each harmonic gets its own phase value
		//phaseData[i] = fftOutput[i + 1];
		
		sampleData[count++] = std::sin(2.f * M_PI * fftOutput[i + 1]);
		totalAmp += fftOutput[i];
		//DEBUG("sample = %f", sampleData[i]);
		//DEBUG("phase = %f", phaseData[i]);
	}

	count = 0;
	//apply amplitude values
	for (int i = 2; i < BUCKETS * 2; i+=2){
		sample += sampleData[count++] * fftOutput[i];
	}
	
	//normalize the total using the norming value based on the sum of the amplitudes (this is like removing DC Offset I think)
	return sample / 1000; //totalAmp;
}

/*
Changes all elements of circular buffer.
*/
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