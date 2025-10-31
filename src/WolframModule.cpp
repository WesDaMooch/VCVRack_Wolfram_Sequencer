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

// TODO: Pulse out not gates
// TODO: Fix type in value behaviour of Select encoder and Length dial
// TODO: Game - space defence, control platform with offset, 'floor' rises when letting a bit through
// TODO: The panel
// TODO: Save state
// TODO: onReset and onRandomize

#include "wolfram.hpp"

enum DisplayMenu {
	SEED,
	MODE,
	SYNC,
	FUNCTION,
	ALGORITHM,
	LOOK,
	MENU_LEN
};

enum Look {
	BEFACO,
	OLED,
	RACK,
	ACID,
	MONO,
	LOOK_LEN
};


enum Algorithm {
	WOLF,
	LIFE,
	ALGORITHM_LEN
};

struct WolframModule : Module {
	enum ParamId {
		SELECT_PARAM,
		MENU_PARAM,
		MODE_PARAM,
		LENGTH_PARAM,
		CHANCE_PARAM,
		OFFSET_PARAM,
		X_SCALE_PARAM,
		Y_SCALE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RESET_INPUT,
		CHANCE_INPUT,
		OFFSET_INPUT,
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
		X_GATE_LIGHT,
		Y_CV_LIGHT,
		Y_GATE_LIGHT,
		TRIG_LIGHT,
		INJECT_LIGHT,
		LIGHTS_LEN
	};

	WolframEngine wEngine; 

	Look moduleLook = Look::BEFACO;
	DisplayMenu moduleMenu = DisplayMenu::SEED;

	dsp::TBiquadFilter<float> dcBlockFilter;
	//dsp::TRCFilter<float> dcBlockFilter2;

	dsp::RCFilter dcFilter[2];

	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger posInjectTrigger;
	dsp::SchmittTrigger negInjectTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator yPulseGenerator;
	dsp::BooleanTrigger menuTrigger;
	dsp::BooleanTrigger modeTrigger;
	dsp::Timer ruleDisplayTimer;

	static constexpr int PARAM_CONTROL_INTERVAL = 64;

	int sequenceLength = 8;
	std::array<int, 10> sequenceLengths = { 2, 3, 4, 5, 6, 8, 12, 16, 32, 64 }; // static constexpr errors
	float chance = 1;
	int offset = 0;	//display
	int offsetPending = 0;
	bool resetPending = false;
	bool ruleResetPending = false;
	int injectPendingState = 0;
	bool gen = false;
	bool genPending = false;
	bool sync = false;
	bool menu = false;
	bool vcoMode = false;
	float lastTrigVoltage = 0.f;

	// Encoder
	static constexpr float encoderIndent = 1.f / 30.f;
	float prevEncoderValue = 0.f;
	bool encoderReset = false;
	
	// Display
	bool displayRule = false;
	bool displayUpdate = true;
	int displayUpdateInterval = 1000;
	bool dirty = true;

	// is this really nessisary 
	inline int fastRoundInt(float value) {
		return static_cast<int>(value + (value >= 0.f ? 0.5f : -0.5f));
	}

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(MENU_PARAM, "Menu");
		configButton(MODE_PARAM, "Mode");
		//configParam<EncoderParamQuantity>(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Select");
		//configParam<LengthParamQuantity>(LENGTH_PARAM, 0.f, 9.f, 5.f, "Length");
		configParam(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Select");
		configParam(LENGTH_PARAM, 0.f, 9.f, 5.f, "Length");
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance", "%", 0.f, 100.f);
		paramQuantities[CHANCE_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(X_SCALE_PARAM, 0.f, 1.f, 0.5f, "X CV Scale", "V", 0.f, 10.f);
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 1.f, 0.5f, "Y CV Scale", "V", 0.f, 10.f);
		paramQuantities[Y_SCALE_PARAM]->displayPrecision = 3;
		configInput(RESET_INPUT, "Reset");
		configInput(CHANCE_INPUT, "Chance CV");
		configInput(OFFSET_INPUT, "Offset CV");
		configInput(TRIG_INPUT, "Trigger");
		configInput(INJECT_INPUT, "Inject");
		configOutput(X_CV_OUTPUT, "X CV");
		configOutput(X_PULSE_OUTPUT, "X Gate");
		configOutput(Y_CV_OUTPUT, "Y CV");
		configOutput(Y_PULSE_OUTPUT, "Y Gate");
		configLight(MODE_LIGHT, "Mode");
		configLight(X_CV_LIGHT, "X CV");
		configLight(X_GATE_LIGHT, "X Gate");
		configLight(Y_CV_LIGHT, "Y CV");
		configLight(Y_GATE_LIGHT, "Y Gate");
		configLight(TRIG_LIGHT, "Trigger");
		configLight(INJECT_LIGHT, "Inject");

		//wEngine.setReadHead(0);
		//wEngine.setWriteHead(1);

		onSampleRateChange();
	}

