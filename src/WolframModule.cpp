// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg


// Use rack pulseGenerator for pulse outs...

#include "plugin.hpp"
#include "WolframCA.hpp"
#include <vector> // Dont need?


namespace Colours {
	const NVGcolor lcdBackground = nvgRGB(50, 0, 0);
	//const NVGcolor primaryAccent = nvgRGB(251, 134, 38);
	const NVGcolor primaryAccent = nvgRGB(251, 0, 0);
	const NVGcolor primaryAccentOff = nvgRGB(50, 0, 0);
	const NVGcolor secondaryAccent = nvgRGB(255, 0, 255);
}

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
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	constexpr static int CONTROL_INTERVAL = 64;

	CellularAutomata Ca;

	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger injectTrigger;
	dsp::PulseGenerator xPulseGenerator;
	dsp::PulseGenerator yPulseGenerator;

	float rotaryEncoderIndent = 1.0f / 60.0f;
	float prevRotaryEncoderValue = 0.f;

	dsp::BooleanTrigger menuBoolean;
	bool menuState = false;
	int menuPage = 0;
	static constexpr int MAX_MENU_PAGES = 3;
	std::array < std::function<void(NVGcontext*, float)>, MAX_MENU_PAGES> menuPages;

	dsp::BooleanTrigger modeBoolean;
	int modeState = 0;

	bool displayRule = false;
	dsp::Timer ruleDisplayTimer;

	float prevOffset = 0;

	// Redraw matrix display 
	bool dirty = true;

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Button
		configButton(MENU_PARAM, "Menu");
		configButton(MODE_PARAM, "Mode");
		// Dial
		configParam(SELECT_PARAM, -INFINITY, +INFINITY, 0, "Select");				// Rotary encoder
		configParam(LENGTH_PARAM, 2.f, 64.f, 8.f, "Length"); // Fix
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance", "%", 0.f, 100.f);
		paramQuantities[CHANCE_PARAM]->displayPrecision = 3;
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		configParam(X_SCALE_PARAM, 0.f, 10.f, 5.f, "X CV Scale", "V");
		paramQuantities[X_SCALE_PARAM]->displayPrecision = 3;
		configParam(Y_SCALE_PARAM, 0.f, 10.f, 5.f, "Y CV Scale", "V");
		paramQuantities[Y_SCALE_PARAM]->displayPrecision = 3;
		// Input
		configInput(TRIG_INPUT, "Trigger input");
		configInput(INJECT_INPUT, "Inject input");
		// Output
		configOutput(X_CV_OUTPUT, "X CV output");
		configOutput(X_PULSE_OUTPUT, "X pulse output");
		configOutput(Y_CV_OUTPUT, "Y CV");
		configOutput(Y_PULSE_OUTPUT, "Y pulse output");

		menuPages = {
			// Depending on Algo, display text could be Ca menuItem1,2,3 etc return "NAME"
			
			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, row, "SEED", nullptr);
				char seedStr[5];
				snprintf(seedStr, sizeof(seedStr), "%4.3d", this->Ca.getSeed());
				nvgText(vg, 0, row * 2, seedStr, nullptr);
			},
			[](NVGcontext* vg, float row) {
				nvgText(vg, 0, row, "MODE", nullptr);
				nvgText(vg, 0, row*2, "CLIP", nullptr);
				//WRAP/RAND/CLIP
			},
			[this](NVGcontext* vg, float row) {
				nvgText(vg, 0, row, "ALGO", nullptr);
				nvgText(vg, 0, row * 2, "WOLF", nullptr);
			}
		};

	}


	// Rotary encoder handling - see how Phrase16 does it
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

		//	*****	INPUT	*****
		if (((args.frame + this->id) % CONTROL_INTERVAL) == 0) {
			// Do param control stuff here
			// need a smart way to set dirty flag when params change or things are triggered
			// not calling dirty when I dont need to, like when menu is open
			Ca.setSequenceLength(params[LENGTH_PARAM].getValue());
			Ca.setChance(params[CHANCE_PARAM].getValue());

			if (menuBoolean.process(params[MENU_PARAM].getValue())) {
				menuState ^= true;
				menuPage = 0;
				dirty = true;
			}

			if (modeBoolean.process(params[MODE_PARAM].getValue())) {
				if (menuState) {
					menuPage++;
					if (menuPage >= MAX_MENU_PAGES) { menuPage = 0; }
					dirty = true;
				}
				else {
					//
					modeState++;
				}
			}

			int selectDelta = rotaryEncoder(params[SELECT_PARAM].getValue());
			if (selectDelta != 0) {
				//Could we do menuStruc, menuUpdate[menuPage] ... do a defined thing
				if (menuState) {
					switch (menuPage) {
					case 0:
						Ca.changeSeed(selectDelta);
						break;
					case 1:
						break;
					//...
					}

				}
				else {
					Ca.changeRule(selectDelta);
					displayRule = true;
					ruleDisplayTimer.reset();
				}
				dirty = true;
			}

			if (injectTrigger.process(inputs[INJECT_INPUT].getVoltage(), 0.1f, 2.f)) {
				Ca.inject();
				dirty = true;
			}

			// Apply offset
			//Ca.setOffset(params[OFFSET_PARAM].getValue());
			float offset = params[OFFSET_PARAM].getValue();
			if (offset != prevOffset) {
				Ca.setOffset(offset);
				dirty = true;
			}
			prevOffset = offset;	
		}

		if (displayRule && ruleDisplayTimer.process(args.sampleTime) > 0.75f) {
			displayRule = false;
			dirty = true;
		}
	
		//	*****	STEP	*****

		float trigInput = inputs[TRIG_INPUT].getVoltage();					// Squared for bipolar signals
		if (trigTrigger.process(trigInput * trigInput, 0.2f, 4.f)) {
			Ca.step();

			// Redraw matrix display
			if (!menuState) {
				dirty = true;
			}
		}

		//	*****	OUTPUT	*****

		// Works nicely, not sure if its the correct use of the pulse generator, its more a gate now
		bool xPulseTrigger = (Ca.getRow() >> 7) & 1;
		if (xPulseTrigger) {
			xPulseGenerator.trigger(1e-3f);
		}

		bool yPulseTrigger = Ca.getColumn() & 1;
		if (yPulseTrigger) {
			yPulseGenerator.trigger(1e-3f);
		}

		float xCV = (Ca.getRow() / 255.f) * params[X_SCALE_PARAM].getValue(); // div is slow, this could be handled in Seq?
		outputs[X_CV_OUTPUT].setVoltage(xCV);
		bool xPulseOutput = xPulseGenerator.process(args.sampleTime);
		outputs[X_PULSE_OUTPUT].setVoltage(xPulseOutput ? 10.f : 0.f);

		float yCV = (Ca.getColumn() / 255.f) * params[Y_SCALE_PARAM].getValue(); 
		outputs[Y_CV_OUTPUT].setVoltage(yCV);
		bool yPulseOutput = yPulseGenerator.process(args.sampleTime);
		outputs[Y_PULSE_OUTPUT].setVoltage(yPulseOutput ? 10.f : 0.f);
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

