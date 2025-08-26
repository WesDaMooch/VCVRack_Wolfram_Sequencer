// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg


// Use rack pulseGenerator for pulse outs...

#include "plugin.hpp"
#include "WolframCA.hpp"

#include <vector>

struct WolframModule : Module {
	enum ParamId {
		RULE_PARAM,
		LENGTH_PARAM,
		CHANCE_PARAM,
		OFFSET_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X_CV_OUTPUT,
		DEBUG_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	CellularAutomata Ca;

	dsp::SchmittTrigger clockTrigger;

	//uint8_t rule = paramQuantities[RULE_PARAM]->getDefaultValue();
	// Want some paramChanged thing
	uint8_t rule = 30;
	uint8_t prevRule = rule; // dont like prev

	bool menuState = true;

	// Redraw matrix display 
	bool dirty = true;

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Params
		configParam(RULE_PARAM, 0.f, 255.f, 30.f, "Rule");
		paramQuantities[RULE_PARAM]->snapEnabled = true;
		configParam(LENGTH_PARAM, 2.f, 64.f, 8.f, "Length");
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		configParam(CHANCE_PARAM, 0.f, 100.f, 100.f, "Chance");
		configParam(OFFSET_PARAM, -4.f, 4.f, 0.f, "Offset");
		paramQuantities[OFFSET_PARAM]->snapEnabled = true;
		// Inputs
		configInput(CLOCK_INPUT, "Clock");
		// Outputs
		configOutput(X_CV_OUTPUT, "X CV");
	}

	void process(const ProcessArgs& args) override {
		Ca.setSequenceLength(params[LENGTH_PARAM].getValue());
		Ca.setChance(params[CHANCE_PARAM].getValue());

		rule = params[RULE_PARAM].getValue();
		if (rule != prevRule) { Ca.setRule(rule); }
		prevRule = rule;

		int trig = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f);
		if (trig) {
			Ca.step();

			// Redraw matrix display;
			if (!menuState) {
				dirty = true;
			}
		}

		float XoutputVoltage = (Ca.getVoltageX() / 255.f) * 10.f; // div is slow
		outputs[X_CV_OUTPUT].setVoltage(XoutputVoltage);

		//outputs[DEBUG_OUTPUT].setVoltage(params[CHANCE_PARAM].getValue());
	}
};


struct MatrixDisplayBuffer : FramebufferWidget {
	WolframModule* module;
	MatrixDisplayBuffer(WolframModule* m) {
		module = m;
	}
	void step() override {
		if (module->dirty) {
			FramebufferWidget::dirty = true;
			module->dirty = false;
		}
		FramebufferWidget::step();
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

	void draw(NVGcontext* vg) override {
		// Background
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, mm2px(matrixSize), mm2px(matrixSize));
		nvgFillColor(vg, nvgRGB(0, 0, 0));
		nvgFill(vg);
		nvgClosePath(vg);

		if (module->menuState) {

			// this is a bad way to do it
			std::array<int, 8> space{ 36, 36, 36, 36, 36, 36, 36, 36 };
			//std::array<int, 8> wolf{ 36, 9 + 23, 36, 9 + 15, 36, 9 + 12, 36, 9 + 6 };
			//std::array<int, 8> ram{ 9 + 18, 36, 9 + 1, 36 ,9 + 13 , 36, 36, 36};

			std::array<int, 8> wolf{ 36, 0, 36, 1, 36, 2, 36, 3 };
			std::array<int, 8> ram{ 4, 36, 5, 36, 6, 36, 7, 36 };

			drawText(vg, 0, segementSize, space);
			drawText(vg, 2, segementSize, wolf);
			drawText(vg, 4, segementSize, ram);
			drawText(vg, 6, segementSize, space);
		}
		else {
			for (int row = 0; row < matrixRows; row++) {

				int rowOffset = (row - 7) * -1;

				// Preview calls draw before Module init
				uint8_t displayRow = 0;
				if (module) {
					displayRow = module->Ca.getDisplayRow(rowOffset);
				}

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

	void drawCell(NVGcontext* vg, float col, float row, float size, uint16_t cellState) {
		float grid = size * 0.1;
		float x = col * size;
		float y = row * size;

		for (int segmentNum = 0; segmentNum < 11; segmentNum++) {
			// Get the points to draw for each segment
			const auto& drawingPoints = segmentShapes[segmentNum];

			nvgBeginPath(vg);
			nvgMoveTo(vg, x + drawingPoints[0].x * grid, y + drawingPoints[0].y * grid);
			for (size_t i = 1; i < drawingPoints.size(); i++) {
				nvgLineTo(vg, x + drawingPoints[i].x * grid, y + drawingPoints[i].y * grid);
			}
			nvgClosePath(vg);


			bool on = (cellState >> segmentNum) & 1;
			nvgFillColor(vg, on ? nvgRGB(255, 0, 0) : nvgRGB(40, 40, 40));
			nvgFill(vg);
		}
	}

	void drawText(NVGcontext* vg, int row, float size, const std::array<int, 8>& displayCharacters) {

		// Size of one cell - 11 segement display
		//float tenthGrid = size * 0.1;


		for (size_t col = 0; col < 8; col++) {
			//float x = col * size;
			//float yUpper = row * size;
			//float yLower = (row + 1) * size;

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
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/WolframV3Silkscreen.svg")));
		

		// Srews
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// Params
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(52.8, 18.5)), module, WolframModule::RULE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 60)), module, WolframModule::LENGTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(45.72, 60)), module, WolframModule::CHANCE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.48, 80)), module, WolframModule::OFFSET_PARAM));
		// Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4, 111.5)), module, WolframModule::CLOCK_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.5)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34, 96.5)), module, WolframModule::DEBUG_OUTPUT));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));

		////MatrixDisplay* matrixDisplay = createWidgetCentered<MatrixDisplay>(mm2px(Vec(30.1, 26.14)));
		//MatrixDisplay* matrixDisplay = createWidgetCentered<MatrixDisplay>(Vec(box.size.x * 0.5, mm2px(26.14)));
		////matrixDisplay->setSize(mm2px(Vec(32.f, 32.f)));	// Makes display vanish, why?
		//matrixDisplay->module = module;
		//addChild(matrixDisplay);



		MatrixDisplayBuffer* fb = new MatrixDisplayBuffer(module);
		MatrixDisplay* dw = new MatrixDisplay(module);
		//SomeDrawingWidget* dw = createWidgetCentered<SomeDrawingWidget>(Vec(box.size.x * 0.5, mm2px(26.14)));
		fb->addChild(dw);
		addChild(fb);
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");
