// Wolfram.cpp
// Part of the Modular Mooch Wolfram module (VCV Rack)
//
// GitHub: https://github.com/WesDaMooch/Modular-Mooch-VCV
// 
// Copyright (c) 2026 Wesley Lawrence Leggo-Morrell
// License: GPL-3.0-or-later
//
// TSKVFZSIFXEOZSVEDMJLTLTHBEDGGNDZTXOVELCGOHRIEXENGKTSGX
// UYEPQIOENITZSWXZOOLSSZNALTNNKKJMEASGIYISNGAZSULJGFTFLC
// LXOAVSSKUVAMYEUYOUEZKMAYJIWVRGLAIYMEOUKVGPAMYSMLTMDRNZ
// ZLJEGOAOEXGRBEEFLKEXKPZJIUHEIQOIEJYALXMZLXIVWWJEDLTHFO
// PTSBNPBISPPPWGOXKRPEAESROYZLFSAAALOQZLBKSGKMEX




// PLANNED UPDATES:
//
// TODO: figure out if the EngineToUiLayer is the best way to do share data.
//
// V1.1:
// - Replace Slew menu page with FX page.
// here an effect can be selected that is applied to the output,
// the amount of effect that is applied is contolled by the Scale params.
// See Audible Instruments Macro Oscillator 2 for multi-purpose knobs with dynamic tool tips.
// Effects:
// GAIN - (0 - 10Vpp).
// RISE - Slew rise (left = exponencial, right = linear).
// FALL - Slew fall.
// FOLD - Wavefolding (left = minus, right = plus)?
// - V1.1 Manual inculed Effects section. 
//
// V1.2:
// - onRandomize.
// - New Effects: 
// XMOD - some cross modulation with the other output (left = ?, right = ?).
//
// V2:
// - Polyphonic engines, multiple outputs
// or an expander which opens up all Algos at once. 
//
// New algos!
// A true Oscillator mode could use the algos to generate wavetables.

#include "Wolfram/algoEngine.hpp"
#include "Wolfram/wolfEngine.hpp"
#include "Wolfram/lifeEngine.hpp"
#include <string>
#include <atomic>

static constexpr int NUM_ENGINES = 2;
static constexpr int NUM_MENU_PAGES = 4;
static constexpr int NUM_DISPLAY_STYLES = 5;
static constexpr int NUM_CELL_STYLES = 2;

class SlewLimiter {
public:
	void setSlewAmountMs(float slew_ms, float sr) {
		if (sr <= 0) 
			sr = 1;

		slew_ms = rack::clamp(slew_ms, 1e-3f, 1000.f);
		slew = (1000.f / sr) / slew_ms;
	}

	void reset() { 
		y = 0; 
	}

	float process(float x) {
		// TODO: Could pass x as reference.
		y += rack::clamp(x - y, -slew, slew);
		return y;
	}

protected:
	float y = 0.f;
	float slew = 20.f;
};

struct Wolfram : Module {
	enum ParamId {
		SELECT_PARAM,
		MENU_PARAM,
		MODE_PARAM,
		LENGTH_PARAM,
		PROBABILITY_PARAM,
		OFFSET_PARAM,
		X_SCALE_PARAM,
		Y_SCALE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RESET_INPUT,
		PROBABILITY_CV_INPUT,
		OFFSET_CV_INPUT,
		RULE_CV_INPUT,
		ENGINE_CV_INPUT,
		TRIG_INPUT,
		INJECT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X_OUTPUT,
		X_PULSE_OUTPUT,
		Y_OUTPUT,
		Y_PULSE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		MODE_LIGHT,
		X_LIGHT,
		X_PULSE_LIGHT,
		Y_LIGHT,
		Y_PULSE_LIGHT,
		LIGHTS_LEN
	};

	struct EncoderParamQuantity : ParamQuantity {
		// Custom behaviour to display rule when hovering over Select encoder 
		std::string getDisplayValueString() override {
			std::string defaultString = "30";

			auto* m = dynamic_cast<Wolfram*>(module);
			if (!m)
				return defaultString;

			bool engineModulation = m->engineModulation;
			bool ruleModulation = m->ruleModulation;
			int engineSelect = m->engineSelect;
			int engineIndex = m->engineIndex;

			EngineToUiLayer* engineLayer = m->engineToUiLayerPtr.load(std::memory_order_acquire);

			std::string ruleSelectString = std::string(engineLayer[engineSelect].ruleSelectLabel);
			ruleSelectString.erase(0, ruleSelectString.find_first_not_of(" "));

			if (!engineModulation && !ruleModulation)
				return ruleSelectString;

			std::string ruleActiveString = std::string(engineLayer[engineIndex].ruleActiveLabel);
			ruleActiveString.erase(0, ruleActiveString.find_first_not_of(" "));

			return ruleSelectString + " ( " + ruleActiveString + " )";
		}
		
		// Suppress behaviour
		void setDisplayValueString(std::string s) override {}
	};

	struct LengthParamQuantity : ParamQuantity {
		// Custom behaviour to display sequence length when hovering over Length knob
		float getDisplayValue() override {
			auto* m = dynamic_cast<Wolfram*>(module);
			return m ? static_cast<float>(m->sequenceLength) : 8.f;
		}
		
		// Suppress behaviour
		void setDisplayValueString(std::string s) override {}
	};

	// Engine
	WolfEngine wolfEngine;
	LifeEngine lifeEngine;
	std::array<AlgoEngine*, NUM_ENGINES> engine{};
	std::array<EngineCoreParams, NUM_ENGINES> engineCoreParams{};
	std::array<EngineMenuParams, NUM_ENGINES> engineMenuParams{};
	static constexpr int engineDefault = 0;
	int engineSelect = engineDefault;
	float syncedEngineCv = 0;
	int engineIndex = 0;

