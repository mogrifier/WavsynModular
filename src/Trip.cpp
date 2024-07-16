#include "plugin.hpp"

using namespace math;

struct Trip : Module {
	const int STEPS = 8;
	int mode = 0;
	float volts = 0.f;
	float voltsFraction = 0.f;
	double voltsInteger = 0.f;
	float space = 0.f;
	float gate = 0.f;
	float skipChance = 0.f;
	float skipped = false;
	std::string step = "";
	int currentStep = 1;
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
	bool reset = false;

	//for enum use
	std::string VOLTS = "VOLTS";
	std::string SPACE = "SPACE";
	std::string GATE = "GATE";
	std::string CV = "CV";
	std::string STEP = "STEP";

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

	enum ParamId {
		OCTAVE_PARAM,
		MODULO_PARAM,
		SKIP_PARAM,
		REVERSAL_PARAM,
		LENGTH_PARAM,
		MODE_PARAM,
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
		TRIGGER_OUTPUT,
		CV1_OUTPUT,
		CV2_OUTPUT,
		CV3_OUTPUT,
		CV4_OUTPUT,
		CV5_OUTPUT,
		CV6_OUTPUT,
		CV7_OUTPUT,
		CV8_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		STEP1, STEP2, STEP3, STEP4, STEP5, STEP6, STEP7, STEP8, LIGHTS_LEN
	};

	Trip() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		//On 0, the random setting is OFF. Greater than zero it represents the chance 
		//for each step for the random event to happen, modifiying the step or the sequence

		//restrict values to whole numbers
		configSwitch(OCTAVE_PARAM, -3.f, 3.f, 0.f, "Add to VOLTS for each step", {"-3", "-2", "-1", "0", "1", "2", "3"});

		//causes a shift in the pattern by changing where it starts- maybe a switch with settings 1-8?
		configSwitch(MODULO_PARAM, 1, 8, 1, "Starting Step:", {"1", "2", "3", "4", "5", "6", "7", "8"});

		//skip a step (gate out is zero). Same chance for every step.
		configParam(SKIP_PARAM, 0.f, 1.f, 0.f, "Chance to skip a step");

		//start a 8 instead of 1
		configParam(REVERSAL_PARAM, 0.f, 1.f, 0.f, "Chance to reverse sequence");

		//maybe a switch with settings 1-8?. Adjust SPACE so the times still add up to 100%
		configSwitch(LENGTH_PARAM, 1, 8, 8, "Sequence Length:", {"1", "2", "3", "4", "5", "6", "7", "8"});

		//multiple switch positions- does quantization of VOLTS or not
		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Quantization:", {"None", "12-Tone", "Quartertone"});

		//CV output is simply called VOLTS since it cud be a pitch or a control signal but both are VOLTS
		configParam(VOLTS1_PARAM, 0.f, 10.f, 2.167f, "Set the Step CV output");
		configParam(VOLTS2_PARAM, 0.f, 10.f, 5.f, "Set the Step CV output");
		configParam(VOLTS3_PARAM, 0.f, 10.f, 2.33f, "Set the Step CV output");
		configParam(VOLTS4_PARAM, 0.f, 10.f, 2.75f, "Set the Step CV output");
		configParam(VOLTS5_PARAM, 0.f, 10.f, 2.5f, "Set the Step CV output");
		configParam(VOLTS6_PARAM, 0.f, 10.f, 5.f, "Set the Step CV output");
		configParam(VOLTS7_PARAM, 0.f, 10.f, 5.f, "Set the Step CV output");
		configParam(VOLTS8_PARAM, 0.f, 10.f, 5.f, "Set the Step CV output");

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
		configParam(GATE1_PARAM, 0.f, 1.f, 0.6f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE2_PARAM, 0.f, 1.f, 0.f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE3_PARAM, 0.f, 1.f, 0.35f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE4_PARAM, 0.f, 1.f, 0.35f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE5_PARAM, 0.f, 1.f, 0.15f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE6_PARAM, 0.f, 1.f, 0.f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE7_PARAM, 0.f, 1.f, 0.f, "Gate duration", "%", 0.f, 100.f);
		configParam(GATE8_PARAM, 0.f, 1.f, 0.f, "Gate duration", "%", 0.f, 100.f);

