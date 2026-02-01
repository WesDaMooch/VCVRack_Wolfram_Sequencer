#pragma once
#include "../../plugin.hpp"
#include <array>
#include <cstdint>

static constexpr int NUM_DISPLAY_STYLES = 6;
static constexpr int NUM_CELL_STYLES = 2;

struct UI {
	// VARIABLES
	float padding = 0;

	// Text
	float fontSize = 0;
	float textBgSize = 0;
	float textBgPadding = 0;
	std::array<Vec, 4> textPos{};
	std::array<Vec, 16> textBgPos{};

	// Cell
	float cellPadding = 0;
	std::array<Vec, 64> cellCirclePos{};
	std::array<Vec, 64> cellSquarePos{};
	std::array<Vec, 64> cellRoundedSquarePos{};

	// Cell styles
	int cellStyleIndex = 0;
	static constexpr float circleCellSize = 5.f;
	static constexpr float squareCellSize = 10.f;
	static constexpr float roundedSquareCellSize = 10.f;
	static constexpr float roundedSquareCellBevel = 1.f;

	// Display Style Colours
	int displayStyleIndex = 0;
	static const std::array<std::array<NVGcolor, 3>, NUM_DISPLAY_STYLES> displayStyle;

	// Wolfram specific
	float wolfSeedSize = 0;
	static constexpr float wolfSeedBevel = 3.f;
	std::array<Vec, 8> wolfSeedPos{};
	
	// FUNCTIONS
	void init(float newPadding, float newFontSize, float newCellPadding);

	// GETTERS
	const NVGcolor& getScreenColour() const;
	const NVGcolor& getForegroundColour() const;
	const NVGcolor& getBackgroundColour() const;

	// DRAW
	void getCellPath(NVGcontext* vg, int col, int row);

	void drawText(NVGcontext* vg, const char* text, int row);

	void drawMenuText(NVGcontext* vg, const char* l1,
		const char* l2, const char* l3, const char* l4);

	void drawTextBg(NVGcontext* vg, int row);

	void drawWolfSeedDisplay(NVGcontext* vg, int layer, uint8_t seed);
};