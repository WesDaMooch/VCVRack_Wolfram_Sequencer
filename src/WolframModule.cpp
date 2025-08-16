// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental
// NanoVG: https://github.com/memononen/nanovg

#include "plugin.hpp"
#include "WolframCA.hpp"

struct WolframModule : Module {
	enum ParamId {
		RULE_PARAM,
		CHANCE_PARAM,
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
		configParam(CHANCE_PARAM, 0.f, 100.f, 100.f, "Chance");
		// Inputs
		configInput(CLOCK_INPUT, "Clock");
		// Outputs
		configOutput(X_CV_OUTPUT, "X CV");
	}

	void process(const ProcessArgs& args) override {
		rule = params[RULE_PARAM].getValue();
		if (rule != prevRule) { Ca.setRule(rule); }
		prevRule = rule;

		int trig = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f);
		if (trig) {
			Ca.step(params[CHANCE_PARAM].getValue());
		}

		float XoutputVoltage = (Ca.getRow(0) / 255.f) * 10.f; // div is slow
		outputs[X_CV_OUTPUT].setVoltage(XoutputVoltage);

		outputs[DEBUG_OUTPUT].setVoltage(params[CHANCE_PARAM].getValue());
	}
};


struct MatrixDisplay : Widget {
	// Draw LED matrix
	WolframModule* module;
;	
	void draw(const DrawArgs& args) override {

		// Draw LED matrix
		// Pos middle 30.1 x 26.14
		int matrixCols = 8; 
		int matrixRows = 8;

		for (int y = 0; y < matrixCols; y++) {

			// Preview calls draw before Module init
			// Needs something to draw
			//int row = 0;
			uint8_t rowBits = 0;
			if (module) {
				//rowBits = module->Ca.outputBuffer[y];
				rowBits = module->Ca.getRow(y);
				//rowBits = module->Ca.circularBuffer[y];
			}

			for (int x = 0; x < matrixRows; x++) {
				nvgBeginPath(args.vg);

				// Draw rects
				// circle(args, x, y, size)
				// rect(args, x, y, w, h)
				
				nvgCircle(args.vg, 10 + x * 15, 10 + y * 15, 5);
				//nvgRect(args.vg, 10 + x * 15, 10 + y * 15, 10, 10);

				// Prevent preview crash 
				//if (module && ((module->Ca.displayBuffer[row] >> (7 - x)) & 1)) {
				//	nvgFillColor(args.vg, nvgRGB(255, 0, 0));
				//}
				//else {
				//	nvgFillColor(args.vg, nvgRGB(0, 0, 0));
				//}
				//nvgFill(args.vg);

				if ((rowBits >> (7 - x)) & 1) {
					nvgFillColor(args.vg, nvgRGB(255, 0, 0));
				}
				else {
					nvgFillColor(args.vg, nvgRGB(0, 0, 0));
				}
				nvgFill(args.vg);
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
		// Params
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(52.8, 18.5)), module, WolframModule::RULE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(45.72, 60)), module, WolframModule::CHANCE_PARAM));
		// Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4, 111.5)), module, WolframModule::CLOCK_INPUT));
		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 96.5)), module, WolframModule::X_CV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53.34, 96.5)), module, WolframModule::DEBUG_OUTPUT));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));

		MatrixDisplay* matrixDisplay = createWidget<MatrixDisplay>(mm2px(Vec(0.f, 20.f)));
		matrixDisplay->module = module;
		addChild(matrixDisplay);

	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");