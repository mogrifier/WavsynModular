#include "plugin.hpp"
#include <algorithm>
#include <iterator>

using namespace math;

struct Trip : Module {
	const int STEPS = 8;
	int quant = 0;
	int timefit = 1;
	float volts = 0.f;
	float voltsFraction = 0.f;
	double voltsInteger = 0.f;
	float space = 0.f;
	float gate = 0.f;
	float skipChance = 0.f;
	float skipped = false;
	std::string step = "";
	int stepIndex = 0;
	float stepSpace = 0.f;
	float stepDuration = 0.f;

	int octave = 0;
	double duration = 0.f;
	int count = 0;
	int stepCount = 0;
	//clock handling
	bool clockLow = true;
	bool betweenStates = false;
	double bpm = 100.0f; //default
	double oldBPM = 100.0f;
	double clockDelay = 0.0f;
	int ticks = 115200; //default based on 100 BPM
	bool clockTriggered = false;
	dsp::SchmittTrigger clockTrigger;
	double clockVoltage = 0.0f;
	//for reset sensing
	dsp::SchmittTrigger trigger;
	dsp::BooleanTrigger timefitButtonTrigger;
	bool reset = false;

	//for enum use
	std::string VOLTS = "VOLTS";
	std::string SPACE = "SPACE";
	std::string GATE = "GATE";
	std::string CV = "CV";
	std::string STEP = "STEP";
	std::string G = "G";

	//for quantization- is this right? zero start or 1 end?? or both? 13 notes are an octave.
	const float twelveTone[13] = {0.f, 0.083f, 0.167f, 0.25f, 0.333f, 0.417f, 0.5f, 0.583f, 0.666f, 0.75f, 0.833f, 0.917f, 1.f };
	const float quarterTone[25] = {0.f, 0.0417f, 0.083f, 0.125f, 0.167f, 0.208, 0.25f, 0.292f, 0.333f, 0.375f, 0.417f, 0.458f, 0.5f,
	 	0.542f, 0.583f, 0.625f, 0.666f, 0.708f, 0.75f, 0.792f, 0.833f, 0.875f, 0.917f, 0.958f, 1.f};

	//for space parameter change tracking
	float spaceSetting[8] = {0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125};
	float spaceInc = 0.f;
	float spaceTotal = 0.f;
	bool firstRun = true;
	const float EPSILON = 1e-4f;