	struct EncoderParamQuantity : ParamQuantity {
		// Custom behaviour to display rule when hovering over Select encoder.
		float getDisplayValue() override {
			//auto* m = dynamic_cast<WolframModule*>(module);
			//return m ? m->rule : 30;
			return 0;
		}
	};

	struct LengthParamQuantity : ParamQuantity {
		// Custom behaviour to display sequence length when hovering over Length dial.
		float getDisplayValue() override {
			auto* m = dynamic_cast<WolframModule*>(module);
			return m ? m->sequenceLength : 8;
		}
	};

	void onSampleRateChange() override {
		// 
		const float sampleRate = APP->engine->getSampleRate();
		displayUpdateInterval = static_cast<int>(sampleRate / 30.f);

		// set DC blocker to ~20Hz.
		//const float freq = 22.05f / sampleRate;
		
		// TODO: Use sampleRate? 20Hz?
		// Set up X & Y DC Blocking filters
		const float sampleTime = APP->engine->getSampleTime();
		for (int i = 0; i < 2; i++) {
			dcFilter[i].setCutoffFreq(10.f * sampleTime);
		}
	}

	void processEncoder(const ProcessArgs& args) {
		const float encoderValue = params[SELECT_PARAM].getValue();
		float difference = encoderValue - prevEncoderValue;
		int delta = static_cast<int>(std::round(difference / encoderIndent));

		if (delta != 0)
			prevEncoderValue += delta * encoderIndent;

		// Select encoder drag and double-click selection logic
		if (menu) {
			switch (moduleMenu) {
			case SEED: {
				if (encoderReset) {
					wEngine.seedUpdate(0, true);
					encoderReset = false;
					displayUpdate = true;
					break;
				}
				if (delta == 0)
					break;
				wEngine.seedUpdate(delta, false);
				displayUpdate = true;
				break;
			}
			case MODE: {
				if (encoderReset) {
					wEngine.modeUpdate(0, true);
					encoderReset = false;
					displayUpdate = true;
					break;
				}
				if (delta == 0)
					break;
				wEngine.modeUpdate(delta, false);
				displayUpdate = true;
				break;
			}
			case SYNC: {
				if (encoderReset) {
					sync = false;
					encoderReset = false;
					displayUpdate = true;
				}
				
				if (delta == 0)
					break;

				sync = !sync;

				displayUpdate = true;
				break;
			}
			case FUNCTION: {
				if (encoderReset) {
					vcoMode = false;
					encoderReset = false;
					displayUpdate = true;
				}

				if (delta == 0)
					break;

				vcoMode = !vcoMode;

				displayUpdate = true;
				break;
			}
			case ALGORITHM: {
				if (encoderReset) {
					wEngine.algoithmUpdate(0, true);
					encoderReset = false;
					displayUpdate = true;
					break;
				}
				if (delta == 0)
					break;
				wEngine.algoithmUpdate(delta, false);
				wEngine.outputMatrixPush();
				displayUpdate = true;
				break;
			}
			case LOOK: {
				if (encoderReset) {
					moduleLook = Look::BEFACO;
					encoderReset = false;
					displayUpdate = true;
				}

				if (delta == 0)
					break;

				const int numLooks = static_cast<int>(Look::LOOK_LEN);
				int lookIndex = static_cast<int>(moduleLook);
				lookIndex = (lookIndex + delta + numLooks) % numLooks;
				moduleLook = static_cast<Look>(lookIndex);

				displayUpdate = true;
				break;
			}
			default: { break; }
			}
		}
		else {
			// Rule select
			if (encoderReset) {
				//wolframEngine.resetRule();
				wEngine.ruleUpdate(0, true);
				encoderReset = false;
			}
			else if (delta != 0) {
				//wolframEngine.updateRule(delta);
				wEngine.ruleUpdate(delta, false);
			}
			else {
				return; 
			}

			if (sync) {
				ruleResetPending = true;
			}
			else {
				gen = random::get<float>() < chance;
				genPending = true;
				ruleResetPending = false;
				if (gen) {
					//wolframEngine.generateReset(internalReadHead);
					//wolframEngine.pushToOutputMatix(internalReadHead);
					wEngine.generateReset();
					wEngine.outputMatrixPush();
				}
			}
			displayRule = true;
			ruleDisplayTimer.reset();
			displayUpdate = true;
		}
	}

