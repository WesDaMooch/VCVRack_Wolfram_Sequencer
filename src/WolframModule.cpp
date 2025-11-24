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
// TODO: Slew Amount, where Func is, put func (vco mode in context menu), could use the same slew/filter in vco mode?
// TODO: Put sync mode in contex menu?
// TODO: Some anti-aliasing for vco mode, BLEP BEEP BOOP or whatever

// TODO: Looks change Led colour

#include "wolfram.hpp"
#include <vector>

struct SlewLimiter {
	float y = 0.f;
	float slew = 20.f;

	void setSlewAmountMs(float s_ms, float sr) {
		slew = (1000.f / sr) / clamp(s_ms, 1e-3f, 1000.f);
	}

	float process(float x) {
		y += clamp(x - y, -slew, slew);
		return y;
	}

	void reset() {
		y = 0;
	}
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

	//AlgoithmDispatcher algo; 
	LookAndFeel lookAndFeel;
	MenuPages menuPages = MenuPages::SEED;

	WolfAlgoithm wolf;
	LifeAlgoithm life;

	static constexpr int NUM_ALGORITHMS = 2;
	std::array<AlgorithmBase*, NUM_ALGORITHMS> algorithms{};
	AlgorithmBase* activeAlogrithm;
	int algorithmIndex = 0;
	int algorithmSelect = 0;
	int algorithmCV = 0;

	//int offset = 0;

	// Drawing 
	//LookAndFeel lookAndFeel;
	//LookAndFeel* lookAndFeel;

	//float padding = 0;
	//float fontSize = 0;

	// 
	dsp::PulseGenerator xPulse;
	dsp::PulseGenerator yPulse;
	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger posInjectTrigger;
	dsp::SchmittTrigger negInjectTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::BooleanTrigger menuTrigger;
	dsp::BooleanTrigger modeTrigger;
	dsp::Timer ruleDisplayTimer;
	dsp::RCFilter dcFilter[2];

	SlewLimiter slewLimiter[2];

	int sequenceLength = 8;
	std::array<int, 9> sequenceLengths { 2, 3, 4, 6, 8, 12, 16, 32, 64 };
	float prob = 1;
	int offset = 0;
	int offsetPending = 0;
	bool resetPending = false;
	bool ruleResetPending = false;
	int injectPendingState = 0;
	bool gen = false;
	bool genPending = false;
	float lastTrigVoltage = 0.f;
	float sr = 0;

	int slewParam = 0;
	//float slewAmount = 0;
	bool sync = false;
	bool menu = false;
	bool vcoMode = false;
	
	// Encoder
	static constexpr float encoderIndent = 1.f / 20.f;
	float prevEncoderValue = 0.f;
	bool encoderReset = false;
	
	// Display
	bool displayRule = false;

	// is this really nessisary 
	inline int fastRoundInt(float value) {
		return static_cast<int>(value + (value >= 0.f ? 0.5f : -0.5f));
	}


	// 

	void setAlgoithmCV(float cv) {
		algorithmCV = std::round(cv * (NUM_ALGORITHMS - 1));
	}

	void algoithmUpdate(int delta, bool reset) {
		if (reset) {
			algorithmSelect = 0;
		}
		else {
			algorithmSelect = (algorithmSelect + delta + NUM_ALGORITHMS) % NUM_ALGORITHMS;
		}
	}

	void tick() {
		algorithmIndex = clamp(algorithmSelect + algorithmCV, 0, (NUM_ALGORITHMS - 1));
		activeAlogrithm = algorithms[algorithmIndex];
		activeAlogrithm->Tick();
	}

	// Common algoithm specific functions.
	//void setReadHead(int readHead) { activeAlogrithm->SetReadHead(readHead); }
	//void setWriteHead(int writeHead) { activeAlogrithm->SetWriteHead(writeHead); }
	//uint64_t getDisplayMatrix() { return activeAlogrithm->GetDisplayMatrix(); }

	//void setLookAndFeel(LookAndFeel* l) {
	//	//lookAndFeel = l;
	//	//for (int i = 0; i < NUM_ALGORITHMS; i++) {
	//	//	algorithms[i]->SetLookAndFeel(lookAndFeel);
	//	//}
	//}

