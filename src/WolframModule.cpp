// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg

// Act on this:
// Therefore, modules with a CLOCK and RESET input, or similar variants, should ignore CLOCK triggers up to 1 ms 
// after receiving a RESET trigger.You can use dsp::Timer for keeping track of time.
// Or do what SEQ3 does?

// Could have different structs or class for each algo with a void setParameters()
// See Befaco NoisePlethora


// To Do
// TODO: Have x y row mode and average mode (this would be good for 2D world)
// TODO: Use scale param as a filter when in vco mode?
// TODO: Game - space defence, control platform with offset, 'floor' rises when letting a bit through


#include "plugin.hpp"
#include <array>

// Put this out here?
// are these the right types
constexpr static int PARAM_CONTROL_INTERVAL = 64;
constexpr static size_t MAX_SEQUENCE_LENGTH = 64;

enum Mode {
	WRAP,
	CLIP,
	RAND,
	MODE_LEN
};

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

	Mode moduleMode = Mode::WRAP;	// const?
	Look moduleLook = Look::BEFACO;
	DisplayMenu moduleMenu = DisplayMenu::SEED;

	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger posInjectTrigger;
	dsp::SchmittTrigger negInjectTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator yPulseGenerator;
	dsp::BooleanTrigger menuTrigger;
	dsp::BooleanTrigger modeTrigger;
	dsp::Timer ruleDisplayTimer;

	std::array<uint8_t, MAX_SEQUENCE_LENGTH> internalCircularBuffer = {};
	std::array<uint8_t, 8> outputCircularBuffer = {};

	int internalReadHead = 0;
	int internalWriteHead = 1;
	int outputReadHead = 0;

	int sequenceLength = 8;
	std::array<int, 10> sequenceLengths = { 2, 3, 4, 5, 6, 8, 12, 16, 32, 64 };	// const?
	uint8_t rule = 30;
	uint8_t seed = 8;
	int seedSelect = seed;
	bool randSeed = false;
	float chance = 1;
	int offset = 0;
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

	const float rotaryEncoderIndent = 1.f / 40.f;
	float prevRotaryEncoderValue = 0.f;
	float outputScaler = 1.f / 255.f;
	
	// Display
	bool displayRule = false;
	bool displayUpdate = true;
	int displayUpdateInterval = 1000;
	bool dirty = true;

	inline int fastRoundInt(float value) {
		return static_cast<int>(value + (value >= 0.f ? 0.5f : -0.5f));
	}

	WolframModule() {
		internalCircularBuffer.fill(0);
		outputCircularBuffer.fill(0);
		internalCircularBuffer[internalReadHead] = seed;
		//outputCircularBuffer[outputReadHead] = seed;

		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(MENU_PARAM, "Menu");
		configButton(MODE_PARAM, "Mode");
		configParam(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Select");
		configParam(LENGTH_PARAM, 0.f, 9.f, 5.f, "Length", " Sel");
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance", "%", 0.f, 100.f);
		paramQuantities[CHANCE_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(X_SCALE_PARAM, 0.f, 1.f, 0.5f, "X CV Scale", "V", 10.f);
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 1.f, 0.5f, "Y CV Scale", "V", 10.f);
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
	}

	uint8_t applyOffset(uint8_t row) {
		int shift = offset;
		if (shift < 0) {
			shift = -shift;
			return (row << shift) | (row >> (8 - shift));
		}
		else if (shift > 0) {
			return (row >> shift) | (row << (8 - shift));
		}
		return row;
	}

	uint8_t getRow(int i = 0) {
		i = clamp(i, 0, 8);
		size_t rowIndex = (outputReadHead - i + 8) & 7;
		return applyOffset(outputCircularBuffer[rowIndex]);
	}

	uint8_t getColumn() {
		uint8_t col = 0;
		for (int i = 0; i < 8; i++) {
			size_t row = getRow(i);
			col |= ((row & 0x01) << (7 - i));
		}
		return col;
	}

	uint8_t applyInject(uint8_t row, bool remove = false) {
		// Is this an ok level of efficentcy
		uint8_t targetMask = row;
		if (remove) {
			if (row == 0x00) return row;
		}
		else {
			if (row == 0xFF) return row;
			targetMask = ~row;	// Flip row
		}

		int targetCount = __builtin_popcount(targetMask); // Count target bits
		int target = random::get<uint8_t>() % targetCount;	// Random target index

		// Find corresponding bit position
		uint8_t bitMask;
		for (bitMask = 1; target || !(targetMask & bitMask); bitMask <<= 1) {
			if (targetMask & bitMask) target--;
		}

		row = remove ? (row & ~bitMask) : (row | bitMask);
		return row;
	}

	int rotaryEncoder(float value) {
		float difference = value - prevRotaryEncoderValue;
		int delta = 0;
		while (difference >= rotaryEncoderIndent) {
			delta++;
			prevRotaryEncoderValue += rotaryEncoderIndent;
			difference = value - prevRotaryEncoderValue;
		}
		while (difference <= -rotaryEncoderIndent) {
			delta--;
			prevRotaryEncoderValue -= rotaryEncoderIndent;
			difference = value - prevRotaryEncoderValue;
		}
		return delta;
	}

	void generateRow() {
		// Cellular automata 
		for (int i = 0; i < 8; i++) {

			int left = i - 1;
			int right = i + 1;
			int leftIndex = left;
			int rightIndex = right;

			// Wrap
			if (left < 0) { leftIndex = 7; }
			if (right > 7) { rightIndex = 0; }

			int leftCell = (internalCircularBuffer[internalReadHead] >> leftIndex) & 1;
			int cell = (internalCircularBuffer[internalReadHead] >> i) & 1;
			int rightCell = (internalCircularBuffer[internalReadHead] >> rightIndex) & 1;

			switch (moduleMode) {
			case CLIP:
				if (left < 0) { leftCell = 0; }
				if (right > 7) { rightCell = 0; }
				break;
			case RAND:
				if (left < 0) { leftCell = random::get<bool>(); }
				if (right > 7) { rightCell = random::get<bool>(); }
				break;
			default:
				break;
			}

			int tag = 7 - ((leftCell << 2) | (cell << 1) | rightCell);

			bool ruleBit = (rule >> (7 - tag)) & 1;
			if (ruleBit) {
				internalCircularBuffer[internalWriteHead] |= (1 << i);
			}
			else {
				internalCircularBuffer[internalWriteHead] &= ~(1 << i);
			}
		}
	}

	void process(const ProcessArgs& args) override {

		// BUTTONS & ENCODER

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
				int numModes = static_cast<int>(Mode::MODE_LEN);
				int modeIndex = static_cast<int>(moduleMode);
				modeIndex++;
				if (modeIndex >= numModes) { modeIndex = 0; }
				moduleMode = static_cast<Mode>(modeIndex);
			}
		}

		int selectDelta = rotaryEncoder(params[SELECT_PARAM].getValue());
		if (selectDelta != 0) {
			if (menu) {
				// Menu selection
				switch (moduleMenu) {
				case SEED: {
					// Seed options are 256 + 1 (RAND) 
					seedSelect = seedSelect + selectDelta;
					if (seedSelect > 256) { seedSelect -= 257; }
					else if (seedSelect < 0) { seedSelect += 257; }
					if (seedSelect == 256) {
						randSeed = true;
					}
					else {
						seed = static_cast<uint8_t>(seedSelect);
						randSeed = false;
					}
					break;
				}
				case MODE: {
					int numModes = static_cast<int>(Mode::MODE_LEN);
					int modeIndex = static_cast<int>(moduleMode);
					modeIndex = (modeIndex + selectDelta + numModes) % numModes;
					moduleMode = static_cast<Mode>(modeIndex);
					break;
				}
				case SYNC: {
					sync = !sync;
					break;
				}
				case FUNCTION: {
					vcoMode = !vcoMode;
					break;
				}
				case ALGORITHM: {
					break;
				}
				case LOOK: {
					int numLooks = static_cast<int>(Look::LOOK_LEN);
					int lookIndex = static_cast<int>(moduleLook);
					lookIndex = (lookIndex + selectDelta + numLooks) % numLooks;
					moduleLook = static_cast<Look>(lookIndex);
					break;
				}
				default: { break; }
				}
			}
			else {
				// Rule selection
				rule = static_cast<uint8_t>((rule + selectDelta + 256) % 256);
				ruleResetPending = true;
				displayRule = true;
				ruleDisplayTimer.reset();
			}
			displayUpdate = true;
		}


		if (displayRule && ruleDisplayTimer.process(args.sampleTime) > 0.75f) {
			displayRule = false;
			if (!menu) displayUpdate = true;
		}

		// DIALS & INPUTS
		
		sequenceLength = sequenceLengths[static_cast<int>(params[LENGTH_PARAM].getValue())];

		float chanceCv = clamp(inputs[CHANCE_INPUT].getVoltage(), 0.f, 10.f) * 0.1f;
		chance = clamp(params[CHANCE_PARAM].getValue() + chanceCv, 0.f, 1.f); // Used the min max thing here too

		float offsetCv = clamp(inputs[OFFSET_INPUT].getVoltage(), -10.f, 10.f) * 0.8f;
		int newOffset = std::min(std::max(fastRoundInt(params[OFFSET_PARAM].getValue() + offsetCv), -4), 4);
		if (sync) {
			offsetPending = newOffset;
		}
		else {
			if (!menu && (offset != newOffset)) { displayUpdate = true; }
			offset = newOffset;
		}

		bool posInject = posInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), 0.1f, 2.f);
		bool negInject = negInjectTrigger.process(inputs[INJECT_INPUT].getVoltage(), -2.f, -0.1f);
		if (posInject || negInject) {
			if (sync) {
				if (posInject) {
					injectPendingState = 1;
				}
				else {
					injectPendingState = 2;
				}
			}
			else {
				internalCircularBuffer[internalReadHead] = applyInject(internalCircularBuffer[internalReadHead], posInject ? false : true);

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
				if (gen) {
					if (randSeed) {
						internalCircularBuffer[internalReadHead] = random::get<uint8_t>();
					}
					else {
						internalCircularBuffer[internalReadHead] = seed;
					}
				}
				else {
					internalReadHead = 0;
					internalWriteHead = 1;
				}
				genPending = true;
				resetPending = false;
				if (!menu) displayUpdate = true;
			}
		}

		// STEP		
		bool trig = false;
		float trigVotagte = inputs[TRIG_INPUT].getVoltage();
		if (vcoMode) {
			trig = (trigVotagte > 0.f && lastTrigVoltage <= 0.f) || (trigVotagte < 0.f && lastTrigVoltage >= 0.f);
		}
		else {
			trig = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f);
		}
		lastTrigVoltage = trigVotagte;

		// do the dont trigger when reseting thing SEQ3
		if (trig) {
			if (!genPending) {
				// Find gen if not found when reset triggered and sync false
				gen = random::get<float>() < chance;
			}

			// Synced offset
			if (sync) {
				offset = offsetPending;
			}

			// Synced inject
			switch (injectPendingState) {
			case 1:
				internalCircularBuffer[internalWriteHead] = applyInject(internalCircularBuffer[internalWriteHead], false);
				break;
			case 2:
				internalCircularBuffer[internalWriteHead] = applyInject(internalCircularBuffer[internalWriteHead], true);
				break;
			default: 
				break;
			}

			// Synced reset
			if (resetPending || ruleResetPending) {
				if (gen || ruleResetPending) {
					if (randSeed) {
						internalCircularBuffer[internalWriteHead] = random::get<uint8_t>();
					}
					else {
						internalCircularBuffer[internalWriteHead] = seed;
					}
				}
				else {
					internalWriteHead = 0;
				}
			}

			if (gen && !resetPending && !ruleResetPending && !injectPendingState) {
				generateRow();
			}

			// Advance internal read and write heads
			internalReadHead = internalWriteHead;
			internalWriteHead = (internalWriteHead + 1) % sequenceLength;

			// Advance output read head
			outputReadHead = (outputReadHead + 1) & 7;

			// Reset gen and sync penders
			gen = false;
			genPending = false;
			resetPending = false;
			ruleResetPending = false;
			injectPendingState = 0;

			// Redraw matrix display
			if (!menu) displayUpdate = true;
		}
		// Update output buffer
		outputCircularBuffer[outputReadHead] = internalCircularBuffer[internalReadHead];

		// OUTPUT
		uint8_t x = getRow();
		float xCv = x * outputScaler * 10.f;
		if (vcoMode) {
			xCv -= 5.f;
		}
		xCv *= params[X_SCALE_PARAM].getValue();;

		outputs[X_CV_OUTPUT].setVoltage(xCv);
		bool xGate = (x >> 7) & 1;
		outputs[X_PULSE_OUTPUT].setVoltage(xGate ? 10.f : 0.f);

		uint8_t y = getColumn();
		float yCv = y * outputScaler * 10.f;
		if (vcoMode) {
			yCv -= 5.f;
		}
		yCv *= params[Y_SCALE_PARAM].getValue();;
		outputs[Y_CV_OUTPUT].setVoltage(yCv);
		bool yGate = y & 1;
		outputs[Y_PULSE_OUTPUT].setVoltage(yGate ? 10.f : 0.f);

		// Debug output
		//outputs[Y_PULSE_OUTPUT].setVoltage(sequenceLength);

		// LIGHTS
		lights[MODE_LIGHT].setBrightnessSmooth(static_cast<float>(moduleMode) / (Mode::MODE_LEN - 1), args.sampleTime); // bad div
		lights[X_CV_LIGHT].setBrightness(xCv * 0.1);
		lights[X_GATE_LIGHT].setBrightness(xGate);
		lights[Y_CV_LIGHT].setBrightness(yCv * 0.1);
		lights[Y_GATE_LIGHT].setBrightness(yGate);
		lights[TRIG_LIGHT].setBrightnessSmooth(trig, args.sampleTime);
		lights[INJECT_LIGHT].setBrightnessSmooth(posInject, args.sampleTime);

		// UPDATE DISPLAY - 30fps limited
		if (displayUpdate && (((args.frame + this->id) % displayUpdateInterval) == 0)) {
			displayUpdate = false;
			dirty = true;
		}
	}

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();
		displayUpdateInterval = static_cast<int>(sampleRate / 30.f);
	}
};

