#pragma once
#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelWolframModule;

// Knobs

struct M1900hBlackKnob : RoundKnob {
	widget::SvgWidget* fg;

	M1900hBlackKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/M1900hBlackKnob.svg")));

		fg = new widget::SvgWidget;
		fb->addChildAbove(fg, tw);
		fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/M1900hKnob_fg.svg")));
	}
};

struct M1900hBlackEncoder : RoundKnob {
	widget::SvgWidget* fg;

	M1900hBlackEncoder() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/M1900hBlackEncoder.svg")));

		fg = new widget::SvgWidget;
		fb->addChildAbove(fg, tw);
		fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/M1900hKnob_fg.svg")));
	}
};

// Jacks
struct BananutBlack : app::SvgPort {
	// Befaco style
	BananutBlack() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
	}
};
