// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental

#include "plugin.hpp"
#include "WolframCA.hpp"

struct WolframModule : Module {
	enum ParamId {
		RULE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SINE_OUTPUT,
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
	uint8_t prevRule = rule;

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(RULE_PARAM, 0.f, 255.f, 30.f, "Rule");
		paramQuantities[RULE_PARAM]->snapEnabled = true;
		configInput(CLOCK_INPUT, "Clock");
		configOutput(SINE_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
		

		rule = params[RULE_PARAM].getValue();
		if (rule != prevRule) { Ca.setRule(rule); }
		prevRule = rule;

		int trig = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f);
		if (trig) {
			// Step
			Ca.generateRow();
		}

		float outputVoltage = (Ca.getRow() / 255.f) * 10.f;
		outputs[SINE_OUTPUT].setVoltage(outputVoltage);
	}
};


struct MatrixDisplay : Widget {
	// Draw LED matrix
	WolframModule* module;
;	
	void draw(const DrawArgs& args) override {
		for (int y = 0; y < 8; y++) {	
			int row = (module->Ca.readRow + 1 + y) % 8;

			for (int x = 0; x < 8; x++) {
				nvgBeginPath(args.vg);

				// Draw rects
				
				nvgCircle(args.vg, 10 + x * 15, 10 + y * 15, 5);

				// Preview crash errrr
				if (module && ((module->Ca.circularBuffer[row] >> (7 - x)) & 1)) {
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

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.f, 77.478)), module, WolframModule::RULE_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 77.478)), module, WolframModule::CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 108.713)), module, WolframModule::SINE_OUTPUT));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));

		MatrixDisplay* matrixDisplay = createWidget<MatrixDisplay>(mm2px(Vec(0.f, 20.f)));
		matrixDisplay->module = module;
		addChild(matrixDisplay);

	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");