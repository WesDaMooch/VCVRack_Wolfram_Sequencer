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


enum MenuPages {
	SEED,
	SLEW,
	MODE,
	SYNC,
	ALGORITHM,
	MENU_LEN
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

	AlgoithmDispatcher algo; 
	LookAndFeel lookAndFeel;
	MenuPages menuPages = MenuPages::SEED;

	dsp::PulseGenerator xPulse;
	dsp::PulseGenerator yPulse;
	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger posInjectTrigger;
	dsp::SchmittTrigger negInjectTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::BooleanTrigger menuTrigger;
	dsp::BooleanTrigger modeTrigger;
	dsp::Timer ruleDisplayTimer;
	dsp::RCFilter dcFilter[2];	// DC blocking filter.
	SlewLimiter slewLimiter;

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

	int slewParam = 0;
	float slewAmount = 0;
	bool sync = false;
	bool menu = false;
	bool vcoMode = false;

	float sr = 48000;
	

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

		onSampleRateChange();
	}

	struct EncoderParamQuantity : ParamQuantity {
		// Custom behaviour to display rule when hovering over Select encoder.
		std::string getDisplayValueString() override {
			auto* m = dynamic_cast<WolframModule*>(module);
			return m ? m->algo.getRuleString() : "30";
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
				algo.seedUpdate(delta, encoderReset);
				break;

			case MODE:
				algo.modeUpdate(delta, encoderReset);
				break;

			case SYNC:
				if (encoderReset) {
					sync = false;
					break;
				}
				sync = !sync;
				break;

			case SLEW: 
				slewParam = clamp(slewParam + delta, 0, 100);
				if (encoderReset)
					slewParam = 0;
		
				// Convert slewParam (0 - 100) to ms (0, 1000)
				slewLimiter.setSlewAmountMs(slewParam * 10.f, sr);
				break;

			case ALGORITHM:
				algo.algoithmUpdate(delta, encoderReset);
				algo.update();
				break;

			default:
				break;
			}
		}
		else {
			algo.ruleUpdate(delta, encoderReset);

			if (sync) {
				ruleResetPending = true;
			}
			else {
				gen = random::get<float>() < prob;
				genPending = true;
				ruleResetPending = false;
				if (gen) {
					algo.seedReset();
					algo.update();
				}
			}
			displayRule = true;
			ruleDisplayTimer.reset();
		}

		encoderReset = false;
	}

	void process(const ProcessArgs& args) override {

		// BUTTONS & ENCODER
		processEncoder();

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
				algo.modeUpdate(1, false);
			}
		}

		// DIALS & INPUTS
		sequenceLength = sequenceLengths[static_cast<int>(params[LENGTH_PARAM].getValue())];

		float probCv = clamp(inputs[PROB_CV_INPUT].getVoltage(), 0.f, 10.f) * 0.1f;
		prob = clamp(params[PROB_PARAM].getValue() + probCv, 0.f, 1.f); // TODO: Used the min max thing here too, see offset

		float offsetCv = clamp(inputs[OFFSET_CV_INPUT].getVoltage(), -10.f, 10.f) * 0.8f;
		int newOffset = std::min(std::max(fastRoundInt(params[OFFSET_PARAM].getValue() + offsetCv), -4), 4);
		if (sync) {
			offsetPending = newOffset;
		}
		else if (offset != newOffset) {
			offset = newOffset;
			algo.setOffset(newOffset);
			algo.update();
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
				algo.inject(positiveInject ? true : false);
				algo.update();
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
					algo.seedReset();
				}
				else {
					algo.setReadHead(0);
					algo.setWriteHead(1);
				}
				algo.update();
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
				algo.setOffset(offsetPending);
			}

			// Synced inject
			switch (injectPendingState) {
			case 1:
				algo.inject(true, true);
				break;
			case 2:
				algo.inject(false, true);
				break;
			default: 
				break;
			}

			// Synced reset
			if (resetPending || ruleResetPending) {
				if (gen) {
					algo.seedReset(true);
				}
				else if (!ruleResetPending) {
					algo.setWriteHead(0);
				}
			}

			if (gen && !resetPending && !ruleResetPending && !injectPendingState) {
				algo.generate();
			}

			algo.step(sequenceLength);
			algo.update();

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			ruleResetPending = false;
			injectPendingState = 0;
		}

		// OUTPUT
		float xCv = algo.getXVoltage();
		float yCv = algo.getYVoltage();
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

		xCv = slewLimiter.process(xCv);


		// Cv outputs -  0V to 10V or -5V to 5V (10Vpp).
		xCv = xCv * params[X_SCALE_PARAM].getValue() * 10.f;
		yCv = yCv * params[Y_SCALE_PARAM].getValue() * 10.f;
		outputs[X_CV_OUTPUT].setVoltage(xCv);
		outputs[Y_CV_OUTPUT].setVoltage(yCv);

		// Pulse outputs - 0V to 10V.
		if(algo.getXPulse())
			xPulse.trigger(1e-3f);
		if (algo.getYPulse())
			yPulse.trigger(1e-3f);

		bool xGate = xPulse.process(args.sampleTime);
		bool yGate = yPulse.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(algo.getModeLEDValue(), args.sampleTime); 
		lights[X_CV_LIGHT].setBrightness(xCv * 0.1);
		lights[X_PULSE_LIGHT].setBrightnessSmooth(xGate, args.sampleTime);
		lights[Y_CV_LIGHT].setBrightness(yCv * 0.1);
		lights[Y_PULSE_LIGHT].setBrightnessSmooth(yGate, args.sampleTime);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		lights[INJECT_LIGHT].setBrightnessSmooth(positiveInject | negativeInject, args.sampleTime);

		algo.tick();
	}
};

