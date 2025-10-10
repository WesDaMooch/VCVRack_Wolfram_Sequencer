#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelWolframModule;

// Add custom knobs
struct MoochDavies1900hBlackKnob : Davies1900hKnob {
	MoochDavies1900hBlackKnob() {
		//setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Davies1900hBlack.svg")));
		//bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Davies1900hDarkGrey_bg.svg")));
	}
};

struct MoochDavies1900hBlackEncoder : Davies1900hKnob {
	MoochDavies1900hBlackEncoder() {
		//setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Davies1900hBlack.svg")));
		//bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Davies1900hDarkGrey_bg.svg")));
	}
};

struct BananutBlack : app::SvgPort {
	// Befaco style port
	BananutBlack() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
	}
};