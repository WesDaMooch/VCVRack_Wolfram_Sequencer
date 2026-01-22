#include "ui.hpp"

void UI::init() {
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
			float a = (cellSpacing * 0.5f) + padding;
			cellCirclePos[i].x = (cellSpacing * c) + a;
			cellCirclePos[i].y = (cellSpacing * r) + a;

			float b = (cellSpacing * 0.5f) - (squareCellSize * 0.5f) + padding;
			cellSquarePos[i].x = (cellSpacing * c) + b;
			cellSquarePos[i].y = (cellSpacing * r) + b;
		}
	}

	// Wolf seed display
	float halfFs = fontSize * 0.5f;
	wolfSeedSize = halfFs - 2.f;
	for (int c = 0; c < 8; c++) {
		float a = (halfFs * 0.5f) - (wolfSeedSize * 0.5f) + padding;
		wolfSeedPos[c].x = (halfFs * c) + a;
		wolfSeedPos[c].y = (halfFs * 4.f) + a;
	}
};

// GETTERS
NVGcolor& UI::getScreenColour() { 
	return displayStyle[displayStyleIndex][0]; 
}

NVGcolor& UI::getForegroundColour() { 
	return displayStyle[displayStyleIndex][1];
}

NVGcolor& UI::getBackgroundColour() { 
	return displayStyle[displayStyleIndex][2];
}

// DRAWING	
void UI::getCellPath(NVGcontext* vg, int col, int row) {
	// Must call nvgBeginPath before and nvgFill after this function! 
	int i = row * 8 + col;

	if ((i < 0) || (i >= 64))
		return;

	if (cellStyleIndex == 1) {
		// Square
		nvgRoundedRect(vg, cellSquarePos[i].x, cellSquarePos[i].y,
			squareCellSize, squareCellSize, squareCellBevel);
	}
	else {
		// Circle
		nvgCircle(vg, cellCirclePos[i].x, cellCirclePos[i].y, circleCellSize);
	}
}

void UI::drawText(NVGcontext* vg, const std::string& string, int row) {
	// Draw a four character row of text
	if ((row < 0) || (row >= 4))
		return;

	std::string outputStr;

	if (string.size() >= 4)
		outputStr = string.substr(string.size() - 4);
	else
		outputStr = std::string(4 - string.size(), ' ') + string;

	nvgBeginPath(vg);
	nvgFillColor(vg, getForegroundColour());
	nvgText(vg, textPos[row].x, textPos[row].y, outputStr.c_str(), nullptr);
}

void UI::drawMenuText(NVGcontext* vg, std::string l1,
	std::string l2, std::string l3, std::string l4) {
	// Helper for drawing four lines of menu text
	std::array<std::string, 4> text = { l1, l2, l3, l4 };
	for (int i = 0; i < 4; i++)
		drawText(vg, text[i], i);
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
	// TODO: make layer int (layer == 1) and (layer == 0)

	if (layer) {
		// Lines
		NVGcolor colour = getForegroundColour();

		// Special case colour for Eva look
		if (displayStyleIndex == 3)
			colour = nvgRGB(115, 255, 166);

		nvgStrokeColor(vg, colour);
		nvgBeginPath(vg);
		for (int col = 0; col < 8; col++) {
			if ((col >= 1) && (col <= 7)) {
				nvgMoveTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y - 1);
				nvgLineTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y + 1);

				nvgMoveTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) - 1);
				nvgLineTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) + 1);
			}
		}
		nvgStrokeWidth(vg, 0.5f);
		nvgStroke(vg);
		//nvgClosePath(args.vg); ?
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