struct MatrixDisplay2 : Widget {
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

	MatrixDisplay2(WolframModule* m) {
		module = m;
		box.pos = Vec(mm2px(30.1) - matrixSize * 0.5, mm2px(26.14) - matrixSize * 0.5);
		box.size = Vec(matrixSize, matrixSize);

		// Load font
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/mt_wolf.otf"));
		font = APP->window->loadFont(fontPath);
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
			module->menuPages[module->menuPage](vg, fontSize);
		}
		else {
			int startRow = 0;
			if (module->displayRule) {
				startRow = matrixRows - 4;

				nvgFillColor(vg, Colours::primaryAccent);
				nvgText(vg, 0, 0, "RULE", nullptr);
				char ruleStr[5];
				snprintf(ruleStr, sizeof(ruleStr), "%4.3d", module->Ca.getRule());
				nvgText(vg, 0, fontSize, ruleStr, nullptr);
			}

			for (int row = startRow; row < matrixRows; row++) {
				int rowOffset = (row - 7) * -1;

				uint8_t displayRow = module->Ca.getRow(rowOffset);

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

struct MatrixDisplay : Widget {
	WolframModule* module;
	MatrixDisplay(WolframModule* m) {
		module = m;
		box.pos = mm2px(Vec(30.1 - matrixSize * 0.5, 26.14 - matrixSize * 0.5));
		box.size = mm2px(Vec(matrixSize, matrixSize));
	}

	float matrixSize = 32;	//31.7
	int matrixCols = 8;
	int matrixRows = 8;
	float segementSize = mm2px(matrixSize / matrixCols);

	//static const?
	std::array<std::vector<Vec>, 11> segmentShapes{ {
			{ Vec(0,0), Vec(0,4), Vec(1,5), Vec(4,2), Vec(4,0), Vec(0,0) },				// Segment 0 - top left
			{ Vec(4,0), Vec(4,2), Vec(5,1), Vec(4,0) },									// Segment 1 - top middle left
			{ Vec(5,1), Vec(6,2), Vec(6,0), Vec(5,1) },									// Segment 2 - top middle right
			{ Vec(6,0), Vec(6,2), Vec(9,5), Vec(10,4), Vec(10,0), Vec(6,0) },			// Segment 3 - top right
			{ Vec(1,5), Vec(4,8), Vec(4,2), Vec(1,5) },									// Segment 4 - middle left
			{ Vec(4,2), Vec(4,8), Vec(5,9), Vec(6,8), Vec(6,2), Vec(5,1), Vec(4,2) },	// Segment 5 - middle
			{ Vec(6,2), Vec(6,8), Vec(9,5), Vec(6,2) },									// Segment 6 - middle right
			{ Vec(0,6), Vec(0,10), Vec(4,10), Vec(4,8), Vec(1,5), Vec(0,6) },			// Segment 7 - bottom left
			{ Vec(4,8), Vec(4,10), Vec(5,9), Vec(4,8) },								// Segment 8 - bottom middle left
			{ Vec(5,9), Vec(6,10), Vec(6,8), Vec(5,9) },								// Segment 9 - bottom middle right
			{ Vec(6,8), Vec(6,10), Vec(10,10), Vec(10,6), Vec(9,5), Vec(6,8) }			// Segment 10 - bottom right
	} };

	std::array<std::array<uint16_t, 2>, 37> charactersSegments{ {
		{0b11111110000, 0b00001111111},	// 0
		{0b10001111100, 0b10001001000},	// 1
		{0b11001001111, 0b11110010011},	// 2
		{0b11101001111, 0b11111001110},	// 3
		{0b11111011001, 0b10001001000},	// 4
		{0b00110011111, 0b11111001100},	// 5
		{0b00010011111, 0b11110001111},	// 6
		{0b10001001111, 0b10001001000},	// 7
		{0b01101011111, 0b11111010110},	// 8
		{0b11111011111, 0b10001001000},	// 9
		{0b10011110000, 0b10011011111},	// A 
		{0b01111010011, 0b00111010111},	// B
		{0b00010011111, 0b11110010001},	// C
		{0b11011110011, 0b00111111101},	// D
		{0b01110011111, 0b11110010111},	// E
		{0b00010011111, 0b00010011111},	// F
		{0b00010011111, 0b11111011101},	// G
		{0b11111011001, 0b10011011111},	// H
		{0b01100101111, 0b11110100110},	// I
		{0b01100101111, 0b00110100110},	// J
		{0b00111111101, 0b11011110011},	// K
		{0b00010010001, 0b11110010001},	// L
		{0b10011111111, 0b10011011001},	// M
		{0b11110000000, 0b10011011001},	// N
		{0b10011011111, 0b11111011001},	// O
		{0b11111011111, 0b00010010001},	// P
		{0b10011110000, 0b11001111001},	// Q
		{0b11111011111, 0b11011110011},	// R
		{0b00100011111, 0b11111000100},	// S
		{0b01100101111, 0b01100100110},	// T
		{0b10010000000, 0b11111011001},	// U
		{0b10011011001, 0b00001111001},	// V
		{0b10011011001, 0b11111111001},	// W
		{0b01101011001, 0b10011010110},	// X
		{0b01101011001, 0b01100100110},	// Y
		{0b01001001111, 0b11110010010},	// Z
		{0, 0},							// Space
	} };
	int spaceChar = 36;

	void draw(NVGcontext* vg) override {
		// Background
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, mm2px(matrixSize), mm2px(matrixSize));
		nvgFillColor(vg, nvgRGB(0, 0, 0));
		nvgFill(vg);
		nvgClosePath(vg);

		if (module) {
			for (int row = 0; row < matrixRows; row++) {
				// Work out what needs to be drawn on each row
				if (module->menuState) {
					//drawText(vg, row, segementSize, { 9+18, spaceChar, 9+21, spaceChar, 9+12, spaceChar, 9+5, spaceChar });
					if (row == 0) { drawBlankRow(vg, row, segementSize); }
				}
				else if (module->displayRule && row < 4) {
					// Display rule number
					if (row == 0) { drawBlankRow(vg, row, segementSize); }
					else if (row == 1) {
						uint8_t displayRule = module->Ca.getRule();
						std::array<int, 8> ruleDigits{ 36, 36, 36, 36, 36, 36, 36, 36 };
						if (displayRule >= 100) {
							ruleDigits[2] = (displayRule / 100) % 10;	// Hundreds
						}
						if (displayRule >= 10) {
							ruleDigits[4] = (displayRule / 10) % 10;	// Tens
						}
						ruleDigits[6] = displayRule % 10;				// Ones
						drawText(vg, row, segementSize, ruleDigits);
					}
					else if (row == 3) {
						drawBlankRow(vg, row, segementSize);
					}
				}
				else {
					int rowOffset = (row - 7) * -1;

					// Preview calls draw before Module init
					uint8_t displayRow = module->Ca.getRow(rowOffset);;

					for (int col = 0; col < matrixCols; col++) {
						uint16_t state = 0;
						if ((displayRow >> (7 - col)) & 1) {
							state = 0b00111111100; //0b11111111111, 0b00111111100, 0b11001110011
						}
						drawCell(vg, col, row, segementSize, state);
					}
				}
			}
		}

		//if (module && module->menuState) {

		//	// this is a bad way to do it
		//	std::array<int, 8> space{ 36, 36, 36, 36, 36, 36, 36, 36 };
		//	//std::array<int, 8> wolf{ 36, 9 + 23, 36, 9 + 15, 36, 9 + 12, 36, 9 + 6 };
		//	//std::array<int, 8> ram{ 9 + 18, 36, 9 + 1, 36 ,9 + 13 , 36, 36, 36};

		//	std::array<int, 8> wolf{ 36, 0, 36, 1, 36, 2, 36, 3 };
		//	std::array<int, 8> ram{ 4, 36, 5, 36, 6, 36, 7, 36 };

		//	drawText(vg, 0, segementSize, space);
		//	drawText(vg, 2, segementSize, wolf);
		//	drawText(vg, 4, segementSize, ram);
		//	drawText(vg, 6, segementSize, space);
		//}
		//else {
		//	if (module->displaySelectState) {

		//	}
		//	else
		//	{
		//		// Display 
		//		for (int row = 0; row < matrixRows; row++) {

		//			int rowOffset = (row - 7) * -1;

		//			// Preview calls draw before Module init
		//			uint8_t displayRow = 0;
		//			if (module) {
		//				displayRow = module->Ca.getRowX(rowOffset);
		//			}

		//			for (int col = 0; col < matrixCols; col++) {
		//				uint16_t state = 0;
		//				if ((displayRow >> (7 - col)) & 1) {
		//					state = 0b00111111100; //0b11111111111, 0b00111111100, 0b11001110011
		//				}

		//				drawCell(vg, col, row, segementSize, state);
		//			}
		//		}
		//	}
		//}
	}

	void drawCell(NVGcontext* vg, float col, float row, float size, uint16_t cellState) {
		float grid = size * 0.1;
		float x = col * size;
		float y = row * size;

		for (int segmentNum = 0; segmentNum < 11; segmentNum++) {
			// Get the points to draw for each segment
			const auto& drawingPoints = segmentShapes[segmentNum];

			nvgBeginPath(vg);
			nvgMoveTo(vg, x + drawingPoints[0].x * grid, y + drawingPoints[0].y * grid);
			for (size_t point = 1; point < drawingPoints.size(); point++) {
				nvgLineTo(vg, x + drawingPoints[point].x * grid, y + drawingPoints[point].y * grid);
			}
			nvgClosePath(vg);

			bool on = (cellState >> segmentNum) & 1;
			nvgFillColor(vg, on ? Colours::primaryAccent : Colours::primaryAccentOff);
			nvgFill(vg);
		}
	}

	void drawBlankRow(NVGcontext* vg, int row, float size) {
		for (size_t col = 0; col < 8; col++) {
			drawCell(vg, col, row, size, 0);
		}
	}

	void drawText(NVGcontext* vg, int row, float size, const std::array<int, 8>& displayCharacters) {
		for (size_t col = 0; col < 8; col++) {
			auto segmentPair = charactersSegments[displayCharacters[col]];

			drawCell(vg, col, row, size, segmentPair[0]);
			drawCell(vg, col, row + 1, size, segmentPair[1]);
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
		// Params
		// Buttons
		//addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(7.4f, 35.03f)), module, WolframModule::MENU_PARAM, WolframModule::MENU_LIGHT));
		//addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(7.4f, 35.03f)), module, WolframModule::MODE_PARAM, WolframModule::MENU_LIGHT));

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
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4f, 111.5f)), module, WolframModule::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35.56f, 111.5f)), module, WolframModule::INJECT_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 96.5f)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 111.5f)), module, WolframModule::X_PULSE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 96.5f)), module, WolframModule::Y_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34f, 111.5f)), module, WolframModule::Y_PULSE_OUTPUT));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));

		//MatrixDisplayBuffer* matrixDisplayFb = new MatrixDisplayBuffer(module);
		//MatrixDisplay* matrixDisplay = new MatrixDisplay(module);
		//matrixDisplayFb->addChild(matrixDisplay);
		//addChild(matrixDisplayFb);

		MatrixDisplayBuffer* matrixDisplayFb = new MatrixDisplayBuffer(module);
		MatrixDisplay2* matrixDisplay = new MatrixDisplay2(module);
		matrixDisplayFb->addChild(matrixDisplay);
		addChild(matrixDisplayFb);
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");