	//step tracking
	int stepOrder[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	int tempOrder[8] = {1, 2, 3, 4, 5, 6, 7, 8};

	enum ParamId {
		OCTAVE_PARAM,
		QUANT_PARAM,
		MODULO_PARAM,
		SKIP_PARAM,
		REVERSAL_PARAM,
		EVOL_PARAM,
		TIMEFIT_PARAM,
		VOLTS1_PARAM,
		VOLTS2_PARAM,
		VOLTS3_PARAM,
		VOLTS4_PARAM,
		VOLTS5_PARAM,
		VOLTS6_PARAM,
		VOLTS7_PARAM,
		VOLTS8_PARAM,
		SPACE1_PARAM,
		SPACE2_PARAM,
		SPACE3_PARAM,
		SPACE4_PARAM,
		SPACE5_PARAM,
		SPACE6_PARAM,
		SPACE7_PARAM,
		SPACE8_PARAM,
		GATE1_PARAM,
		GATE2_PARAM,
		GATE3_PARAM,
		GATE4_PARAM,
		GATE5_PARAM,
		GATE6_PARAM,
		GATE7_PARAM,
		GATE8_PARAM,
		TIMEFITBUTTON_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ALLCVOUT_OUTPUT,
		GATEOUT_OUTPUT,
		CV1_OUTPUT,
		CV2_OUTPUT,
		CV3_OUTPUT,
		CV4_OUTPUT,
		CV5_OUTPUT,
		CV6_OUTPUT,
		CV7_OUTPUT,
		CV8_OUTPUT,
		G1_OUTPUT,
		G2_OUTPUT,
		G3_OUTPUT,
		G4_OUTPUT,
		G5_OUTPUT,
		G6_OUTPUT,
		G7_OUTPUT,
		G8_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		STEP1, STEP2, STEP3, STEP4, STEP5, STEP6, STEP7, STEP8, DIRTY, LIGHTS_LEN
	};

	Trip() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		//On 0, the random setting is OFF. Greater than zero it represents the chance 
		//for each step for the random event to happen, modifiying the step or the sequence

		//restrict values to whole numbers
		configSwitch(OCTAVE_PARAM, -3, 3, 0, "Add to VOLTS for each step", {"-3", "-2", "-1", "0", "1", "2", "3"});

		//free or 1-bar mode for how long the pattern is and if the space values add to 100% or not
		configSwitch(TIMEFIT_PARAM, 1, 8, 1, "Time Fit", {"1 Bar", "2 Bars", "3 Bars", "4 Bars", "5 Bars", "6 Bars", "7 Bars", "8 Bars"});

		configButton(TIMEFITBUTTON_PARAM, "Time Fit");

		//causes a shift in the pattern by changing where it starts- add the shift amount to get new starting step
		configSwitch(MODULO_PARAM, 0, 7, 0, "Starting Step", {"0", "1", "2", "3", "4", "5", "6", "7"});

		//skip a step (gate out is zero). Same chance for every step.
		configParam(SKIP_PARAM, 0.f, 0.99f, 0.f, "Chance to skip a step", "%", 0.f, 101.01f);

		//start a 8 instead of 1
		configParam(REVERSAL_PARAM, 0.f, 1.f, 0.f, "Chance to reverse sequence", "%", 0.f, 100.f);

		//maybe a switch with settings 1-8?. Adjust SPACE so the times still add up to 100%
		configParam(EVOL_PARAM, 0.f, 1.f, 0.f, "Pattern Evolution", "%", 0.f, 100.f);

		//multiple switch positions- does quantization of VOLTS or not
		configSwitch(QUANT_PARAM, 0.f, 2.f, 0.f, "Quantization", {"None", "12-Tone", "Quartertone"});

		//CV output is simply called VOLTS since it cud be a pitch or a control signal but both are VOLTS
		configParam(VOLTS1_PARAM, -3.f, 6.f, 2.167f, "Set the Step CV output");
		configParam(VOLTS2_PARAM, -3.f, 6.f, 5.f, "Set the Step CV output");
		configParam(VOLTS3_PARAM, -3.f, 6.f, 2.33f, "Set the Step CV output");
		configParam(VOLTS4_PARAM, -3.f, 6.f, 2.75f, "Set the Step CV output");
		configParam(VOLTS5_PARAM, -3.f, 6.f, 2.5f, "Set the Step CV output");
		configParam(VOLTS6_PARAM, -3.f, 6.f, 5.f, "Set the Step CV output");
		configParam(VOLTS7_PARAM, -3.f, 6.f, 5.f, "Set the Step CV output");
		configParam(VOLTS8_PARAM, -3.f, 6.f, 5.f, "Set the Step CV output");

		//this parameter specifies the percentage of 1 BAR that a step uses. Must always total 100%,
		//so turning one know adjusts the others to compensate.
		configParam(SPACE1_PARAM, 0.f, 1.f, 0.125f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE2_PARAM, 0.f, 1.f, 0.125f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE3_PARAM, 0.f, 1.f, 0.125f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE4_PARAM, 0.f, 1.f, 0.125f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE5_PARAM, 0.f, 1.f, 0.5f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE6_PARAM, 0.f, 1.f, 0.f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE7_PARAM, 0.f, 1.f, 0.f, "Max Step Time", "%", 0.f, 100.f);
		configParam(SPACE8_PARAM, 0.f, 1.f, 0.f, "Max Step Time", "%", 0.f, 100.f);

		//each step is given an amount of by its SPACE setting. Gate governs how much of the SPACE is used.
		configParam(GATE1_PARAM, 0.f, 0.99f, 0.6f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE2_PARAM, 0.f, 0.99f, 0.f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE3_PARAM, 0.f, 0.99f, 0.35f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE4_PARAM, 0.f, 0.99f, 0.35f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE5_PARAM, 0.f, 0.99f, 0.15f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE6_PARAM, 0.f, 0.99f, 0.f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE7_PARAM, 0.f, 0.99f, 0.f, "Gate duration", "%", 0.f, 101.01f);
		configParam(GATE8_PARAM, 0.f, 0.99f, 0.f, "Gate duration", "%", 0.f, 101.01f);

		configOutput(CV1_OUTPUT, "Step 1 Individual CV");
		configOutput(CV2_OUTPUT, "Step 2 Individual CV");
		configOutput(CV3_OUTPUT, "Step 3 Individual CV");
		configOutput(CV4_OUTPUT, "Step 4 Individual CV");
		configOutput(CV5_OUTPUT, "Step 5 Individual CV");
		configOutput(CV6_OUTPUT, "Step 6 Individual CV");
		configOutput(CV7_OUTPUT, "Step 7 Individual CV");
		configOutput(CV8_OUTPUT, "Step 8 Individual CV");

		configOutput(G1_OUTPUT, "Step 1 Individual Gate");
		configOutput(G2_OUTPUT, "Step 2 Individual Gate");
		configOutput(G3_OUTPUT, "Step 3 Individual Gate");
		configOutput(G4_OUTPUT, "Step 4 Individual Gate");
		configOutput(G5_OUTPUT, "Step 5 Individual Gate");
		configOutput(G6_OUTPUT, "Step 6 Individual Gate");
		configOutput(G7_OUTPUT, "Step 7 Individual Gate");
		configOutput(G8_OUTPUT, "Step 8 Individual Gate");

		configInput(CLOCK_INPUT, "External clock for sequencer tempo");
		configInput(RESET_INPUT, "Reset the sequencer to Step 1 on a trigger");
		configOutput(ALLCVOUT_OUTPUT, "CV signal for each step");
		configOutput(GATEOUT_OUTPUT, "Gate signal for each step");
	}