	// UI
	static constexpr int ENGINE_TO_UI_UPDATE_INTERVAL = 512; // TODO: needs updating in srate change
	static constexpr float MINI_MENU_DISPLAY_TIME = 0.75f;
	std::array<EngineToUiLayer, NUM_ENGINES> engineToUiLayerA{};
	std::array<EngineToUiLayer, NUM_ENGINES> engineToUiLayerB{};
	std::atomic<EngineToUiLayer*> engineToUiLayerPtr{ engineToUiLayerA.data() };
	int pageCounter = 0;
	int pageNumber = 0;
	bool menuActive = false;
	int displayStyleIndex = 0;
	int cellStyleIndex = 0;
	bool miniMenuActive = false;

	// Select encoder
	static constexpr float ENCODER_INDENT = 1.f / 30.f;
	float prevEncoderValue = 0.f;
	bool encoderReset = false;

	// Parameters
	std::array<size_t, 9> sequenceLengths { 2, 3, 4, 6, 8, 12, 16, 32, 64 };
	size_t sequenceLength = 8;
	int slewValue = 0;
	bool sync = false;
	bool audioRateMode = false;
	bool ruleModulation = false;
	bool engineModulation = false;
	float prevStepVoltage = 0.f;

	// DSP
	int srate = 44100;	
	dsp::PulseGenerator xPulse, yPulse;
	dsp::SchmittTrigger trigTrigger, resetTrigger, posInjectTrigger, negInjectTrigger;
	dsp::BooleanTrigger menuTrigger, modeTrigger;
	dsp::Timer ruleDisplayTimer;
	dsp::RCFilter dcFilter[2];
	SlewLimiter slewLimiter[2];

	Wolfram() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(MENU_PARAM, "Menu");
		configButton(MODE_PARAM, "Mode");
		configParam<EncoderParamQuantity>(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Rule");
		configParam<LengthParamQuantity>(LENGTH_PARAM, 0.f, 8.f, 4.f, "Length");
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(PROBABILITY_PARAM, 0.f, 1.f, 1.f, "Probability", "%", 0.f, 100.f);
		paramQuantities[PROBABILITY_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, 0.f, 7.f, 4.f, "Offset", "", 0.f, 1.f, -4.f);
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(X_SCALE_PARAM, 0.f, 1.f, 0.5f, "X CV Scale", "V", 0.f, 10.f);
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 1.f, 0.5f, "Y CV Scale", "V", 0.f, 10.f);
		paramQuantities[Y_SCALE_PARAM]->displayPrecision = 3;
		configInput(RESET_INPUT, "Reset");
		configInput(PROBABILITY_CV_INPUT, "Probability CV");
		configInput(RULE_CV_INPUT, "Rule CV");
		configInput(ENGINE_CV_INPUT, "Algorithm CV");
		configInput(OFFSET_CV_INPUT, "Offset CV");
		configInput(TRIG_INPUT, "Trigger");
		configInput(INJECT_INPUT, "Inject");
		configOutput(X_OUTPUT, "X");
		configOutput(X_PULSE_OUTPUT, "X Pulse");
		configOutput(Y_OUTPUT, "Y");
		configOutput(Y_PULSE_OUTPUT, "Y Pulse");
		configLight(MODE_LIGHT, "Mode");
		configLight(X_LIGHT, "X");
		configLight(X_PULSE_LIGHT, "X Pulse");
		configLight(Y_LIGHT, "Y");
		configLight(Y_PULSE_LIGHT, "Y Pulse");

		// Load engines
		engine[0] = &wolfEngine;
		engine[1] = &lifeEngine;

