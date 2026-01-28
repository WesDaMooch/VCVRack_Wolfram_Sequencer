// Make with: export RACK_DIR=/home/wes-l/Rack-SDK

// Dev Mode:
//	cd "/c/Program Files/VCV/Rack2Free"
//	./Rack - d - u "C:\Users\wes-l\AppData\Local\Rack2"

// - Debug in terminal - 
//
// make clean
// make DEBUG=1 install
// objdump --syms plugin.dll | grep    // checking
// cd "/c/Program Files/VCV/Rack2Free"
// gdb ./Rack.exe  // use:  gdb --args "/c/Program Files/VCV/Rack2Free/Rack.exe"
// 
// set pagination off
// catch throw
// catch catch
// 
// run
// bt
// 
//  - Inspect variables -
// frame 0
// print someVariable
// 
// quit

// - Set breakpoints -
// break WolframModule::process
//  - or -
// break src/Wolfram.cpp:214
// run


// The dreaded crash
// No dataToJson - crashed on 2919.578
// Running Wolf, default settings
// No custom encoder text - 1244.122 
// No custom length text - 2613.784
// No dataFromJson - 75.442
// No display - 150.421
// No custom ligths - 205.329
// No processEncoder - 5157.470
// No contexMenu - NO crash up to 2186.513
// No contexMenu - NO crash up to 12477.420 
// With contextMenu - NO crash up to 11274.010
// With processEncoer - NO crash uo to 2326.813 


// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg

// TODO: maybe this
// Therefore, modules with a CLOCK and RESET input, or similar variants, should ignore CLOCK triggers up to 1 ms 
// after receiving a RESET trigger.You can use dsp::Timer for keeping track of time.
// Or do what SEQ3 does?

// TODO: onRandomize
// TODO: json saves to signed 64 bit number, i need unsigned (uint64_t), maybe not a issue
// TODO: algo select context menu
// TODO: clean up all all custom svgs
// TODO: redrick off cells darker?

// BUGS
// TODO: on wolf vcomode, seed not init or something
// TODO: not setting wolf rand seed?

// Update
// - multi channel triggers each engine...
// - multi channel outs for each engine...

#include <string>
#include <atomic>
#include "ui.hpp"
#include "baseEngine.hpp"
#include "wolfEngine.hpp"
#include "lifeEngine.hpp"

static constexpr int NUM_ENGINES = 2; 
static constexpr int NUM_MENU_PAGES_DEFAULT = 4;
static constexpr int NUM_MENU_PAGES_ENGINE_MOD = (NUM_ENGINES * 3) + 2;

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

struct WolframModule : Module {
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
		/*
		std::string getDisplayValueString() override {
			std::string defaultString = "30";

			auto* m = dynamic_cast<WolframModule*>(module);
			if (!m)
				return defaultString;

			bool engineModulation = m->engineModulation;
			bool ruleModulation = m->ruleModulation;
			int engineSelect = m->engineSelect;
			int engineActive = m->engineIndex;

			EngineStateSnapshot* state = m->engineStateSnapshotPtr.load(std::memory_order_acquire);
			//if (!readState)
			//	return defaultString;

			std::string ruleSelectString = std::string(state[engineActive].ruleSelectLabel);
			ruleSelectString.erase(0, ruleSelectString.find_first_not_of(" "));

			if (!engineModulation && !ruleModulation)
				return ruleSelectString;

			std::string ruleActiveString = std::string(state[engineActive].ruleActiveLabel);
			ruleActiveString.erase(0, ruleActiveString.find_first_not_of(" "));

			if (engineModulation) {
				std::string ruleEngineSelectString = std::string(state[engineSelect].ruleActiveLabel);
				ruleEngineSelectString.erase(0, ruleEngineSelectString.find_first_not_of(" "));
				return ruleEngineSelectString + " ( " + ruleActiveString + " )";
			}
				
			if (ruleModulation)
				return ruleSelectString + " ( " + ruleActiveString + " )";

			return defaultString;
		}
		*/

