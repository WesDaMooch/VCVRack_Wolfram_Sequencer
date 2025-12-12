// Make with: export RACK_DIR=/home/wes-l/Rack-SDK

// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg

// TODO: maybe this
// Therefore, modules with a CLOCK and RESET input, or similar variants, should ignore CLOCK triggers up to 1 ms 
// after receiving a RESET trigger.You can use dsp::Timer for keeping track of time.
// Or do what SEQ3 does?

// Could have different structs or class for each algo with a void setParameters()
// See Befaco NoisePlethora

// TODO: The panel
// TODO: Save state
// TODO: onReset and onRandomize
// TODO: Some anti-aliasing for vco mode, BLEP BEEP BOOP or whatever
// TODO: inject puts 4 in when in life algo?
// TODO: Font - new O, < or >, new % maybe?
// TODO: seperate x y slew bools contex menu

#include "wolfram.hpp"
#include <vector>

class SlewLimiter {
public:

	void setSlewAmountMs(float s_ms, float sr) {
		slew = (1000.f / sr) / clamp(s_ms, 1e-3f, 1000.f);
	}

	void reset() { y = 0; }

	float process(float x) {
		y += clamp(x - y, -slew, slew);
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
	
	WolfEngine wolfEngine;
	LifeEngine lifeEngine;

	LookAndFeel lookAndFeel;
	WolfUI wolfUI;
	LifeUI lifeUI;

	static constexpr int NUM_ALGOS = 2;
	std::array<AlgoEngine*, NUM_ALGOS> engine{};
	std::array<AlgoUI*, NUM_ALGOS> ui{};
	AlgoEngine* a; // Meh..

	// Algorithms
	static constexpr int algoDefault = 0;
	int algoIndex = 0;
	int algoSelect = algoDefault;
	int algoCV = 0;

	// Menu
	LookAndFeel::Page algoPage;
	LookAndFeel::Page slewPage;
	// TODO: Mini page here
	std::vector<LookAndFeel::Page*> menuPages;
	int pageCounter = 0;
	int pageIndex = 0;
	bool menu = false;
	
	// Params
	int sequenceLength = 8;
	std::array<int, 9> sequenceLengths { 2, 3, 4, 6, 8, 12, 16, 32, 64 };

	float prob = 1;
	int offset = 0;
	int offsetPending = 0;
	bool resetPending = false;
	bool seedPushPending = false;
	int injectPendingState = 0;
	bool gen = false;
	bool genPending = false;
	int algoCvPending = 0;
	float lastTrigVoltage = 0.f;

	int slewParam = 0;
	bool slewX = true;
	bool slewY = true;
	bool sync = false;
	bool vcoMode = false;
	
	// Select encoder
	static constexpr float encoderIndent = 1.f / 30.f;
	float prevEncoderValue = 0.f;
	bool encoderReset = false;
	
	// Display
	bool displayRule = false;
	bool ruleMod = true;
	bool algoMod = false;

	// Samplerate
	float samplerate = 44100;

	dsp::PulseGenerator xPulse, yPulse;
	dsp::SchmittTrigger trigTrigger, resetTrigger, posInjectTrigger, negInjectTrigger;
	dsp::BooleanTrigger menuTrigger, modeTrigger;
	dsp::Timer ruleDisplayTimer;
	dsp::RCFilter dcFilter[2];
	SlewLimiter slewLimiter[2];

	WolframModule() 
		: wolfUI(&wolfEngine, &lookAndFeel),
		lifeUI(&lifeEngine, &lookAndFeel)
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(MENU_PARAM, "Menu");
		configButton(MODE_PARAM, "Mode");
		configParam<EncoderParamQuantity>(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Rule");
		configParam<LengthParamQuantity>(LENGTH_PARAM, 0.f, 8.f, 4.f, "Length");
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(PROB_PARAM, 0.f, 1.f, 1.f, "Prob", "%", 0.f, 100.f);
		paramQuantities[PROB_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(X_SCALE_PARAM, 0.f, 1.f, 0.5f, "X CV Scale", "V", 0.f, 10.f);
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 1.f, 0.5f, "Y CV Scale", "V", 0.f, 10.f);
		paramQuantities[Y_SCALE_PARAM]->displayPrecision = 3;
		configInput(RESET_INPUT, "Reset");
		configInput(PROB_CV_INPUT, "Prob CV");
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

		// Alogrithms
		engine[0] = &wolfEngine;
		engine[1] = &lifeEngine;
		ui[0] = &wolfUI;
		ui[1] = &lifeUI;

		for (int i = 0; i < NUM_ALGOS; i++)
			engine[i]->updateRule(0, true);

		// Set default alogrithm
		a = engine[algoIndex];

		// Push matrix
		engine[algoIndex]->updateMatrix(sequenceLength, offset, false);
		
		// Algo and Slew menu pages
		lookAndFeel.makeMenuPage(
			algoPage,
			"menu",
			"ALGO",
			[this] { return engine[algoIndex]->getAlgoName(); },
			[this](int d, bool r) { setAlgoSelect(d, r); }
		);

		lookAndFeel.makeMenuPage(
			slewPage,
			"menu",
			"SLEW",
			[this] { return std::to_string(slewParam) + "%"; },
			[this](int d, bool r) { setSlewSelect(d, r); }
		);

		updateMenu();
		onSampleRateChange();
	}

	struct EncoderParamQuantity : ParamQuantity {
		// Custom behaviour to display rule when hovering over Select encoder 
		std::string getDisplayValueString() override {

			auto* m = dynamic_cast<WolframModule*>(module);
			if (!m)
				return "";

			bool algoMod = m->algoMod;
			bool ruleMod = m->ruleMod;
			bool menu = m->menu;
			int page = m->pageIndex;

			std::string ruleStr = m->a->getRuleName();
			std::string selectStr = m->a->getRuleSelectName();

			// No algorithm modulation & no rule modulation
			if (!algoMod && !ruleMod)
				return selectStr;

			// Menu closed or no algorithm modulation
			if (!algoMod || !menu) {
				if (!ruleMod)
					return selectStr;

				return selectStr + " ( " + ruleStr + " )";
			}

			// Menu open & algorithm modulation
			if (page == 0) {
				// Wolf rule page
				std::string s = m->engine[0]->getRuleSelectName();
				std::string r = ruleMod ? 
					m->engine[0]->getRuleName() :
					ruleStr;

				return s + " ( " + r + " )";
			}

			if (page == 3) {
				// Life rule page
				std::string s = m->engine[1]->getRuleSelectName();
				std::string r = ruleMod ? 
					m->engine[1]->getRuleName() :
					ruleStr;

				return s + " ( " + r + " )";
			}

			// Non-rule pages
			if (!ruleMod)
				return selectStr;

			return selectStr + " ( " + ruleStr + " )";
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

	void updateMenu() {

		menuPages.clear();

		// if (newAlgoMod == algoMod)
		// return;

		if (!algoMod) {
			menuPages.push_back(ui[algoIndex]->getSeedPage());
			menuPages.push_back(ui[algoIndex]->getModePage());
		}
		else {
			for (int i = 0; i < NUM_ALGOS; i++) {
				menuPages.push_back(ui[i]->getRulePage());
				menuPages.push_back(ui[i]->getSeedPage());
				menuPages.push_back(ui[i]->getModePage());
			}
		}
		menuPages.push_back(&slewPage);
		menuPages.push_back(&algoPage);
	}

	void updateAlgo() {
		algoIndex = clamp(algoSelect + algoCV, 0, (NUM_ALGOS - 1));
		a = engine[algoIndex];
		a->updateMatrix(sequenceLength, offset, false);

		updateMenu();
	}

	void setAlgoCV(float cv) {
		int newCV = std::round(cv * (NUM_ALGOS - 1));

		if (newCV == algoCV)
			return;

		if (sync) {
			algoCvPending = newCV;
			return;
		}

		algoCV = newCV;
		updateAlgo();
	}

	void setAlgoSelect(int delta, bool reset) {
		int newSelect = a->updateSelect(algoSelect,
			NUM_ALGOS, algoDefault, delta, reset);

		if (newSelect == algoSelect)
			return;

		algoSelect = newSelect;
		updateAlgo();
	}

	void setSlewSelect(int delta, bool reset) {
		slewParam = clamp(slewParam + delta, 0, 200);

		if (reset)
			slewParam = 0;

		// Convert slewParam (0 - 200) to ms (0, 2000),
		// or (0 - 20) if vcoMode is true.
		float slew = vcoMode ? (slewParam * 0.1f) : (slewParam * 10.f);
		for (int i = 0; i < 2; i++)
			slewLimiter[i].setSlewAmountMs(slew, samplerate);
	}

	void onReset(const ResetEvent& e) override {
		
		Module::onReset(e);
		
		sync = true;
		vcoMode = false;
		pageCounter = 0;

		setSlewSelect(0, true);
		setAlgoSelect(0, true);

		a->setReadHead(0);
		a->setWriteHead(1);
		a->setDisplayMatrix(0);

		for (int i = 0; i < NUM_ALGOS; i++) {
			engine[i]->setBufferFrame(0, 0, true);
			engine[i]->updateRule(0, true);
			engine[i]->updateSeed(0, true);
			engine[i]->upateMode(0, true);
			engine[i]->pushSeed(false);
			engine[i]->updateMatrix(sequenceLength, offset, false);
		}

		for (int i = 0; i < 2; i++) {
			dcFilter[i].reset();
			slewLimiter[i].reset();
		}
	}

	void onSampleRateChange() override {
		samplerate = APP->engine->getSampleRate();

		// Set DC blocker to ~10Hz,
		// calculate slew time
		for (int i = 0; i < 2; i++) {
			dcFilter[i].setCutoffFreq(10.f / samplerate);
			slewLimiter[i].setSlewAmountMs(vcoMode ? (slewParam * 0.1f) : (slewParam * 10.f), samplerate);
		}
	}

	/*
	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		
		// Save sequencer settings
		json_object_set_new(rootJ, "sync", json_boolean(sync));
		json_object_set_new(rootJ, "vcoMode", json_boolean(vcoMode));
		json_object_set_new(rootJ, "slew", json_integer(slewParam));

		// Save algorithm settings
		json_object_set_new(rootJ, "algo", json_integer(algoSelect));
		json_object_set_new(rootJ, "readHead", json_integer(engine[algoIndex]->getReadHead()));
		json_object_set_new(rootJ, "writeHead", json_integer(engine[algoIndex]->getWriteHead()));
		json_object_set_new(rootJ, "displayMatrix", json_integer(engine[algoIndex]->getDisplayMatrix()));

		// Save algorithm specifics
		json_t* rulesJ = json_array();
		json_t* seedsJ = json_array();
		json_t* modesJ = json_array();
		json_t* buffersJ = json_array();

		for (int i = 0; i < NUM_ALGOS; i++) {
			json_array_append_new(rulesJ, json_integer(engine[i]->getRuleSelect()));
			json_array_append_new(seedsJ, json_integer(engine[i]->getSeed()));
			json_array_append_new(modesJ, json_integer(engine[i]->getMode()));

			// Save buffers
			json_t* rowJ = json_array();
			for (int j = 0; j < 64; j++)
				json_array_append_new(rowJ, json_integer(engine[i]->getBufferFrame(j)));

			json_array_append_new(buffersJ, rowJ);
		}

		json_object_set_new(rootJ, "rules", rulesJ);
		json_object_set_new(rootJ, "seeds", seedsJ);
		json_object_set_new(rootJ, "modes", modesJ);
		json_object_set_new(rootJ, "buffers", buffersJ);
 		
		// Save LookAndFeel settings
		json_object_set_new(rootJ, "look", json_integer(lookAndFeel.lookIndex));
		json_object_set_new(rootJ, "cellStyle", json_integer(lookAndFeel.cellStyleIndex));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Load sequencer settings
		json_t* syncJ = json_object_get(rootJ, "sync");
		if (syncJ)
			sync = json_boolean_value(syncJ);

		json_t* vcoModeJ = json_object_get(rootJ, "vcoMode");
		if (vcoModeJ)
			vcoMode = json_boolean_value(vcoModeJ);

		json_t* slewParamJ = json_object_get(rootJ, "slew");
		if (slewParamJ)
			slewParam = json_integer_value(slewParamJ);

		// Load algorithm settings
		json_t* algoSelectJ = json_object_get(rootJ, "algo");
		if (algoSelectJ) {
			algoSelect = json_integer_value(algoSelectJ);
			updateAlgo();
		}

		json_t* readHeadJ = json_object_get(rootJ, "readHead");
		if (readHeadJ)
			engine[algoIndex]->setReadHead(json_integer_value(readHeadJ));
		
		json_t* writeHeadJ = json_object_get(rootJ, "writeHead");
		if (writeHeadJ)
			engine[algoIndex]->setWriteHead(json_integer_value(writeHeadJ));

		json_t* displayMatrixJ = json_object_get(rootJ, "displayMatrix");
		if (displayMatrixJ)
			engine[algoIndex]->setDisplayMatrix(static_cast<uint64_t>(json_integer_value(displayMatrixJ)));

		// Load algorithm specifics
		json_t* rulesJ = json_object_get(rootJ, "rules");
		json_t* seedsJ = json_object_get(rootJ, "seeds");
		json_t* modesJ = json_object_get(rootJ, "modes");
		json_t* buffersJ = json_object_get(rootJ, "buffers");

		for (int i = 0; i < NUM_ALGOS; i++) {
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
							engine[i]->setBufferFrame(json_integer_value(valueJ), j, false);
					}
				}
				
			}
		}

		// Load LookAndFeel settings
		json_t* lookJ = json_object_get(rootJ, "look");
		if (lookJ)
			lookAndFeel.lookIndex = json_integer_value(lookJ);

		json_t* cellStyleJ = json_object_get(rootJ, "cellStyle");
		if (cellStyleJ)
			lookAndFeel.cellStyleIndex = json_integer_value(cellStyleJ);

	}
	*/

	void processEncoder() {
		const float encoderValue = params[SELECT_PARAM].getValue();
		float difference = encoderValue - prevEncoderValue;
		int delta = static_cast<int>(std::round(difference / encoderIndent));
			
		if ((delta == 0) && !encoderReset)
			return;
		
		prevEncoderValue += delta * encoderIndent;
		
		if (menu) {
			menuPages[pageIndex]->set(delta, encoderReset);
		}
		else {
			// Rule select
			a->updateRule(delta, false);
			if (sync) {
				seedPushPending = true;
			}
			else {
				gen = random::get<float>() < prob;
				genPending = true;
				seedPushPending = false;
				if (gen) {
					a->pushSeed(false);
					a->updateMatrix(sequenceLength, offset, false);
				}
			}
			if (!encoderReset) {
				displayRule = true;
				ruleDisplayTimer.reset();
			}
		}
		
		encoderReset = false;
	}

	void process(const ProcessArgs& args) override {

		// Knobs & CV inputs
		bool newAlgoMod = inputs[ALGO_CV_INPUT].isConnected();
		if (newAlgoMod != algoMod) {
			algoMod = newAlgoMod;
			updateMenu();
		}

		setAlgoCV(clamp(inputs[ALGO_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		ruleMod = inputs[RULE_CV_INPUT].isConnected();
		a->setRuleCV(clamp(inputs[RULE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		float probCV = clamp(inputs[PROB_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		prob = clamp(params[PROB_PARAM].getValue() + probCV, 0.f, 1.f);

		sequenceLength = sequenceLengths[static_cast<int>(params[LENGTH_PARAM].getValue())];

		float offsetCv = clamp(inputs[OFFSET_CV_INPUT].getVoltage(), -10.f, 10.f) * 0.8f; // cast to int here
		int newOffset = clamp(static_cast<int>(std::round(params[OFFSET_PARAM].getValue() + offsetCv)), -4, 4);
		if (sync) {
			offsetPending = newOffset;
		}
		else if (offset != newOffset) {
			offset = newOffset;
			a->updateMatrix(sequenceLength, offset, false);
		}

		// Buttons
		if (menuTrigger.process(params[MENU_PARAM].getValue()))
			menu = !menu;

		if (modeTrigger.process(params[MODE_PARAM].getValue())) {
			if (menu) {
				pageCounter++;
			}
			else {
				a->upateMode(1, false);
			}
		}

		pageIndex = (pageCounter + menuPages.size()) % menuPages.size();

		processEncoder();

		// Trigger inputs
		bool positiveInject = posInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), 0.1f, 2.f);
		bool negativeInject = negInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), -2.f, -0.1f);
		if (positiveInject || negativeInject) {
			if (sync) {
				if (positiveInject) {
					injectPendingState = 1;
				}
				else {
					injectPendingState = 2;
				}
			}
			else {
				a->inject(positiveInject ? true : false, false);
				a->updateMatrix(sequenceLength, offset, false);
				injectPendingState = 0;
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
					a->pushSeed(false);
				}
				else {
					a->setReadHead(0);
					a->setWriteHead(1);
				}
				a->updateMatrix(sequenceLength, offset, false);
			}
		}

		bool trig = false;
		float trigVotagte = inputs[TRIG_INPUT].getVoltage();
		if (vcoMode) {
			// Zero crossing
			trig = (trigVotagte > 0.f && lastTrigVoltage <= 0.f) || (trigVotagte < 0.f && lastTrigVoltage >= 0.f);
		}
		else {
			// Trigger pulse
			trig = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f);
		}
		lastTrigVoltage = trigVotagte;

		// TODO: do the dont trigger when reseting thing SEQ3
		// STEP
		if (trig) {
			// Find gen if not found when reset triggered and sync off
			if (!genPending)
				gen = random::get<float>() < prob;

			if (sync) {
				algoCV = algoCvPending;
				updateAlgo();

				offset = offsetPending;

				if (injectPendingState == 1) {
					a->inject(true, true);
				}
				else if (injectPendingState == 2) {
					a->inject(false, true);
				}

				if (resetPending || seedPushPending) {
					if (gen) {
						a->pushSeed(true);
					}
					else if (!seedPushPending) {
						a->setWriteHead(0);
					}
				}
			}

			if (gen && !resetPending && !seedPushPending && !injectPendingState)
				a->generate();

			engine[algoIndex]->updateMatrix(sequenceLength, offset, true);

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			seedPushPending = false;
			injectPendingState = 0;
			algoCvPending = 0;
		}

		// OUTPUT
		float xCv = a->getXVoltage();
		float yCv = a->getYVoltage();

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
		xCv = vcoMode ? xAudio : xCv;
		yCv = vcoMode ? yAudio : yCv;

		/*
		if (vcoMode) {
			// TODO should be processing all the time?
			// Or call reset when vcoMode is changed?
			xCv -= 0.5f;
			dcFilter[0].process(xCv);
			xCv = dcFilter[0].highpass();

			yCv -= 0.5f;
			dcFilter[1].process(yCv);
			yCv = dcFilter[1].highpass();
		}
		*/

		// CV outputs - 0V to 10V or -5V to 5V in VCO mode (10Vpp).
		xCv = xCv * params[X_SCALE_PARAM].getValue() * 10.f;
		yCv = yCv * params[Y_SCALE_PARAM].getValue() * 10.f;
		outputs[X_CV_OUTPUT].setVoltage(xCv);
		outputs[Y_CV_OUTPUT].setVoltage(yCv);

		// Pulse outputs - 0V to 10V.
		// TODO: goes HIGH when triggered fast (vcoMode)
		if (a->getXPulse())
			xPulse.trigger(1e-3f);

		if (a->getYPulse())
			yPulse.trigger(1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(a->getModeLEDValue(), args.sampleTime);
		lights[X_CV_LIGHT].setBrightness(xCv * 0.1);
		lights[X_PULSE_LIGHT].setBrightnessSmooth(xGate, args.sampleTime);
		lights[Y_CV_LIGHT].setBrightness(yCv * 0.1);
		lights[Y_PULSE_LIGHT].setBrightnessSmooth(yGate, args.sampleTime);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		lights[INJECT_LIGHT].setBrightnessSmooth(positiveInject | negativeInject, args.sampleTime);
		
		// Hide rule display
		if (displayRule && ruleDisplayTimer.process(args.sampleTime) > 0.75f)
			displayRule = false;

		engine[algoIndex]->tick();
	}
};

/*
struct DisplayFramebuffer : FramebufferWidget {
	WolframModule* module;

	int prevLook = 0;
	int prevFeel = 0;
	bool prevMenu = false;
	int prevPage = 0;
	bool prevDisplayRule = false;
	uint64_t prevMatrix = 0;

	DisplayFramebuffer(WolframModule* m) { module = m; }

	void step() override {

		//int look = module ? module->lookAndFeel.getLookIndex() : 0;
		//int feel = module ? module->lookAndFeel.getFeelIndex() : 0;

		int look = module ? module->lookAndFeel.lookIndex : 0;
		int feel = module ? module->lookAndFeel.feelIndex : 0;

		bool menu = module ? module->menu : false;
		int page = module ? module->pageCounter : 0;
		bool displayRule = module ? module->displayRule : false;
		uint64_t matrix = module ? module->a->getDisplayMatrix() : 0;

		bool dirty = false //module ? module->lookAndFeel.getRedrawBg() : false;
		dirty |= (look != prevLook);
		dirty |= (feel != prevFeel);
		dirty |= (menu != prevMenu);
		if (menu) {
			dirty |= (page != prevPage);
		}
		else {
			dirty |= (displayRule != prevDisplayRule);
			dirty |= (matrix != prevMatrix);

			prevDisplayRule = displayRule;
			prevMatrix = matrix;
		}

		dirty = false;

		prevLook = look;
		prevFeel = feel;
		prevMenu = menu;
		prevPage = page;
		
		if (dirty) {
			FramebufferWidget::dirty = true;
			dirty = false;
		}
		FramebufferWidget::step();
	}
};
*/

struct Display : TransparentWidget {
	WolframModule* module;
	LookAndFeel* lookAndFeel;

	std::shared_ptr<Font> font;
	std::string fontPath;

	int cols = 8;
	int rows = 8;
	float padding = 1.f;
	float cellSize = 5.f;

	float widgetSize = 0;
	float fontSize = 0;

	// TODO: std pair for col and row (int, int)
	std::vector<Vec> aliveCellCord{};

	Display(WolframModule* m, float y, float w, float s) {
		module = m;

		/* Wiget params */
		widgetSize = s;
		float screenSize = widgetSize - (padding * 2.f);
		fontSize = (screenSize / cols) * 2.f;

		float cellPadding = (screenSize / cols);

		// Widget size
		box.pos = Vec((w * 0.5f) - (widgetSize * 0.5f), y);
		box.size = Vec(widgetSize, widgetSize);

		// Get font 
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/mtf_wolf5.otf"));
		font = APP->window->loadFont(fontPath);

		if (!module)
			return;
		// Set look & feel
		lookAndFeel = &module->lookAndFeel;
		lookAndFeel->padding = padding;
		lookAndFeel->fontSize = fontSize;
		lookAndFeel->cellSpacing = cellPadding;
		lookAndFeel->init();
	}

	void drawMenuBackgroud(const DrawArgs& args, int i) {
		if (module->menu) {
			int pageIndex = module->pageIndex;
			module->menuPages[pageIndex]->bg(args.vg);
			return;
		}

		if (module->displayRule) {
			int algoIndex = module->algoIndex;
			LookAndFeel::Page* miniPage = module->ui[algoIndex]->getMiniPage();
			miniPage->bg(args.vg);
		}
	}

	void drawMenuText(const DrawArgs& args, int i) {
		if (module->menu) {
			int pageIndex = module->pageIndex;

			module->menuPages[pageIndex]->fg(args.vg, module->algoMod);
			return;
		}

		if (module->displayRule) {
			int algoIndex = module->algoIndex;

			LookAndFeel::Page* miniPage = module->ui[algoIndex]->getMiniPage();
			miniPage->fg(args.vg, false);
		}
	}

	void draw(const DrawArgs& args) override {
		// Draw background & border.
		NVGcolor backgroundColour = module ? *lookAndFeel->getScreenColour() : nvgRGB(58, 16, 19);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, padding * 0.5f, padding * 0.5f,
			widgetSize - padding, widgetSize - padding, 2.f);
		nvgFillColor(args.vg, backgroundColour);
		nvgFill(args.vg);

		nvgStrokeWidth(args.vg, padding);
		nvgStrokeColor(args.vg, nvgRGB(16, 16, 16));
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		// Draw dead cells & menu background,
		// setup drawing alive cells on the self-illuminating layer
		aliveCellCord.clear();

		int firstRow = 0;
		if (module) {
			if (module->menu || module->displayRule)
				drawMenuBackgroud(args, module->pageIndex);

			if (module->menu)
				return;

			firstRow = module->displayRule ? (rows - 4) : 0;
		}

		// Requires flipping
		uint64_t matrix = module ? module->a->getDisplayMatrix() : 8;

		// Draw dead cell, store alive cell 
		nvgBeginPath(args.vg);
		for (int row = firstRow; row < rows; row++) {
			int rowInvert = (row - 7) * -1;
			for (int col = 0; col < cols; col++) {
				int colInvert = 7 - col;
				int cellIndex = rowInvert * 8 + colInvert;

				if ((matrix >> cellIndex) & 1ULL) {
					// Store alive cell coordinates
					aliveCellCord.emplace_back(Vec(col, row));
				}
				else {
					// Draw dead cells
					if (module)
						lookAndFeel->drawCell(args.vg, col, row, false);
				}
			}
		}
		nvgFill(args.vg);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

		if (module && (module->menu || module->displayRule))
			drawMenuText(args, module->pageIndex);

		// Draw alive cells.
		nvgBeginPath(args.vg);
		for (const Vec& pos : aliveCellCord) {
			int col = pos.x;
			int row = pos.y;
			if (module)
				lookAndFeel->drawCell(args.vg, col, row, true);
		}
		nvgFill(args.vg);

		Widget::drawLayer(args, layer);
	}
};


struct WolframModuleWidget : ModuleWidget {
	WolframModule* m;

	// Custom knobs & dials.
	struct LengthKnob : RoundLargeBlackKnob {
		LengthKnob() {
			minAngle = -0.75f * M_PI;
			maxAngle = 0.5f * M_PI;
		}
	};

	struct ProbKnob : RoundLargeBlackKnob {
		ProbKnob() {
			minAngle = -0.75f * M_PI;
			maxAngle = 0.75f * M_PI;
		}
	};

	struct SelectEncoder : RoundBlackKnob {
		void onDoubleClick(const DoubleClickEvent& e) override {
			// Reset Select encoder to default of current selection,
			// see processEncoder for details.
			auto* m = static_cast<WolframModule*>(module);
			m->encoderReset = true;
		}

	};

	// Count Modula's custom LEDs.
	template <typename TBase>
	struct RectangleLedDiagonal : TBase {

		// -45 degree rotation.
		float rotation = -M_PI / 4.f;

		void drawBackground(const DrawArgs& args) override {
			nvgSave(args.vg);
			nvgTranslate(args.vg, this->box.size.x / 2.f, this->box.size.y / 2.f);
			nvgRotate(args.vg, rotation);
			nvgTranslate(args.vg, -this->box.size.x / 2.f, -this->box.size.y / 2.f);

			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0, 0, this->box.size.x, this->box.size.y);

			// Background
			if (this->bgColor.a > 0.0) {
				nvgFillColor(args.vg, this->bgColor);
				nvgFill(args.vg);
			}

			// Border
			if (this->borderColor.a > 0.0) {
				nvgStrokeWidth(args.vg, 0.5);
				nvgStrokeColor(args.vg, this->borderColor);
				nvgStroke(args.vg);
			}
			nvgRestore(args.vg);
		}

		void drawLight(const DrawArgs& args) override {
			nvgSave(args.vg);
			nvgTranslate(args.vg, this->box.size.x / 2.f, this->box.size.y / 2.f);
			nvgRotate(args.vg, rotation);
			nvgTranslate(args.vg, -this->box.size.x / 2.f, -this->box.size.y / 2.f);

			// Foreground
			if (this->color.a > 0.0) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0, 0, this->box.size.x, this->box.size.y);

				nvgFillColor(args.vg, this->color);
				nvgFill(args.vg);
			}
			nvgRestore(args.vg);
		}
	};

	// TODO: Rename based on the name of the LED on switch electronics
	template <typename TBase>
	struct WolframLed : TBase {

		//WolframModule* module = nullptr;

		WolframLed() {
			this->box.size = mm2px(Vec(5.f, 2.f));
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

			//NVGcolor colour = module ? *module->lookAndFeel.getForegroundColour() :
			//						   TBase::color;

			nvgBeginPath(args.vg);
			nvgRect(args.vg, -br, -br, this->box.size.x + 2 * br, this->box.size.y + 2 * br);
			//NVGcolor icol = color::mult(TBase::color, halo);
			NVGcolor icol = color::mult(TBase::color, halo);
			NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
			nvgFillPaint(args.vg, nvgBoxGradient(args.vg, 0, 0, this->box.size.x, this->box.size.y, cr, br, icol, ocol));
			nvgFill(args.vg);
		}
	};

	WolframModuleWidget(WolframModule* module) {
		m = module;
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/wolframLight.svg")));

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
		addChild(createLightCentered<RectangleLedDiagonal<WolframLed<RedLight>>>(mm2px(Vec(53.34f, 47.767f)), module, WolframModule::MODE_LIGHT)); 
		addChild(createLightCentered<RectangleLight<WolframLed<RedLight>>>(mm2px(Vec(7.62f, 90.225f)), module, WolframModule::X_CV_LIGHT));		
		addChild(createLightCentered<RectangleLight<WolframLed<RedLight>>>(mm2px(Vec(19.05f, 90.225f)), module, WolframModule::X_PULSE_LIGHT));	
		addChild(createLightCentered<RectangleLight<WolframLed<RedLight>>>(mm2px(Vec(41.91f, 90.225f)), module, WolframModule::Y_PULSE_LIGHT));	
		addChild(createLightCentered<RectangleLight<WolframLed<RedLight>>>(mm2px(Vec(53.34f, 90.225f)), module, WolframModule::Y_CV_LIGHT));		

		
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
		} ));
		menu->addChild(createBoolPtrMenuItem("VCO Mode", "", &module->vcoMode));

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Look",
			{ "Redrick", "OLED", "Rack", "Eva", "Purple", "Mono"},
			[ = ]() {
				return module->lookAndFeel.lookIndex;
			},
			[ = ](int i) {
				module->lookAndFeel.lookIndex = i;
			} 
		));

		menu->addChild(createIndexSubmenuItem("Cell Style",
			{ "Circle", "Square"},
			[ = ]() {
				return module->lookAndFeel.cellStyleIndex;
			},
			[ = ](int i) {
				module->lookAndFeel.cellStyleIndex = i;
			}
		));
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");