		onSampleRateChange();
	}

	bool checkEngineIsNull(int index) {
		if (engine[index] == nullptr) {
			DEBUG("error: engine[%d] is NULL (ptr=%p)", index, (void*)engine[index]);
			return true;
		}
		return false;
	}

	void setEngine(int newEngineSelect, float newEngineCv = 0.f) {
		engineSelect = rack::clamp(newEngineSelect, 0, NUM_ENGINES - 1);
		int engineCv = std::round(newEngineCv * (NUM_ENGINES - 1));
		engineIndex = engineModulation ? engineCv : engineSelect;
	}

	void updateEngineToUiLayer() {
		static bool flip = false;
		EngineToUiLayer* writeState = flip ? engineToUiLayerA.data() : engineToUiLayerB.data();
		flip = !flip;

		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i))
				continue;
			
			for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++)
				writeState[i].matrixBuffer[j] = engine[i]->getBufferFrame(j);

			writeState[i].display = engine[i]->getBufferFrame(0, true);
			writeState[i].displaySave = engine[i]->getBufferFrame(0, false, true);
			writeState[i].readHead = engine[i]->getReadHead();
			writeState[i].writeHead = engine[i]->getWriteHead();
			writeState[i].ruleSelect = engine[i]->getRuleSelect();
			writeState[i].seed = engine[i]->getSeed();
			writeState[i].mode = engine[i]->getMode();
			engine[i]->getEngineLabel(writeState[i].engineLabel);
			engine[i]->getRuleActiveLabel(writeState[i].ruleActiveLabel);
			engine[i]->getRuleSelectLabel(writeState[i].ruleSelectLabel);
			engine[i]->getSeedLabel(writeState[i].seedLabel);
			engine[i]->getModeLabel(writeState[i].modeLabel);
			
		}
		engineToUiLayerPtr.store(writeState, std::memory_order_release);
	}

	void setSlew(int newSlewSelect) {
		// Skew slewParam (0 - 100%) -> (0 - 1)
		// Convert to ms, if Audio Rate mode (0 - 10ms) else (0 - 1000ms)
		slewValue = rack::clamp(newSlewSelect, 0, 100);
		float slewSkew = std::pow(slewValue * 0.01f, 2.f);
		float slew = audioRateMode ? (slewSkew * 10.f) : (slewSkew * 1000.f);

		for (int i = 0; i < 2; i++)
			slewLimiter[i].setSlewAmountMs(slew, srate);
	}

	void onSampleRateChange() override {
		srate = APP->engine->getSampleRate();

		// Set DC blocker to ~10Hz,
		// Set Slew time (ms)
		setSlew(slewValue);

		for (int i = 0; i < 2; i++) {
			dcFilter[i].setCutoffFreq(10.f / srate);
			dcFilter[i].reset();
			slewLimiter[i].reset();
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);

		sync = false;
		audioRateMode = false;
		menuActive = false;
		miniMenuActive = false;
		pageCounter = 0; 
		setSlew(0);
		setEngine(0);
		
		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i))
				continue;
			engine[i]->reset();
		}
		
		displayStyleIndex = 0;
		cellStyleIndex = 0;
	}
	
	json_t* dataToJson() override {
		// Engine buffers & display use uint64_t, yet the patch json uses 'int'.
		// There seems to be no issue at the moment with this conflict.
		// TODO: Could the 64-bit buffer int by encoded as a string?

		json_t* rootJ = json_object();

		// Save sequencer settings
		json_object_set_new(rootJ, "audioRateMode", json_boolean(audioRateMode));
		json_object_set_new(rootJ, "sync", json_boolean(sync));
		json_object_set_new(rootJ, "slewValue", json_integer(slewValue));

		// Save engine selection
		json_object_set_new(rootJ, "engine", json_integer(engineSelect));

		// Save UI settings
		json_object_set_new(rootJ, "displayStyle", json_integer(displayStyleIndex));
		json_object_set_new(rootJ, "cellStyle", json_integer(cellStyleIndex));

		// Save engine specifics
		EngineToUiLayer* engineLayer = engineToUiLayerPtr.load(std::memory_order_acquire);

		json_t* readHeadsJ = json_array();
		json_t* writeHeadsJ = json_array();
		json_t* rulesJ = json_array();
		json_t* seedsJ = json_array();
		json_t* modesJ = json_array();
		json_t* displaysJ = json_array();
		json_t* buffersJ = json_array();

		for (int i = 0; i < NUM_ENGINES; i++) {
			json_array_append_new(rulesJ, json_integer(engineLayer[i].ruleSelect));
			json_array_append_new(seedsJ, json_integer(engineLayer[i].seed));
			json_array_append_new(modesJ, json_integer(engineLayer[i].mode));
			json_array_append_new(readHeadsJ, json_integer(engineLayer[i].readHead));
			json_array_append_new(writeHeadsJ, json_integer(engineLayer[i].writeHead));
			json_array_append_new(displaysJ, json_integer(engineLayer[i].displaySave));

			json_t* rowJ = json_array();
			for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++)
				json_array_append_new(rowJ, json_integer(engineLayer[i].matrixBuffer[j]));
			json_array_append_new(buffersJ, rowJ);
		}
		
		json_object_set_new(rootJ, "readHeads", readHeadsJ);
		json_object_set_new(rootJ, "writeHeads", writeHeadsJ);
		json_object_set_new(rootJ, "rules", rulesJ);
		json_object_set_new(rootJ, "seeds", seedsJ);
		json_object_set_new(rootJ, "modes", modesJ);
		json_object_set_new(rootJ, "displays", displaysJ);
		json_object_set_new(rootJ, "buffers", buffersJ);
		
		return rootJ;
	}
	
	void dataFromJson(json_t* rootJ) override {
		// Load sequencer settings
		json_t* syncJ = json_object_get(rootJ, "sync");
		if (syncJ)
			sync = json_boolean_value(syncJ);

		json_t* audioRateModeJ = json_object_get(rootJ, "audioRateMode");
		if (audioRateModeJ)
			audioRateMode = json_boolean_value(audioRateModeJ);

		json_t* slewValueJ = json_object_get(rootJ, "slewValue");
		if (slewValueJ)
			setSlew(json_integer_value(slewValueJ));

		// Load engine selection
		json_t* engineSelectJ = json_object_get(rootJ, "engine");
		if (engineSelectJ)
			setEngine(json_integer_value(engineSelectJ));

		json_t* displayStyleJ = json_object_get(rootJ, "displayStyle");
		if (displayStyleJ)
			displayStyleIndex = rack::clamp(json_integer_value(displayStyleJ), 0, NUM_DISPLAY_STYLES - 1);

		json_t* cellStyleJ = json_object_get(rootJ, "cellStyle");
		if (cellStyleJ)
			cellStyleIndex = rack::clamp(json_integer_value(cellStyleJ), 0, NUM_CELL_STYLES - 1);

		// Load engine specifics
		json_t* rulesJ = json_object_get(rootJ, "rules");
		json_t* seedsJ = json_object_get(rootJ, "seeds");
		json_t* modesJ = json_object_get(rootJ, "modes");
		json_t* readHeadsJ = json_object_get(rootJ, "readHeads");
		json_t* writeHeadsJ = json_object_get(rootJ, "writeHeads");
		json_t* buffersJ = json_object_get(rootJ, "buffers");
		json_t* displaysJ = json_object_get(rootJ, "displays");

		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i))
				continue;

			if (readHeadsJ) {
				json_t* valueJ = json_array_get(readHeadsJ, i);

				if (valueJ)
					engine[i]->setReadHead(static_cast<size_t>(json_integer_value(valueJ)));
			}
			if (writeHeadsJ) {
				json_t* valueJ = json_array_get(writeHeadsJ, i);

				if (valueJ)
					engine[i]->setWriteHead(static_cast<size_t>(json_integer_value(valueJ)));
			}
			if (rulesJ) {
				json_t* valueJ = json_array_get(rulesJ, i);

				if (valueJ)
					engine[i]->setRuleSelect(json_integer_value(valueJ));
			}
			if (seedsJ) {
				json_t* valueJ = json_array_get(seedsJ, i);

				if (valueJ)
					engine[i]->setSeed(json_integer_value(valueJ));
			}
			if (modesJ) {
				json_t* valueJ = json_array_get(modesJ, i);

				if (valueJ)
					engine[i]->setMode(json_integer_value(valueJ));
			}
			if (displaysJ) {
				json_t* valueJ = json_array_get(displaysJ, i);

				if (valueJ)
					engine[i]->setBufferFrame(static_cast<uint64_t>(json_integer_value(valueJ)), 0, true);
			}

			// Load Buffers
			if (buffersJ) {
				json_t* rowJ = json_array_get(buffersJ, i);

				if (rowJ) {
					for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++) {
						json_t* valueJ = json_array_get(rowJ, j);

						if (valueJ)
							engine[i]->setBufferFrame(static_cast<uint64_t>(json_integer_value(valueJ)), j);
					}
				}
			}
			engine[i]->updateDisplay(false);
		}
	}
	
	void process(const ProcessArgs& args) override {
		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i)) {
				outputs[X_OUTPUT].setVoltage(0.f);
				outputs[Y_OUTPUT].setVoltage(0.f);
				outputs[X_PULSE_OUTPUT].setVoltage(0.f);
				outputs[Y_PULSE_OUTPUT].setVoltage(0.f);
				lights[MODE_LIGHT].setBrightness(0.f);
				lights[X_LIGHT].setBrightness(0.f);
				lights[Y_LIGHT].setBrightness(0.f);
				lights[X_PULSE_LIGHT].setBrightness(0.f);
				lights[Y_PULSE_LIGHT].setBrightness(0.f);
				return;
			}
		}

		for (int i = 0; i < NUM_ENGINES; i++) {
			// Clear menu parameter's delta and reset
			for (int j = 0; j < EngineMenuParams::DELTA_LEN; j++) {
				engineMenuParams[i].menuDelta[j] = 0;
				engineMenuParams[i].menuReset[j] = false;
			}
		}

		// Step
		bool step = false;
		float stepVoltage = inputs[TRIG_INPUT].getVoltage();
		if (audioRateMode)	// Zero crossing
			step = (stepVoltage > 0.f && prevStepVoltage <= 0.f) || (stepVoltage < 0.f && prevStepVoltage >= 0.f);
		else				// Pulse trigger	
			step = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f);
		prevStepVoltage = stepVoltage;

		engineModulation = inputs[ENGINE_CV_INPUT].isConnected();
		float newEngineCv = rack::clamp(inputs[ENGINE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		if (sync && step)
			syncedEngineCv = newEngineCv;
		setEngine(engineSelect, sync ? syncedEngineCv : newEngineCv);

		engineCoreParams[engineIndex].step = step;
		ruleModulation = inputs[RULE_CV_INPUT].isConnected();
		engineCoreParams[engineIndex].ruleCv = rack::clamp(inputs[RULE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		engineCoreParams[engineIndex].reset = resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f);
		engineCoreParams[engineIndex].sync = sync;

		size_t lengthIndex = rack::clamp(static_cast<int>(params[LENGTH_PARAM].getValue()), 0, 8);
		sequenceLength = sequenceLengths[lengthIndex];
		engineCoreParams[engineIndex].length = sequenceLength;

		float probabilityCv = inputs[PROBABILITY_CV_INPUT].getVoltage() * 0.1f;
		engineCoreParams[engineIndex].probability = rack::clamp(params[PROBABILITY_PARAM].getValue() + probabilityCv, 0.f, 1.f);

		int offsetCv = std::round(inputs[OFFSET_CV_INPUT].getVoltage() * 0.7f);
		int offsetParam = std::round(params[OFFSET_PARAM].getValue());
		engineCoreParams[engineIndex].offset = rack::clamp(offsetParam + offsetCv, 0, 7);

		// Menu and mode buttons
		if (menuTrigger.process(params[MENU_PARAM].getValue()))
			menuActive = !menuActive;

		if (modeTrigger.process(params[MODE_PARAM].getValue())) {
			if (menuActive)
				pageCounter++;
			else
				engineMenuParams[engineSelect].menuDelta[EngineMenuParams::MODE_DELTA] += 1;
		}

		pageNumber = pageCounter % NUM_MENU_PAGES;
		if (pageNumber < 0)
			pageNumber += NUM_MENU_PAGES;

		// Encoder
		engineCoreParams[engineIndex].miniMenuChanged = false;
		float encoderDifference = params[SELECT_PARAM].getValue() - prevEncoderValue;
		int delta = std::round(encoderDifference / ENCODER_INDENT);

		if ((delta != 0) || encoderReset) {
			prevEncoderValue += delta * ENCODER_INDENT;

			if (menuActive) {
				switch (pageNumber) {
					case 0: {
						// Seed page
						engineMenuParams[engineSelect].menuDelta[EngineMenuParams::SEED_DELTA] = delta;
						engineMenuParams[engineSelect].menuReset[EngineMenuParams::SEED_RESET] = encoderReset;
						break;
					}
					case 1: {
						// Mode page
						engineMenuParams[engineSelect].menuDelta[EngineMenuParams::MODE_DELTA] = delta;
						engineMenuParams[engineSelect].menuReset[EngineMenuParams::MODE_RESET] = encoderReset;
						break;
					}
					case 2: {
						// Slew page
						setSlew(encoderReset ? 0 : (slewValue + delta));
						break;
					}
					case 3: {
						// Algo page
						if (encoderReset)
							engineSelect = engineDefault;
						else
							engineSelect = (engineSelect + delta + NUM_ENGINES) % NUM_ENGINES;
						break;
					}
					default: { break; }
				}
			}
			else {
				// Mini menu
				if (!engineModulation)
					engineCoreParams[engineIndex].miniMenuChanged = true;

				engineMenuParams[engineSelect].menuDelta[EngineMenuParams::RULE_DELTA] = delta;
				engineMenuParams[engineSelect].menuReset[EngineMenuParams::RULE_RESET] = miniMenuActive ? encoderReset : false;

				if (!encoderReset || (miniMenuActive && encoderReset)) {
					miniMenuActive = true;
					ruleDisplayTimer.reset();
				}
			}
			encoderReset = false;
		}
		
		// Inject
		int injectState = 0;
		if (posInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), 0.1f, 2.f))
			injectState = 1;
		else if (negInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), -2.f, -0.1f))
			injectState = -1;
		engineCoreParams[engineIndex].inject = injectState;
		
		// OUTPUTS
		float xCv = 0.f; 
		float yCv = 0.f;
		bool xBit = false;
		bool yBit = false;
		float modeLED = 0.f;
	
		for (int i = 0; i < NUM_ENGINES; i++)
			engine[i]->updateMenuParams(engineMenuParams[i]);

		engine[engineIndex]->process(engineCoreParams[engineIndex], &xCv, &yCv, &xBit, &yBit, &modeLED);

		xCv = slewLimiter[0].process(xCv);
		yCv = slewLimiter[1].process(yCv);

		float xAudio = xCv - 0.5;
		float yAudio = yCv - 0.5;
		dcFilter[0].process(xAudio);
		dcFilter[1].process(yAudio);
		xAudio = dcFilter[0].highpass();
		yAudio = dcFilter[1].highpass();

		// CV outputs - 0V to 10V or -5V to 5V in Audio Rate Mode (10Vpp)
		float xOut = audioRateMode ? xAudio : xCv;
		float yOut = audioRateMode ? yAudio : yCv;
		xOut = xOut * params[X_SCALE_PARAM].getValue() * 10.f;
		yOut = yOut * params[Y_SCALE_PARAM].getValue() * 10.f;
		outputs[X_OUTPUT].setVoltage(xOut);
		outputs[Y_OUTPUT].setVoltage(yOut);

		// Pulse outputs (0V to 10V)
		if (xBit)
			xPulse.trigger(audioRateMode ? args.sampleTime : 1e-3f);

		if (yBit)
			yPulse.trigger(audioRateMode ? args.sampleTime : 1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(modeLED, args.sampleTime);
		lights[X_LIGHT].setBrightness(xOut * 0.1);
		lights[Y_LIGHT].setBrightness(yOut * 0.1);
		lights[X_PULSE_LIGHT].setBrightnessSmooth(xGate, args.sampleTime);
		lights[Y_PULSE_LIGHT].setBrightnessSmooth(yGate, args.sampleTime);
		
		// Mini menu display
		if (miniMenuActive && (ruleDisplayTimer.process(args.sampleTime) >= MINI_MENU_DISPLAY_TIME))
			miniMenuActive = false;

		if (((args.frame + this->id) % ENGINE_TO_UI_UPDATE_INTERVAL) == 0)
			updateEngineToUiLayer();
	}
};