	WolframModule() {
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

		// Alogrithms.
		algorithms[0] = &wolf;
		algorithms[1] = &life;

		// Set default alogrithm.
		activeAlogrithm = algorithms[algorithmIndex];

		// Set alogrithm's Look and Feel.
		for (int i = 0; i < NUM_ALGORITHMS; i++) {
			algorithms[i]->SetLookAndFeel(&lookAndFeel);
		}

		activeAlogrithm->Update(offset);
		onSampleRateChange();
	}

	struct EncoderParamQuantity : ParamQuantity {
		// Custom behaviour to display rule when hovering over Select encoder.
		std::string getDisplayValueString() override {
			auto* m = dynamic_cast<WolframModule*>(module);
			//return m ? m->algo.getRuleString() : "30";

			return m ? m->activeAlogrithm->GetRuleString() : "30";
		}

		// Suppress behaviour.
		void setDisplayValueString(std::string s) override {}
	};

	struct LengthParamQuantity : ParamQuantity {
		// Custom behaviour to display sequence length when hovering over Length dial.
		float getDisplayValue() override {
			auto* m = dynamic_cast<WolframModule*>(module);
			return m ? m->sequenceLength : 8;
		}

		// Suppress behaviour.
		void setDisplayValueString(std::string s) override {}
	};

	void onSampleRateChange() override {
		sr = APP->engine->getSampleRate();

		// set DC blocker to ~10Hz.
		for (int i = 0; i < 2; i++) {
			dcFilter[i].setCutoffFreq(10.f / sr);
			slewLimiter[i].setSlewAmountMs(vcoMode ? (slewParam * 0.075f) : (slewParam * 10.f), sr);
		}

		
	}

	void processEncoder() {
		const float encoderValue = params[SELECT_PARAM].getValue();
		float difference = encoderValue - prevEncoderValue;
		int delta = static_cast<int>(std::round(difference / encoderIndent));
			
		if ((delta == 0) && !encoderReset)
			return;
		
		if (delta != 0)
			prevEncoderValue += delta * encoderIndent;

		if (menu) {
			switch (menuPages) {
			case SEED:
				//algo.seedUpdate(delta, encoderReset);
				activeAlogrithm->SeedUpdate(delta, encoderReset);
				break;

			case MODE:
				//algo.modeUpdate(delta, encoderReset);
				activeAlogrithm->ModeUpdate(delta, encoderReset);
				break;

			case SYNC:
				if (encoderReset) {
					sync = false;
					break;
				}
				sync = !sync;
				break;

			case SLEW: {
				slewParam = clamp(slewParam + delta, 0, 100);
				if (encoderReset)
					slewParam = 0;
				// Convert slewParam (0 - 100) to ms (0, 1000),
				// or (0 - 10) if vcoMode is true.
				float slew = vcoMode ? (slewParam * 0.1f) : (slewParam * 10.f);
				for (int i = 0; i < 2; i++) {
					slewLimiter[i].setSlewAmountMs(slew, sr);
				}
				break;
			}
			case ALGORITHM:
				//algo.algoithmUpdate(delta, encoderReset);
				algoithmUpdate(delta, encoderReset);
				activeAlogrithm->Update(offset);
				//algo.update();
				break;

			default:
				break;
			}
		}
		else {
			//algo.ruleUpdate(delta, encoderReset);
			activeAlogrithm->RuleUpdate(delta, encoderReset);
			if (sync) {
				ruleResetPending = true;
			}
			else {
				gen = random::get<float>() < prob;
				genPending = true;
				ruleResetPending = false;
				if (gen) {
					//algo.seedReset();
					//algo.update();
					activeAlogrithm->SeedReset(false);
					activeAlogrithm->Update(offset);
				}
			}
			displayRule = true;
			ruleDisplayTimer.reset();
		}

		encoderReset = false;
	}