	void process(const ProcessArgs& args) override {
		
		//FIXME not needed anymore since spaceSetting is not used.
		if (firstRun) {
		//one-time setup code
			int i = 0;
			try {
				//set the space settings to values from the UI (moved here since not working in the constructor properly)
				for (i = 0; i < STEPS; i++) {
					spaceSetting[stepOrder[i] - 1] = params[getSpaceEnum(SPACE + std::to_string(stepOrder[i]))].getValue();
					//DEBUG("setting step %i = %f", stepOrder[i], spaceSetting[stepOrder[i] - 1]);
				}
			}
			catch(const std::invalid_argument& e) {
				//one lookup value (i or j) is bad. All the try/catch blocks are just a developer aid, BTW.
				DEBUG("lookup is bad = %i", stepOrder[i]);
				//by returning, the module will do nothing, signaling a problem
				return;
			}
			firstRun = false;
		}
		//get current time fit setting (number of bars)
		timefit = params[TIMEFIT_PARAM].getValue();
		bool timefitButtonTriggered = timefitButtonTrigger.process(params[TIMEFITBUTTON_PARAM].getValue());
		if (timefitButtonTriggered) {
			//scale all the space settings to fit the number of bars selected
			//DEBUG("timefit %i", timefit);
			spaceScale(timefit);
		}

		//need the total space for scling and the dirty light
		spaceTotal = 0.f;
		for (int j = 0; j < STEPS; j++) {
			spaceTotal += params[getSpaceEnum(SPACE + std::to_string(stepOrder[j]))].getValue();
		}

		if (abs(spaceTotal - 1.f) > EPSILON) {
			//turn on the dirty light
			lights[DIRTY].setBrightness(1.f);
		}
		else {
			//turn off the dirty light
			lights[DIRTY].setBrightness(0.f);
		}

		//need to get a tempo from the clock input
		if (inputs[CLOCK_INPUT].isConnected()){
			//check for clock transition
			clockVoltage = inputs[CLOCK_INPUT].getVoltage();
			clockDelay++;
			clockTriggered = clockTrigger.process(clockVoltage, 0.1f, 2.f);
			if (!betweenStates && clockTriggered && clockLow) {
				//clock just went from low to high
				clockLow = false;
				//calculate new BPM
				bpm = (args.sampleRate / clockDelay) * 60.0f;
				if (abs(bpm - oldBPM) > 1) {
					//DEBUG("new bpm = %f", bpm);
					//assume tempo change, not just a small fluctuation
					//start counting for new bpm from point in time o ftempo change
					count = 0;
					oldBPM = bpm;
				}
				
				betweenStates = true;
				clockDelay = 0;
			}
			else if (betweenStates && (clockVoltage == 0.f) && !clockLow) {
				//clock went from high to low
				clockLow = true;
				betweenStates = false;
			}
			//compute the length of one bar using BPM. SPACE will divvy up the bar into the correct number of samples per step
			ticks = 4 * (60.0f/bpm) *  args.sampleRate;
		}
		else {
			//no clock input so do nothing
			return;
		}

		/*
		for (int j = 0; j < STEPS; j++) {
			spaceTotal += params[getSpaceEnum(SPACE + std::to_string(stepOrder[j]))].getValue();
		}
		*/
	
		count++;
		stepCount++;

		if (inputs[RESET_INPUT].isConnected()) {
			//check for reset signal
			if (trigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
				//trigger received, reset step the value specified by the MODULO param
				reset = true;
				//turn off all lights
				lightsOff();
			}
			else {
				//no reset signal
				reset = false;
			}
		}

		if (reset){
			count = 0;
			//back to the beginning. stepIndex is the index to stepOrder, so it still moves from 0 ..8 (zero based)
			stepIndex = 0;
		}

		//set the current step output to its output and to all cv ouput
		octave = params[OCTAVE_PARAM].getValue();
		quant = params[QUANT_PARAM].getValue();
		try {
			volts = params[getVoltsEnum(VOLTS + std::to_string(stepOrder[stepIndex]))].getValue();
		}
		catch( const std::invalid_argument& e ) {
    			// do stuff with exception... 
				DEBUG("volts lookup is bad = %i", stepOrder[stepIndex]);
				//by returning, the module will do nothing, signaling a problem
				return;
		}
		//also sets voltInteger
		voltsFraction = modf(volts, &voltsInteger);

		//DEBUG("int = %f fraction = %f", voltsInteger, voltsFraction);
		//DEBUG("quant = %d", quant);
		//switch on quant value to perform quantization of output (or not)
		switch (quant) {
			case 0:
			//do nothing since volts is already accurate
			//DEBUG("quant = %d", 0);
			break;
			case 1:
			//12 tone
			volts = quantize12tone(voltsFraction) + voltsInteger;
			break;
			case 2:
			//quartertone
			volts = quantize24tone(voltsFraction) + voltsInteger;
			break;
		}

		//DEBUG("volts = %f", volts);
		try {
			//add in the octave value bias but only if all will STAY IN RANGE -3 to 6. Don't clamp, just change octave value.
			//what about negative? nothing below -3v changes pitch with some VCO's. Ignore?
		 	if (getMaxVolts() + octave > 6.f) {
				//out of range. Reduce octave setting appropriately.
				if (getMaxVolts() + octave - 1 > 6.f) {
				//out of range. Reduce octave setting appropriately.
				 	if (getMaxVolts() + octave - 2 > 6.f) {
						//set ocatve to zero since +3 is too high for settings (we know octave is 3 at this point)
						params[OCTAVE_PARAM].setValue(0.f);
					}
					else {
						params[OCTAVE_PARAM].setValue(octave - 2.f);
					}
				}
				else {
					params[OCTAVE_PARAM].setValue(octave - 1.f);
				}
			}

			//set individual CV output values
			outputs[getOutputEnum(CV + std::to_string(stepOrder[stepIndex]))].setVoltage(volts + octave);
			outputs[ALLCVOUT_OUTPUT].setVoltage(volts + octave);

			//calculate duration for step pointed to by stepIndex and total space for the step in number of samples
			stepSpace = params[getSpaceEnum(SPACE + std::to_string(stepOrder[stepIndex]))].getValue() * ticks;
			//step duration is a fraction of the total allocated space for the step
			stepDuration = params[getGateEnum(GATE + std::to_string(stepOrder[stepIndex]))].getValue() * stepSpace;
		}
		catch( const std::invalid_argument& e ) {
    			// do stuff with exception... 
				DEBUG("lookup is bad = %i", stepOrder[stepIndex]);
				//by returning, the module will do nothing, signaling a problem
				return;
		}


		/* this logic also controls if the Gate signal is off or on. */
		
		/*
		I think this has to be reworked since the count is not making sense relative to a single step.
		Need to track count for each step maybe, plus a total count. 
		*/

		if (!skipped && stepCount <= stepDuration * timefit)
		{
			//what is happening here is that the output "sticks" on the last value until a new value is created. 
			//keep gate on
			outputs[GATEOUT_OUTPUT].setVoltage(10.f, 0);
			//set individual gate output on
			outputs[getIndividualGateOutputEnum(G + std::to_string(stepOrder[stepIndex]))].setVoltage(10.f, 0);
			outputs[GATEOUT_OUTPUT].setChannels(1);
			//turn on the light for the step
			try {
				lights[getLightEnum(STEP + std::to_string(stepOrder[stepIndex]))].setBrightness(1.f);
			}
			catch( const std::invalid_argument& e ) {
				DEBUG("lights lookup is bad = %i", stepOrder[stepIndex]);
			}
			return;
		}
		else if ((stepCount > stepDuration * timefit) && (stepCount < stepSpace * timefit)) 
		{
			//turn off gate output
			outputs[GATEOUT_OUTPUT].setVoltage(0.f, 0);
			//set individual gate output off
			outputs[getIndividualGateOutputEnum(G + std::to_string(stepOrder[stepIndex]))].setVoltage(0.f, 0);
			outputs[GATEOUT_OUTPUT].setChannels(1);
			//turn off the light
			try {
				lights[getLightEnum(STEP + std::to_string(stepOrder[stepIndex]))].setBrightness(0.f);
			}
			catch( const std::invalid_argument& e ) {
				DEBUG("lights lookup is bad = %i", stepOrder[stepIndex]);
			}
			return;
		}
		
		if (stepCount >= stepSpace * timefit){
			stepCount = 0;
			stepIndex++;
			//once you skip a step, it is skipped for the duration (Space) of the step. only check at beggining of each step.
			if (random::uniform() <= params[SKIP_PARAM].getValue()) {
				skipped = true;
				//DEBUG("skipped = %f", 1.f);
			}
			else {
				skipped = false;
				//DEBUG("skipped = %f", 0.f);
			}
			
		}
		
		if (count >= ticks) {
			count = 0;
		}

		if (stepIndex >= STEPS) {
			//reset the index to stepOrder
			stepIndex = 0;
			//need to always verify/update the order, since you might set it back to zero
			//rewrite the order using a modulo formula to shift the pattern
			int shift = 0;
			//save the current order to avoid rewriting error. The purpose of the modulo shift is to KEEP SHIFTING
			std::copy(stepOrder, stepOrder + STEPS, tempOrder);
			for (int i = 0; i < STEPS; i++) {
				shift = i + params[MODULO_PARAM].getValue();
				//read the value pointed to by the modulo operation and move it. This should be able to shift a reversed order.
				stepOrder[i] = tempOrder[shift % STEPS];
				//DEBUG("step index = %i, value = %i,  temp order value = %i", i, stepOrder[i], tempOrder[i]);
			}
			//check for reversal and perfrom
			if (random::uniform() <= params[REVERSAL_PARAM].getValue()) {
				//rewrite the current pattern in stepOrder backwards
				std::reverse(std::begin(stepOrder), std::end(stepOrder));
			}
			//check for evolution changes and perform them
			if (random::uniform() <= params[EVOL_PARAM].getValue()) {
				evolve();
			}
		}
	}

/*
Trigger evolution of the pattern. Only 1 change is made per sequence.
*/
void evolve() {
	std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(0, 6); // define the range
	int mutant = distr(gen);
	//get a step number and set up other random number generators 
	std::uniform_int_distribution<> distrStep(1, 8);
	int evolStep = distrStep(gen);
	std::uniform_int_distribution<> distrOct(-3, 6);
	std::uniform_int_distribution<> distrMod(0, 7);

	//Read and modify the parameter pointed to by value of mutant. Must respect ranges.
	// build the evolution amount based on current value to guarantee simple and legal change.
	//simplifies programming if choosing current value (randomly) is allowed so that's what I am doing.
	switch (mutant) {
		case 0:
			//oct -3 to 6
			params[OCTAVE_PARAM].setValue(distrOct(gen));
			break;
		case 1:
			//mod 0 - 7
			params[MODULO_PARAM].setValue(distrMod(gen));
			break;
		case 2:
			//skip 0 - 40%
			params[SKIP_PARAM].setValue(random::uniform() * 0.4f);
			break;
		case 3:
			//reverse  0 - 100%
			params[REVERSAL_PARAM].setValue(random::uniform());
			break;
		case 4:
			//volts- using evolStep. change to -3 to 6
			params[getVoltsEnum(VOLTS + std::to_string(evolStep))].setValue((random::uniform() * 9 - 3));
			//DEBUG("volts = %f", params[getVoltsEnum(VOLTS + std::to_string(evolStep))].getValue());
			break;
		case 5: 
			//space- using evolStep. change to  0 - 100%
			params[getSpaceEnum(SPACE + std::to_string(evolStep))].setValue(0.05f + random::uniform() * 0.95f);
			break;
		case 6:
			//gate- using evolStep. change to  0 - 100%
			params[getGateEnum(GATE + std::to_string(evolStep))].setValue(0.2f + random::uniform() * 0.8f);
			break;
	}
}

//get max voltage for all steps being used (space > 0). 
float getMaxVolts() {
	float max = 0.f;
	float current = 0.f;
	for (int i = 0; i < STEPS; i++) {
		current = params[getVoltsEnum(VOLTS + std::to_string(stepOrder[i]))].getValue();
		//ignore steps that are skipped (spac = 0)
		if (current > max && params[getSpaceEnum(SPACE + std::to_string(stepOrder[i]))].getValue() > 0.f)
		{
			max = current;
		}
	}
	return max;
}

void lightsOff() {
    //no reason to check for exception since calling code already does
    for (int i = 1; i <= STEPS; i++) {
        if (i != stepOrder[stepIndex]) {
            lights[getLightEnum(STEP + std::to_string(i))].setBrightness(0.f);
        }
    }
}

//quantize- 
float quantize12tone(float v) {
	//iterate the 12 tone array and find closest value. 
	float min = 1.f;
	float qVal = 0.f;
	for (int i = 0; i < 13; i++){
		if (abs(v - twelveTone[i]) < min) {
			min = abs(v - twelveTone[i]);
			//DEBUG("quant = %f", 12.f);
			//found new minimum
			qVal = twelveTone[i];
		} 
	}
	return qVal;
}

//quantize- 
float quantize24tone(float v) {
	//iterate the 24 tone array and find closest value. 
	float min = 1.f;
	float qVal = 0;
	for (int i = 0; i < 25; i++) {
		if (abs(v - quarterTone[i]) < min) {
			min = abs(v - quarterTone[i]);
			//DEBUG("min = %f", min);
			//found new minimum
			qVal = quarterTone[i];
		}
	}
	return qVal;
}

/* Rescale all the space settings that are non-zero to fit and fill the number of bars.
spaceSetting and stepOrder are needed. Keep space in range 0-100. */
void spaceScale(int bars) {
	//rescale to make total ~1
	float scale = 1.f / spaceTotal;
	//DEBUG("original space = %f, bars = %i, scale = %f", spaceTotal, bars, scale);
	//reset spaceTotal and recalculate
	spaceTotal = 0.f;
	for (int j = 0; j < STEPS; j++) {
		float rescaled = params[getSpaceEnum(SPACE + std::to_string(stepOrder[j]))].getValue() * scale;
		//DEBUG("rescaled = %f", rescaled);
		params[getSpaceEnum(SPACE + std::to_string(stepOrder[j]))].setValue(rescaled);
		spaceTotal += params[getSpaceEnum(SPACE + std::to_string(stepOrder[j]))].getValue();
	}
	//DEBUG("rescaled space = %f", spaceTotal);
}

//bunch of utility methods to make it easy to use a string built in a loop to get 
//corresponding enum values
ParamId getVoltsEnum(const std::string lookup) {
    if (lookup == "VOLTS1") return ParamId::VOLTS1_PARAM;
    else if (lookup == "VOLTS2") return ParamId::VOLTS2_PARAM;
    else if (lookup == "VOLTS3") return ParamId::VOLTS3_PARAM;
	else if (lookup == "VOLTS4") return ParamId::VOLTS4_PARAM;
    else if (lookup == "VOLTS5") return ParamId::VOLTS5_PARAM;
	else if (lookup == "VOLTS6") return ParamId::VOLTS6_PARAM;
    else if (lookup == "VOLTS7") return ParamId::VOLTS7_PARAM;
	else if (lookup == "VOLTS8") return ParamId::VOLTS8_PARAM;
	else throw std::invalid_argument("received bad lookup value");
}

ParamId getSpaceEnum(const std::string lookup) {
	if (lookup == "SPACE1") return ParamId::SPACE1_PARAM;
    else if (lookup == "SPACE2") return ParamId::SPACE2_PARAM;
    else if (lookup == "SPACE3") return ParamId::SPACE3_PARAM;
	else if (lookup == "SPACE4") return ParamId::SPACE4_PARAM;
    else if (lookup == "SPACE5") return ParamId::SPACE5_PARAM;
	else if (lookup == "SPACE6") return ParamId::SPACE6_PARAM;
    else if (lookup == "SPACE7") return ParamId::SPACE7_PARAM;
	else if (lookup == "SPACE8") return ParamId::SPACE8_PARAM;
	else throw std::invalid_argument("received bad lookup value");
}

ParamId getGateEnum(const std::string lookup) {
	if (lookup == "GATE1") return ParamId::GATE1_PARAM;
    else if (lookup == "GATE2") return ParamId::GATE2_PARAM;
    else if (lookup == "GATE3") return ParamId::GATE3_PARAM;
	else if (lookup == "GATE4") return ParamId::GATE4_PARAM;
    else if (lookup == "GATE5") return ParamId::GATE5_PARAM;
	else if (lookup == "GATE6") return ParamId::GATE6_PARAM;
    else if (lookup == "GATE7") return ParamId::GATE7_PARAM;
	else if (lookup == "GATE8") return ParamId::GATE8_PARAM;
	else throw std::invalid_argument("received bad lookup value");
}

OutputId getOutputEnum(const std::string lookup) {
    if (lookup == "CV1") return OutputId::CV1_OUTPUT;
    else if (lookup == "CV2") return OutputId::CV2_OUTPUT;
    else if (lookup == "CV3") return OutputId::CV3_OUTPUT;
	else if (lookup == "CV4") return OutputId::CV4_OUTPUT;
    else if (lookup == "CV5") return OutputId::CV5_OUTPUT;
	else if (lookup == "CV6") return OutputId::CV6_OUTPUT;
    else if (lookup == "CV7") return OutputId::CV7_OUTPUT;
	else if (lookup == "CV8") return OutputId::CV8_OUTPUT;
	else throw std::invalid_argument("received bad lookup value");
}

OutputId getIndividualGateOutputEnum(const std::string lookup) {
    if (lookup == "G1") return OutputId::G1_OUTPUT;
    else if (lookup == "G2") return OutputId::G2_OUTPUT;
    else if (lookup == "G3") return OutputId::G3_OUTPUT;
	else if (lookup == "G4") return OutputId::G4_OUTPUT;
    else if (lookup == "G5") return OutputId::G5_OUTPUT;
	else if (lookup == "G6") return OutputId::G6_OUTPUT;
    else if (lookup == "G7") return OutputId::G7_OUTPUT;
	else if (lookup == "G8") return OutputId::G8_OUTPUT;
	else throw std::invalid_argument("received bad lookup value");
}

LightId getLightEnum(const std::string lookup) {
    if (lookup == "STEP1") return LightId::STEP1;
    else if (lookup == "STEP2") return LightId::STEP2;
    else if (lookup == "STEP3") return LightId::STEP3;
	else if (lookup == "STEP4") return LightId::STEP4;
    else if (lookup == "STEP5") return LightId::STEP5;
	else if (lookup == "STEP6") return LightId::STEP6;
    else if (lookup == "STEP7") return LightId::STEP7;
	else if (lookup == "STEP8") return LightId::STEP8;
	else throw std::invalid_argument("received bad lookup value");
}

};