	void process(const ProcessArgs& args) override {

		// BUTTONS & ENCODER

		processEncoder(args);

		if (menuTrigger.process(params[MENU_PARAM].getValue())) {
			menu = !menu;
			displayUpdate = true;
		}

		if (modeTrigger.process(params[MODE_PARAM].getValue())) {
			if (menu) {
				int numMenus = static_cast<int>(DisplayMenu::MENU_LEN);
				int menuIndex = static_cast<int>(moduleMenu);
				menuIndex++;
				if (menuIndex >= numMenus) { menuIndex = 0; }
				moduleMenu = static_cast<DisplayMenu>(menuIndex);
				displayUpdate = true;
			}
			else {
				wEngine.modeUpdate(1, false);
			}
		}

		// DIALS & INPUTS
		// static_cast<size_t> ???
		sequenceLength = sequenceLengths[static_cast<int>(params[LENGTH_PARAM].getValue())];

		float chanceCv = clamp(inputs[CHANCE_INPUT].getVoltage(), 0.f, 10.f) * 0.1f;
		chance = clamp(params[CHANCE_PARAM].getValue() + chanceCv, 0.f, 1.f); // TODO: Used the min max thing here too, see offset

		float offsetCv = clamp(inputs[OFFSET_INPUT].getVoltage(), -10.f, 10.f) * 0.8f;
		int newOffset = std::min(std::max(fastRoundInt(params[OFFSET_PARAM].getValue() + offsetCv), -4), 4);
		if (sync) {
			offsetPending = newOffset;
		}
		else if (offset != newOffset) {
			offset = newOffset;
			wEngine.setOffset(newOffset);
			wEngine.outputMatrixPush();
			if (!menu) { displayUpdate = true; }
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
				wEngine.inject(positiveInject ? true : false);
				wEngine.outputMatrixPush();
				injectPendingState = 0;
				if (!menu) displayUpdate = true;
			}
		}

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
			if (sync) {
				resetPending = true;
			}
			else {
				gen = random::get<float>() < chance;
				genPending = true;
				resetPending = false;
				if (gen) {
					wEngine.generateReset();
				}
				else {
					wEngine.setReadHead(0);
					wEngine.setWriteHead(1);
				}
				wEngine.outputMatrixPush();
				if (!menu) displayUpdate = true;
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
				gen = random::get<float>() < chance;
			}

			// Synced offset
			if (sync) {
				wEngine.setOffset(offsetPending);
			}

			// Synced inject
			switch (injectPendingState) {
			case 1:
				wEngine.inject(true, true);
				break;
			case 2:
				wEngine.inject(false, true);
				break;
			default: 
				break;
			}

			// Synced reset
			if (resetPending || ruleResetPending) {
				if (gen) {
					wEngine.generateReset(true);
				}
				else if (!ruleResetPending) {
					wEngine.setWriteHead(0);
				}
			}

			if (gen && !resetPending && !ruleResetPending && !injectPendingState) {
				wEngine.generate();
			}

			wEngine.advanceHeads(sequenceLength);
			wEngine.outputMatrixStep();
			wEngine.outputMatrixPush();

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			ruleResetPending = false;
			injectPendingState = 0;

