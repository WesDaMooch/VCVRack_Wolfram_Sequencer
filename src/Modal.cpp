//export RACK_DIR=/home/wes-l/Rack-SDK

#include "plugin.hpp"
#include <array>
#include <vector>
#include "Modal\resonator.hpp"
#include "Modal\bessel.hpp"

// Ideas
// Multiple layers of modes

// Params
// 
// Resonator
// Pitch
// Shape
// Harmo

struct Harmoincs
{


	// Circular
	// https://www.soundonsound.com/techniques/synthesizing-percussion
	// Hard coded text
	std::array<float, MAX_MODES> circleHarmonicRation = {
		1.0,
		1.59,
		2.14,
		2.30,
		2.65,
		2.92,
		3.16,
		3.5,
		3.6,
		3.65,
		4.06,
		4.15
	};
	// Rectangle

	// get freq (clamp input?)

	// bessel
	std::array<float, MAX_MODES> circle2 = {};


	// string
	std::array<float, MAX_MODES> string = {};


	float getHarmonicRatio(int shape, int index) // float (0 - 1) harmonicModifier?
	{
		shape = rack::clamp(shape, 0, 1);
		index = rack::clamp(index, 0, MAX_MODES);

		switch (shape)
		{
		case 0:	// String
			// Mod = string pos = amplitude changes
			return index + 1;
			
		case 1: // Rectangle



			return 1;

		default:
			return index;
		}
		
		//index = rack::clamp(index, 0, MAX_MODES);
		//return circleHarmonicRation[index];
	}

};

struct Modal : Module
{
	enum ParamId
	{
		FREQ_PARAM,
		HARMO_PARAM,
		DECAY_PARAM,
		PARAMS_LEN
	};
	enum InputId
	{
		AUDIO_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		TEST_LIGHT,
		LIGHTS_LEN
	};

	// DSP
	int srate = 48000;
	std::vector<IIRResonator> resonator;

	Bessel bessel;

	Harmoincs harmonics;

	

	Modal() 
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Parameter
		configParam(FREQ_PARAM, 20.f, 1000.f, 440.f, "Freq", "Hz");
		configParam(HARMO_PARAM, 1e-6f, 3.f, 1.f, "Harmo");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.25f, "Decay");
		// Input
		configInput(AUDIO_INPUT, "Input");
		// Output
		configOutput(AUDIO_OUTPUT, "Output");

		onSampleRateChange();

		for (int i = 0; i < MAX_MODES; i++)
		{
			resonator.emplace_back();
		}
	}


	void onSampleRateChange() override 
	{
		srate = APP->engine->getSampleRate();
		bessel.setSamplerate(srate);
	}

	void onReset(const ResetEvent& e) override 
	{

	}
	
	json_t* dataToJson() override 
	{
		json_t* rootJ = json_object();

		return rootJ;
	}
	
	void dataFromJson(json_t* rootJ) override 
	{

	}
	
	void process(const ProcessArgs& args) override
	{
		// Parameters
		//float fundimentalFreq = params[FREQ_PARAM].getValue();

		//float harmoParam = params[HARMO_PARAM].getValue();

		float maxDecayTime = 10.f;
		float decayParam = params[DECAY_PARAM].getValue();
		float decay = decayParam * maxDecayTime;

		float input = inputs[AUDIO_INPUT].getVoltage();
		// Convert to digital audio range (-1 tp +1)
		input *= 0.1;
		input = rack::clamp(input, -1.f, 1.f);

		bessel.setPitch(220.f);
		//bessel.setSize(1.f);
		//bessel.setPosition(0.3f);
		//bessel.setDamping(0.5f);
		//bessel.setOvertones(0.5f);
		bessel.update();

		float output = 0.0;
		for (int i = 0; i < MAX_MODES; i++)
		{
			//float amplitude = 1.f;
			float amplitude = bessel.getWeight(i);
			float freq = bessel.getFreq(i);

			resonator[i].set(srate, freq, decay, amplitude);
			output += resonator[i].proccess(input);
		}

		output = output / (float)MAX_MODES;

		// Convert to voltage range (-10 to +10)
		output *= 10.f;

		outputs[AUDIO_OUTPUT].setVoltage(output);
		//outputs[AUDIO_OUTPUT].setVoltage(bessel.getFreq(0));
	}
		
};

struct ModalModuleWidget : ModuleWidget 
{

	ModalModuleWidget(Modal* module) 
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/modal.svg")));

		// Srews
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// Parameters
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.f, 20.f)), module, Modal::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.f, 40.f)), module, Modal::HARMO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.f, 60.f)), module, Modal::DECAY_PARAM));
		// Inputs
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(10.f, 22.14f)), module, Modal::AUDIO_INPUT));
		// Ouputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.f, 99.852f)), module, Modal::AUDIO_OUTPUT));
	}
	
	void appendContextMenu(Menu* menu) override
	{
		//Modal* module = dynamic_cast<Modal*>(this->module);

		menu->addChild(new MenuSeparator);
	}

};

Model* modelModal = createModel<Modal, ModalModuleWidget>("Modal");