struct Display : TransparentWidget {
	// TODO: see ZZC Clock for glowing display.
	
	Wolfram* module = nullptr;

	static constexpr int NUM_COLS = 8;
	static constexpr int NUM_ROWS = 8;
	static constexpr int NUM_CELLS = NUM_COLS * NUM_ROWS;
	static constexpr int NUM_TEXT_CHARS = 4;
	// General
	static constexpr float widgetSize = 94.49f; //mm2px(32.f);
	static constexpr float padding = 1.f;
	// Cells
	static constexpr float circleCellSize = 5.f;
	static constexpr float cellPadding = ((widgetSize - (padding * 2.f)) / NUM_COLS);
	static constexpr float circleCellPadding = (cellPadding * 0.5f) + padding;

	static constexpr float roundedSquareCellSize = 10.f;
	static constexpr float roundedSquareCellBevel = 1.f;
	// Text
	static constexpr float fontSize = cellPadding * 2.f;
	static constexpr float textBgSize = fontSize - 2.f;
	static constexpr float textBgPadding = (fontSize * 0.5f) - (textBgSize * 0.5f) + padding;
	static constexpr float textBgBevel = 3.f;
	// Wolf
	static constexpr float wolfSeedSize = (fontSize * 0.5f) - 2.f;
	static constexpr float wolfSeedLineWidth = 0.5f;
	static constexpr float wolfSeedBevel = 3.f;