		// Suppress behaviour
		void setDisplayValueString(std::string s) override {}
	};

	struct LengthParamQuantity : ParamQuantity {
		// Custom behaviour to display sequence length when hovering over Length knob
		/*
		float getDisplayValue() override {
			auto* m = dynamic_cast<WolframModule*>(module);
			return m ? m->sequenceLength : 8;
		}
		*/
		// Suppress behaviour
		void setDisplayValueString(std::string s) override {}
	};

	// Engine
	WolfEngine wolfEngine;
	LifeEngine lifeEngine;
	std::array<BaseEngine*, NUM_ENGINES> engines{};
	static constexpr int engineDefault = 0;
	int engineSelect = engineDefault;
	int engineCv = 0;
	int engineCvPending = 0;
	int engineIndex = 0;

	// UI
	static constexpr int UI_UPDATE_INTERVAL = 512;
	std::array<EngineStateSnapshot, NUM_ENGINES> engineStateSnapshotA{};
	std::array<EngineStateSnapshot, NUM_ENGINES> engineStateSnapshotB{};
	std::atomic<EngineStateSnapshot*> engineStateSnapshotPtr{ engineStateSnapshotA.data() };
	int pageCounter = 0;
	int pageNumber = 0;
	bool menuActive = false;
	int displayStyleIndex = 0;
	int cellStyleIndex = 0;

	// Parameters
	int sequenceLength = 8;
	std::array<int, 9> sequenceLengths { 2, 3, 4, 6, 8, 12, 16, 32, 64 };
	float probability = 1.f;
	int slewValue = 0;
	bool slewX = true;
	bool slewY = true;
	bool sync = false;
	bool vcoMode = false;
	int offset = 0;
	int offsetPending = 0;
	bool resetPending = false;
	bool seedPushPending = false;
	int injectPending = 0;
	bool gen = false;
	bool genPending = false;
	float prevTrigVoltage = 0.f;
	
	// Select encoder
	static constexpr float encoderIndent = 1.f / 30.f;
	float prevEncoderValue = 0.f;
	bool encoderReset = false;
	
	// Display
	float miniMenuDisplayTime = 0.75f;
	bool miniMenuActive = false;
	bool ruleModulation = false;
	bool engineModulation = false;

	// DSP
	int srate = 44100;	
	dsp::PulseGenerator xPulse, yPulse;
	dsp::SchmittTrigger trigTrigger, resetTrigger, posInjectTrigger, negInjectTrigger;
	dsp::BooleanTrigger menuTrigger, modeTrigger;
	dsp::Timer ruleDisplayTimer;
	dsp::RCFilter dcFilter[2];
	SlewLimiter slewLimiter[2];

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(MENU_PARAM, "Menu");
		configButton(MODE_PARAM, "Mode");
		configParam<EncoderParamQuantity>(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Rule");
		configParam<LengthParamQuantity>(LENGTH_PARAM, 0.f, 8.f, 4.f, "Length");
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(PROBABILITY_PARAM, 0.f, 1.f, 1.f, "Probability", "%", 0.f, 100.f);
		paramQuantities[PROBABILITY_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
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
		engines[0] = &wolfEngine;
		engines[1] = &lifeEngine;

		// Init 
		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i))
				continue;

			engines[i]->updateRule(0, true);
			engines[i]->updateSeed(0, true);
			engines[i]->updateMode(0, true);
			engines[i]->updateMatrix(sequenceLength, offset, false);
		}
	}

	bool checkEngineIsNull(int index) {
		if (engines[index] == nullptr) {
			DEBUG("error: %d : NULL ENGINE", errno);
			return true;
		}
		return false;
	}

	void updateEngine() {
		int newEngineIndex = rack::clamp(engineSelect + engineCv, 0, NUM_ENGINES - 1);
		bool engineChanged = (engineIndex != newEngineIndex);
		engineIndex = newEngineIndex;

		if (checkEngineIsNull(engineIndex))
			return;

		if (engineChanged)
			engines[engineIndex]->updateMatrix(sequenceLength, offset, false);
	}

	void updateEngineSelect(int newSelect) {
		newSelect = rack::clamp(newSelect, 0, NUM_ENGINES - 1);

		if (engineSelect != newSelect) {
			engineSelect = newSelect;
			updateEngine();
		}
	}

	void updateEngineStateSnapshot() {
		// UI
		static bool flip = false;
		EngineStateSnapshot* writeState = flip ? engineStateSnapshotA.data() : engineStateSnapshotB.data();
		flip = !flip;

		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i))
				continue;

			for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++)
				writeState[i].matrixBuffer[j] = engines[i]->getBufferFrame(j);

			writeState[i].display = engines[i]->getBufferFrame(-1);
			writeState[i].readHead = engines[i]->getReadHead();
			writeState[i].writeHead = engines[i]->getWriteHead();
			writeState[i].ruleSelect = engines[i]->getRuleSelect();
			writeState[i].seed = engines[i]->getSeed();
			writeState[i].mode = engines[i]->getMode();
			engines[i]->getEngineLabel(writeState[i].engineLabel);
			engines[i]->getRuleActiveLabel(writeState[i].ruleActiveLabel);
			engines[i]->getRuleSelectLabel(writeState[i].ruleSelectLabel);
			engines[i]->getSeedLabel(writeState[i].seedLabel);
			engines[i]->getModeLabel(writeState[i].modeLabel);
		}
		engineStateSnapshotPtr.store(writeState, std::memory_order_release);
	}

	void updateSlewSelect(int newSelect) {
		slewValue = rack::clamp(newSelect, 0, 100);

		// Skew slewParam (0 - 100%) -> (0 - 1)
		// Convert to ms, if VCO mode (0 - 10ms) else (0 - 1000ms)
		float slewSkew = std::pow(slewValue * 0.01f, 2.f);
		float slew = vcoMode ? (slewSkew * 10.f) : (slewSkew * 1000.f);

		for (int i = 0; i < 2; i++)
			slewLimiter[i].setSlewAmountMs(slew, srate);
	}

	void onSampleRateChange() override {
		srate = APP->engine->getSampleRate();

		// Set DC blocker to ~10Hz,
		// Set Slew time (ms)
		updateSlewSelect(slewValue);

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
		pageCounter = 0;
		updateSlewSelect(0);
		updateEngineSelect(0);

		for (int i = 0; i < NUM_ENGINES; i++) {
			if (checkEngineIsNull(i))
				continue;
			
			for (int j = -1; j < MAX_SEQUENCE_LENGTH; j++)
				engines[i]->setBufferFrame(0, j);

			engines[i]->setReadHead(0);
			engines[i]->setWriteHead(1);
			engines[i]->updateRule(0, true);
			engines[i]->updateSeed(0, true);
			engines[i]->updateMode(0, true);
			engines[i]->pushSeed(false);
			engines[i]->updateMatrix(sequenceLength, offset, false);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// TODO: buffers saved as signed 64 bits, when unsigned is used
		
		/*
		// Save sequencer settings
		json_object_set_new(rootJ, "vco", json_boolean(vcoMode));
		json_object_set_new(rootJ, "sync", json_boolean(sync));
		json_object_set_new(rootJ, "slewX", json_boolean(slewX));
		json_object_set_new(rootJ, "slewY", json_boolean(slewY));
		json_object_set_new(rootJ, "slewValue", json_integer(slewValue));

		// Save engine index
		json_object_set_new(rootJ, "engine", json_integer(engineSelect));

		// Save UI settings
		json_object_set_new(rootJ, "displayStyle", json_integer(displayStyleIndex));
		json_object_set_new(rootJ, "cellStyle", json_integer(cellStyleIndex));

		
		// Save engine specifics
		
		EngineStateSnapshot* state = engineStateSnapshotPtr.load(std::memory_order_acquire);

		json_t* readHeadsJ = json_array();
		json_t* writeHeadsJ = json_array();
		json_t* rulesJ = json_array();
		json_t* seedsJ = json_array();
		json_t* modesJ = json_array();
		json_t* displaysJ = json_array();
		json_t* buffersJ = json_array();

		for (int i = 0; i < NUM_ENGINES; i++) {
			json_array_append_new(rulesJ, json_integer(state[i].ruleSelect));
			json_array_append_new(seedsJ, json_integer(state[i].seed));
			json_array_append_new(modesJ, json_integer(state[i].mode));
			json_array_append_new(readHeadsJ, json_integer(state[i].readHead));
			json_array_append_new(writeHeadsJ, json_integer(state[i].writeHead));
			json_array_append_new(displaysJ, json_integer(state[i].display));

			json_t* rowJ = json_array();
			for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++)
				json_array_append_new(rowJ, json_integer(state[i].matrixBuffer[j]));
			json_array_append_new(buffersJ, rowJ);
			
			// OLD
			if (checkEngineIsNull(i))
				continue;

			json_array_append_new(rulesJ, json_integer(engines[i]->getRuleSelect()));
			json_array_append_new(seedsJ, json_integer(engines[i]->getSeed()));
			json_array_append_new(modesJ, json_integer(engines[i]->getMode()));
			json_array_append_new(readHeadsJ, json_integer(engines[i]->getReadHead()));
			json_array_append_new(writeHeadsJ, json_integer(engines[i]->getWriteHead()));
			json_array_append_new(displaysJ, json_integer(engines[i]->getBufferFrame(-1)));

			// Save buffers
			json_t* rowJ = json_array();
			for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++)
				json_array_append_new(rowJ, json_integer(engines[i]->getBufferFrame(j)));
			json_array_append_new(buffersJ, rowJ);
			
		}
		

		json_object_set_new(rootJ, "readHeads", readHeadsJ);
		json_object_set_new(rootJ, "writeHeads", writeHeadsJ);
		json_object_set_new(rootJ, "rules", rulesJ);
		json_object_set_new(rootJ, "seeds", seedsJ);
		json_object_set_new(rootJ, "modes", modesJ);
		json_object_set_new(rootJ, "displays", displaysJ);
		json_object_set_new(rootJ, "buffers", buffersJ);
		*/
		
		return rootJ;
	}
	
	void dataFromJson(json_t* rootJ) override {
		// Load sequencer settings
		/*
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
			updateSlewSelect(json_integer_value(slewValueJ));

		// Load engine index
		json_t* engineSelectJ = json_object_get(rootJ, "engine");
		if (engineSelectJ)
			updateEngineSelect(json_integer_value(engineSelectJ));

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
					engines[i]->setReadHead(json_integer_value(valueJ));
			}
			if (writeHeadsJ) {
				json_t* valueJ = json_array_get(writeHeadsJ, i);

				if (valueJ)
					engines[i]->setWriteHead(json_integer_value(valueJ));
			}
			if (rulesJ) {
				json_t* valueJ = json_array_get(rulesJ, i);

				if (valueJ)
					engines[i]->setRule(json_integer_value(valueJ));
			}
			if (seedsJ) {
				json_t* valueJ = json_array_get(seedsJ, i);

				if (valueJ)
					engines[i]->setSeed(json_integer_value(valueJ));
			}
			if (modesJ) {
				json_t* valueJ = json_array_get(modesJ, i);

				if (valueJ)
					engines[i]->setMode(json_integer_value(valueJ));
			}
			if (displaysJ) {
				json_t* valueJ = json_array_get(displaysJ, i);

				if (valueJ)
					engines[i]->setBufferFrame(static_cast<uint64_t>(json_integer_value(valueJ)), -1);
			}

			// Load Buffers
			if (buffersJ) {
				json_t* rowJ = json_array_get(buffersJ, i);

				if (rowJ) {
					for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++) {
						json_t* valueJ = json_array_get(rowJ, j);

						if (valueJ)
							engines[i]->setBufferFrame(static_cast<uint64_t>(json_integer_value(valueJ)), j);
					}
				}
			}
		}
		*/
	}
	
	void processEncoder() {
		float encoderValue = params[SELECT_PARAM].getValue();
		float difference = encoderValue - prevEncoderValue;
		int delta = static_cast<int>(std::round(difference / encoderIndent)); //TODO: better way to do
			
		if ((delta == 0) && !encoderReset)
			return;
		
		prevEncoderValue += delta * encoderIndent;

		if (menuActive) {
			// Main menu
			// TODO: Clean this up a bit
			if (engineModulation) {
				if (pageNumber == 0)
					engines[0]->updateRule(delta, encoderReset);
				else if (pageNumber == 1)
					engines[0]->updateSeed(delta, encoderReset);
				else if (pageNumber == 2)
					engines[0]->updateMode(delta, encoderReset);
				else if (pageNumber == 3)
					engines[1]->updateRule(delta, encoderReset);
				else if (pageNumber == 4)
					engines[1]->updateSeed(delta, encoderReset);
				else if (pageNumber == 5)
					engines[1]->updateMode(delta, encoderReset);
				else if (pageNumber == 6)
					updateSlewSelect(encoderReset ? 0 : (slewValue + delta));
				else if (pageNumber == 7)
					updateEngineSelect(encoderReset ? engineDefault : ((engineSelect + delta + NUM_ENGINES) % NUM_ENGINES));
			}
			else {
				if (pageNumber == 0)
					engines[engineIndex]->updateSeed(delta, encoderReset);
				else if (pageNumber == 1)
					engines[engineIndex]->updateMode(delta, encoderReset);
				else if (pageNumber == 2)
					updateSlewSelect(encoderReset ? 0 : (slewValue + delta));
				else if (pageNumber == 3)
					updateEngineSelect(encoderReset ? engineDefault : ((engineSelect + delta + NUM_ENGINES) % NUM_ENGINES));
			}
		}
		else {
			// Mini menu
			engines[engineIndex]->updateRule(delta, miniMenuActive ? encoderReset : false);

			if (sync && (delta || miniMenuActive)) {
				seedPushPending = true;
			}
			else {
				gen = random::get<float>() < probability;
				genPending = true;
				seedPushPending = false;

				if (gen) {
					engines[engineIndex]->pushSeed(false);
					engines[engineIndex]->updateMatrix(sequenceLength, offset, false);
				}
			}
			if (!encoderReset || (miniMenuActive && encoderReset)) {
				miniMenuActive = true;
				ruleDisplayTimer.reset();
			}
		}
		encoderReset = false;
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

		// Knobs & CV inputs
		engineModulation = inputs[ENGINE_CV_INPUT].isConnected();
		float newEngineCvVoltage = rack::clamp(inputs[ENGINE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		int newEngineCv = std::round(newEngineCvVoltage * (NUM_ENGINES - 1));

		if (sync)
			engineCvPending = newEngineCv;
		else
			engineCv = newEngineCv;

		updateEngine();

		// Rule CV
		ruleModulation = inputs[RULE_CV_INPUT].isConnected();
		engines[engineIndex]->setRuleCV(rack::clamp(inputs[RULE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		// Prob knob & CV
		float probabilityCv = rack::clamp(inputs[PROBABILITY_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		probability = rack::clamp(params[PROBABILITY_PARAM].getValue() + probabilityCv, 0.f, 1.f);

		// Length knob
		int lengthParam = static_cast<int>(params[LENGTH_PARAM].getValue());
		sequenceLength = sequenceLengths[lengthParam];

		// Offset knob and CV
		float offsetCv = rack::clamp(inputs[OFFSET_CV_INPUT].getVoltage(), -10.f, 10.f) * 0.8f; // cast to int here
		int newOffset = rack::clamp(static_cast<int>(std::round(params[OFFSET_PARAM].getValue() + offsetCv)), -4, 4);
		if (sync) {
			offsetPending = newOffset;
		}
		else if (offset != newOffset) {
			offset = newOffset;
			engines[engineIndex]->updateMatrix(sequenceLength, offset, false);
		}

		// Buttons
		if (menuTrigger.process(params[MENU_PARAM].getValue()))
			menuActive = !menuActive;
		
		if (modeTrigger.process(params[MODE_PARAM].getValue())) {
			if (menuActive)
				pageCounter++;
			else
				engines[engineIndex]->updateMode(1, false);
		}
		int menuPageAmount = engineModulation ? NUM_MENU_PAGES_ENGINE_MOD : NUM_MENU_PAGES_DEFAULT;
		pageNumber = pageCounter % menuPageAmount;
		if (pageNumber < 0) 
			pageNumber += menuPageAmount;

		// Trigger inputs
		// Inject
		bool positiveInject = posInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), 0.1f, 2.f);
		bool negativeInject = negInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), -2.f, -0.1f);
		if (positiveInject || negativeInject) {
			if (sync) {
				if (positiveInject)
					injectPending = 1;
				else
					injectPending = 2;
			}
			else {
				engines[engineIndex]->inject(positiveInject ? true : false, false);
				engines[engineIndex]->updateMatrix(sequenceLength, offset, false);
				injectPending = 0;
			}
		}

		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
			if (sync) {
				resetPending = true;
			}
			else {
				gen = random::get<float>() < probability;
				genPending = true;
				resetPending = false;
				if (gen) {
					engines[engineIndex]->pushSeed(false);
				}
				else {
					engines[engineIndex]->setReadHead(0);
					engines[engineIndex]->setWriteHead(1);
				}
				engines[engineIndex]->updateMatrix(sequenceLength, offset, false);
			}
		}

		bool trig = false;
		float trigVoltage = inputs[TRIG_INPUT].getVoltage();
		if (vcoMode) {
			// Zero crossing
			trig = (trigVoltage > 0.f && prevTrigVoltage <= 0.f) || (trigVoltage < 0.f && prevTrigVoltage >= 0.f);
		}
		else {
			// Trigger pulse
			trig = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f);
		}
		prevTrigVoltage = trigVoltage;

		// TODO: do the dont trigger when reseting thing SEQ3
		// STEP
		if (trig) {
			// Find gen if not found when reset triggered and sync off
			if (!genPending)
				gen = random::get<float>() < probability;

			if (sync) {
				if (engineCv != engineCvPending) {
					engineCv = engineCvPending;
					updateEngine();
				}
				
				offset = offsetPending;

				if (injectPending == 1)
					engines[engineIndex]->inject(true, true);
				else if (injectPending == 2)
					engines[engineIndex]->inject(false, true);

				if (resetPending || seedPushPending) {
					if (gen)
						engines[engineIndex]->pushSeed(true);
					else if (!seedPushPending)
						engines[engineIndex]->setWriteHead(0);
				}
			}

			if (gen && !resetPending && !seedPushPending && !injectPending)
				engines[engineIndex]->generate();

			engines[engineIndex]->updateMatrix(sequenceLength, offset, true);

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			seedPushPending = false;
			injectPending = 0;
		}

		// OUTPUT
		float xCv = engines[engineIndex]->getXVoltage();
		float yCv = engines[engineIndex]->getYVoltage();

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

		// Pulse outputs - 0V to 10V.
		if (engines[engineIndex]->getXPulse())
			xPulse.trigger(vcoMode ? args.sampleTime : 1e-3f);

		if (engines[engineIndex]->getYPulse())
			yPulse.trigger(vcoMode ? args.sampleTime : 1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(engines[engineIndex]->getModeLEDValue(), args.sampleTime);
		lights[X_CV_LIGHT].setBrightness(xOut * 0.1);
		lights[Y_CV_LIGHT].setBrightness(yOut * 0.1);
		lights[X_PULSE_LIGHT].setBrightnessSmooth(xGate, args.sampleTime);
		lights[Y_PULSE_LIGHT].setBrightnessSmooth(yGate, args.sampleTime);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		// TODO: use ||?
		lights[INJECT_LIGHT].setBrightnessSmooth(positiveInject | negativeInject, args.sampleTime);
		
		// Encoder
		processEncoder();

		// Mini menu display
		if (miniMenuActive && (ruleDisplayTimer.process(args.sampleTime) >= miniMenuDisplayTime))
			miniMenuActive = false;

		if (((args.frame + this->id) % UI_UPDATE_INTERVAL) == 0)
			updateEngineStateSnapshot();

		engines[engineIndex]->tick();
	}
};

struct Display : TransparentWidget {
	WolframModule* module;
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

	Display(WolframModule* m, float yPos, float w, float size) {
		module = m;

		// Wiget params
		widgetSize = size;
		float screenSize = widgetSize - (padding * 2.f);
		fontSize = (screenSize / cols) * 2.f;
		cellPadding = (screenSize / cols);

		// Widget size
		box.pos = Vec((w * 0.5f) - (widgetSize * 0.5f), yPos);
		box.size = Vec(widgetSize, widgetSize);

		// Get font 
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/wolf.otf"));
		font = APP->window->loadFont(fontPath);
		
		ui.init(padding, fontSize, cellPadding);
	}

	void wolfSeedDisplayHelper(NVGcontext* vg, int seed, char* text) {
		if (seed == 256) {
			std::copy("RAND", "RAND" + 4, text);
		}
		else {
			std::copy("    ", "    " + 4, text);
			ui.drawWolfSeedDisplay(vg, 1, seed);
		}
	}

	void drawDisplay(NVGcontext* vg, int layer) {
		EngineStateSnapshot* readState = nullptr;
		if (module)
			readState = module->engineStateSnapshotPtr.load(std::memory_order_acquire);

		int firstRow = 0;
		bool menuActive = module ? module->menuActive : false;
		bool miniMenuActive = module ? module->miniMenuActive : false;
		int engineIndex = module ? rack::clamp(module->engineIndex, 0, 2 - 1) : 0;

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
			bool engineMod = module->engineModulation;

			// Text background
			if (layer == 0) {
				if (((pageNumber == 0) && (engineIndex == 0) && !engineMod) || ((pageNumber == 1) && engineMod)) {
					// Special Wolf seed display
					int seed = readState[0].seed;
					if (seed == 256) {
						ui.drawTextBg(vg, 2);
					}
					else {
						ui.drawWolfSeedDisplay(vg, layer, seed);
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

				if (engineMod) {
					// Engine modulation menu
					// Convert capitalised engine name to lower case
					char wolfName[5]{};
					char lifeName[5]{};
					for (int i = 0; i < 4; i++) {
						wolfName[i] = std::tolower(static_cast<unsigned char>(readState[0].engineLabel[i]));
						lifeName[i] = std::tolower(static_cast<unsigned char>(readState[1].engineLabel[i]));
					}
					wolfName[4] = '\0';
					lifeName[4] = '\0';

					int engineToDisplay = 0;
					if (pageNumber >= 0 && pageNumber < 3) {
						std::copy(wolfName, wolfName + 4, header);
					}
					else if (pageNumber >= 3 && pageNumber < 6) {
						std::copy(lifeName, lifeName + 4, header);
						engineToDisplay = 1;
					}
					
					if (pageNumber == 0 || pageNumber == 3) {
						std::copy("RULE", "RULE" + 4, title);
						std::copy(readState[engineToDisplay].ruleActiveLabel, readState[engineToDisplay].ruleActiveLabel + 4, value);
					}
					else if (pageNumber == 1 || pageNumber == 4) {
						std::copy("SEED", "SEED" + 4, title);

						if (pageNumber == 1)
							wolfSeedDisplayHelper(vg, readState[0].seed, value);
						else
							std::copy(readState[engineToDisplay].seedLabel, readState[engineToDisplay].seedLabel + 4, value);
					}
					else if (pageNumber == 2 || pageNumber == 5) {
						std::copy("MODE", "MODE" + 4, title);
						std::copy(readState[engineToDisplay].modeLabel, readState[engineToDisplay].modeLabel + 4, value);
					}
				}
				else {
					// Default menu
					if (pageNumber == 0) {
						std::copy("SEED", "SEED" + 4, title);
						if (engineIndex == 0)
							wolfSeedDisplayHelper(vg, readState[0].seed, value);
						else 
							std::copy(readState[engineIndex].seedLabel, readState[engineIndex].seedLabel + 4, value);
					}
					else if (pageNumber == 1) {
						std::copy("MODE", "MODE" + 4, title);
						std::copy(readState[engineIndex].modeLabel, readState[engineIndex].modeLabel + 4, value);
					}
				}

				// Slew & engine select menus
				if (pageNumber == (engineMod ? NUM_ENGINES * 3 : 2)) {
					std::copy("SLEW", "SLEW" + 4, title);
					char slewString[5]{};
					snprintf(slewString, sizeof(slewString), "%3d%%", module->slewValue);
					std::copy(slewString, slewString + 4, value);
				}
				else if (pageNumber == (engineMod ? (NUM_ENGINES * 3) + 1 : 3)) {
					std::copy("ALGO", "ALGO" + 4, title);
					std::copy(readState[engineIndex].engineLabel, readState[engineIndex].engineLabel + 4, value);
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
				ui.drawText(vg, readState[engineIndex].ruleActiveLabel, 1);
			}
		}

		// Cells
		uint64_t matrix = 0x81C326F48FCULL;
		if (module)
			matrix = readState[engineIndex].display;

		//matrix = engine[engineIndex]->getBufferFrame(-1);

		nvgBeginPath(vg);
		nvgFillColor(vg, colour);

		for (int row = firstRow; row < 8; row++) {
			int rowInvert = 7 - row;

			uint8_t rowBits = (matrix >> (rowInvert * 8)) & 0xFF;

			if (layer == 0)
				rowBits = ~rowBits;

			rowBits &= 0xFF;

			// TODO: is __builtin_ctz ok?
			int i = 0;
			while (rowBits && (i < rows)) {
				int colInvert = __builtin_ctz(rowBits);
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
			// Reset Select encoder to default of current selection,
			// see processEncoder for details
			auto* m = static_cast<WolframModule*>(module);
			m->encoderReset = true;
		}
	};

	// Custom lights
	// TODO: is this wrong
	template <typename TBase>
	struct LuckyLight : RectangleLight<TSvgLight<TBase>> {
		// Count Modula's custom light code
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

	WolframModuleWidget(WolframModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/wolfram.svg")));

		// Srews
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// Buttons
		addParam(createParamCentered<CKD6>(mm2px(Vec(7.62f, 38.14f)), module, WolframModule::MENU_PARAM));
		addParam(createParamCentered<CKD6>(mm2px(Vec(53.34f, 38.14f)), module, WolframModule::MODE_PARAM));
		// Dials
		addParam(createParamCentered<SelectEncoder>(mm2px(Vec(53.34f, 22.14f)), module, WolframModule::SELECT_PARAM));
		addParam(createParamCentered<LengthKnob>(mm2px(Vec(15.24f, 61.369f)), module, WolframModule::LENGTH_PARAM));
		addParam(createParamCentered<ProbabilityKnob>(mm2px(Vec(45.72f, 61.369f)), module, WolframModule::PROBABILITY_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48f, 80.597f)), module, WolframModule::OFFSET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62f, 80.597f)), module, WolframModule::X_SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(53.34f, 80.597f)), module, WolframModule::Y_SCALE_PARAM));
		// Inputs
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 22.14f)), module, WolframModule::RESET_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(30.48f, 99.852f)), module, WolframModule::OFFSET_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(30.48f, 114.852f)), module, WolframModule::PROBABILITY_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(41.91f, 114.852f)), module, WolframModule::RULE_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(53.34f, 114.852f)), module, WolframModule::ENGINE_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 114.852f)), module, WolframModule::TRIG_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(19.05f, 114.852f)), module, WolframModule::INJECT_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 99.852f)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(19.05f, 99.852f)), module, WolframModule::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 99.852f)), module, WolframModule::Y_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.91f, 99.852f)), module, WolframModule::Y_PULSE_OUTPUT));
		// LEDs
		//addChild(createLightCentered<DiagonalLuckyLight<RedLight>>(mm2px(Vec(53.34f, 47.767f)), module, WolframModule::MODE_LIGHT)); 
		//addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(7.62f, 90.225f)), module, WolframModule::X_CV_LIGHT));		
		//addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(19.05f, 90.225f)), module, WolframModule::X_PULSE_LIGHT));	
		//addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(41.91f, 90.225f)), module, WolframModule::Y_PULSE_LIGHT));	
		//addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(53.34f, 90.225f)), module, WolframModule::Y_CV_LIGHT));
		
		//Display* display = new Display(module, mm2px(10.14f), box.size.x, mm2px(32.f));
		//addChild(display);
	}

	
	void appendContextMenu(Menu* menu) override {
		WolframModule* module = dynamic_cast<WolframModule*>(this->module);
		
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
	
		menu->addChild(createBoolMenuItem("VCO", "",
			[=]() {
				return module->vcoMode;
			},
			[=](bool vco) {
				module->vcoMode = vco;
				module->onSampleRateChange();
			}
		));

		menu->addChild(createSubmenuItem("Slew", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("X", "", &module->slewX));
				menu->addChild(createBoolPtrMenuItem("Y", "", &module->slewY));
			}
		));

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Display",
			{ "Redrick", "OLED", "Rack", "Windows", "Lamp", "Mono"},
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

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");