			// Redraw matrix display
			if (!menu) displayUpdate = true;
		}

		// OUTPUT
		float xCv = wEngine.getXVoltage();
		float yCv = wEngine.getYVoltage();
		if (vcoMode) {
			xCv -= 0.5f;
			dcFilter[0].process(xCv);
			xCv = dcFilter[0].highpass();

			yCv -= 0.5f;
			dcFilter[1].process(yCv);
			yCv = dcFilter[1].highpass();
		}
		// Cv outputs -  0V to 10V or -5V to 5V (10Vpp).
		xCv = xCv * params[X_SCALE_PARAM].getValue() * 10.f; 
		outputs[X_CV_OUTPUT].setVoltage(xCv);

		yCv *= params[Y_SCALE_PARAM].getValue() * 10.f;
		outputs[Y_CV_OUTPUT].setVoltage(yCv);

		// Pulse outputs - 0V to 10V.
		bool xGate = wEngine.getXGate();
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);

		bool yGate = wEngine.getYGate();
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// Debug output
		//outputs[Y_PULSE_OUTPUT].setVoltage(sequenceLength);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(wEngine.getModeValue(), args.sampleTime); 
		lights[X_CV_LIGHT].setBrightness(xCv * 0.1);
		lights[X_GATE_LIGHT].setBrightness(xGate);
		lights[Y_CV_LIGHT].setBrightness(yCv * 0.1);
		lights[Y_GATE_LIGHT].setBrightness(yGate);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		lights[INJECT_LIGHT].setBrightnessSmooth(positiveInject, args.sampleTime);

		// UPDATE DISPLAY
		
		if (displayRule && ruleDisplayTimer.process(args.sampleTime) > 0.75f) {
			displayRule = false;
			if (!menu) displayUpdate = true;
		}

		// 30fps limited
		if (displayUpdate && (((args.frame + this->id) % displayUpdateInterval) == 0)) {
			displayUpdate = false;
			dirty = true;
		}
	}
};

struct MatrixDisplayBuffer : FramebufferWidget {
	WolframModule* module;
	MatrixDisplayBuffer(WolframModule* m) {
		module = m;
		// what is this do ing?
	}
	void step() override {
		if (module && module->dirty) {
			FramebufferWidget::dirty = true;
			module->dirty = false;
		}
		FramebufferWidget::step();
	}
};

// TODO: Should be a Widget, there is other stuff that might work better 
// See CVfunk-Modules Ouros, that a cool way to do stuff

struct MatrixDisplay : Widget {
	// TODO: Should be using something like this?
	// WolframModule* m = static_cast<WolframModule*>(module);
	// Check out the base Wiget 
	WolframModule* module;

	std::shared_ptr<Font> font;
	std::string fontPath;
	// Sort out mm2px stuff, need a rule
	int matrixCols = 8;
	int matrixRows = 8;

	float matrixSize = mm2px(32.f);

	float cellPos = matrixSize / matrixCols;
	float cellSize = cellPos * 0.5f;
	float fontSize = (matrixSize / matrixCols) * 2.f;

	// Default Befaco colours 
	NVGcolor lcdBackground = nvgRGB(67, 38, 38);		
	NVGcolor primaryAccent = nvgRGB(195, 67, 67);		
	NVGcolor primaryAccentOff = lcdBackground;
	NVGcolor secondaryAccent = nvgRGB(76, 40, 40);

	MatrixDisplay(WolframModule* m) {
		module = m;	// TODO: does this get used
		box.pos = Vec(mm2px(30.1) - matrixSize * 0.5, mm2px(26.14) - matrixSize * 0.5);
		box.size = Vec(matrixSize, matrixSize);
		
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/mtf_wolf4.otf"));
		font = APP->window->loadFont(fontPath);
	}

