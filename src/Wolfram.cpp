// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
// NanoVG: https://github.com/memononen/nanovg





// TODO: add headers

// V2:
// - onRandomize.
// - Polyphonic engines. Replace 'Algo' CV with Slew CV
//	 or an expander which opens up all Algos at once. 

#include <string>
#include <atomic>
#include "Wolfram/ui.hpp"
#include "Wolfram/baseEngine.hpp"
#include "Wolfram/wolfEngine.hpp"
#include "Wolfram/lifeEngine.hpp"

static constexpr int NUM_ENGINES = 2;
static constexpr int NUM_MENU_PAGES = 4;

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
		X_CV_OUTPUT,
		X_PULSE_OUTPUT,
		Y_CV_OUTPUT,
		Y_PULSE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		MODE_LIGHT,
		X_CV_LIGHT,
		X_PULSE_LIGHT,
		Y_CV_LIGHT,
		Y_PULSE_LIGHT,
		TRIG_LIGHT,
		INJECT_LIGHT,
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
	std::array<BaseEngine*, NUM_ENGINES> engine{};
	std::array<EngineCoreParams, NUM_ENGINES> engineCoreParams{};
	std::array<EngineMenuParams, NUM_ENGINES> engineMenuParams{};
	static constexpr int engineDefault = 0;
	int engineSelect = engineDefault;
	float syncedEngineCv = 0;
	int engineIndex = 0;

	// UI
	static constexpr int ENGINE_TO_UI_UPDATE_INTERVAL = 512;
	static constexpr  float MINI_MENU_DISPLAY_TIME = 0.75f;
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
	bool slewX = true;
	bool slewY = true;
	bool sync = false;
	bool vcoMode = false;
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
		configOutput(X_CV_OUTPUT, "X CV");
		configOutput(X_PULSE_OUTPUT, "X Pulse");
		configOutput(Y_CV_OUTPUT, "Y CV");
		configOutput(Y_PULSE_OUTPUT, "Y Pulse");
		configLight(MODE_LIGHT, "Mode");
		configLight(X_CV_LIGHT, "X CV");
		configLight(X_PULSE_LIGHT, "X Pulse");
		configLight(Y_CV_LIGHT, "Y CV");
		configLight(Y_PULSE_LIGHT, "Y Pulse");
		configLight(TRIG_LIGHT, "Trigger");
		configLight(INJECT_LIGHT, "Inject");
		onSampleRateChange();

		// Engines
		engine[0] = &wolfEngine;
		engine[1] = &lifeEngine;
	}

	bool checkEngineIsNull(int index) {
		if (engine[index] == nullptr) {
			DEBUG("error: %d : NULL ENGINE", errno);
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
		// Convert to ms, if VCO mode (0 - 10ms) else (0 - 1000ms)
		slewValue = rack::clamp(newSlewSelect, 0, 100);
		float slewSkew = std::pow(slewValue * 0.01f, 2.f);
		float slew = vcoMode ? (slewSkew * 10.f) : (slewSkew * 1000.f);

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
		vcoMode = false;
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
		json_t* rootJ = json_object();

		// Save sequencer settings
		json_object_set_new(rootJ, "vco", json_boolean(vcoMode));
		json_object_set_new(rootJ, "sync", json_boolean(sync));
		json_object_set_new(rootJ, "slewX", json_boolean(slewX));
		json_object_set_new(rootJ, "slewY", json_boolean(slewY));
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

		json_t* vcoModeJ = json_object_get(rootJ, "vco");
		if (vcoModeJ)
			vcoMode = json_boolean_value(vcoModeJ);

		json_t* slewXJ = json_object_get(rootJ, "slewX");
		if (slewXJ)
			slewX = json_boolean_value(slewXJ);

		json_t* slewYJ = json_object_get(rootJ, "slewY");
		if (slewYJ)
			slewY = json_boolean_value(slewYJ);

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
				outputs[X_CV_OUTPUT].setVoltage(0.f);
				outputs[Y_CV_OUTPUT].setVoltage(0.f);
				outputs[X_PULSE_OUTPUT].setVoltage(0.f);
				outputs[Y_PULSE_OUTPUT].setVoltage(0.f);
				lights[MODE_LIGHT].setBrightness(0.f);
				lights[X_CV_LIGHT].setBrightness(0.f);
				lights[Y_CV_LIGHT].setBrightness(0.f);
				lights[X_PULSE_LIGHT].setBrightness(0.f);
				lights[Y_PULSE_LIGHT].setBrightness(0.f);
				lights[TRIG_LIGHT].setBrightness(0.f);
				lights[INJECT_LIGHT].setBrightness(0.f);
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
		if (vcoMode)// Zero crossing
			step = (stepVoltage > 0.f && prevStepVoltage <= 0.f) || (stepVoltage < 0.f && prevStepVoltage >= 0.f);
		else		// Pulse trigger	
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
		float encoderValue = params[SELECT_PARAM].getValue();
		float difference = encoderValue - prevEncoderValue;
		int delta = static_cast<int>(std::round(difference / ENCODER_INDENT));

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
		
		// OUTPUT
		float xCv = 0.f; 
		float yCv = 0.f;
		bool xBit = false;
		bool yBit = false;
		float modeLED = 0.f;
	
		for (int i = 0; i < NUM_ENGINES; i++)
			engine[i]->updateMenuParams(engineMenuParams[i]);

		engine[engineIndex]->process(engineCoreParams[engineIndex], &xCv, &yCv, &xBit, &yBit, &modeLED);

		float xCvSlew = slewLimiter[0].process(xCv);
		float yCvSlew = slewLimiter[1].process(yCv);
		xCv = slewX ? xCvSlew : xCv;
		yCv = slewY ? yCvSlew : yCv;

		float xAudio = xCv - 0.5;
		float yAudio = yCv - 0.5;
		dcFilter[0].process(xAudio);
		dcFilter[1].process(yAudio);
		xAudio = dcFilter[0].highpass();
		yAudio = dcFilter[1].highpass();

		// CV outputs - 0V to 10V or -5V to 5V in VCO mode (10Vpp)
		float xOut = vcoMode ? xAudio : xCv;
		float yOut = vcoMode ? yAudio : yCv;
		xOut = xOut * params[X_SCALE_PARAM].getValue() * 10.f;
		yOut = yOut * params[Y_SCALE_PARAM].getValue() * 10.f;
		outputs[X_CV_OUTPUT].setVoltage(xOut);
		outputs[Y_CV_OUTPUT].setVoltage(yOut);

		// Pulse outputs (0V to 10V)
		if (xBit)
			xPulse.trigger(vcoMode ? args.sampleTime : 1e-3f);

		if (yBit)
			yPulse.trigger(vcoMode ? args.sampleTime : 1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(modeLED, args.sampleTime);
		lights[X_CV_LIGHT].setBrightness(xOut * 0.1);
		lights[Y_CV_LIGHT].setBrightness(yOut * 0.1);
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
	Wolfram* module;
	UI ui;

	std::shared_ptr<Font> font;
	std::string fontPath;

	int cols = 8;
	int rows = 8;
	float padding = 1.f;
	float cellSize = 5.f;

	float cellPadding = 0;
	float widgetSize = 0;
	float fontSize = 0;

	Display(Wolfram* m, float yPos, float w, float size) {
		module = m;

		// Wiget parameters
		widgetSize = size;
		float screenSize = widgetSize - (padding * 2.f);
		fontSize = (screenSize / cols) * 2.f;
		cellPadding = (screenSize / cols);

		// Widget size
		box.pos = Vec((w * 0.5f) - (widgetSize * 0.5f), yPos);
		box.size = Vec(widgetSize, widgetSize);

		// Get font 
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/wolfram.otf"));
		font = APP->window->loadFont(fontPath);
		
		ui.init(padding, fontSize, cellPadding);
	}

	void drawDisplay(NVGcontext* vg, int layer) {
		EngineToUiLayer* engineLayer = nullptr;
		if (module)
			engineLayer = module->engineToUiLayerPtr.load(std::memory_order_acquire);

		int firstRow = 0;
		bool menuActive = module ? module->menuActive : false;
		bool miniMenuActive = module ? module->miniMenuActive : false;
		int engineSelect = module ? module->engineSelect : 0;
		int engineIndex = module ? module->engineIndex : 0;

		// Set colour
		NVGcolor colour = nvgRGB(0, 0, 0);
		if (module) {
			colour = (layer == 1) ?
				ui.getForegroundColour() :
				ui.getBackgroundColour();
		}
		else {
			colour = (layer == 1) ?
				nvgRGB(228, 7, 7) :
				nvgRGB(78, 12, 9);
		}

		// Menu
		if (menuActive || miniMenuActive) {
			// Set font
			if (!font)
				return;

			nvgFontSize(vg, fontSize);
			nvgFontFaceId(vg, font->handle);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		}

		if (menuActive) {
			// Main menu
			int pageNumber = module->pageNumber;

			// Text background
			if (layer == 0) {
				if ((pageNumber == 0) && (engineSelect == 0)) {
					// Special Wolf seed display
					int seed = engineLayer[0].seed;
					if (seed == 256) {
						ui.drawTextBg(vg, 2);
					}
					else {
						ui.drawWolfSeedDisplay(vg, layer, 
							static_cast<uint8_t>(seed));
					}
					ui.drawTextBg(vg, 0);
					ui.drawTextBg(vg, 1);
					ui.drawTextBg(vg, 3);
				}
				else {
					for (int i = 0; i < 4; i++)
						ui.drawTextBg(vg, i);
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
							engineLayer[engineSelect].engineLabel[i]));
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
						int seed = engineLayer[0].seed;
						if (seed == 256) {
							std::copy("RAND", "RAND" + 4, value);
						}
						else {
							std::copy("    ", "    " + 4, value);
							ui.drawWolfSeedDisplay(vg, layer,
								static_cast<uint8_t>(seed));
						}
					}
					else {
						std::copy(engineLayer[engineSelect].seedLabel,
							engineLayer[engineSelect].seedLabel + 4, value);
					}
					break;
				}
				case 1: {
					// Mode page
					std::copy("MODE", "MODE" + 4, title);
					std::copy(engineLayer[engineSelect].modeLabel,
						engineLayer[engineSelect].modeLabel + 4, value);
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
					std::copy(engineLayer[engineSelect].engineLabel,
						engineLayer[engineSelect].engineLabel + 4, value);
					break;
				}
				default: { break; }
				}
				ui.drawMenuText(vg, header, title, value, footer);
			}
			return;
		}
		else if (miniMenuActive) {
			// Mini menu
			firstRow = rows - 4;
			// Text background
			if (layer == 0) {
				for (int i = 0; i < 2; i++)
					ui.drawTextBg(vg, i);
			}
			// Text
			else if (layer == 1) {
				ui.drawText(vg, "RULE", 0);
				ui.drawText(vg, engineLayer[engineSelect].ruleSelectLabel, 1);
			}
		}

		// Matrix display
		uint64_t matrix = 0x81C326F48FCULL;
		if (module)
			matrix = engineLayer[engineIndex].display;

		nvgBeginPath(vg);
		nvgFillColor(vg, colour);

		for (int row = firstRow; row < 8; row++) {
			int rowInvert = 7 - row;

			uint8_t rowBits = (matrix >> (rowInvert << 3)) & 0xFF;

			if (layer == 0)
				rowBits = static_cast<uint8_t>(~rowBits);
			
			int i = 0;
			while (rowBits && (i < rows)) {
				int colInvert = __builtin_ctz(static_cast<unsigned>(rowBits));
				rowBits &= rowBits - 1;

				int col = 7 - colInvert;

				if (module) {
					ui.getCellPath(vg, col, row);
				}
				else {
					// Preview window drawing
					float pad = (cellPadding * 0.5f) + padding;
					nvgCircle(vg, (cellPadding * col) + pad,
						(cellPadding * row) + pad, 5.f);
				}
				i++;
			}
		}
		nvgFill(vg);
	}

	void draw(const DrawArgs& args) override {
		ui.displayStyleIndex = module ? module->displayStyleIndex : 0;
		ui.cellStyleIndex = module ? module->cellStyleIndex : 0;

		NVGcolor backgroundColour = ui.getScreenColour();

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, padding * 0.5f, padding * 0.5f,
			widgetSize - padding, widgetSize - padding, 2.f);
		nvgFillColor(args.vg, backgroundColour);
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, padding);
		nvgStrokeColor(args.vg, nvgRGB(16, 16, 16));
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		drawDisplay(args.vg, 0);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		ui.displayStyleIndex = module ? module->displayStyleIndex : 0;
		ui.cellStyleIndex = module ? module->cellStyleIndex : 0;
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

	// Custom lights
	template <typename TBase>
	struct LuckyLight : RectangleLight<TSvgLight<TBase>> {	// Cursed
		// Count Modula's custom light
		LuckyLight() {
			this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/RectangleLight.svg")));
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
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 99.852f)), module, Wolfram::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(19.05f, 99.852f)), module, Wolfram::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 99.852f)), module, Wolfram::Y_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.91f, 99.852f)), module, Wolfram::Y_PULSE_OUTPUT));
		// LEDs
		addChild(createLightCentered<DiagonalLuckyLight<RedLight>>(mm2px(Vec(53.34f, 47.767f)), module, Wolfram::MODE_LIGHT)); 
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(7.62f, 90.225f)), module, Wolfram::X_CV_LIGHT));		
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(19.05f, 90.225f)), module, Wolfram::X_PULSE_LIGHT));	
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(41.91f, 90.225f)), module, Wolfram::Y_PULSE_LIGHT));	
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(53.34f, 90.225f)), module, Wolfram::Y_CV_LIGHT));
		
		Display* display = new Display(module, mm2px(10.14f), box.size.x, mm2px(32.f));
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

		menu->addChild(createSubmenuItem("Slew", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("X", "", &module->slewX));
				menu->addChild(createBoolPtrMenuItem("Y", "", &module->slewY));
			}
		));

		menu->addChild(createBoolPtrMenuItem("Sync", "", &module->sync));
	
		menu->addChild(createBoolMenuItem("VCO", "",
			[=]() {
				return module->vcoMode;
			},
			[=](bool vco) {
				module->vcoMode = vco;
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