	void process(const ProcessArgs& args) override {
		// KNOBS & INPUTS
		// Algoithm CV input.
		//algo.setAlgoithmCV(inputs[ALGO_CV_INPUT].getVoltage());
		setAlgoithmCV(clamp(inputs[ALGO_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		// Rule CV input.
		//algo.setRuleCV(inputs[RULE_CV_INPUT].getVoltage());
		activeAlogrithm->SetRuleCV(clamp(inputs[RULE_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f));

		// Length knob.
		sequenceLength = sequenceLengths[static_cast<int>(params[LENGTH_PARAM].getValue())]; // TODO: round?

		// Prob knob & CV input.
		float probCV = clamp(inputs[PROB_CV_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		prob = clamp(params[PROB_PARAM].getValue() + probCV, 0.f, 1.f);

		// BUTTONS
		if (menuTrigger.process(params[MENU_PARAM].getValue())) {
			menu = !menu;
		}

		if (modeTrigger.process(params[MODE_PARAM].getValue())) {
			if (menu) {
				int numMenus = static_cast<int>(MenuPages::MENU_LEN);
				int menuIndex = static_cast<int>(menuPages);
				menuIndex++;
				if (menuIndex >= numMenus) { menuIndex = 0; }
				menuPages = static_cast<MenuPages>(menuIndex);
			}
			else {
				//algo.modeUpdate(1, false);
				activeAlogrithm->ModeUpdate(1, false);
			}
		}

		// SELECT ENCODER
		processEncoder();


		float offsetCv = clamp(inputs[OFFSET_CV_INPUT].getVoltage(), -10.f, 10.f) * 0.8f;
		int newOffset = std::min(std::max(fastRoundInt(params[OFFSET_PARAM].getValue() + offsetCv), -4), 4);
		if (sync) {
			offsetPending = newOffset;
		}
		else if (offset != newOffset) {
			offset = newOffset;
			//algo.setOffset(newOffset);
			//algo.update();
			activeAlogrithm->Update(offset);
		}

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
				//algo.inject(positiveInject ? true : false);
				//algo.update();
				activeAlogrithm->Inject(positiveInject ? true : false, false);
				activeAlogrithm->Update(offset);
				injectPendingState = 0;
			}
		}

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
			if (sync) {
				resetPending = true;
			}
			else {
				gen = random::get<float>() < prob;
				genPending = true;
				resetPending = false;
				if (gen) {
					//algo.seedReset();
					activeAlogrithm->SeedReset(false);
				}
				else {
					//algo.setReadHead(0);
					//algo.setWriteHead(1);
					activeAlogrithm->SetReadHead(0);
					activeAlogrithm->SetWriteHead(1);
				}
				//algo.update();
				activeAlogrithm->Update(offset);
			}
		}

		// STEP		
		bool trig = false;
		float trigVotagte = inputs[TRIG_INPUT].getVoltage();
		if (vcoMode) {
			// Detect zero crossing
			trig = (trigVotagte > 0.f && lastTrigVoltage <= 0.f) || (trigVotagte < 0.f && lastTrigVoltage >= 0.f);
		}
		else {
			// Dectect trigger pulse
			trig = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f);
		}
		lastTrigVoltage = trigVotagte;

		// TODO: do the dont trigger when reseting thing SEQ3
		if (trig) {
			if (!genPending) {
				// Find gen if not found when reset triggered and sync off
				gen = random::get<float>() < prob;
			}

			// Synced offset
			if (sync) {
				//algo.setOffset(offsetPending);
				offset = offsetPending; //TODO: this right?
			}

			// Synced inject
			switch (injectPendingState) {
			case 1:
				//algo.inject(true, true);
				activeAlogrithm->Inject(true, true);
				break;

			case 2:
				activeAlogrithm->Inject(false, true);
				//algo.inject(false, true);
				break;

			default: 
				break;
			}

			// Synced reset
			if (resetPending || ruleResetPending) {
				if (gen) {
					//algo.seedReset(true);
					activeAlogrithm->SeedReset(true);
				}
				else if (!ruleResetPending) {
					//algo.setWriteHead(0);
					activeAlogrithm->SetWriteHead(0);
				}
			}

			if (gen && !resetPending && !ruleResetPending && !injectPendingState) {
				//algo.generate();
				activeAlogrithm->Generate();
			}

			//algo.step(sequenceLength);
			//algo.update();

			activeAlogrithm->Step(sequenceLength);
			activeAlogrithm->Update(offset);

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			ruleResetPending = false;
			injectPendingState = 0;
		}

		// OUTPUT
		//float xCv = algo.getXVoltage();
		//float yCv = algo.getYVoltage();

		float xCv = activeAlogrithm->GetXVoltage();
		float yCv = activeAlogrithm->GetYVoltage();

		xCv = slewLimiter[0].process(xCv);
		yCv = slewLimiter[1].process(yCv);

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

		// Cv outputs -  0V to 10V or -5V to 5V (10Vpp).
		xCv = xCv * params[X_SCALE_PARAM].getValue() * 10.f;
		yCv = yCv * params[Y_SCALE_PARAM].getValue() * 10.f;
		outputs[X_CV_OUTPUT].setVoltage(xCv);
		outputs[Y_CV_OUTPUT].setVoltage(yCv);

		// Pulse outputs - 0V to 10V.
		//if(algo.getXPulse())
		if (activeAlogrithm->GetXPulse())
			xPulse.trigger(1e-3f);

		//if (algo.getYPulse())
		if (activeAlogrithm->GetYPulse())
			yPulse.trigger(1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		//lights[MODE_LIGHT].setBrightnessSmooth(algo.getModeLEDValue(), args.sampleTime); 
		lights[MODE_LIGHT].setBrightnessSmooth(activeAlogrithm->GetModeLEDValue(), args.sampleTime);
		lights[X_CV_LIGHT].setBrightness(xCv * 0.1);
		lights[X_PULSE_LIGHT].setBrightnessSmooth(xGate, args.sampleTime);
		lights[Y_CV_LIGHT].setBrightness(yCv * 0.1);
		lights[Y_PULSE_LIGHT].setBrightnessSmooth(yGate, args.sampleTime);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		lights[INJECT_LIGHT].setBrightnessSmooth(positiveInject | negativeInject, args.sampleTime);
		
		// Hide rule display.
		if (displayRule && ruleDisplayTimer.process(args.sampleTime) > 0.75f)
			displayRule = false;

		//algo.tick();
		tick();
	}
};

struct DisplayFramebuffer : FramebufferWidget {
	WolframModule* module;

	bool dirty = false;

	uint64_t prevMatrix = 0;
	bool prevMenu = false;
	bool prevDisplayRule = false;
	MenuPages prevPage = MenuPages::SEED;

	DisplayFramebuffer(WolframModule* m) {
		module = m;
	}

	void step() override {
		uint64_t matrix = module ? module->activeAlogrithm->GetDisplayMatrix() : 0;
		bool menu = module ? module->menu : false;
		bool displayRule = module ? module->displayRule : false;
		MenuPages page = module ? module->menuPages : MenuPages::SEED;


		// Naah this is all drawn on layer 1???
		// encoderReset
		// encdoer

		// menu Button
		// modeButton
		// look and feel


		// TODO: sort this out
		if (menu != prevMenu)
			dirty = true;

		if (menu && (page != prevPage))
			dirty = true;

		if (!menu && (displayRule != prevDisplayRule))
			dirty = true;

		if (!menu && (matrix != prevMatrix))
			dirty = true;

		dirty = true;

		prevMatrix = matrix;
		prevMenu = menu;
		prevPage = page;
		prevDisplayRule = displayRule;

		if (dirty) {
			FramebufferWidget::dirty = true;
			dirty = false;
		}
		FramebufferWidget::step();
	}
};

//struct Display : TransparentWidget {
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

	// std pair for col and row?
	std::vector<Vec> aliveCellCord{};

	Display(WolframModule* m, float y, float w, float s) {
		module = m;

		// Params
		widgetSize = s;
		float screenSize = widgetSize - (padding * 2.f);
		fontSize = (screenSize / cols) * 2.f;

		float cellPadding = (screenSize / cols);

		// Widget size
		box.pos = Vec((w * 0.5f) - (widgetSize * 0.5f), y);
		box.size = Vec(widgetSize, widgetSize);

		// Load font
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/mtf_wolf5.otf"));
		font = APP->window->loadFont(fontPath);

		if (!module)
			return;

		lookAndFeel = &module->lookAndFeel;
		lookAndFeel->setDrawingParams(padding, fontSize, cellPadding);
	}

	/*
	void drawMenu(NVGcontext* vg, MenuPages Menu, NVGcolor* c) {

		module->algo.drawTextBackground(vg, 0);
		module->algo.drawTextBackground(vg, 3);

		nvgFillColor(vg, *c);
		nvgText(vg, padding, padding, "menu", nullptr);

		switch (Menu) {
		case SEED:
			module->algo.drawSeedMenuText(vg);
			break;
		case MODE:
			module->algo.drawModeMenuText(vg);
			break;
		case SYNC:
			module->algo.drawTextBackground(vg, 1);
			module->algo.drawTextBackground(vg, 2);

			nvgFillColor(vg, *c);
			nvgText(vg, padding, fontSize + padding, "SYNC", nullptr);
			if (module->sync)
				nvgText(vg, padding, (fontSize * 2.f) + padding, "LOCK", nullptr);
			else
				nvgText(vg, padding, (fontSize * 2.f) + padding, "FREE", nullptr);
			break;
		case SLEW:
			module->algo.drawTextBackground(vg, 1);
			module->algo.drawTextBackground(vg, 2);

			nvgFillColor(vg, *c);
			nvgText(vg, padding, fontSize + padding, "SLEW", nullptr);

			char slewString[5];
			snprintf(slewString, sizeof(slewString), "%4.3d", module->slewParam);
			nvgText(vg, padding, (fontSize * 2.f) + padding, slewString, nullptr);
			break;
		case ALGORITHM:
			module->algo.drawAlgoithmMenuText(vg);
			break;
		default:
			break;
		}

		nvgFillColor(vg, *c);
		nvgText(vg, padding, (fontSize * 3.f) + padding, "<#@>", nullptr);
	}

	void drawMenuBackground(NVGcontext* vg, MenuPages Menu) {

		module->algo.drawTextBackground(vg, 0);
		module->algo.drawTextBackground(vg, 3);

		switch (Menu) {
		case SEED:
			module->algo.drawMenuBackground(vg);
			break;
		case MODE:
			module->algo.drawMenuBackground(vg);
			break;
		case SYNC:
			module->algo.drawTextBackground(vg, 1);
			module->algo.drawTextBackground(vg, 2);
			break;
		case SLEW:
			module->algo.drawTextBackground(vg, 1);
			module->algo.drawTextBackground(vg, 2);
			break;
		case ALGORITHM:
			module->algo.drawMenuBackground(vg);
			break;
		default:
			break;
		}
	}
	
	void drawMenuText(NVGcontext* vg, MenuPages Menu) {
		nvgFillColor(vg, primaryColour);
		nvgText(vg, padding, padding, "menu", nullptr);

		switch (Menu) {
		case SEED:
			module->algo.drawSeedMenuText(vg);
			break;

		case MODE:
			module->algo.drawModeMenuText(vg);
			break;

		case SYNC:
			nvgFillColor(vg, primaryColour);
			nvgText(vg, padding, fontSize + padding, "SYNC", nullptr);
			if (module->sync)
				nvgText(vg, padding, (fontSize * 2.f) + padding, "LOCK", nullptr);
			else
				nvgText(vg, padding, (fontSize * 2.f) + padding, "FREE", nullptr);
			break;

		case SLEW:
			nvgFillColor(vg, primaryColour);
			nvgText(vg, padding, fontSize + padding, "SLEW", nullptr);

			char slewString[5];
			snprintf(slewString, sizeof(slewString), "%4.3d", module->slewParam);
			nvgText(vg, padding, (fontSize * 2.f) + padding, slewString, nullptr);
			break;

		case ALGORITHM:
			module->algo.drawAlgoithmMenuText(vg);
			break;
		default:
			break;
		}

		nvgFillColor(vg, primaryColour);
		nvgText(vg, padding, (fontSize * 3.f) + padding, "<#@>", nullptr);
	}
	*/

	void drawMenuBackgroud(NVGcontext* vg, MenuPages page, bool rule) {

		// Rule.
		if (rule) {
			lookAndFeel->drawTextBackground(vg, 0);
			lookAndFeel->drawTextBackground(vg, 1);
			return;
		}
		
		// Menu.
		lookAndFeel->drawTextBackground(vg, 0);
		lookAndFeel->drawTextBackground(vg, 3);

		if ((page == MenuPages::SEED) || (page == MenuPages::MODE)) {
			module->activeAlogrithm->DrawMenuBackground(vg, page, false);
			return;
		}

		// Slew, Sync, Algo.
		lookAndFeel->drawTextBackground(vg, 1);
		lookAndFeel->drawTextBackground(vg, 2);	
	}

	void drawMenuText(NVGcontext* vg, MenuPages page, bool rule) {
		// Rule.
		if (rule) {
			module->activeAlogrithm->DrawMenuText(vg, page, rule);
			return;
		}

		// Menu.
		lookAndFeel->drawText(vg, "menu", 0);
		lookAndFeel->drawText(vg, "<#@>", 3);

		switch (page) {
		case SLEW: {
			char t[5];
			snprintf(t, sizeof(t), "%4.3d", module->slewParam);
			lookAndFeel->drawText(vg, "SLEW", 1);
			lookAndFeel->drawText(vg, t, 2);
			break;
		}
		case SYNC: {
			lookAndFeel->drawText(vg, "SYNC", 1);
			if (module->sync)
				lookAndFeel->drawText(vg, "LOCK", 2);
			else
				lookAndFeel->drawText(vg, "FREE", 2);
			break;
		}
		case ALGORITHM: {
			lookAndFeel->drawText(vg, "ALGO", 1);
			switch (module->algorithmIndex) {
			case 0:
				lookAndFeel->drawText(vg, "WOLF", 2);
				break;

			case 1:
				lookAndFeel->drawText(vg, "LIFE", 2);
				break;

			default:
				break;
			}
			break;
		}
		default:	// SEED & MODE.
			module->activeAlogrithm->DrawMenuText(vg, page, rule);
			break;
		}
	}

	void draw(const DrawArgs& args) override {
		
		//lookAndFeel->setDrawingContext(args.vg);

		// Draw background & border.
		NVGcolor backgroundColour = module ? *lookAndFeel->getBackgroundColour() : nvgRGB(58, 16, 19);

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
		// setup drawing alive cells and text on the self-illuminating layer.
		aliveCellCord.clear();

		int firstRow = 0;
		if (module) {
			if (module->menu || module->displayRule)
				drawMenuBackgroud(args.vg, module->menuPages, module->displayRule);

			if (module->menu)
				return;

			firstRow = module->displayRule ? (rows - 4) : 0;
		}

		// Requires flipping.
		uint64_t matrix = module ? module->activeAlogrithm->GetDisplayMatrix() : 8;

		// Calculate and store cell 
		nvgBeginPath(args.vg);
		for (int row = firstRow; row < rows; row++) {
			int rowInvert = (row - 7) * -1;
			for (int col = 0; col < cols; col++) {
				int colInvert = 7 - col;
				int cellIndex = rowInvert * 8 + colInvert;

				if ((matrix >> cellIndex) & 1ULL) {
					// Store alive cell coordinates.
					aliveCellCord.emplace_back(Vec(col, row));
				}
				else {
					// Draw dead cells.
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

		//if (module)
		//	lookAndFeel->setDrawingContext(args.vg);

		nvgFontSize(args.vg, fontSize);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

		if (module && (module->menu || module->displayRule))
			drawMenuText(args.vg, module->menuPages, module->displayRule);

		// Draw alive cells.
		nvgBeginPath(args.vg);
		for (const Vec& pos : aliveCellCord) {
			int col = pos.x;
			int row = pos.y;
			if (module) {
				lookAndFeel->drawCell(args.vg, col, row, true);
			}
		}
		nvgFill(args.vg);

		Widget::drawLayer(args, layer);
	}
};

struct WolframModuleWidget : ModuleWidget {

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

			nvgBeginPath(args.vg);
			nvgRect(args.vg, -br, -br, this->box.size.x + 2 * br, this->box.size.y + 2 * br);
			NVGcolor icol = color::mult(TBase::color, halo);
			NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
			nvgFillPaint(args.vg, nvgBoxGradient(args.vg, 0, 0, this->box.size.x, this->box.size.y, cr, br, icol, ocol));
			nvgFill(args.vg);
		}
	};

	WolframModuleWidget(WolframModule* module) {
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

		DisplayFramebuffer* displayFb = new DisplayFramebuffer(module);
		Display* display = new Display(module, mm2px(10.14f), box.size.x, mm2px(32.f));
		displayFb->addChild(display);
		addChild(displayFb);
	}

	void appendContextMenu(Menu* menu) override {
		WolframModule* module = dynamic_cast<WolframModule*>(this->module);
		
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("VCO Mode", "", &module->vcoMode));

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Look",
			{ "Redrick", "OLED", "Rack"},
			[ = ]() {
				return module->lookAndFeel.getLookIndex();
			},
			[ = ](int i) {
				module->lookAndFeel.setLookIndex(i);
			} 
		));

		menu->addChild(createIndexSubmenuItem("Feel",
			{ "Circle", "Square"},
			[=]() {
				return module->lookAndFeel.getFeelIndex();
			},
			[=](int i) {
				module->lookAndFeel.setFeelIndex(i);
			}
		));
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");