	void drawMenu(NVGcontext* vg, DisplayMenu Menu) {
		nvgFillColor(vg, primaryAccent);
		nvgText(vg, 0, 0, "menu", nullptr);
		switch (Menu) {
			case SEED: {
				module->wEngine.drawSeedMenu(vg, fontSize);
				break;
			}
			case MODE: {
				module->wEngine.drawModeMenu(vg, fontSize);
				break;
			}
			case SYNC: {
				nvgText(vg, 0, fontSize, "SYNC", nullptr);
				if (module->sync) {
					nvgText(vg, 0, fontSize * 2, "LOCK", nullptr);
				}
				else {
					nvgText(vg, 0, fontSize * 2, "FREE", nullptr);
				}
				break;
			}
			case FUNCTION: {
				// CV AUD
				nvgText(vg, 0, fontSize, "FUNC", nullptr);
				if (module->vcoMode) {
					nvgText(vg, 0, fontSize * 2, " VCO", nullptr);
				}
				else {
					nvgText(vg, 0, fontSize * 2, " SEQ", nullptr);
				}
				break;
			}
			case ALGORITHM: {
				// WOLF ANT LIFE
				module->wEngine.drawAlgoithmMenu(vg, fontSize);
				break;
			}
			case LOOK: {
				nvgText(vg, 0, fontSize, "LOOK", nullptr);
				switch (module->moduleLook) {
				case OLED:
					nvgText(vg, 0, fontSize * 2, "OLED", nullptr);
					lcdBackground = nvgRGB(33, 62, 115);
					primaryAccent = nvgRGB(183, 236, 255);
					primaryAccentOff = lcdBackground;
					break;
				case RACK:
					nvgText(vg, 0, fontSize * 2, "RACK", nullptr);
					lcdBackground = nvgRGB(18, 18, 18);
					primaryAccent = SCHEME_YELLOW;
					primaryAccentOff = lcdBackground;
					break;
				case ACID:
					nvgText(vg, 0, fontSize * 2, "ACID", nullptr);
					lcdBackground = nvgRGB(6, 56, 56);
					primaryAccent = nvgRGB(175, 210, 44);	// Create theme
					primaryAccentOff = lcdBackground;
					break;
				case MONO:
					nvgText(vg, 0, fontSize * 2, "MONO", nullptr);
					lcdBackground = nvgRGB(18, 18, 18);		// Create theme
					primaryAccent = nvgRGB(255, 255, 255);
					primaryAccentOff = lcdBackground;
					break;
				default:
					// Befaco 
					nvgText(vg, 0, fontSize * 2, "BFAC", nullptr);
					lcdBackground = nvgRGB(67, 38, 38);
					primaryAccent = nvgRGB(195, 67, 67);
					primaryAccentOff = lcdBackground;
					break;
				}
				module->dirty = true;	// TODO: feels bad doing this
				break;
			}
			default: { break; }
		}
		nvgText(vg, 0, fontSize * 3, "<*+>", nullptr);
	}

	void draw(NVGcontext* vg) override {
		// Background
		nvgBeginPath(vg);
		//nvgRect(vg, 0, 0, matrixSize, matrixSize);
		nvgRoundedRect(vg, 0.f, 0.f, matrixSize, matrixSize, 2.f);
		nvgFillColor(vg, lcdBackground);
		nvgFill(vg);
		// Border
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, nvgRGB(0x10, 0x10, 0x10));
		nvgStroke(vg);
		nvgClosePath(vg);

		// TODO: improve border
		// TODO: improve cell look

		if (!module) {
			// TODO: Draw preview 
			return;
		}

		if (!font) return;

		nvgFontSize(vg, fontSize);
		nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

		if (module->menu) {
			drawMenu(vg, module->moduleMenu);
		}
		else {
			int startRow = 0;
			if (module->displayRule) {
				startRow = matrixRows - 4;
				nvgFillColor(vg, primaryAccent);
				module->wEngine.drawRule(vg, fontSize);
			}

			// Requires flipping before drawing
			uint64_t matrix = module->wEngine.getOutputMatrix();

			for (int row = startRow; row < matrixRows; row++) {
				int rowInvert = (row - 7) * -1;

				for (int col = 0; col < matrixCols; col++) {
					int colInvert = 7 - col;
					int cellIndex = rowInvert * 8 + colInvert;
					bool cellState = (matrix >> cellIndex) & 1ULL;  

					nvgBeginPath(vg);
					nvgCircle(vg, (cellPos * col) + cellSize, (cellPos * row) + cellSize, cellSize);
					nvgFillColor(vg, cellState ? primaryAccent : primaryAccentOff);
					nvgFill(vg);
				}
			}
		}
	}
};

struct MoochEncoder : RoundBlackKnob { 
	// Custom behaviour for Select encoder.
	MoochEncoder() { }

