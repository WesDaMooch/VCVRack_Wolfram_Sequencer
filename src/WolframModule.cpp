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


// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg

// TODO: maybe this
// Therefore, modules with a CLOCK and RESET input, or similar variants, should ignore CLOCK triggers up to 1 ms 
// after receiving a RESET trigger.You can use dsp::Timer for keeping track of time.
// Or do what SEQ3 does?

// TODO: onRandomize
// TODO: json saves to signed 64 bit number, i need unsigned (uint64_t), maybe not a issue

// BUGS
// TODO: algo cv still buggy when sync off
// - make sure engine ref are set ever process call
// TODO: on wolf vcomode, seed not init or something

// CRASH
// TODO: fix framebuffer crash
// - Updated SDK fix?
// - Wolfram crashing
// - Crashes without Display and custom lights
// - Crashes when data save / load is disabled
// - Crashes when not being triggered
// - Crashes faster when Display and custom lights are active (unconfirmed)
// - Did not crash in dev mode (unconfirmed)

#include "wolfram.hpp"
#include <vector> // TODO: not using?

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
		PROB_PARAM,
		OFFSET_PARAM,
		X_SCALE_PARAM,
		Y_SCALE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RESET_INPUT,
		PROB_CV_INPUT,
		OFFSET_CV_INPUT,
		RULE_CV_INPUT,
		ALGO_CV_INPUT,
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
	// Engine
	WolfEngine wolfEngine;
	LifeEngine lifeEngine;
	static constexpr int NUM_ALGOS = 2;
	std::array<AlgoEngine*, NUM_ALGOS> engine{};
	int eIndex = 0;
	static constexpr int algoDefault = 0;
	int algoSelect = algoDefault;
	int algoCV = 0;
	int algoCvPending = 0;

	// UI
	LookAndFeel lookAndFeel;
	int pageCounter = 0;
	int pageNumber = 0;
	bool menuActive = false;

	const int defaultMenuPageAmount = 4;
	const int algoModMenuPageAmount = (NUM_ALGOS * 3) + 2;

	// Parameters
	int sequenceLength = 8;
	std::array<int, 9> sequenceLengths { 2, 3, 4, 6, 8, 12, 16, 32, 64 };
	float prob = 1;
	int slewParam = 0;
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
	bool ruleMod = false;
	bool algoMod = false;

	// DPS
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
		configParam(PROB_PARAM, 0.f, 1.f, 1.f, "Probability", "%", 0.f, 100.f);
		paramQuantities[PROB_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(X_SCALE_PARAM, 0.f, 1.f, 0.5f, "X CV Scale", "V", 0.f, 10.f);
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 1.f, 0.5f, "Y CV Scale", "V", 0.f, 10.f);
		paramQuantities[Y_SCALE_PARAM]->displayPrecision = 3;
		configInput(RESET_INPUT, "Reset");
		configInput(PROB_CV_INPUT, "Probability CV");
		configInput(RULE_CV_INPUT, "Rule CV");
		configInput(ALGO_CV_INPUT, "Algo CV");
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

		// Set alogrithm engines
		engine[0] = &wolfEngine;
		engine[1] = &lifeEngine;
		// Init engines
		for (int i = 0; i < NUM_ALGOS; i++) {
			engine[i]->updateRule(0, true);
			engine[i]->updateSeed(0, true);
			engine[i]->updateMode(0, true);
		}

		// Init display matrix
		engine[eIndex]->updateMatrix(sequenceLength, offset, false);
		onSampleRateChange();
	}

	struct EncoderParamQuantity : ParamQuantity {
		// Custom behaviour to display rule when hovering over Select encoder 
		std::string getDisplayValueString() override {
			std::string defaultString = "30";

			auto* m = dynamic_cast<WolframModule*>(module);
			if (!m)
				return defaultString;

			// TODO: use numalgos globally
			int numAlgos = m->NUM_ALGOS;

			std::array<AlgoEngine*, numAlgos> e{};
			for (int i = 0; i < numAlgos; i++) {
				e[i] = m->engine[i];
				if (!e[i])
					return defaultString;
				// set rule strings here?
			}

			bool menuActive = m->menuActive;
			bool algoMod = m->algoMod;
			bool ruleMod = m->ruleMod;

			int eIndex = rack::clamp(m->eIndex, 0, numAlgos - 1);
			int menuPageAmount = algoMod ? m->algoModMenuPageAmount : m->defaultMenuPageAmount;
			int pageNumber = rack::clamp(m->pageNumber, 0, menuPageAmount);
			
			// TODO: used array of strings instead of accessing engine so much?
			// std::array<std::string, numAlgos> ruleString{};
			std::string activeRuleString = e[eIndex]->getRuleName();
			std::string selectedRuleString = e[eIndex]->getRuleSelectName();

			// No algorithm modulation & no rule modulation
			if (!algoMod && !ruleMod)
				return selectedRuleString;

			// Menu closed or no algorithm modulation
			if (!algoMod || !menuActive) {
				if (!ruleMod)
					return selectedRuleString;

				return selectedRuleString + " ( " + activeRuleString + " )";
			}

			// Menu open & algorithm modulation
			if (pageNumber == 0) {
				// Wolf rule page
				std::string s = e[0]->getRuleSelectName();
				std::string r = ruleMod ? 
					e[0]->getRuleName() :
					activeRuleString;

				return s + " ( " + r + " )";
			}

			if (pageNumber == 3) {
				// Life rule page
				std::string s = e[1]->getRuleSelectName();
				std::string r = ruleMod ? 
					e[1]->getRuleName() :
					activeRuleString;

				return s + " ( " + r + " )";
			}

			// Non-rule pages
			if (!ruleMod)
				return selectedRuleString;

			return selectedRuleString + " ( " + activeRuleString + " )";
		}

		// Suppress behaviour
		void setDisplayValueString(std::string s) override {}
	};

	struct LengthParamQuantity : ParamQuantity {
		// Custom behaviour to display sequence length when hovering over Length knob
		float getDisplayValue() override {
			auto* m = dynamic_cast<WolframModule*>(module);
			return m ? m->sequenceLength : 8;
		}

		// Suppress behaviour
		void setDisplayValueString(std::string s) override {}
	};

	void onAlgoChange() {
		// TODO: find eIndex ever process call
		eIndex = rack::clamp(algoSelect + algoCV, 0, NUM_ALGOS - 1);
		engine[eIndex]->updateMatrix(sequenceLength, offset, false);
	}

	void setAlgoCV(float cv) {
		int newCV = std::round(cv * (NUM_ALGOS - 1));

		if (algoCV == newCV)
			return;

		if (sync) {
			algoCvPending = newCV;
			return;
		}

		algoCV = newCV;
		onAlgoChange();
	}

	void setAlgoSelect(int delta, bool reset) {
		int newSelect = engine[eIndex]->updateSelect(algoSelect,
			NUM_ALGOS, algoDefault, delta, reset);

		if (newSelect == algoSelect)
			return;

		algoSelect = newSelect;
		onAlgoChange();
	}

	void setSlewSelect(int delta, bool reset) {
		if (reset)
			slewParam = 0;
		else
			slewParam = rack::clamp(slewParam + delta, 0, 100);

		// Skew slewParam (0 - 100%) -> (0 - 1)
		// Convert to ms, if VCO mode (0 - 10ms) else (0 - 1000ms)
		float slewSkew = std::pow(slewParam * 0.01f, 2.f);
		float slew = vcoMode ? (slewSkew * 10.f) : (slewSkew * 1000.f);

		for (int i = 0; i < 2; i++)
			slewLimiter[i].setSlewAmountMs(slew, srate);
	}

	void onSampleRateChange() override {
		srate = APP->engine->getSampleRate();

		// Set DC blocker to ~10Hz,
		// Set Slew time (ms)
		float slewSkew = std::pow(slewParam * 0.01f, 2.f);
		float slew = vcoMode ? (slewSkew * 10.f) : (slewSkew * 1000.f);

		for (int i = 0; i < 2; i++) {
			dcFilter[i].setCutoffFreq(10.f / srate);
			dcFilter[i].reset();
			slewLimiter[i].setSlewAmountMs(slew, srate);
			slewLimiter[i].reset();
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		
		sync = true;
		vcoMode = false;
		pageCounter = 0;
		setSlewSelect(0, true);
		setAlgoSelect(0, true);

		for (int i = 0; i < NUM_ALGOS; i++) {
			for (int j = -1; j < 64; j++)
				engine[i]->setBufferFrame(0, j);

			engine[i]->setReadHead(0);
			engine[i]->setWriteHead(1);
			engine[i]->updateRule(0, true);
			engine[i]->updateSeed(0, true);
			engine[i]->updateMode(0, true);
			engine[i]->pushSeed(false);
			engine[i]->updateMatrix(sequenceLength, offset, false);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		
		// TODO: save in correct order
		// TODO: buffers saved as signed 64 bits, when unsigned is used

		// Save sequencer settings
		json_object_set_new(rootJ, "sync", json_boolean(sync));
		json_object_set_new(rootJ, "vco", json_boolean(vcoMode));
		json_object_set_new(rootJ, "slewValue", json_integer(slewParam));
		json_object_set_new(rootJ, "slewX", json_boolean(slewX));
		json_object_set_new(rootJ, "slewY", json_boolean(slewY));

		// Save algorithm specifics
		json_t* rulesJ = json_array();
		json_t* seedsJ = json_array();
		json_t* modesJ = json_array();
		json_t* readHeadsJ = json_array();
		json_t* writeHeadsJ = json_array();
		json_t* buffersJ = json_array();
		json_t* displaysJ = json_array();

		for (int i = 0; i < NUM_ALGOS; i++) {
			json_array_append_new(rulesJ, json_integer(engine[i]->getRuleSelect()));
			json_array_append_new(seedsJ, json_integer(engine[i]->getSeed()));
			json_array_append_new(modesJ, json_integer(engine[i]->getMode()));
			json_array_append_new(readHeadsJ, json_integer(engine[i]->getReadHead()));
			json_array_append_new(writeHeadsJ, json_integer(engine[i]->getWriteHead()));

			// Save buffers
			json_t* rowJ = json_array();
			for (int j = 0; j < 64; j++)
				json_array_append_new(rowJ, json_integer(engine[i]->getBufferFrame(j)));
			json_array_append_new(buffersJ, rowJ);

			json_array_append_new(displaysJ, json_integer(engine[i]->getBufferFrame(-1)));
		}

		json_object_set_new(rootJ, "rules", rulesJ);
		json_object_set_new(rootJ, "seeds", seedsJ);
		json_object_set_new(rootJ, "modes", modesJ);
		json_object_set_new(rootJ, "readHeads", readHeadsJ);
		json_object_set_new(rootJ, "writeHeads", writeHeadsJ);
		json_object_set_new(rootJ, "buffers", buffersJ);
		json_object_set_new(rootJ, "displays", displaysJ);
		
		// Save algorithm settings
		json_object_set_new(rootJ, "algo", json_integer(algoSelect));

		// Save LookAndFeel settings
		json_object_set_new(rootJ, "look", json_integer(lookAndFeel.displayStyleIndex));
		json_object_set_new(rootJ, "cellStyle", json_integer(lookAndFeel.cellStyleIndex));

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

		json_t* slewValueJ = json_object_get(rootJ, "slewValue");
		if (slewValueJ)
			slewParam = json_integer_value(slewValueJ);

		json_t* slewXJ = json_object_get(rootJ, "slewX");
		if (slewXJ)
			slewX = json_boolean_value(slewXJ);

		json_t* slewYJ = json_object_get(rootJ, "slewY");
		if (slewYJ)
			slewY = json_boolean_value(slewYJ);

		// Load algorithm specifics
		json_t* rulesJ = json_object_get(rootJ, "rules");
		json_t* seedsJ = json_object_get(rootJ, "seeds");
		json_t* modesJ = json_object_get(rootJ, "modes");
		json_t* readHeadsJ = json_object_get(rootJ, "readHeads");
		json_t* writeHeadsJ = json_object_get(rootJ, "writeHeads");
		json_t* buffersJ = json_object_get(rootJ, "buffers");
		json_t* displaysJ = json_object_get(rootJ, "displays");

		for (int i = 0; i < NUM_ALGOS; i++) {
			if (readHeadsJ) {
				json_t* valueJ = json_array_get(readHeadsJ, i);

				if (valueJ)
					engine[i]->setReadHead(json_integer_value(valueJ));
			}
			if (writeHeadsJ) {
				json_t* valueJ = json_array_get(writeHeadsJ, i);

				if (valueJ)
					engine[i]->setWriteHead(json_integer_value(valueJ));
			}
			if (rulesJ) {
				json_t* valueJ = json_array_get(rulesJ, i);

				if (valueJ)
					engine[i]->setRule(json_integer_value(valueJ));
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

			// Load Buffers
			if (buffersJ) {
				json_t* rowJ = json_array_get(buffersJ, i);

				if (rowJ) {
					for (int j = 0; j < 64; j++) {
						json_t* valueJ = json_array_get(rowJ, j);

						if (valueJ)
							engine[i]->setBufferFrame(static_cast<uint64_t>(json_integer_value(valueJ)), j);
					}
				}
			}

			if (displaysJ) {
				json_t* valueJ = json_array_get(displaysJ, i);

				if (valueJ)
					engine[i]->setBufferFrame(static_cast<uint64_t>(json_integer_value(valueJ)), -1);
			}
		}
	
		// Load algorithm
		json_t* algoSelectJ = json_object_get(rootJ, "algo");
		if (algoSelectJ) {
			algoSelect = json_integer_value(algoSelectJ);
			onAlgoChange();
		}

		// Load LookAndFeel settings
		json_t* lookJ = json_object_get(rootJ, "look");
		if (lookJ)
			lookAndFeel.displayStyleIndex = json_integer_value(lookJ);

		json_t* cellStyleJ = json_object_get(rootJ, "cellStyle");
		if (cellStyleJ)
			lookAndFeel.cellStyleIndex = json_integer_value(cellStyleJ);
	}
	

	void processEncoder() {
		float encoderValue = params[SELECT_PARAM].getValue();
		float difference = encoderValue - prevEncoderValue;
		int delta = static_cast<int>(std::round(difference / encoderIndent));
			
		if ((delta == 0) && !encoderReset)
			return;
		
		prevEncoderValue += delta * encoderIndent;
		int e = rack::clamp(eIndex, 0, NUM_ALGOS - 1); //TODO: can remove this...?

		if (menuActive) {
			// Main menu
			// TODO: Clean this up a bit
			if (algoMod) {
				int p = rack::clamp(pageNumber, 0, algoModMenuPageAmount);
				if (p == 0)
					engine[0]->updateRule(delta, encoderReset);
				else if (p == 1)
					engine[0]->updateSeed(delta, encoderReset);
				else if (p == 2)
					engine[0]->updateMode(delta, encoderReset);
				else if (p == 3)
					engine[1]->updateRule(delta, encoderReset);
				else if (p == 4)
					engine[1]->updateSeed(delta, encoderReset);
				else if (p == 5)
					engine[1]->updateMode(delta, encoderReset);
				else if (p == 6)
					setSlewSelect(delta, encoderReset);
				else if (p == 7)
					setAlgoSelect(delta, encoderReset);
			}
			else {
				int p = rack::clamp(pageNumber, 0, defaultMenuPageAmount);
				if (p == 0)
					engine[e]->updateSeed(delta, encoderReset);
				else if (p == 1)
					engine[e]->updateMode(delta, encoderReset);
				else if (p == 2)
					setSlewSelect(delta, encoderReset);
				else if (p == 3)
					setAlgoSelect(delta, encoderReset);
			}
		}
		else {
			// Mini menu
			engine[e]->updateRule(delta, miniMenuActive ? encoderReset : false);

			if (sync && (delta || miniMenuActive)) {
				seedPushPending = true;
			}
			else {
				gen = random::get<float>() < prob;
				genPending = true;
				seedPushPending = false;

				if (gen) {
					engine[eIndex]->pushSeed(false);
					engine[eIndex]->updateMatrix(sequenceLength, offset, false);
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
		// Knobs & CV inputs
		bool newAlgoMod = inputs[ALGO_CV_INPUT].isConnected();
		if (newAlgoMod != algoMod)
			algoMod = newAlgoMod;
		
		setAlgoCV(rack::clamp(inputs[ALGO_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		// Rule CV
		ruleMod = inputs[RULE_CV_INPUT].isConnected();
		engine[eIndex]->setRuleCV(rack::clamp(inputs[RULE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		// Prob knob & CV
		float probCV = rack::clamp(inputs[PROB_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		prob = rack::clamp(params[PROB_PARAM].getValue() + probCV, 0.f, 1.f);

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
			engine[eIndex]->updateMatrix(sequenceLength, offset, false);
		}

		// Buttons
		// Menu
		if (menuTrigger.process(params[MENU_PARAM].getValue()))
			menuActive = !menuActive;
		
		// Mode
		if (modeTrigger.process(params[MODE_PARAM].getValue())) {
			if (menuActive)
				pageCounter++;
			else
				engine[eIndex]->updateMode(1, false);
		}
		int menuPageAmount = algoMod ? algoModMenuPageAmount : defaultMenuPageAmount;
		pageNumber = (pageCounter + menuPageAmount) % menuPageAmount;

		// Encoder
		processEncoder();

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
				engine[eIndex]->inject(positiveInject ? true : false, false);
				engine[eIndex]->updateMatrix(sequenceLength, offset, false);
				injectPending = 0;
			}
		}

		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
			if (sync) {
				resetPending = true;
			}
			else {
				gen = random::get<float>() < prob;
				genPending = true;
				resetPending = false;
				if (gen) {
					engine[eIndex]->pushSeed(false);
				}
				else {
					engine[eIndex]->setReadHead(0);
					engine[eIndex]->setWriteHead(1);
				}
				engine[eIndex]->updateMatrix(sequenceLength, offset, false);
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
				gen = random::get<float>() < prob;

			if (sync) {
				if (algoCV != algoCvPending) {
					algoCV = algoCvPending;
					onAlgoChange();
				}
				
				offset = offsetPending;

				if (injectPending == 1)
					engine[eIndex]->inject(true, true);
				else if (injectPending == 2)
					engine[eIndex]->inject(false, true);

				if (resetPending || seedPushPending) {
					if (gen)
						engine[eIndex]->pushSeed(true);
					else if (!seedPushPending)
						engine[eIndex]->setWriteHead(0);
				}
			}

			if (gen && !resetPending && !seedPushPending && !injectPending)
				engine[eIndex]->generate();

			engine[eIndex]->updateMatrix(sequenceLength, offset, true);

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			seedPushPending = false;
			injectPending = 0;
		}

		// OUTPUT
		float xCv = engine[eIndex]->getXVoltage();
		float yCv = engine[eIndex]->getYVoltage();

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
		if (engine[eIndex]->getXPulse())
			xPulse.trigger(vcoMode ? args.sampleTime : 1e-3f);

		if (engine[eIndex]->getYPulse())
			yPulse.trigger(vcoMode ? args.sampleTime : 1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(engine[eIndex]->getModeLEDValue(), args.sampleTime);
		lights[X_CV_LIGHT].setBrightness(xOut * 0.1);
		lights[Y_CV_LIGHT].setBrightness(yOut * 0.1);
		lights[X_PULSE_LIGHT].setBrightnessSmooth(xGate, args.sampleTime);
		lights[Y_PULSE_LIGHT].setBrightnessSmooth(yGate, args.sampleTime);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		// TODO: use ||?
		lights[INJECT_LIGHT].setBrightnessSmooth(positiveInject | negativeInject, args.sampleTime);
		
		// Mini menu display
		if (miniMenuActive && (ruleDisplayTimer.process(args.sampleTime) >= miniMenuDisplayTime))
			miniMenuActive = false;

		engine[eIndex]->tick();
	}
};

struct Display : TransparentWidget {
	WolframModule* module;
	LookAndFeel* lookAndFeel;

	std::shared_ptr<Font> font;
	std::string fontPath;

	int cols = 8;
	int rows = 8;
	float padding = 1.f;
	float cellSize = 5.f;

	float cellPadding = 0;
	float widgetSize = 0;
	float fontSize = 0;

	Display(WolframModule* m, float y, float w, float s) {
		module = m;

		// Wiget params
		widgetSize = s;
		float screenSize = widgetSize - (padding * 2.f);
		fontSize = (screenSize / cols) * 2.f;

		cellPadding = (screenSize / cols);

		// Widget size
		box.pos = Vec((w * 0.5f) - (widgetSize * 0.5f), y);
		box.size = Vec(widgetSize, widgetSize);

		// Get font 
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/wolf.otf"));
		font = APP->window->loadFont(fontPath);

		if (!module)
			return;

		// Set look & feel
		lookAndFeel = &module->lookAndFeel;
		// TODO: set in init
		lookAndFeel->padding = padding;
		lookAndFeel->fontSize = fontSize;
		lookAndFeel->cellSpacing = cellPadding;
		lookAndFeel->init();
	}

	void drawDisplay(NVGcontext* vg, int layer) {
		int firstRow = 0;
		bool menuActive = module ? module->menuActive : false;
		bool miniMenuActive = module ? module->miniMenuActive : false;
		int engineIndex = module ? rack::clamp(module->eIndex, 0, 2 - 1) : 0;

		// TODO: use numalgo
		std::array<AlgoEngine*, 2> e{};
		for (int i = 0; i < 2; i++) {
			e[i] = module ? module->engine[i] : nullptr;
			// TODO: use numalgo
			if ((menuActive || miniMenuActive) && !e[i])
				return;
		}

		// Set colour
		NVGcolor colour = nvgRGB(0, 0, 0);
		if (module) {
			colour = (layer == 1) ?
				lookAndFeel->getForegroundColour() :
				lookAndFeel->getBackgroundColour();
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
			bool mod = module->algoMod;

			// Text background
			if (layer == 0) {
				if (((pageNumber == 0) && (engineIndex == 0)) || ((pageNumber == 1) && mod)) {
					// Special Wolf seed display
					lookAndFeel->drawTextBg(vg, 0);	
					lookAndFeel->drawTextBg(vg, 1);
					lookAndFeel->drawWolfSeedDisplay(vg, layer, e[0]->getSeed());
					lookAndFeel->drawTextBg(vg, 3);
				}
				else {
					for (int i = 0; i < 4; i++)
						lookAndFeel->drawTextBg(vg, i);
				}
			}
			// Text
			else if (layer == 1) {
				std::string header = "menu";
				std::string footer = "<#@>";
				if (mod) {
					pageNumber = rack::clamp(pageNumber, 0, module->algoModMenuPageAmount - 1);

					// Convert capitalised algorithm name to lower case
					std::string wolfHeader = e[0]->getAlgoName();
					std::transform(wolfHeader.begin(),
						wolfHeader.end(), wolfHeader.begin(), ::tolower);

					std::string lifeHeader = e[1]->getAlgoName();
					std::transform(lifeHeader.begin(),
						lifeHeader.end(), lifeHeader.begin(), ::tolower);

					if (pageNumber == 0) {
						lookAndFeel->drawMenuText(vg, wolfHeader, "RULE", e[0]->getRuleName(), footer);
					}
					else if (pageNumber == 1) {
						lookAndFeel->drawMenuText(vg, wolfHeader, "SEED", "", footer);
						lookAndFeel->drawWolfSeedDisplay(vg, layer, e[0]->getSeed());
					}
					else if (pageNumber == 2) {
						lookAndFeel->drawMenuText(vg, wolfHeader, "MODE", e[0]->getModeName(), footer);
					}
					else if (pageNumber == 3) {
						lookAndFeel->drawMenuText(vg, lifeHeader, "RULE", e[1]->getRuleName(), footer);
					}
					else if (pageNumber == 4) {
						lookAndFeel->drawMenuText(vg, lifeHeader, "SEED", e[1]->getSeedName(), footer);
					}
					else if (pageNumber == 5) {
						lookAndFeel->drawMenuText(vg, lifeHeader, "MODE", e[1]->getModeName(), footer);
					}
					else if (pageNumber == 6) {
						lookAndFeel->drawMenuText(vg, header, "SLEW", std::to_string(module->slewParam) + "%", footer);
					}
					else if (pageNumber == 7) {
						lookAndFeel->drawMenuText(vg, header, "ALGO", e[engineIndex]->getAlgoName(), footer);
					}
				}
				else {
					pageNumber = rack::clamp(pageNumber, 0, module->defaultMenuPageAmount - 1);

					if (pageNumber == 0) {
						if (engineIndex == 0) {
							lookAndFeel->drawMenuText(vg, header, "SEED", "", footer);
							lookAndFeel->drawWolfSeedDisplay(vg, layer, e[engineIndex]->getSeed());
						}
						else {
							lookAndFeel->drawMenuText(vg, header, "SEED", e[engineIndex]->getSeedName(), footer);
						}
					}
					else if (pageNumber == 1) {
						lookAndFeel->drawMenuText(vg, header, "MODE", e[engineIndex]->getModeName(), footer);
					}
					else if (pageNumber == 2) {
						lookAndFeel->drawMenuText(vg, header, "SLEW", std::to_string(module->slewParam) + "%", footer);
					}
					else if (pageNumber == 3) {
						lookAndFeel->drawMenuText(vg, header, "ALGO", e[engineIndex]->getAlgoName(), footer);
					}
				}
			}
			return;
		}
		else if (miniMenuActive) {
			// Mini menu
			firstRow = rows - 4;
			// Text background
			if (layer == 0) {
				for (int i = 0; i < 2; i++)
					lookAndFeel->drawTextBg(vg, i);
			}
			// Text
			else if (layer == 1) {
				lookAndFeel->drawText(vg, "RULE", 0);
				lookAndFeel->drawText(vg, e[engineIndex]->getRuleName(), 1);
			}
		}

		// Cells
		uint64_t matrix = 0x81C326F48FCULL;
		if (e[engineIndex])
			matrix = e[engineIndex]->getBufferFrame(-1);

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

				int c = 7 - colInvert;

				if (module) {
					lookAndFeel->getCellPath(vg, c, row);
				}
				else {
					// Preview window drawing
					float pad = (cellPadding * 0.5f) + padding;
					nvgCircle(vg, (cellPadding * c) + pad,
						(cellPadding * row) + pad, 5.f);
				}
				i++;
			}
		}
		nvgFill(vg);
	}

	void draw(const DrawArgs& args) override {
		lookAndFeel = module ? &module->lookAndFeel : nullptr;

		// Draw background & border
		NVGcolor backgroundColour = module ? lookAndFeel->getScreenColour() : nvgRGB(58, 16, 19);

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

		lookAndFeel = module ? &module->lookAndFeel : nullptr;
		drawDisplay(args.vg, layer);
		Widget::drawLayer(args, layer);
	}
};

struct WolframModuleWidget : ModuleWidget {
	WolframModule* m;

	// Custom knobs & dials
	struct LengthKnob : M1900hBlackKnob {
		LengthKnob() {
			minAngle = -0.75f * M_PI;
			maxAngle = 0.5f * M_PI;
		}
	};

	struct ProbKnob : M1900hBlackKnob {
		ProbKnob() {
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

	// Count Modula's custom LEDs
	template <typename TBase>
	struct LuckyLight : RectangleLight<TSvgLight<TBase>> {
		LuckyLight() {
			this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/RectangleLight.svg")));
		}

		void drawHalo(const DrawArgs& args) override {
			// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser.
			if (args.fb)
				return;

			const float halo = settings::haloBrightness;
			if (halo == 0.f)
				return;

			// If light is off, rendering the halo gives no effect.
			if (this->color.a == 0.f)
				return;
		
			float br = 30.0;	// Blur radius.
			float cr = 5.0;		// Corner radius.

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
		m = module;
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
		addParam(createParamCentered<ProbKnob>(mm2px(Vec(45.72f, 61.369f)), module, WolframModule::PROB_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48f, 80.597f)), module, WolframModule::OFFSET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62f, 80.597f)), module, WolframModule::X_SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(53.34f, 80.597f)), module, WolframModule::Y_SCALE_PARAM));
		// Inputs
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 22.14f)), module, WolframModule::RESET_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(30.48f, 99.852f)), module, WolframModule::OFFSET_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(30.48f, 114.852f)), module, WolframModule::PROB_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(41.91f, 114.852f)), module, WolframModule::RULE_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(53.34f, 114.852f)), module, WolframModule::ALGO_CV_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 114.852f)), module, WolframModule::TRIG_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(19.05f, 114.852f)), module, WolframModule::INJECT_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 99.852f)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(19.05f, 99.852f)), module, WolframModule::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 99.852f)), module, WolframModule::Y_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.91f, 99.852f)), module, WolframModule::Y_PULSE_OUTPUT));
		// LEDs
		addChild(createLightCentered<DiagonalLuckyLight<RedLight>>(mm2px(Vec(53.34f, 47.767f)), module, WolframModule::MODE_LIGHT)); 
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(7.62f, 90.225f)), module, WolframModule::X_CV_LIGHT));		
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(19.05f, 90.225f)), module, WolframModule::X_PULSE_LIGHT));	
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(41.91f, 90.225f)), module, WolframModule::Y_PULSE_LIGHT));	
		addChild(createLightCentered<LuckyLight<RedLight>>(mm2px(Vec(53.34f, 90.225f)), module, WolframModule::Y_CV_LIGHT));
		
		Display* display = new Display(module, mm2px(10.14f), box.size.x, mm2px(32.f));
		addChild(display);

		//DisplayFramebuffer* displayFb = new DisplayFramebuffer(module);
		//displayFb->addChild(display);
		//addChild(displayFb);
	}

	void appendContextMenu(Menu* menu) override {
		WolframModule* module = dynamic_cast<WolframModule*>(this->module);
		
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Sync", "", &module->sync));
		menu->addChild(createSubmenuItem("Slew", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("X", "", &module->slewX));
				menu->addChild(createBoolPtrMenuItem("Y", "", &module->slewY));
			} 
		));
	
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
			{ "Redrick", "OLED", "Rack", "Eva", "Purple", "Lamp", "Mono"},
			[=]() {
				return module->lookAndFeel.displayStyleIndex;
			},
			[=](int i) {
				module->lookAndFeel.displayStyleIndex = i;
			} 
		));

		menu->addChild(createIndexSubmenuItem("Cells",
			{ "LED", "Block"},
			[=]() {
				return module->lookAndFeel.cellStyleIndex;
			},
			[=](int i) {
				module->lookAndFeel.cellStyleIndex = i;
			}
		));
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");