struct MatrixDisplayBuffer : FramebufferWidget {
	WolframModule* module;
	MatrixDisplayBuffer(WolframModule* m) {
		module = m;
	}
	void step() override {
		if (module && module->dirty) {
			FramebufferWidget::dirty = true;
			module->dirty = false;
		}
		FramebufferWidget::step();
	}
};


struct MatrixDisplay : Widget {
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
		module = m;	// does this get used
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
				nvgText(vg, 0, fontSize, "SEED", nullptr);
				if (module->randSeed) {
					nvgText(vg, 0, fontSize * 2, "RAND", nullptr);
				}
				else {
					for (int col = 0; col < matrixCols; col++) {
						bool seedCell = false;
						if ((module->seed >> (7 - col)) & 1) {
							seedCell = true;
						}
						nvgBeginPath(vg);
						nvgCircle(vg, (cellPos * col) + cellSize, fontSize * 2.5, cellSize);
						nvgFillColor(vg, seedCell ? primaryAccent : primaryAccentOff);
						nvgFill(vg);
					}
				}
				nvgFillColor(vg, primaryAccent);	// Reset Colour
				break;
			}
			case MODE: {
				nvgText(vg, 0, fontSize, "MODE", nullptr);
				switch (module->moduleMode) {
				case WRAP:
					nvgText(vg, 0, fontSize * 2, "WRAP", nullptr);
					break;
				case CLIP:
					nvgText(vg, 0, fontSize * 2, "CLIP", nullptr);
					break;
				case RAND:
					nvgText(vg, 0, fontSize * 2, "RAND", nullptr);
					break;
				default:
					break;
				}
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
				nvgText(vg, 0, fontSize, "ALGO", nullptr);
				nvgText(vg, 0, fontSize * 2, "WOLF", nullptr);
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
					primaryAccent = nvgRGB(175, 210, 44);	// Fix
					primaryAccentOff = lcdBackground;
					break;
				case MONO:
					nvgText(vg, 0, fontSize * 2, "MONO", nullptr);
					lcdBackground = nvgRGB(18, 18, 18);	// Fix
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
				module->dirty = true;	// feels bad doing this
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

		if (!module) {
			// Draw preview 
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
				nvgText(vg, 0, 0, "RULE", nullptr);
				char ruleStr[5];
				snprintf(ruleStr, sizeof(ruleStr), "%4.3d", module->rule);
				nvgText(vg, 0, fontSize, ruleStr, nullptr);
			}

			for (int row = startRow; row < matrixRows; row++) {
				int rowOffset = (row - 7) * -1;	// clamp?

				uint8_t displayRow = module->getRow(rowOffset);

				for (int col = 0; col < matrixCols; col++) {
					bool cellState = false;
					if ((displayRow >> (7 - col)) & 1) {
						cellState = true;
					}
					nvgBeginPath(vg);
					nvgCircle(vg, (cellPos * col) + cellSize, (cellPos * row) + cellSize, cellSize);
					nvgFillColor(vg, cellState ? primaryAccent : primaryAccentOff);
					nvgFill(vg);
				}
			}
		}
	}
};

