//export RACK_DIR=/home/wes-l/Rack-SDK

#include "plugin.hpp"

struct Modal : Module 
{
	enum ParamId 
	{
		TEST_PARAM,
		PARAMS_LEN
	};
	enum InputId 
	{
		TEST_INPUT,
		INPUTS_LEN
	};
	enum OutputId 
	{
		TEST_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId 
	{
		TEST_LIGHT,
		LIGHTS_LEN
	};

	// DSP
	int srate = 44100;	

	Modal() 
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		onSampleRateChange();
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

	}
		
};

struct ModalModuleWidget : ModuleWidget 
{

	ModalModuleWidget(Modal* module) 
	{
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/wolfram.svg")));

		// Srews
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
	
	void appendContextMenu(Menu* menu) override
	{
		//Modal* module = dynamic_cast<Modal*>(this->module);

		menu->addChild(new MenuSeparator);
	}

};

Model* modelModal = createModel<Modal, ModalModuleWidget>("Modal");