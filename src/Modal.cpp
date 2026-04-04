//export RACK_DIR=/home/wes-l/Rack-SDK

#include "plugin.hpp"
#include <array>
#include <vector>

#include <cmath>

static constexpr int NUM_MODES = 2;

// Second Order 'Biquad' Direct Form I Filter
class BiquadDF1
{
protected:
	float b0 = 0.0;
	float a1 = 0.0, a2 = 0.0;
	
	float y1 = 0.0, y2 = 0.0;

public:

	void set(float fs, float frequency, float T60, float amplitude)
	{
		float omega = 2.0 * M_PI * frequency / fs;

		float decay = std::exp(-6.91 / (T60 * fs));

		a1 = -2.0 * decay * std::cos(omega);
		a2 = decay * decay;

		b0 = amplitude * (1.0 - decay);
	}

	void reset()
	{
		y1 = 0.0;
		y2 = 0.0;
	}

	float proccess(float x)
	{
		double y = b0 * x - a1 * y1 - a2 * y2;

		y2 = y1;
		y1 = y;

		return y;
	}
};

struct Modal : Module
{
	enum ParamId
	{
		TEST_PARAM,
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
	
	//dsp::RCFilter mode;
	//std::array<Filter, NUM_MODES> mode = {};

	std::vector<BiquadDF1> modes;

	Modal() 
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(AUDIO_INPUT, "Input");
		configOutput(AUDIO_OUTPUT, "Output");

		onSampleRateChange();

		BiquadDF1 m1;
		m1.set(srate, 440.f, 1.f, 1.f);
		modes.push_back(m1);

		BiquadDF1 m2;
		m2.set(srate, 444.f, 3.f, 1.f);
		modes.push_back(m2);
	}


	void onSampleRateChange() override 
	{
		srate = APP->engine->getSampleRate();
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
		float input = inputs[AUDIO_INPUT].getVoltage();

		// Convert to digital audio range (-1 tp +1)
		input *= 0.1;
		input = rack::clamp(input, -1.f, 1.f);

		float output = 0.0;
		for (int i = 0; i < NUM_MODES; i++)
		{
			output += modes[i].proccess(input);
		}

		output = output / (float)NUM_MODES;

		// Convert to voltage range (-10 to +10)
		output *= 10.f;

		outputs[AUDIO_OUTPUT].setVoltage(output);
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
		// Inputs
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(7.62f, 22.14f)), module, Modal::AUDIO_INPUT));
		// Ouputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62f, 99.852f)), module, Modal::AUDIO_OUTPUT));
	}
	
	void appendContextMenu(Menu* menu) override
	{
		//Modal* module = dynamic_cast<Modal*>(this->module);

		menu->addChild(new MenuSeparator);
	}

};

Model* modelModal = createModel<Modal, ModalModuleWidget>("Modal");