struct CustomShapeLight : ModuleLightWidget {
	void drawLight(const DrawArgs& args) override {
		// Example: draw a rectangle instead of a circle
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgRect(args.vg, 0, 0, 40, 40);
		nvgFillColor(args.vg, nvgRGB(255, 0, 0));
		nvgFill(args.vg);
	}
};

struct WolframModuleWidget : ModuleWidget {
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
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(52.8f, 18.5f)), module, WolframModule::SELECT_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(15.24f, 60.f)), module, WolframModule::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(45.72f, 60.f)), module, WolframModule::CHANCE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48f, 80.f)), module, WolframModule::OFFSET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.16f, 80.f)), module, WolframModule::X_SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(50.8f, 80.f)), module, WolframModule::Y_SCALE_PARAM));
		// Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.4f, 18.5f)), module, WolframModule::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4f, 96.5f)), module, WolframModule::CHANCE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35.56f, 96.5f)), module, WolframModule::OFFSET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4f, 111.5f)), module, WolframModule::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35.56f, 111.5f)), module, WolframModule::INJECT_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 96.5f)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 111.5f)), module, WolframModule::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 96.5f)), module, WolframModule::Y_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 111.5f)), module, WolframModule::Y_PULSE_OUTPUT));
		// LEDs
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(52.8f, 45.67f)), module, WolframModule::MODE_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(16.51f, 96.5f)), module, WolframModule::X_CV_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(16.51f, 111.5f)), module, WolframModule::X_GATE_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(44.45f, 96.5f)), module, WolframModule::Y_CV_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(44.45f, 111.5f)), module, WolframModule::Y_GATE_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(16.51f, 104.f)), module, WolframModule::TRIG_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(44.45, 104.f)), module, WolframModule::INJECT_LIGHT));

		MatrixDisplayBuffer* matrixDisplayFb = new MatrixDisplayBuffer(module);
		MatrixDisplay* matrixDisplay = new MatrixDisplay(module);
		matrixDisplayFb->addChild(matrixDisplay);
		addChild(matrixDisplayFb);
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");