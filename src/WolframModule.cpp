// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
//
// Docs: https://vcvrack.com/docs-v2/
// Fundimentals: https://github.com/VCVRack/Fundamental

#include "plugin.hpp"
#include "WolframCA.hpp"

struct WolframModule : Module {
	enum ParamId {
		PITCH_PARAM,
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

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configInput(CLOCK_INPUT, "Clock");
		configOutput(SINE_OUTPUT, "");
	}

	 int leds[64] = {};

	void process(const ProcessArgs& args) override {
		
		int trig = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f);

		if (trig) {
			// Step
			Ca.generateRow();

			// Debug
			outputs[SINE_OUTPUT].setVoltage(1.f);
		}
		else
		{
			outputs[SINE_OUTPUT].setVoltage(0.f);
		}

		// Display
		// Want to talk to Ca
		for (int i = 0; i < 64; i++) {
			leds[i] = Ca.circularBuffer[i];
		}
	}
};


struct MatrixDisplay : Widget {
	// Draw one led
	WolframModule* module;
;	
	void draw(const DrawArgs& args) override {
		int i = 0;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, 10 + x * 15, 10 + y * 15, 5);

				if (module && module->leds[i]) {
					nvgFillColor(args.vg, nvgRGB(255, 0, 0));
				}
				else {
					nvgFillColor(args.vg, nvgRGB(0, 0, 0));
				}

				nvgFill(args.vg);
				i += 1;
			}
		}

		//for (int x = 0; x < 8; x++) {
		//	nvgBeginPath(args.vg);
		//	nvgCircle(args.vg, 10 + x * 15, 10, 5);
		//	// Safe in module preview
		//	if (module && module->leds[x]) {
		//		nvgFillColor(args.vg, nvgRGB(255, 0, 0));
		//	}
		//	else {
		//		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		//	}
		//	nvgFill(args.vg);
		//}
	}
};


struct WolframModuleWidget : ModuleWidget {
	WolframModuleWidget(WolframModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/WolframModule.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 46.063)), module, WolframModule::PITCH_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 77.478)), module, WolframModule::CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 108.713)), module, WolframModule::SINE_OUTPUT));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));

		MatrixDisplay* matrixDisplay = createWidget<MatrixDisplay>(mm2px(Vec(0.f, 20.f)));
		matrixDisplay->module = module;
		addChild(matrixDisplay);
	}
};

Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");