	int displayStyleIndex = 0;
	int cellStyleIndex = 0;

	std::shared_ptr<Font> font;

	std::array<Vec, NUM_CELLS> cellCirclePos{};
	std::array<Vec, NUM_CELLS> cellRoundedSquarePos{};
	std::array<Vec, NUM_TEXT_CHARS> textPos{};
	std::array<Vec, 16> textBgPos{};
	std::array<rack::math::Vec, NUM_COLS> wolfSeedPos{};

	// Display styles
	std::array<std::array<NVGcolor, 3>, NUM_DISPLAY_STYLES> displayStyle{ {
		{ nvgRGB(228, 7, 7),		nvgRGB(78, 12, 9),		nvgRGB(58, 16, 19) },		// Redrick
		{ nvgRGB(205, 254, 254),	nvgRGB(39, 70, 153),	nvgRGB(37, 59, 99) },		// Oled
		{ SCHEME_YELLOW,			SCHEME_DARK_GRAY,		SCHEME_DARK_GRAY },			// Rack  
		{ nvgRGB(210, 255, 0),		nvgRGB(42, 47, 37),		nvgRGB(42, 47, 37) },		// Lamp 
		{ nvgRGB(255, 255, 255),	nvgRGB(0, 0, 0),		nvgRGB(0, 0, 0) },			// Mono
	} };

	Display(Wolfram* m, float yPos, float moduleWidth) {
		module = m;

		box.pos = Vec((moduleWidth * 0.5f) - (widgetSize * 0.5f), yPos);
		box.size = Vec(widgetSize, widgetSize);

		// Text positions
		for (int i = 0; i < NUM_TEXT_CHARS; i++) {
			textPos[i].x = padding;
			textPos[i].y = padding + (fontSize * i);
		}

		// Text background positions 
		for (int col = 0; col < NUM_TEXT_CHARS; col++) {
			for (int row = 0; row < NUM_TEXT_CHARS; row++) {
				int i = row * NUM_TEXT_CHARS + col;
				textBgPos[i].x = (fontSize * col) + textBgPadding;
				textBgPos[i].y = (fontSize * row) + textBgPadding;
			}
		}

		// Cell positions
		float roundedSquareCellPadding = (cellPadding * 0.5f) - (roundedSquareCellSize * 0.5f) + padding;
		for (int col = 0; col < NUM_COLS; col++) {
			for (int row = 0; row < NUM_ROWS; row++) {
				int i = row * NUM_COLS + col;
				cellCirclePos[i].x = (cellPadding * col) + circleCellPadding;
				cellCirclePos[i].y = (cellPadding * row) + circleCellPadding;
				cellRoundedSquarePos[i].x = (cellPadding * col) + roundedSquareCellPadding;
				cellRoundedSquarePos[i].y = (cellPadding * row) + roundedSquareCellPadding;

			}
		}

		// Wolf seed display
		float halfFontSize = fontSize * 0.5f;
		float wolfSeedPadding = (halfFontSize * 0.5f) - (wolfSeedSize * 0.5f) + padding;
		for (int col = 0; col < NUM_COLS; col++) {
			wolfSeedPos[col].x = (halfFontSize * col) + wolfSeedPadding;
			wolfSeedPos[col].y = (halfFontSize * 4.f) + wolfSeedPadding;
		}
	}