struct WolframModuleWidget : ModuleWidget {

	struct WolframDisplay : TransparentWidget {
		WolframModule* module;
		LookAndFeel* lookAndFeel;

		std::shared_ptr<Font> font;
		std::string fontPath;

		int cols = 8;
		int rows = 8;
		float padding = 1.f;
		float cellSize = 5.f;

		float widgetSize = 0;
		float screenSize = 0;
		float cellSpacing = 0;
		float fontSize = 0;

		WolframDisplay(WolframModule* m, float y, float w, float s) {
			module = m;

			// Params
			widgetSize = s;
			screenSize = widgetSize - (padding * 2.f);
			cellSpacing = screenSize / cols;
			fontSize = (screenSize / cols) * 2.f;

			// Widget size
			box.pos = Vec((w * 0.5f) - (widgetSize * 0.5f), y);
			box.size = Vec(widgetSize, widgetSize);

			// Get font
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/mtf_wolf5.otf"));
			font = APP->window->loadFont(fontPath);

			if (!module)
				return;

			lookAndFeel = &module->lookAndFeel;
			module->algo.setDrawParams(lookAndFeel, padding, fontSize);
		}

		void drawMenu(NVGcontext* vg, MenuPages Menu, NVGcolor* c) {

			module->algo.drawTextBackground(vg, 0);
			module->algo.drawTextBackground(vg, 3);

			nvgFillColor(vg, *c);
			nvgText(vg, padding, padding, "menu", nullptr);

			switch (Menu) {
			case SEED:
				module->algo.drawSeedMenu(vg);
				break;
			case MODE:
				module->algo.drawModeMenu(vg);
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
				module->algo.drawAlgoithmMenu(vg);
				break;
			default:
				break;
			}

			nvgFillColor(vg, *c);
			nvgText(vg, padding, (fontSize * 3.f) + padding, "<#@>", nullptr);
		}

		void draw(const DrawArgs& args) override {
			NVGcolor backgroundColour = module ? *lookAndFeel->getBackgroundColour() : nvgRGB(58, 16, 19);

			// Draw background & border.
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, padding * 0.5f, padding * 0.5f,
				widgetSize - padding, widgetSize - padding, 2.f);
			nvgFillColor(args.vg, backgroundColour);
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, padding);
			nvgStrokeColor(args.vg, nvgRGB(16, 16, 16));
			nvgStroke(args.vg);
			nvgClosePath(args.vg);
		}

		void drawLayer(const DrawArgs& args, int layer) override {
			if (layer == 1) {
				// Draw cells & menu.
				NVGcolor primaryColour = module ? *lookAndFeel->getPrimaryColour() : nvgRGB(228, 7, 7);
				NVGcolor secondaryColour = module ? *lookAndFeel->getSecondaryColour() : nvgRGB(78, 12, 9);

				nvgFontSize(args.vg, fontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

				if (module && module->menu) {
					drawMenu(args.vg, module->menuPages, &primaryColour);
					return;
				}

				int firstRow = 0;
				if (module && module->displayRule) {
					firstRow = rows - 4;
					nvgFillColor(args.vg, primaryColour);
					module->algo.drawRuleMenu(args.vg);
				}


				// Requires flipping before drawing
				uint64_t matrix = module ? module->algo.getDisplayMatrix() : 8;

				// TODO: this is slow
				for (int row = firstRow; row < rows; row++) {
					int rowInvert = (row - 7) * -1;

					for (int col = 0; col < cols; col++) {
						int colInvert = 7 - col;
						int cellIndex = rowInvert * 8 + colInvert;
						bool cellState = (matrix >> cellIndex) & 1ULL;

						nvgBeginPath(args.vg);
						if (module) {
							lookAndFeel->drawCell(args.vg, row, col, cellState, padding, cellSpacing);
						}
						else {
							nvgFillColor(args.vg, cellState ? primaryColour : secondaryColour);
							nvgCircle(args.vg, (cellSpacing * col) + (cellSpacing * 0.5f) + padding,
								(cellSpacing * row) + (cellSpacing * 0.5f) + padding, 5.f);
						}
						nvgFill(args.vg);
					}
				}
				
			}
			Widget::drawLayer(args, layer);
		}
	};

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

		WolframDisplay* display = new WolframDisplay(module, mm2px(10.14f), box.size.x, mm2px(32.f));
		addChild(display);
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