// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg


// Use rack pulseGenerator for pulse outs...

#include "plugin.hpp"
#include "WolframCA.hpp"

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
		}

		float XoutputVoltage = (Ca.getVoltageX() / 255.f) * 10.f; // div is slow
		outputs[X_CV_OUTPUT].setVoltage(XoutputVoltage);

		//outputs[DEBUG_OUTPUT].setVoltage(params[CHANCE_PARAM].getValue());
	}
};


struct MatrixDisplay : Widget {

	// Display ideas
	// Can get all number digits in 3x5 square matrix


	MatrixDisplay() {
		setSize(mm2px(Vec(matrixSize, matrixSize)));
	}

	WolframModule* module;	

	int matrixCols = 8;
	int matrixRows = 8;

	// Working in mm
	float matrixSize = 31.7;
	float ledSize = 5;
	float ledRectSize = mm2px(matrixSize / matrixCols);
	float ledPaddingPx = mm2px(matrixSize / matrixCols);

	void draw(const DrawArgs& args) override {

		// Background
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, mm2px(matrixSize), mm2px(matrixSize));
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);
		nvgClosePath(args.vg);

		for (int y = 0; y < matrixCols; y++) {

			int rowOffset = (y - 7) * -1;
			
			// Preview calls draw before Module init
			// Needs something to draw
			uint8_t displayRow = 0;
			if (module) {
				displayRow = module->Ca.getDisplayRow(rowOffset);
			}

			for (int x = 0; x < matrixRows; x++) {
				nvgBeginPath(args.vg);

				// circle(args, x, y, size)
				// rect(args, x, y, w, h)
				
				// Cirlce LEDs
				//nvgCircle(args.vg, (ledPaddingPx / 2) + x * ledPaddingPx, (ledPaddingPx / 2) + y * ledPaddingPx, ledSize);
				// Rect LEDs
				//nvgRect(args.vg, x * ledPaddingPx, y * ledPaddingPx, ledRectSize, ledRectSize);
				// Robus LEDs
				drawRhombus(args.vg, x * ledPaddingPx, y * ledPaddingPx, ledRectSize, displayRow);

				if ((displayRow >> (7 - x)) & 1) {
					nvgFillColor(args.vg, nvgRGB(255, 0, 0));
				}
				else {
					nvgFillColor(args.vg, nvgRGB(0, 0, 0));
				}
				nvgFill(args.vg);
				nvgClosePath(args.vg);
			}
		}

		
	}

	void drawRhombus(NVGcontext* vg, float x, float y, float size, uint8_t row) {
		// Draw a rhombus
		//   __
		//	/ /
		//	--

		nvgBeginPath(vg);

		nvgMoveTo(vg, x, y + size);					// Bottom right
		nvgLineTo(vg, x + size * 0.5, y + size);	// Bottom middle
		nvgLineTo(vg, x + size, y + size * 0.5);	// Right middle
		nvgLineTo(vg, x + size, y);					// Right top
		nvgLineTo(vg, x + size * 0.5, y);			// Top Middle
		nvgLineTo(vg, x, y + size * 0.5);			// Left Middle
		nvgLineTo(vg, x, y + size);					// Bottom right

		//if ((row >> (7 - x)) & 1) {
		//	nvgFillColor(vg, nvgRGB(255, 0, 0));
		//}
		//else {
		//	nvgFillColor(vg, nvgRGB(0, 0, 0));
		//}
		//nvgFill(vg);
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

		//MatrixDisplay* matrixDisplay = createWidgetCentered<MatrixDisplay>(mm2px(Vec(30.1, 26.14)));
		MatrixDisplay* matrixDisplay = createWidgetCentered<MatrixDisplay>(Vec(box.size.x / 2, mm2px(26.14)));
		matrixDisplay->module = module;
		addChild(matrixDisplay);

	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");