	void ensureFont() {
		if (!font || (font->handle < 0))
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/wolfram.ttf"));
	}

	void syncStyle() {
		if (!module)
			return;

		displayStyleIndex = module->displayStyleIndex;
		cellStyleIndex = module->cellStyleIndex;
	}

	// Getters
	const NVGcolor& getForegroundColour() const {
		int styleIndex = rack::clamp(displayStyleIndex, 0, NUM_DISPLAY_STYLES - 1);
		return displayStyle[styleIndex][0];
	}

	const NVGcolor& getBackgroundColour() const {
		int styleIndex = rack::clamp(displayStyleIndex, 0, NUM_DISPLAY_STYLES - 1);
		return displayStyle[styleIndex][1];
	}

	const NVGcolor& getScreenColour() const {
		int styleIndex = rack::clamp(displayStyleIndex, 0, NUM_DISPLAY_STYLES - 1);
		return displayStyle[styleIndex][2];
	}

	void getCellPath(NVGcontext* vg, int col, int row) {
		// Must call nvgBeginPath before and nvgFill after this function! 
		int i = row * NUM_COLS + col;

		if (cellStyleIndex == 1) {
			// Pixel - Rounded square
			nvgRoundedRect(vg, cellRoundedSquarePos[i].x, cellRoundedSquarePos[i].y,
				roundedSquareCellSize, roundedSquareCellSize, roundedSquareCellBevel);
		}
		else {
			// LED - Circle	
			nvgCircle(vg, cellCirclePos[i].x, cellCirclePos[i].y, circleCellSize);
		}
	}

	// Drawers
	void drawText(NVGcontext* vg, const char* text, int row) {
		// Draw a four character row of text
		if ((row < 0) || (row >= NUM_TEXT_CHARS))
			return;

		nvgFillColor(vg, getForegroundColour());
		nvgText(vg, textPos[row].x, textPos[row].y, text, nullptr);
	}

	void drawMenuText(NVGcontext* vg, 
		const char* line1,
		const char* line2, 
		const char* line3, 
		const char* line4) {
		// Helper for drawing four lines of menu text
		drawText(vg, line1, 0);
		drawText(vg, line2, 1);
		drawText(vg, line3, 2);
		drawText(vg, line4, 3);
	}

	void drawTextBg(NVGcontext* vg, int row) {
		// Draw one row of four square text character backgrounds
		if ((row < 0) || (row >= NUM_TEXT_CHARS))
			return;

		nvgBeginPath(vg);
		nvgFillColor(vg, getBackgroundColour());
		for (int col = 0; col < NUM_TEXT_CHARS; col++) {
			int i = row * NUM_TEXT_CHARS + col;
			nvgRoundedRect(vg, textBgPos[i].x, textBgPos[i].y,
				textBgSize, textBgSize, textBgBevel);
		}
		nvgFill(vg);
	}