		configOutput(CV1_OUTPUT, "Step 1 Individual CV Out");
		configOutput(CV2_OUTPUT, "Step 2 Individual CV Out");
		configOutput(CV3_OUTPUT, "Step 3 Individual CV Out");
		configOutput(CV4_OUTPUT, "Step 4 Individual CV Out");
		configOutput(CV5_OUTPUT, "Step 5 Individual CV Out");
		configOutput(CV6_OUTPUT, "Step 6 Individual CV Out");
		configOutput(CV7_OUTPUT, "Step 7 Individual CV Out");
		configOutput(CV8_OUTPUT, "Step 8 Individual CV Out");

		configInput(CLOCK_INPUT, "External clock for sequencer tempo");
		configInput(RESET_INPUT, "Reset the sequencer at Step 1 on a trigger");
		configOutput(ALLCVOUT_OUTPUT, "All steps' output");
		configOutput(GATEOUT_OUTPUT, "Gate signal for each step");
		configOutput(TRIGGER_OUTPUT, "Trigger output at start of each step");	
	}

	void process(const ProcessArgs& args) override {
		
		if (firstRun) {
		//one-time setup code
			int i = 0;
			try {
				//set the space settings to values from the UI (moved here since not working in the constructor properly)
				for (i = 0; i < STEPS; i++) {
					spaceSetting[i] = params[getSpaceEnum(SPACE + std::to_string(i + 1))].getValue();
					//DEBUG("setting %f", spaceSetting[i]);
				}
			}
			catch(const std::invalid_argument& e) {
				//one lookup value (i or j) is bad. All the try/catch blocks are just a developer aid, BTW.
				DEBUG("lookup is bad = %i", i);
				//by returning, the module will do nothing, signaling a problem
				return;
			}
			firstRun = false;
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

		//moved declaration outside loop so it is valid in catch statement
		int j = 0;
		spaceTotal = 0.f;
		//check if the space knobs have changed position and adjust the others
		for (int i = 0; i < STEPS; i++) {
			//check each setting against the prior setting
			try {
				//step is 1-base, but arrays are 0-based
				stepSpace = params[getSpaceEnum(SPACE + std::to_string(i + 1))].getValue();
				//FIXME this always triggers as soon as the delta > 0.03 implying UI thread is separate.
				//believe the fix is an event handling approach- start stop drag, I think.

				//skip a step if the value is really small. can't compare floats to zero
				if (stepSpace < EPSILON) {
					//DEBUG("skipping step %i", i + 1);
					continue;
				}
				//triggers if the space knob has moved
				if (abs(stepSpace - spaceSetting[i]) > .0005f) {
					//DEBUG("setting %f", spaceSetting[i]);
					//DEBUG("stepSpace %f", stepSpace);
					//the knob has moved since the last call to process beyond a deadband of .1 so change other values
					for (j = 0; j < STEPS; j++) {
						//modify all knob settings except knob i AND any knobs set to zero (since user wants zero)
						//need polarity and magnitude of the change; weight the change
						//the spaceInc value is computed for the knob that changed position. It is a small share that is
						//multiplied by the amount of each step's space and applied to it.
						
						spaceInc = (stepSpace - spaceSetting[i]) / (1 - spaceSetting[i]);
						//DEBUG("spaceInc = %f", spaceInc);
						if (j != i) {
							//polarity of spaceInc is important
							params[getSpaceEnum(SPACE + std::to_string(j + 1))].setValue(spaceSetting[j] - 
								(spaceInc * spaceSetting[j]));
							//DEBUG("applying %f to step %i",  spaceInc * spaceSetting[j], j + 1);
							//protect against negative values
							if (params[getSpaceEnum(SPACE + std::to_string(j + 1))].getValue() < 0.f)
							{
								params[getSpaceEnum(SPACE + std::to_string(j + 1))].setValue(0.f);
								//DEBUG("step = %i set to zero", j + 1);
							}
						}
						spaceTotal += params[getSpaceEnum(SPACE + std::to_string(j + 1))].getValue();
						//update the spaceSetting[] with the new values
						spaceSetting[j] = params[getSpaceEnum(SPACE + std::to_string(j + 1))].getValue();
						//DEBUG("step = %i settings = %f", j + 1, spaceSetting[j]);
					}
					//DEBUG("******** spaceTotal =  %f", spaceTotal);
					//note that there is only ONE mouse so only ONE knob can change in a call to process at a time
					break;
				}
			}
			catch( const std::invalid_argument& e ) {
				//one lookup value (i or j) is bad. All the try/catch blocks are just a developer aid, BTW.
				DEBUG("lookup is bad; either %i or %i", i, j);
				//by returning, the module will do nothing, signaling a problem
				return;
			}
		}



		//check value- should always be 1 or so close it doesn't matter. Reason is you lose bar sync if not near 1.
		
		if (spaceTotal > 1) {
			float maxSpace = 0.f;
			int maxSpaceStep = 0;
			//trim off some space from the steps with the most. If the rest of the code is accurate this should not do much
			for (int i = 0; i < STEPS; i++) {
				if (spaceSetting[i] > maxSpace)
				{
					maxSpace = spaceSetting[i];
					//remember, step is + 1 
					maxSpaceStep = i + 1;
				}
			}
			
			try {
				//trim the max space 
				float adjusted = spaceSetting[maxSpaceStep - 1] - (spaceTotal - 1);
				//DEBUG("trimming step %i by %f", maxSpaceStep, adjusted);
				params[getSpaceEnum(SPACE + std::to_string(maxSpaceStep))].setValue(adjusted);
			}
			catch (const std::invalid_argument& e) {
				//one lookup value (i or j) is bad. All the try/catch blocks are just a developer aid, BTW.
				//DEBUG("lookup is bad= %i", maxSpaceStep);
				//by returning, the module will do nothing, signaling a problem
				return;
			}
		}


		
		count++;
		stepCount++;

		/*
		use ticks and BPM and SPACE and GATE to figure out what is the current step and how to set volts and gate

		Algorithm
		compute tempo
		Q: do I reset the sequencer start if tempo changes?? I think not. just adjust going forward for new timing
		based on SPACE, determine what is current STEP
		read voltage and gate for current STEP
		set the associated output to the voltage (also set allCV)
		Q: do I care if connected or not? I don't think I care. Just set the value.
		use GATE to figure out if the gate is off or on at this point. Gate is a % of the SPACE for the step
		*/
		

		if (inputs[RESET_INPUT].isConnected()) {
			//check for reset signal
			if (trigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
				//trigger received, reset step the value specified by the MODULO param
				reset = true;
			}
			else {
				//no reset signal
				reset = false;
			}
		}

		if (reset){
			count = 0;
			//back to the beginning
			currentStep = params[MODULO_PARAM].getValue();
		}


		//set the current step output to its output and to all cv ouput
		octave = params[OCTAVE_PARAM].getValue();
		mode = params[MODE_PARAM].getValue();
		try {
			volts = params[getVoltsEnum(VOLTS + std::to_string(currentStep))].getValue();
		}
		catch( const std::invalid_argument& e ) {
    			// do stuff with exception... 
				DEBUG("volts lookup is bad = %i", currentStep);
				//by returning, the module will do nothing, signaling a problem
				return;
		}
		//also sets voltInteger
		voltsFraction = modf(volts, &voltsInteger);

		//DEBUG("int = %f fraction = %f", voltsInteger, voltsFraction);
		//DEBUG("mode = %d", mode);
		//switch on mode to perform quantization of output (or not)
		switch (mode) {
			case 0:
			//do nothing since volts is already accurate
			//DEBUG("mode = %d", 0);
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
			//add in the octave value bias but only if all will STAY IN RANGE 0-10. Don't clamp. Reduce all.
		 	if (getMaxVolts() + octave > 10.f) {
				//out of range. Reduce octave setting appropriately.
				if (getMaxVolts() + octave - 1 > 10.f) {
				//out of range. Reduce octave setting appropriately.
				 	if (getMaxVolts() + octave - 2 > 10.f) {
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

			/* Min Voltage check is NOT needed.
			-3v volts in the VCV VCO is ok. In fact, sounds good. I thought a 0-10v range was correct. Turns out
			different VCO's have different inputs. If -3 is bad for you, there are level adjusters.
			*/
			//check min values and adjust octave setting as needed
			/*
			if (getMinVolts() + octave < 0.f) {
				//out of range. Reduce octave setting appropriately.
				if (getMinVolts() + octave + 1 < 0.f) {
				//out of range. Reduce octave setting appropriately.
				 	if (getMinVolts() + octave + 2 < 0.f) {
						//set ocatve to zero since -3 is too high for settings (we know octave is -3 at this point)
						params[OCTAVE_PARAM].setValue(0.f);
					}
					else {
						params[OCTAVE_PARAM].setValue(octave + 2.f);
					}
				}
				else {
					params[OCTAVE_PARAM].setValue(octave + 1.f);
				}
			}
			*/


			outputs[getOuputEnum(CV + std::to_string(currentStep))].setVoltage(volts + octave);
			outputs[ALLCVOUT_OUTPUT].setVoltage(volts + octave);

			//calculate duration of currentStep and total space for the step in number of samples
			stepSpace = params[getSpaceEnum(SPACE + std::to_string(currentStep))].getValue() * ticks;
			//step duration is a fraction of the total allocated space for the step
			stepDuration = params[getGateEnum(GATE + std::to_string(currentStep))].getValue() * stepSpace;
		}
		catch( const std::invalid_argument& e ) {
    			// do stuff with exception... 
				DEBUG("lookup is bad = %i", currentStep);
				//by returning, the module will do nothing, signaling a problem
				return;
		}


		/* this logic also controls if the Gate signal is off or on. */
		
		/*
		I think this has to be reworked since the count is not making sense relative to a single step.
		Need to track count for each step maybe, plus a total count. 
		*/

		if (!skipped && stepCount <= stepDuration)
		{
			//what is happening here is that the output "sticks" on the last value until a new value is created. 
			//keep gate on
			outputs[GATEOUT_OUTPUT].setVoltage(10.f, 0);
			outputs[GATEOUT_OUTPUT].setChannels(1);
			//turn on the light for the step
			try {
				lights[getLightEnum(STEP + std::to_string(currentStep))].setBrightness(1.f);
			}
			catch( const std::invalid_argument& e ) {
    			// do stuff with exception... 
				DEBUG("lights lookup is bad = %i", currentStep);
			}
			return;
		}
		else if ((stepCount > stepDuration) && (stepCount < stepSpace)) 
		{
			//turn off gate output
			outputs[GATEOUT_OUTPUT].setVoltage(0.f, 0);
			outputs[GATEOUT_OUTPUT].setChannels(1);
			//turn off the light
						try {
				lights[getLightEnum(STEP + std::to_string(currentStep))].setBrightness(0.f);
			}
			catch( const std::invalid_argument& e ) {
    			// do stuff with exception... 
				DEBUG("lights lookup is bad = %i", currentStep);
			}
			return;
		}
		
		if (stepCount >= stepSpace){
			stepCount = 0;
			currentStep++;
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

		if (currentStep > params[LENGTH_PARAM].getValue()) {
			//FIXME this needs to respect the MOD setting
			currentStep = 1;
		}

	}


//get max voltage for all steps being used (space > 0)
float getMaxVolts() {
	float max = 0.f;
	float current = 0.f;
	for (int i = 1; i <= STEPS; i++) {
		current = params[getVoltsEnum(VOLTS + std::to_string(i))].getValue();
		//ignore steps that are skipped (spac = 0)
		if (current > max && params[getSpaceEnum(SPACE + std::to_string(i))].getValue() > 0.f)
		{
			max = current;
		}
	}
	return max;
}

//get min voltage for all steps being used (space > 0)
float getMinVolts() {
	float min = 10.f;
	float current = 0.f;
	for (int i = 1; i <= STEPS; i++) {
		current = params[getVoltsEnum(VOLTS + std::to_string(i))].getValue();
		//ignore steps that are skipped (spac = 0)
		if (current < min && params[getSpaceEnum(SPACE + std::to_string(i))].getValue() > 0.f)
		{
			min = current;
		}
	}
	return min;
}

int getStep(){
	//for the number of samples since step 1 figure out current step (could still be 1)
	//take the modulo against entire bar. sampleNumber goes from 0 to ticks.

	int step = 1;
	float space = 0.f;
	//need to look at each step's SPACE value to figure out the current step. 
	for (int i = 1; i <= params[LENGTH_PARAM].getValue(); i++) {
		std::string test = SPACE + std::to_string(i);
		space += params[getSpaceEnum(SPACE + std::to_string(i))].getValue();
		DEBUG("space = %f", space);
		//check where count falls 
		if (count <= space * ticks) {
			//means the current count falls in the latest step
			step = i;
			break;
		}
	}

	DEBUG("currentStep = %i", step);
	return step;
}

//quantize- 
float quantize12tone(float v) {
	//iterate the 12 tone array and find closest value. 
	float min = 1.f;
	float qVal = 0.f;
	for (int i = 0; i < 13; i++){
		if (abs(v - twelveTone[i]) < min) {
			min = abs(v - twelveTone[i]);
			//DEBUG("mode = %f", 12.f);
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

OutputId getOuputEnum(const std::string lookup) {
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
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36.406, 22.307)), module, Trip::MODULO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(60.632, 22.307)), module, Trip::SKIP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(84.859, 22.307)), module, Trip::REVERSAL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(109.085, 22.307)), module, Trip::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.18, 43.524)), module, Trip::MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 59.928)), module, Trip::VOLTS1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 59.928)), module, Trip::VOLTS2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 59.928)), module, Trip::VOLTS3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 59.928)), module, Trip::VOLTS4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 59.928)), module, Trip::VOLTS5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 59.928)), module, Trip::VOLTS6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 59.928)), module, Trip::VOLTS7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 59.928)), module, Trip::VOLTS8_PARAM));

		//-3 on y
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 75.844)), module, Trip::SPACE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 75.844)), module, Trip::SPACE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 75.844)), module, Trip::SPACE3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 75.844)), module, Trip::SPACE4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 75.844)), module, Trip::SPACE5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 75.844)), module, Trip::SPACE6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 75.844)), module, Trip::SPACE7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 75.844)), module, Trip::SPACE8_PARAM));

		//-3 on y
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.712, 91.76)), module, Trip::GATE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(38.079, 91.76)), module, Trip::GATE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.446, 91.76)), module, Trip::GATE3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(62.814, 91.76)), module, Trip::GATE4_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(75.181, 91.76)), module, Trip::GATE5_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(87.548, 91.76)), module, Trip::GATE6_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(99.916, 91.76)), module, Trip::GATE7_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(112.283, 91.76)), module, Trip::GATE8_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.57, 41.473)), module, Trip::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.333, 41.473)), module, Trip::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(76.005, 41.473)), module, Trip::ALLCVOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(93.997, 41.473)), module, Trip::GATEOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(111.989, 41.473)), module, Trip::TRIGGER_OUTPUT));

		//add lights above the outputs
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(25.712, 102)), module, Trip::STEP1));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(38.079, 102)), module, Trip::STEP2));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(50.447, 102)), module, Trip::STEP3));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(62.814, 102)), module, Trip::STEP4));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(75.181, 102)), module, Trip::STEP5));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(87.549, 102)), module, Trip::STEP6));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(99.916, 102)), module, Trip::STEP7));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(112.283, 102)), module, Trip::STEP8));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.712, 112.107)), module, Trip::CV1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.079, 112.107)), module, Trip::CV2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(50.447, 112.107)), module, Trip::CV3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(62.814, 112.107)), module, Trip::CV4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(75.181, 112.107)), module, Trip::CV5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(87.549, 112.107)), module, Trip::CV6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(99.916, 112.107)), module, Trip::CV7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(112.283, 112.107)), module, Trip::CV8_OUTPUT));
	}
};


Model* modelTrip = createModel<Trip, TripWidget>("Trip");