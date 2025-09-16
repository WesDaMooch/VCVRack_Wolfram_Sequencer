// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg

// Therefore, modules with a CLOCK and RESET input, or similar variants, should ignore CLOCK triggers up to 1 ms 
// after receiving a RESET trigger.You can use dsp::Timer for keeping track of time.

#include "plugin.hpp"
#include <array>
//#include <vector>

namespace Colours {
	//const NVGcolor lcdBackground = nvgRGB(50, 0, 0);
	//const NVGcolor primaryAccent = nvgRGB(251, 0, 0);
	//const NVGcolor primaryAccentOff = nvgRGB(50, 0, 0);
	
	const NVGcolor lcdBackground = nvgRGB(67, 38, 38);		// Befaco 
	const NVGcolor primaryAccent = nvgRGB(195, 67, 67);		// Befaco 
	const NVGcolor primaryAccentOff = nvgRGB(67, 38, 38);

	//const NVGcolor lcdBackground = nvgRGB(56, 56, 56);			// Phrase16 
	//const NVGcolor primaryAccent = nvgRGB(175, 210, 44);		// Phrase16 
	//const NVGcolor primaryAccentOff = nvgRGB(56, 56, 56);
	//const NVGcolor lcdBackground = nvgRGB(67, 70, 55);
	//const NVGcolor primaryAccentOff = nvgRGB(67, 70, 55);

	const NVGcolor secondaryAccent = nvgRGB(255, 0, 255);
}

// Could have different structs or class for each algo with a void setParameters()
// See Befaco NoisePlethora