	void drawWolfSeedDisplay(NVGcontext* vg, int layer, uint8_t inputSeed) {
		if (layer == 1) {
			// Lines
			nvgStrokeColor(vg, getForegroundColour());
			nvgBeginPath(vg);
			for (int col = 0; col < NUM_COLS; col++) {
				if ((col >= 1) && (col <= 7)) {
					// TODO: move to init
					nvgMoveTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y - 1);
					nvgLineTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y + 1);

					nvgMoveTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) - 1);
					nvgLineTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) + 1);
				}
			}
			nvgStrokeWidth(vg, wolfSeedLineWidth);
			nvgStroke(vg);
		}
		// Display 8-bit value
		nvgFillColor(vg, layer ? getForegroundColour() : getBackgroundColour());
		nvgBeginPath(vg);
		for (int col = 0; col < NUM_COLS; col++) {
			bool seedCell = (inputSeed >> (7 - col)) & 1;

			if ((layer && !seedCell) || (!layer && seedCell))
				continue;

			nvgRoundedRect(vg, wolfSeedPos[col].x, wolfSeedPos[col].y,
				wolfSeedSize, (wolfSeedSize * 2.f) + 2.f, wolfSeedBevel);
		}
		nvgFill(vg);
	}

	void drawMenu(NVGcontext* vg, EngineToUiLayer* eLayer, 
		bool menu, bool miniMenu, 
		int &firstRow, int layer) {

		if (!module || !eLayer)
			return;
		
		int engineSelect = module->engineSelect;

		if (layer == 1) {
			ensureFont();
			nvgFontSize(vg, fontSize);
			nvgFontFaceId(vg, font->handle);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		}

		if (menu) {
			// Main menu
			int pageNumber = module->pageNumber;

			// Text background
			if (layer == 0) {
				if ((pageNumber == 0) && (engineSelect == 0)) {
					// Special Wolf seed display
					int seed = eLayer[0].seed;
					if (seed == 256) {
						drawTextBg(vg, 2);
					}
					else {
						drawWolfSeedDisplay(vg, layer,
							static_cast<uint8_t>(seed));
					}
					drawTextBg(vg, 0);
					drawTextBg(vg, 1);
					drawTextBg(vg, 3);
				}
				else {
					for (int i = 0; i < 4; i++)
						drawTextBg(vg, i);
				}
			}
			// Text
			else if (layer == 1) {
				char header[5] = "menu";
				char title[5]{};
				char value[5]{};
				char footer[5] = "<#@>";

				if (pageNumber < 2) {
					char engineLabel[5]{};
					for (int i = 0; i < 4; i++) {
						engineLabel[i] = std::tolower(static_cast<unsigned char>(
							eLayer[engineSelect].engineLabel[i]));
					}
					engineLabel[4] = '\0';
					std::copy(engineLabel, engineLabel + 4, header);
				}

				// Default menu
				switch (pageNumber) {
				case 0: {
					// Seed page
					std::copy("SEED", "SEED" + 4, title);
					if (engineSelect == 0) {
						// Special Wolf seed display
						int seed = eLayer[0].seed;
						if (seed == 256) {
							std::copy("RAND", "RAND" + 4, value);
						}
						else {
							std::copy("    ", "    " + 4, value);
							drawWolfSeedDisplay(vg, layer,
								static_cast<uint8_t>(seed));
						}
					}
					else {
						std::copy(eLayer[engineSelect].seedLabel,
							eLayer[engineSelect].seedLabel + 4, value);
					}
					break;
				}
				case 1: {
					// Mode page
					std::copy("MODE", "MODE" + 4, title);
					std::copy(eLayer[engineSelect].modeLabel,
						eLayer[engineSelect].modeLabel + 4, value);
					break;
				}
				case 2: {
					// Slew page
					std::copy("SLEW", "SLEW" + 4, title);
					char slewString[5]{};
					snprintf(slewString, sizeof(slewString), "%3d%%", module->slewValue);
					std::copy(slewString, slewString + 4, value);
					break;
				}
				case 3: {
					// Algo page
					std::copy("ALGO", "ALGO" + 4, title);
					std::copy(eLayer[engineSelect].engineLabel,
						eLayer[engineSelect].engineLabel + 4, value);
					break;
				}
				default: { break; }
				}
				drawMenuText(vg, header, title, value, footer);
			}
		}
		else if (miniMenu) {
			// Mini menu
			firstRow = NUM_ROWS - 4;
			// Text background
			if (layer == 0) {
				for (int i = 0; i < 2; i++)
					drawTextBg(vg, i);
			}
			// Text
			else if (layer == 1) {
				drawText(vg, "RULE", 0);
				drawText(vg, eLayer[engineSelect].ruleSelectLabel, 1);
			}
		}
	}

	void drawMatrix(NVGcontext* vg, EngineToUiLayer* eLayer, 
		int firstRow, int layer) {

		uint64_t matrix = 0x81C326F48FCULL;
		if (module && eLayer) {
			int engineIndex = module->engineIndex;
			matrix = eLayer[engineIndex].display;
		}

		nvgBeginPath(vg);
		nvgFillColor(vg, layer ? getForegroundColour() : getBackgroundColour());

		for (int row = firstRow; row < NUM_ROWS; row++) {
			int rowInvert = 7 - row;

			uint8_t rowBits = (matrix >> (rowInvert << 3)) & 0xFF;

			if (layer == 0)
				rowBits = static_cast<uint8_t>(~rowBits);

			while (rowBits) {
				int colInvert = rack::math::log2(rowBits & -rowBits);
				rowBits &= rowBits - 1;

				int col = 7 - colInvert;

				if (module) {
					getCellPath(vg, col, row);
				}
				else {
					// Preview window
					nvgCircle(vg, (cellPadding * col) + circleCellPadding,
						(cellPadding * row) + circleCellPadding, circleCellSize);
				}
			}
		}
		nvgFill(vg);
	}

	void drawDisplay(NVGcontext* vg, int layer) {
		syncStyle();

		EngineToUiLayer* engineLayer = module ?
			module->engineToUiLayerPtr.load(std::memory_order_acquire):
			nullptr;

		int firstRow = 0;
		bool menuActive = module ? module->menuActive : false;
		bool miniMenuActive = module ? module->miniMenuActive : false;

		// Backgound
		if (layer == 0) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, padding * 0.5f, padding * 0.5f,
				widgetSize - padding, widgetSize - padding, 2.f);
			nvgFillColor(vg, getScreenColour());
			nvgFill(vg);
			// Outline
			nvgStrokeWidth(vg, padding);
			nvgStrokeColor(vg, nvgRGB(16, 16, 16));
			nvgStroke(vg);
			nvgClosePath(vg);
		}

		if (menuActive || miniMenuActive)
			drawMenu(vg, engineLayer, menuActive, miniMenuActive, firstRow, layer);

		if (!menuActive)
			drawMatrix(vg, engineLayer, firstRow, layer);
	}

	void draw(const DrawArgs& args) override {
		drawDisplay(args.vg, 0);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		drawDisplay(args.vg, layer);
		Widget::drawLayer(args, layer);
	}
};

struct WolframModuleWidget : ModuleWidget {
	// Custom knobs & dials
	struct LengthKnob : M1900hBlackKnob {
		LengthKnob() {
			minAngle = -0.75f * M_PI;
			maxAngle = 0.5f * M_PI;
		}
	};

	struct ProbabilityKnob : M1900hBlackKnob {
		ProbabilityKnob() {
			minAngle = -0.75f * M_PI;
			maxAngle = 0.75f * M_PI;
		}
	};

	struct SelectEncoder : M1900hBlackEncoder {
		void onDoubleClick(const DoubleClickEvent& e) override {
			// Reset Select encoder to default of current selection
			auto* m = static_cast<Wolfram*>(module);
			m->encoderReset = true;
		}
	};

	// Custom light from Count Modula
	template <typename TBase>
	struct LuckyLight : RectangleLight<TSvgLight<TBase>> {	// Cursed
		LuckyLight() {
			this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/RectangleLuckyLight.svg")));
		}