	void onDoubleClick(const DoubleClickEvent& e) override {
		// Reset Select encoder to default of current selection. See processEncoder for details.
		auto* m = static_cast<WolframModule*>(module);
		m->encoderReset = true;
	}

};

struct WolframModuleWidget : ModuleWidget {
		
	// Should led stuff be out of this widget??
	
	// Using Count Modula's custom LED
	template <typename TBase>
	struct RectangleLightDiagonal : TBase {

		// -45 degree rotation 
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

	// Rename based on the name of the LED on switch electronics
	template <typename TBase>
	struct WolframLed : TBase {
		WolframLed() {
			this->box.size = mm2px(Vec(5.f, 2.f));
		}

		void drawHalo(const DrawArgs& args) override {
			// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
			if (args.fb)
				return;

			const float halo = settings::haloBrightness;
			if (halo == 0.f)
				return;

			// If light is off, rendering the halo gives no effect.
			if (this->color.a == 0.f)
				return;

			float br = 30.0; // Blur radius
			float cr = 5.0; // Corner radius

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
		setPanel(createPanel(asset::plugin(pluginInstance, "res/WolframModule.svg")));
		// Srews
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// Buttons
		addParam(createParamCentered<CKD6>(mm2px(Vec(7.4f, 35.03f)), module, WolframModule::MENU_PARAM));
		addParam(createParamCentered<CKD6>(mm2px(Vec(52.8f, 35.03f)), module, WolframModule::MODE_PARAM));
		// Dials
		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(52.8f, 18.5f)), module, WolframModule::SELECT_PARAM));
		addParam(createParamCentered<MoochEncoder>(mm2px(Vec(52.8f, 18.5f)), module, WolframModule::SELECT_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(15.24f, 60.f)), module, WolframModule::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(45.72f, 60.f)), module, WolframModule::CHANCE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48f, 80.f)), module, WolframModule::OFFSET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.16f, 80.f)), module, WolframModule::X_SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(50.8f, 80.f)), module, WolframModule::Y_SCALE_PARAM));
		// Inputs
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.4f, 18.5f)), module, WolframModule::RESET_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(25.4f, 96.5f)), module, WolframModule::CHANCE_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(35.56f, 96.5f)), module, WolframModule::OFFSET_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(25.4f, 111.5f)), module, WolframModule::TRIG_INPUT));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(35.56f, 111.5f)), module, WolframModule::INJECT_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 96.5f)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 111.5f)), module, WolframModule::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 96.5f)), module, WolframModule::Y_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 111.5f)), module, WolframModule::Y_PULSE_OUTPUT));
		// LEDs
		addChild(createLightCentered<RectangleLightDiagonal<WolframLed<RedLight>>>(mm2px(Vec(52.8f, 45.67f)), module, WolframModule::MODE_LIGHT));
		addChild(createLightCentered<RectangleLightDiagonal<WolframLed<RedLight>>>(mm2px(Vec(16.51f, 96.5f)), module, WolframModule::X_CV_LIGHT));
		addChild(createLightCentered<RectangleLightDiagonal<WolframLed<RedLight>>>(mm2px(Vec(16.51f, 111.5f)), module, WolframModule::X_GATE_LIGHT));
		addChild(createLightCentered<RectangleLightDiagonal<WolframLed<RedLight>>>(mm2px(Vec(44.45f, 96.5f)), module, WolframModule::Y_CV_LIGHT));
		addChild(createLightCentered<RectangleLightDiagonal<WolframLed<RedLight>>>(mm2px(Vec(44.45f, 111.5f)), module, WolframModule::Y_GATE_LIGHT));
		addChild(createLightCentered<RectangleLight<WolframLed<RedLight>>>(mm2px(Vec(16.51f, 104.f)), module, WolframModule::TRIG_LIGHT));
		addChild(createLightCentered<RectangleLight<WolframLed<RedLight>>>(mm2px(Vec(44.45, 104.f)), module, WolframModule::INJECT_LIGHT));

		MatrixDisplayBuffer* matrixDisplayFb = new MatrixDisplayBuffer(module);
		MatrixDisplay* matrixDisplay = new MatrixDisplay(module);
		matrixDisplayFb->addChild(matrixDisplay);
		addChild(matrixDisplayFb);
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");