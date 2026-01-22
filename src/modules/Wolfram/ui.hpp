#pragma once
#include "../../plugin.hpp"
#include <array>
#include <string>

struct UI {
	// Certain grahic elements are drawn on layer 1 (the foreground),
	// these elements, such as text & alive cells. Background items, 
	// such as dead cells, are drawn on layer 0

	void init();

	// GETTERS
	NVGcolor& getScreenColour();
	NVGcolor& getForegroundColour();
	NVGcolor& getBackgroundColour();

	// DRAW
	void getCellPath(NVGcontext* vg, int col, int row);
	void drawText(NVGcontext* vg, const std::string& string, int row);
	void drawMenuText(NVGcontext* vg, std::string l1,
		std::string l2, std::string l3, std::string l4);
	void drawTextBg(NVGcontext* vg, int row);
	void drawWolfSeedDisplay(NVGcontext* vg, int layer, uint8_t seed);

	// VARIABLES
	float padding = 0;

	// Text
	float fontSize = 0;
	float textBgSize = 0;
	float textBgPadding = 0;
	std::array<Vec, 4> textPos{};
	std::array<Vec, 16> textBgPos{};

	// Cell
	float cellSpacing = 0;
	std::array<Vec, 64> cellCirclePos{};
	std::array<Vec, 64> cellSquarePos{};

	// Cell styles
	int cellStyleIndex = 0;
	float circleCellSize = 5.f;
	float squareCellSize = 10.f;
	float squareCellBevel = 1.f;

	// Display Style Colours
	static constexpr int NUM_DISPLAY_STYLES = 7;
	int displayStyleIndex = 0;
	std::array<std::array<NVGcolor, 3>, NUM_DISPLAY_STYLES> displayStyle{ {
		{ nvgRGB(58, 16, 19),	nvgRGB(228, 7, 7),		nvgRGB(78, 12, 9) },		// Redrick
		{ nvgRGB(37, 59, 99),	nvgRGB(205, 254, 254),	nvgRGB(39, 70, 153) },		// Oled
		{ SCHEME_DARK_GRAY,		SCHEME_YELLOW,			SCHEME_DARK_GRAY },			// Rack  
		{ nvgRGB(4, 3, 8),		nvgRGB(244, 84, 22),	nvgRGB(26, 7, 0) },			// Eva MORE red
		{ nvgRGB(17, 3, 20),	nvgRGB(177, 72, 198),	nvgRGB(38, 13, 43) },		// Purple
		{ nvgRGB(42, 47, 37),	nvgRGB(210, 255, 0),	nvgRGB(42, 47, 37) },		// Lamp 
		{ nvgRGB(0, 0, 0),		nvgRGB(255, 255, 255),	nvgRGB(0, 0, 0) },			// Mono
	} };

	// Wolfram specific
	float wolfSeedSize = 0;
	float wolfSeedBevel = 3.f;
	std::array<Vec, 8> wolfSeedPos{};
};