		void drawHalo(const DrawArgs& args) override {
			// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
			if (args.fb)
				return;

			const float halo = settings::haloBrightness;
			if (halo == 0.f)
				return;

			// If light is off, rendering the halo gives no effect
			if (this->color.a == 0.f)
				return;
		
			float br = 30.0;	// Blur radius
			float cr = 5.0;		// Corner radius

			nvgBeginPath(args.vg);
			nvgRect(args.vg, -br, -br, this->box.size.x + 2 * br, this->box.size.y + 2 * br);
			NVGcolor icol = color::mult(TBase::color, halo);
			NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
			nvgFillPaint(args.vg, nvgBoxGradient(args.vg, 0, 0, this->box.size.x, this->box.size.y, cr, br, icol, ocol));
			nvgFill(args.vg);
		}
	};

	template <typename TBase>
	struct DiagonalLuckyLight : LuckyLight<TBase> {
		widget::TransformWidget* tw;
		float rotation = -M_PI / 4.f;	// -45 degree rotation
		
		DiagonalLuckyLight() {
			// Rotate SVG
			float rotationBoxSize = 5.4f;
			Vec fbSize = mm2px(Vec(rotationBoxSize, rotationBoxSize));
			this->fb->box.size = fbSize;
			this->fb->box.pos = this->box.size.div(2.f).minus(fbSize.div(2.f));

			tw = new widget::TransformWidget;
			this->fb->removeChild(this->sw);
			tw->box.size = fbSize;
			this->fb->addChild(tw);
			tw->addChild(this->sw);

			// Recenter SVG 
			this->sw->box.pos = fbSize.minus(this->sw->box.size).div(2.f);
			Vec center = fbSize.div(2.f);

			tw->identity();
			tw->translate(center);
			tw->rotate(rotation);
			tw->translate(-center);
		}
		
		void rotate(const DrawArgs& args) {
			nvgTranslate(args.vg, this->box.size.x / 2.f, this->box.size.y / 2.f);
			nvgRotate(args.vg, rotation);
			nvgTranslate(args.vg, -this->box.size.x / 2.f, -this->box.size.y / 2.f);
		}

		void drawBackground(const DrawArgs& args) override {
			nvgSave(args.vg);
			rotate(args);
			LuckyLight<TBase>::drawBackground(args);
			nvgRestore(args.vg);
		}

		void drawLight(const DrawArgs& args) override {
			nvgSave(args.vg);
			rotate(args);
			LuckyLight<TBase>::drawLight(args);
			nvgRestore(args.vg);
		}
	};

	WolframModuleWidget(Wolfram* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/wolfram.svg")));

		// Srews
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// Buttons
		addParam(createParamCentered<CKD6>(mm2px(Vec(7.62f, 38.14f)), module, Wolfram::MENU_PARAM));
		addParam(createParamCentered<CKD6>(mm2px(Vec(53.34f, 38.14f)), module, Wolfram::MODE_PARAM));
		// Dials
		addParam(createParamCentered<SelectEncoder>(mm2px(Vec(53.34f, 22.14f)), module, Wolfram::SELECT_PARAM));
		addParam(createParamCentered<LengthKnob>(mm2px(Vec(15.24f, 61.369f)), module, Wolfram::LENGTH_PARAM));
		addParam(createParamCentered<ProbabilityKnob>(mm2px(Vec(45.72f, 61.369f)), module, Wolfram::PROBABILITY_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48f, 80.597f)), module, Wolfram::OFFSET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62f, 80.597f)), module, Wolfram::X_SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(53.34f, 80.597f)), module, Wolfram::Y_SCALE_PARAM));
		// Inputs
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 22.14f)), module, Wolfram::RESET_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(30.48f, 99.852f)), module, Wolfram::OFFSET_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(30.48f, 114.852f)), module, Wolfram::PROBABILITY_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(41.91f, 114.852f)), module, Wolfram::RULE_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(53.34f, 114.852f)), module, Wolfram::ENGINE_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 114.852f)), module, Wolfram::TRIG_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(19.05f, 114.852f)), module, Wolfram::INJECT_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 99.852f)), module, Wolfram::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(19.05f, 99.852f)), module, Wolfram::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 99.852f)), module, Wolfram::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.91f, 99.852f)), module, Wolfram::Y_PULSE_OUTPUT));
		// LEDs
		addChild(createLightCentered<DiagonalLuckyLight<RedLight>>(mm2px(Vec(53.34f, 47.767f)), module, Wolfram::MODE_LIGHT)); 
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(7.62f, 90.225f)), module, Wolfram::X_LIGHT));		
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(19.05f, 90.225f)), module, Wolfram::X_PULSE_LIGHT));	
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(41.91f, 90.225f)), module, Wolfram::Y_PULSE_LIGHT));	
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(53.34f, 90.225f)), module, Wolfram::Y_LIGHT));
		
		Display* display = new Display(module, mm2px(10.14f), box.size.x);
		addChild(display);
	}
	
	void appendContextMenu(Menu* menu) override {
		Wolfram* module = dynamic_cast<Wolfram*>(this->module);
		
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Algorithm",
			{ "Wolf", "Life" },
			[=]() {
				return module->engineSelect;
			},
			[=](int i) {
				module->engineSelect = i;
			}
		));

		menu->addChild(createBoolPtrMenuItem("Sync", "", &module->sync));
	
		menu->addChild(createBoolMenuItem("Audio Rate", "",
			[=]() {
				return module->audioRateMode;
			},
			[=](bool audioRateMode) {
				module->audioRateMode = audioRateMode;
				module->onSampleRateChange();
			}
		));

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Display",
			{ "Redrick", "OLED", "Rack", "Lamp", "Mono"},
			[=]() {
				return module->displayStyleIndex;
			},
			[=](int i) {
				module->displayStyleIndex = i;
			} 
		));

		menu->addChild(createIndexSubmenuItem("Cell",
			{ "LED", "Pixel"},
			[=]() {
				return module->cellStyleIndex;
			},
			[=](int i) {
				module->cellStyleIndex = i;
			}
		));
	}
};

Model* modelWolfram = createModel<Wolfram, WolframModuleWidget>("Wolfram");