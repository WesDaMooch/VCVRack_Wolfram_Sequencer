// https://vcvrack.com/docs-v2/
// Make with: export RACK_DIR=/home/wes-l/Rack-SDK
#include "plugin.hpp"


struct WolframModule : Module {
	enum ParamId {
		PITCH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLK_INPUT,
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

	float clockFreq = 1.f;
	float blinkPhase = 0.f;
	dsp::Timer clockTimer;
	dsp::SchmittTrigger clockTrigger;

	WolframModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configInput(CLK_INPUT, "Clock");
		configOutput(SINE_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
		// Look how SEQ3 does it
		// Clock
		if (inputs[CLK_INPUT].isConnected()) {
			clockTimer.process(args.sampleTime);

			if (clockTrigger.process(inputs[CLK_INPUT].getVoltage(), 0.1f, 2.f)) {
				float clockFreq = 1.f / clockTimer.getTime();
				clockTimer.reset();
				if (0.001f <= clockFreq && clockFreq <= 1000.f) {
					this->clockFreq = clockFreq;
				}
			}
		}
		else {
			// Default frequency when clock is unpatched
			clockFreq = 2.f;
		}

		// Blink light at Clock Hz
		// Accumulate the phase
		blinkPhase += clockFreq * args.sampleTime;
		if (blinkPhase >= 1.f)
			blinkPhase -= 1.f;
		lights[BLINK_LIGHT].setBrightness(blinkPhase < 0.5f ? 1.f : 0.f);
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

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 46.063)), module, WolframModule::PITCH_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 77.478)), module, WolframModule::CLK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 108.713)), module, WolframModule::SINE_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, WolframModule::BLINK_LIGHT));
	}
};


Model* modelWolframModule = createModel<WolframModule, WolframModuleWidget>("WolframModule");