// Put this out here?
// are these the right types
constexpr static int PARAM_CONTROL_INTERVAL = 64;
constexpr static int MAX_MENU_PAGES = 5;
constexpr static size_t MAX_ROWS = 64;

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
		BLINK_LIGHT, //
		LIGHTS_LEN
	};

	//CellularAutomata Ca; //

	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger injectTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator xPulseGenerator;
	dsp::PulseGenerator yPulseGenerator;
	dsp::BooleanTrigger menuBoolean;
	dsp::BooleanTrigger modeBoolean;
	dsp::Timer ruleDisplayTimer;
	dsp::Timer menuDisplayTimer;

	std::array<uint8_t, MAX_ROWS> internalCircularBuffer = {};
	std::array<uint8_t, 8> outputCircularBuffer = {};

	int internalReadHead = 0;
	int internalWriteHead = 1;
	int outputReadHead = internalReadHead;
	int outputWriteHead = internalWriteHead;

	float outputScaler = 1.f / 255.f;
	int sequenceLength = 8;
	std::array<int, 10> sequenceLengths = { 2, 3, 4, 5, 6, 8, 12, 16, 32, 64 };
	uint8_t rule = 30;
	uint8_t seed = 8;
	float chance = 1;
	int offset = 0;
	int prevOffset = offset;
	bool resetFlag = false;
	bool ruleResetFlag = false;
	int mode = 0;
	int numModes = 3;

	bool menuState = false;
	int menuPage = 0;

	float rotaryEncoderIndent = 1.f / 40.f;
	float prevRotaryEncoderValue = 0.f;

	bool displayRule = false;
	bool dirty = true;

	// inline? how do they work


	WolframModule() {
		internalCircularBuffer.fill(0);
		outputCircularBuffer.fill(0);
		internalCircularBuffer[internalReadHead] = seed;
		outputCircularBuffer[outputReadHead] = seed;

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
		configParam(X_SCALE_PARAM, 0.f, 10.f, 5.f, "X CV Scale", "V");
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 10.f, 5.f, "Y CV Scale", "V");
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

	uint8_t getRow(int index = 0) {
		size_t rowIndex = (outputReadHead - index + 8) & 7;
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

	int rotaryEncoder(float rotaryEncoderValue) {
		float difference = rotaryEncoderValue - prevRotaryEncoderValue;
		int delta = 0;
		while (difference >= rotaryEncoderIndent) {
			delta++;
			prevRotaryEncoderValue += rotaryEncoderIndent;
			difference = rotaryEncoderValue - prevRotaryEncoderValue;
		}
		while (difference <= -rotaryEncoderIndent) {
			delta--;
			prevRotaryEncoderValue -= rotaryEncoderIndent;
			difference = rotaryEncoderValue - prevRotaryEncoderValue;
		}
		return delta;
	}


	void process(const ProcessArgs& args) override {
		//	*****	PARAM	*****
		if (((args.frame + this->id) % PARAM_CONTROL_INTERVAL) == 0) {
			// Do param control stuff here
			// need a smart way to set dirty flag when params change or things are triggered
			// not calling dirty when I dont need to, like when menu is open

			sequenceLength = sequenceLengths[static_cast<int>(params[LENGTH_PARAM].getValue())];

			if (menuBoolean.process(params[MENU_PARAM].getValue())) {
				menuState ^= true;
				menuDisplayTimer.reset();
				dirty = true;
			}

			if (modeBoolean.process(params[MODE_PARAM].getValue())) {
				if (menuState) {
					menuPage++;
					if (menuPage >= MAX_MENU_PAGES) { menuPage = 0; }
					menuDisplayTimer.reset();
					dirty = true;
				}
				else {
					mode = (mode + 1) % numModes;
				}
			}

			int selectDelta = rotaryEncoder(params[SELECT_PARAM].getValue());
			if (selectDelta != 0) {
				if (menuState) {
					switch (menuPage) {
					case 0:
						// Add random seed mode
						seed = static_cast<uint8_t>((seed + selectDelta + 256) % 256);
						break;
					case 1:
						mode = (mode + 1) % numModes; // This line comes up twice...
						break;
					//...
					}
					menuDisplayTimer.reset();
				}
				else {
					rule = static_cast<uint8_t>((rule + selectDelta + 256) % 256);
					ruleResetFlag = true;
					displayRule = true;
					ruleDisplayTimer.reset();
				}
				dirty = true;
			}
		}

		if (displayRule && ruleDisplayTimer.process(args.sampleTime) > 0.75f) {
			displayRule = false;
			dirty = true;
		}

		if (menuState && menuDisplayTimer.process(args.sampleTime) > 5.0f) {
			menuState = false;
			dirty = true;
		}

		//	*****	INPUT	*****

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f)) {
			resetFlag = true;
		}

		float chanceCv = clamp(inputs[CHANCE_INPUT].getVoltage(), 0.f, 10.f) * 0.1f;
		chance = clamp(params[CHANCE_PARAM].getValue() + chanceCv, 0.f, 1.f);

		float offsetCv = clamp(inputs[OFFSET_INPUT].getVoltage(), -10.f, 10.f) * 0.8f;
		offset = std::round(clamp(params[OFFSET_PARAM].getValue() + offsetCv, -4.f, 4.f));
		
		if (!menuState && offset != prevOffset) {
			dirty = true;
		}
		prevOffset = offset;

		if (injectTrigger.process(inputs[INJECT_INPUT].getVoltage(), 0.1f, 2.f)) {
			// Pos V adds, neg V removes?
			int pos = 3;
			int displayPos = (pos + offset + 8) & 7;
			if (inputs[INJECT_INPUT].getVoltage() < 0.f) {
				// Kill random cell
			}
			else
			{
				// Add
				// internalCircularBuffer[internalReadHead] |= (1 << pos);
				// outputCircularBuffer[outputReadHead] |= (1 << displayPos);
			}
			internalCircularBuffer[internalReadHead] |= (1 << pos);
			outputCircularBuffer[outputReadHead] |= (1 << displayPos);
			if (!menuState) {
				dirty = true;
			}
		}
	
		float trig = inputs[TRIG_INPUT].getVoltage();					
		if (trigTrigger.process(trig * trig, 0.2f, 4.f)) {			// Squared for bipolar signals
			//	*****	STEP	*****

			bool generateFlag = random::get<float>() < chance;

			bool resetCondition = resetFlag || ruleResetFlag;
			if (generateFlag) {
				if (resetCondition) {
					internalCircularBuffer[internalWriteHead] = seed;
					resetFlag = false;
					ruleResetFlag = false;
				}
				else {
					// Generate row
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

						switch (mode) {
						case 1:
							// Clip
							if (left < 0) { leftCell = 0; }
							if (right > 7) { rightCell = 0; }
							break;
						case 2:
							// Rand
							if (left < 0) { leftCell = random::get<bool>(); }
							if (right > 7) { rightCell = random::get<bool>(); }
							break;
						}

						int tag = 7 - ((leftCell << 2) | (cell << 1) | rightCell);

						int ruleBit = (rule >> (7 - tag)) & 1;
						if (ruleBit > 0) {
							internalCircularBuffer[internalWriteHead] |= (1 << i);
						}
						else {
							internalCircularBuffer[internalWriteHead] &= ~(1 << i);
						}
					}
				}
			}
			else if (resetFlag) {
				internalWriteHead = 0;
				resetFlag = false;
			}

			// Copy internal buffer to output buffer
			outputCircularBuffer[outputWriteHead] = internalCircularBuffer[internalWriteHead];

			// Advance output read and write heads
			outputReadHead = outputWriteHead;
			outputWriteHead = (outputWriteHead + 1) & 7;

			// Advance internal read and write heads
			internalReadHead = internalWriteHead;
			internalWriteHead = (internalWriteHead + 1) % sequenceLength;

			// Redraw matrix display
			if (!menuState) {
				dirty = true;
			}
		}

		//	*****	OUTPUT	*****

		// Works nicely, not sure if its the correct use of the pulse generator, its more a gate now
		//bool xPulseTrigger = (Ca.getRow() >> 7) & 1;
		bool xPulseTrigger = (getRow() >> 7) & 1;
		if (xPulseTrigger) {
			xPulseGenerator.trigger(1e-3f);
		}


		bool yPulseTrigger = getColumn() & 1; // & 1?
		if (yPulseTrigger) {
			yPulseGenerator.trigger(1e-3f);
		}

		// Have an audio out mode? (-5V tp 5v)?

		float xCV = (getRow() * outputScaler) * params[X_SCALE_PARAM].getValue(); 
		outputs[X_CV_OUTPUT].setVoltage(xCV);
		bool xPulseOutput = xPulseGenerator.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xPulseOutput ? 10.f : 0.f);

		float yCV = (getColumn() * outputScaler) * params[Y_SCALE_PARAM].getValue();
		outputs[Y_CV_OUTPUT].setVoltage(yCV);
		bool yPulseOutput = yPulseGenerator.process(args.sampleTime);
		outputs[Y_PULSE_OUTPUT].setVoltage(yPulseOutput ? 10.f : 0.f);

		// Debug output
		//outputs[Y_PULSE_OUTPUT].setVoltage(sequenceLength);
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
	float matrixSize = mm2px(32.0f);	//31.7
	int matrixCols = 8;
	int matrixRows = 8;
	float cellPos = matrixSize / matrixCols;
	float cellSize = cellPos * 0.5f;
	float fontSize = (matrixSize / matrixCols) * 2.0f;

	// might need to be a vector oneday
	// seems not efficient
	std::array < std::function<void(NVGcontext*, float)>, MAX_MENU_PAGES> menuPages;

	MatrixDisplay(WolframModule* m) {
		module = m;
		box.pos = Vec(mm2px(30.1) - matrixSize * 0.5, mm2px(26.14) - matrixSize * 0.5);
		box.size = Vec(matrixSize, matrixSize);

		// Load font
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/mtf_wolf2.otf"));
		font = APP->window->loadFont(fontPath);

		menuPages = {
			// Depending on Algo, display text could be Ca menuItem1,2,3 etc return "NAME"

			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, 0, "MENU", nullptr);
				nvgText(vg, 0, row, "SEED", nullptr);
				char seedStr[5];
				snprintf(seedStr, sizeof(seedStr), "%4.3d", module->seed); // this ok?
				nvgText(vg, 0, row * 2, seedStr, nullptr);
				//nvgText(vg, 0, 0, "<*+>", nullptr);
			},
			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, 0, "MENU", nullptr);
				nvgText(vg, 0, row, "MODE", nullptr);
				// This will change... different agors have different modes
				switch (module->mode) {
				case 0:
					nvgText(vg, 0, row * 2, "WRAP", nullptr);
					break;
				case 1:
					nvgText(vg, 0, row * 2, "CLIP", nullptr);
					break;
				case 2:
					nvgText(vg, 0, row * 2, "RAND", nullptr);
					break;
				default:
					nvgText(vg, 0, row * 2, "EROR", nullptr);
					break;
				}
			},
			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, 0, "MENU", nullptr);
				nvgText(vg, 0, row, "ALGO", nullptr);
				nvgText(vg, 0, row * 2, "LIFE", nullptr);
				// WOLF / ANT / LIFE
			},
			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, 0, "MENU", nullptr);
				nvgText(vg, 0, row, "LOOK", nullptr);
				nvgText(vg, 0, row * 2, " RED", nullptr);
				// RED / BLUE / GREN / B&W
			},
			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, 0, "MENU", nullptr);
				nvgText(vg, 0, row, "FEEL", nullptr);
				nvgText(vg, 0, row * 2, "BALL", nullptr);
			}
		};
	}

	void draw(NVGcontext* vg) override {
		// Background
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, matrixSize, matrixSize);
		nvgFillColor(vg, Colours::lcdBackground);
		nvgFill(vg);
		nvgClosePath(vg);

		if (!module) return;
		if (!font) return;

		nvgFontSize(vg, fontSize);
		nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

		if (module->menuState) {
			nvgFillColor(vg, Colours::primaryAccent);
			menuPages[module->menuPage](vg, fontSize);
		}
		else {
			int startRow = 0;
			if (module->displayRule) {
				startRow = matrixRows - 4;

				nvgFillColor(vg, Colours::primaryAccent);
				nvgText(vg, 0, 0, "RULE", nullptr);
				char ruleStr[5];
				snprintf(ruleStr, sizeof(ruleStr), "%4.3d", module->rule);
				nvgText(vg, 0, fontSize, ruleStr, nullptr);
			}

			for (int row = startRow; row < matrixRows; row++) {
				int rowOffset = (row - 7) * -1;

				uint8_t displayRow = module->getRow(rowOffset);

				for (int col = 0; col < matrixCols; col++) {
					bool cellState = false;
					if ((displayRow >> (7 - col)) & 1) {
						cellState = true;
					}
					nvgBeginPath(vg);
					nvgCircle(vg, (cellPos * col) + cellSize, (cellPos * row) + cellSize, cellSize);
					nvgFillColor(vg, cellState ? Colours::primaryAccent : Colours::primaryAccentOff);
					nvgFill(vg);
				}
			}
		}
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
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(15.24f, 60.0f)), module, WolframModule::LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(45.72f, 60.0f)), module, WolframModule::CHANCE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(30.48f, 80.0f)), module, WolframModule::OFFSET_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.16f, 80.0f)), module, WolframModule::X_SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(50.8f, 80.0f)), module, WolframModule::Y_SCALE_PARAM));
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
		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));

		MatrixDisplayBuffer* matrixDisplayFb = new MatrixDisplayBuffer(module);
		MatrixDisplay* matrixDisplay = new MatrixDisplay(module);
		matrixDisplayFb->addChild(matrixDisplay);
		addChild(matrixDisplayFb);
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");