struct TripWidget : ModuleWidget {
	TripWidget(Trip* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Trip.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(12.18, 22.307)), module, Trip::OCTAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(31.561, 22.307)), module, Trip::QUANT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.942, 22.307)), module, Trip::MODULO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(70.323, 22.307)), module, Trip::SKIP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(89.704, 22.307)), module, Trip::REVERSAL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(109.085, 22.307)), module, Trip::EVOL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.41, 41.473)), module, Trip::TIMEFIT_PARAM));

		//timefit button
		addParam(createParamCentered<VCVLightButton<MediumSimpleLight<GreenLight>>>(mm2px(Vec(22.05, 39)), module, Trip::TIMEFITBUTTON_PARAM));
		//dirty light
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(22.05, 46.5)), module, Trip::DIRTY));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 57.928)), module, Trip::VOLTS1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 57.928)), module, Trip::VOLTS2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 57.928)), module, Trip::VOLTS3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 57.928)), module, Trip::VOLTS4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 57.928)), module, Trip::VOLTS5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 57.928)), module, Trip::VOLTS6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 57.928)), module, Trip::VOLTS7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 57.928)), module, Trip::VOLTS8_PARAM));

		//-3 on y
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 70.844)), module, Trip::SPACE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 70.844)), module, Trip::SPACE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 70.844)), module, Trip::SPACE3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 70.844)), module, Trip::SPACE4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 70.844)), module, Trip::SPACE5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 70.844)), module, Trip::SPACE6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 70.844)), module, Trip::SPACE7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 70.844)), module, Trip::SPACE8_PARAM));

		//-3 on y
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 83.76)), module, Trip::GATE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 83.76)), module, Trip::GATE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 83.76)), module, Trip::GATE3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 83.76)), module, Trip::GATE4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 83.76)), module, Trip::GATE5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 83.76)), module, Trip::GATE6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 83.76)), module, Trip::GATE7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 83.76)), module, Trip::GATE8_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(42.5, 41.473)), module, Trip::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(63.41, 41.473)), module, Trip::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(89.52, 41.473)), module, Trip::ALLCVOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(109.07, 41.473)), module, Trip::GATEOUT_OUTPUT));

		//add lights above the outputs
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(25.712, 94)), module, Trip::STEP1));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(38.079, 94)), module, Trip::STEP2));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(50.447, 94)), module, Trip::STEP3));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(62.814, 94)), module, Trip::STEP4));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(75.181, 94)), module, Trip::STEP5));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(87.549, 94)), module, Trip::STEP6));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(99.916, 94)), module, Trip::STEP7));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(112.283, 94)), module, Trip::STEP8));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.712, 114.107)), module, Trip::CV1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.079, 114.107)), module, Trip::CV2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(50.447, 114.107)), module, Trip::CV3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.814, 114.107)), module, Trip::CV4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.181, 114.107)), module, Trip::CV5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(87.549, 114.107)), module, Trip::CV6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(99.916, 114.107)), module, Trip::CV7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(112.283, 114.107)), module, Trip::CV8_OUTPUT));

		//INDIVIDUAL GATE OUTPUTS
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.712, 104.107)), module, Trip::G1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.079, 104.107)), module, Trip::G2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(50.447, 104.107)), module, Trip::G3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.814, 104.107)), module, Trip::G4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.181, 104.107)), module, Trip::G5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(87.549, 104.107)), module, Trip::G6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(99.916, 104.107)), module, Trip::G7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(112.283, 104.107)), module, Trip::G8_OUTPUT));
	}
};


Model* modelTrip = createModel<Trip, TripWidget>("Trip");