// ui.cpp
// Part of the Modular Mooch Wolfram module (VCV Rack)
//
// GitHub: https://github.com/WesDaMooch/Modular-Mooch-VCV
// 
// Copyright (c) 2026 Wesley Lawrence Leggo-Morrell
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui.hpp"

// More display styles:
// Purple: (17, 3, 20), (177, 72, 198), (38, 13, 43)
// Eva: (4, 3, 8), (244, 84, 22), (26, 7, 0) 
// Windows: (200, 200, 200), (0, 1, 220), (200, 200, 20)
const std::array<std::array<NVGcolor, 3>, NUM_DISPLAY_STYLES> UI::displayStyle{ {
	{ nvgRGB(228, 7, 7),		nvgRGB(78, 12, 9),		nvgRGB(58, 16, 19) },		// Redrick
	{ nvgRGB(205, 254, 254),	nvgRGB(39, 70, 153),	nvgRGB(37, 59, 99) },		// Oled
	{ SCHEME_YELLOW,			SCHEME_DARK_GRAY,		SCHEME_DARK_GRAY },			// Rack  
	{ nvgRGB(210, 255, 0),		nvgRGB(42, 47, 37),		nvgRGB(42, 47, 37) },		// Lamp 
	{ nvgRGB(255, 255, 255),	nvgRGB(0, 0, 0),		nvgRGB(0, 0, 0) },			// Mono
} };

void UI::init(float newPadding, float newFontSize, float newCellPadding) {
	padding = newPadding;
	fontSize = newFontSize;
	cellPadding = newCellPadding;

	textBgSize = fontSize - 2.f;
	textBgPadding = (fontSize * 0.5f) - (textBgSize * 0.5f) + padding;

	// Text positions. 
	for (int i = 0; i < 4; i++) {
		textPos[i].x = padding;
		textPos[i].y = padding + (fontSize * i);
	}

	// Text background positions. 
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			int i = r * 4 + c;
			textBgPos[i].x = (fontSize * c) + textBgPadding;
			textBgPos[i].y = (fontSize * r) + textBgPadding;
		}
	}

	// Cell positions
	for (int c = 0; c < 8; c++) {
		for (int r = 0; r < 8; r++) {
			int i = r * 8 + c;
			float circleCellPadding = (cellPadding * 0.5f) + padding;
			cellCirclePos[i].x = (cellPadding * c) + circleCellPadding;
			cellCirclePos[i].y = (cellPadding * r) + circleCellPadding;

			cellSquarePos[i].x = (cellPadding * c) + padding;
			cellSquarePos[i].y = (cellPadding * r) + padding;

			float roundedSquareCellPadding = (cellPadding * 0.5f) - (roundedSquareCellSize * 0.5f) + padding;
			cellRoundedSquarePos[i].x = (cellPadding * c) + roundedSquareCellPadding;
			cellRoundedSquarePos[i].y = (cellPadding * r) + roundedSquareCellPadding;
			
		}
	}

	// Wolf seed display
	float halfFontSize = fontSize * 0.5f;
	wolfSeedSize = halfFontSize - 2.f;
	for (int c = 0; c < 8; c++) {
		float wolfSeedPadding = (halfFontSize * 0.5f) - (wolfSeedSize * 0.5f) + padding;
		wolfSeedPos[c].x = (halfFontSize * c) + wolfSeedPadding;
		wolfSeedPos[c].y = (halfFontSize * 4.f) + wolfSeedPadding;
	}
};

// Colour getters
const NVGcolor& UI::getForegroundColour() const {
	return displayStyle[displayStyleIndex][0];
}

const NVGcolor& UI::getBackgroundColour() const {
	return displayStyle[displayStyleIndex][1];
}

const NVGcolor& UI::getScreenColour() const {
	return displayStyle[displayStyleIndex][2];
}

// Drawers
void UI::getCellPath(NVGcontext* vg, int col, int row) {
	// Must call nvgBeginPath before and nvgFill after this function! 
	int i = row * 8 + col;

	if ((i < 0) || (i >= 64))
		return;

	if (cellStyleIndex == 1) {
		// Pixel - Rounded square
		nvgRoundedRect(vg, cellRoundedSquarePos[i].x, cellRoundedSquarePos[i].y,
			roundedSquareCellSize, roundedSquareCellSize, roundedSquareCellBevel);
	}
	else {
		// LED - Circle
		nvgCircle(vg, cellCirclePos[i].x, cellCirclePos[i].y, circleCellSize);
	}
}

void UI::drawText(NVGcontext* vg, const char* text, int row) {
	// Draw a four character row of text
	if ((row < 0) || (row >= 4))
		return;

	nvgBeginPath(vg);
	nvgFillColor(vg, getForegroundColour());
	nvgText(vg, textPos[row].x, textPos[row].y, text, nullptr);
}

void UI::drawMenuText(NVGcontext* vg, const char* l1,
	const char* l2, const char* l3, const char* l4) {
	// Helper for drawing four lines of menu text
    drawText(vg, l1, 0);
    drawText(vg, l2, 1);
    drawText(vg, l3, 2);
    drawText(vg, l4, 3);
}

void UI::drawTextBg(NVGcontext* vg, int row) {
	// Draw one row of four square text character backgrounds
	if ((row < 0) || (row >= 4))
		return;

	float textBgBevel = 3.f;

	nvgBeginPath(vg);
	nvgFillColor(vg, getBackgroundColour());
	for (int col = 0; col < 4; col++) {
		int i = row * 4 + col;
		nvgRoundedRect(vg, textBgPos[i].x, textBgPos[i].y,
			textBgSize, textBgSize, textBgBevel);
	}
	nvgFill(vg);
}

void UI::drawWolfSeedDisplay(NVGcontext* vg, int layer, uint8_t seed) {
	if (layer == 1) {
		// Lines
		NVGcolor colour = getForegroundColour();

		nvgStrokeColor(vg, colour);
		nvgBeginPath(vg);
		for (int col = 0; col < 8; col++) {
			if ((col >= 1) && (col <= 7)) {
				// TODO: move to init
				nvgMoveTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y - 1);
				nvgLineTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y + 1);

				nvgMoveTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) - 1);
				nvgLineTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) + 1);
			}
		}
		nvgStrokeWidth(vg, 0.5f);
		nvgStroke(vg);
		//TODO:is a nvgClosePath(args.vg) required here
	}

	nvgFillColor(vg, layer ? getForegroundColour() : getBackgroundColour());
	nvgBeginPath(vg);
	for (int col = 0; col < 8; col++) {
		bool cell = (seed >> (7 - col)) & 1;

		if ((layer && !cell) || (!layer && cell))
			continue;

		nvgRoundedRect(vg, wolfSeedPos[col].x, wolfSeedPos[col].y,
			wolfSeedSize, (wolfSeedSize * 2.f) + 2.f, wolfSeedBevel);
	}
	nvgFill(vg);
}