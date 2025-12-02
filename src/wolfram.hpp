#pragma once
#include "plugin.hpp"
#include <array>

class LookAndFeel {
public:
	// Certain grahic elements are drawn on layer 1 (the foreground),
	// these elements, such as text & alive cells, are not associated with a framebuffer,
	// therefore they are drawn immediately.
	// 
	// Background items, such as dead cells, are associated with a framebuffer,
	// and are drawn when required. Most forground element values are
	// calulated with background graphics (contolled by the framebuffer).
	// 
	// See Display & DisplayFramebuffer for details.

	// Setters
	void setDrawingContext(NVGcontext* context) { vg = context; }

	void setDrawingParams(float p, float fs, float cp) { 

		padding = p;
		cellSpacing = cp;
		
		fontSize = fs;
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

		// Cell positions.
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

	void setLook(int i) { lookIndex = i; }
	void setCellStyle(int i) { cellStyleIndex = i; }
	void setRedrawBg() { redrawBg = true; } // TODO: remove

	// Getters
	int getLookIndex() { return lookIndex; }
	int getFeelIndex() { return cellStyleIndex; }
	bool getRedrawBg() { return redrawBg; }

	NVGcolor* getScreenColour() { return &looks[lookIndex][0]; }
	NVGcolor* getForegroundColour() { return &looks[lookIndex][1]; }
	NVGcolor* getBackgroundColour() { return &looks[lookIndex][2]; }

	// Helpers
	std::string formatIntForText(int value) {
		char t[5];
		snprintf(t, sizeof(t), "%4.3d", value);
		return std::string(t);
	}

	// Drawing
	void drawCell(float col, float row, bool state) {

		//if(state)
			//nvgGlobalCompositeBlendFunc(vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

		// Draw cell.
		int i = row * 8 + col;
		nvgFillColor(vg, state ? *getForegroundColour() : *getBackgroundColour());

		//nvgBeginPath(vg);
		switch (cellStyleIndex) {
		case 1: {
			// Square.
			nvgRoundedRect(vg, cellSquarePos[i].x, cellSquarePos[i].y,
				squareCellSize, squareCellSize, squareCellBevel);
			break;
		}

		default:
			// Circle.
			nvgCircle(vg, cellCirclePos[i].x, cellCirclePos[i].y, circleCellSize);
			break;
		}
		//nvgFill(vg);

		/*
		// Draw halo
		// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser.
		//if (args.fb)
		//	return;

		if (!state)
			return;

		const float halo = settings::haloBrightness;
		if (halo == 0.f)
			return;
		
		float radius = circleCellSize / 2.f;
		float oradius = radius + std::min(radius * 4.f, 15.f);

		nvgBeginPath(vg);
		nvgRect(vg, cellCirclePos[i].x - oradius, 
			cellCirclePos[i].y - oradius, 2 * oradius, 2 * oradius);

		NVGcolor icol = color::mult(*getForegroundColour(), halo * 0.4f);
		NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
		NVGpaint paint = nvgRadialGradient(vg, cellCirclePos[i].x, 
			cellCirclePos[i].y, radius, oradius, icol, ocol);
		nvgFillPaint(vg, paint);
		nvgFill(vg);
		*/
	}

	void drawText(const std::string& str, int row) {
		// Draw four character row of text
		std::string outputStr;

		//cool
		//nvgFontBlur(vg, 5.f);

		// special case font colours
		// if (lookIndex && (row == 0)) ...

		if (str.size() >= 4) {
			outputStr = str.substr(str.size() - 4);
		}
		else {
			outputStr = std::string(4 - str.size(), ' ') + str;
		}

		nvgFillColor(vg, *getForegroundColour());
		nvgText(vg, textPos[row].x, textPos[row].y, outputStr.c_str(), nullptr);
	}

	void drawTextBg(int row) {
		// Draw one row of four square text character backgrounds
		float textBgBevel = 3.f;
		nvgFillColor(vg, *getBackgroundColour());

		nvgBeginPath(vg);
		for (int col = 0; col < 4; col++) {

			int i = row * 4 + col;
			nvgRoundedRect(vg, textBgPos[i].x, textBgPos[i].y,
				textBgSize, textBgSize, textBgBevel);
		}
		nvgFill(vg);


		//nvgStrokeWidth(vg, padding);
		//nvgStrokeColor(vg, *getForegroundColour());
		//nvgStroke(vg);
		//nvgClosePath(vg);
		

		redrawBg = false;
	}

	void drawWolfSeedDisplay(int row, bool layer, uint8_t seed) {
		
		nvgFillColor(vg, layer ? *getForegroundColour() : *getBackgroundColour());

		if (layer) {

			NVGcolor c = *getForegroundColour();
			if (lookIndex == 3) {
				// Special case colour for Eva 
				c = nvgRGB(115, 255, 166);
			}
			nvgStrokeColor(vg, c);

			nvgBeginPath(vg);
			for (int col = 0; col < 8; col++) {
				if ((col >= 1) && (col <= 7)) {
					// TODO: pre compute?
					nvgMoveTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y - 1);
					nvgLineTo(vg, wolfSeedPos[col].x - padding, wolfSeedPos[col].y + 1);
					
					nvgMoveTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) - 1);
					nvgLineTo(vg, wolfSeedPos[col].x - padding, (wolfSeedPos[col].y + (fontSize - textBgPadding)) + 1);
					
				}
			}
			nvgStrokeWidth(vg, 0.5f);
			nvgStroke(vg);
		}

		nvgBeginPath(vg);
		for (int col = 0; col < 8; col++) {
			bool cell = (seed >> (7 - col)) & 1;

			if ((layer && !cell) || (!layer && cell))
				continue;
			
			nvgRoundedRect(vg, wolfSeedPos[col].x, wolfSeedPos[col].y,
				wolfSeedSize, (wolfSeedSize * 2.f) + 2.f, wolfSeedBevel);
		}
		nvgFill(vg);

		if (!layer)
			redrawBg = false;
	}

protected:
	// Params
	NVGcontext* vg;
	float padding = 0;

	// Cell
	float cellSpacing = 0;
	std::array<Vec, 64> cellCirclePos{};
	std::array<Vec, 64> cellSquarePos{};

	// Text
	float fontSize = 0;
	float textBgSize = 0;
	float textBgPadding = 0;
	std::array<Vec, 4> textPos{};
	std::array<Vec, 16> textBgPos{};

	// Wolfram 
	float wolfSeedSize = 0;
	float wolfSeedBevel = 3.f;
	std::array<Vec, 8> wolfSeedPos{};

	// Looks
	static constexpr int NUM_LOOKS = 6;
	int lookIndex = 0;
	std::array<std::array<NVGcolor, 3>, NUM_LOOKS> looks { {

		{ nvgRGB(58, 16, 19), nvgRGB(228, 7, 7), nvgRGB(78, 12, 9) },		// Redrick
		{ nvgRGB(37, 59, 99), nvgRGB(205, 254, 254), nvgRGB(39, 70, 153) },	// Oled
		{ nvgRGB(18, 18, 18), SCHEME_YELLOW, nvgRGB(18, 18, 18) },			// Rack
		{ nvgRGB(4, 3, 8), nvgRGB(244, 84, 22), nvgRGB(26, 7, 0) },			// Eva 
		
		{ nvgRGB(0, 0, 0), nvgRGB(255, 255, 255), nvgRGB(0, 0, 0) },	// Purple
		// Green
		//{ nvgRGB(225, 225, 223), nvgRGB(5, 5, 3), nvgRGB(205, 205, 203) },	// White
		{ nvgRGB(0, 0, 0), nvgRGB(255, 255, 255), nvgRGB(0, 0, 0) },	// Mono
		// TODO: add purple
	} };

	// Cell styles
	int cellStyleIndex = 0;
	float circleCellSize = 5.f;
	float squareCellSize = 10.f;
	float squareCellBevel = 1.f;

	// Algoithm can set true to force a framebuffer redraw
	bool redrawBg = false;
};

class Algorithm {
public:
	Algorithm() {}
	~Algorithm() {}

	
	void advanceHeads(int s) {
		// Advance read and write heads
		// TODO: could use if, would be quicker?
		readHead = writeHead;
		writeHead = (writeHead + 1) % s;
	}

	uint8_t applyOffset(uint8_t r, int o) {
		// Apply a horizontal offset to a given row.
		int shift = clamp(o, -4, 4);
		if (shift < 0) {
			shift = -shift;
			r = ((r << shift) | (r >> (8 - shift))) & 0xFF;
		}
		else if (shift > 0) {
			r = ((r >> shift) | (r << (8 - shift))) & 0xFF;
		}
		return r;
	}

	virtual void step(int s) { advanceHeads(s); };
	virtual void update(int o) = 0;
	virtual void generate() = 0;
	virtual void pushSeed(bool w) = 0;
	virtual void inject(bool a, bool w) = 0;
	void tick() { displayMatrixUpdated = false; }

	// Setters
	void setLookAndFeel(LookAndFeel* l) { lookAndFeel = l; }

	void setDisplayMatrix(uint64_t newDisplay) {
		displayMatrix = newDisplay;
	}

	virtual void setBuffer(bool r) = 0;

	void setReadHead(int h) { readHead = h; }
	void setWriteHead(int h) { writeHead = h; }

	int updateSelect(int value, int MAX_VALUE,
		int defaultValue, int delta, bool reset) {
		// Helper for updating values via Select encoder
		if (reset)
			return defaultValue;

		return (value + delta + MAX_VALUE) % MAX_VALUE;
	}

	virtual void updateRule() = 0;

	virtual void setRuleCV(float cv) = 0;
	virtual void setRuleSelect(int d, bool r) = 0;
	virtual void setSeedSelect(int d, bool r) = 0;
	virtual void setModeSelect(int d, bool r) = 0;

	// Getters
	uint64_t getDisplayMatrix() { return displayMatrix; }
	virtual float getXVoltage() = 0;
	virtual float getYVoltage() = 0;
	virtual bool getXPulse() = 0;
	virtual bool getYPulse() = 0;
	virtual float getModeLEDValue() = 0;
	virtual std::string getAlgoStr() = 0;
	virtual std::string getRuleStr() = 0;
	virtual std::string getRuleSelectStr() = 0;
	virtual std::string getSeedStr() = 0;
	virtual std::string getModeStr() = 0;

	void getAlgoBg() {
		lookAndFeel->drawTextBg(2);
	}

	void getRuleBg() {
		lookAndFeel->drawTextBg(1);
	}

	virtual void getSeedBg() {
		lookAndFeel->drawTextBg(2);
	}

	void getModeBg() {
		lookAndFeel->drawTextBg(2);
	}

	//virtual void onReset() {}

protected:
	LookAndFeel* lookAndFeel;

	static constexpr size_t MAX_SEQUENCE_LENGTH = 64;
	// put in each algo..
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{};
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};

	uint64_t displayMatrix = 0;
	bool displayMatrixUpdated = false;

	int readHead = 0;
	int writeHead = 1;
};

class WolfAlgoithm : public Algorithm {
public:
	WolfAlgoithm() {
		rowBuffer[readHead] = seed;	// Init seed
	}

	void step(int s) override {
		advanceHeads(s);
		displayMatrix <<= 8;	// Shift matrix along (up).
	}

	void update(int o) override {
		displayMatrixUpdated = true;

		// Apply lastest offset
		int offsetDifference = o - prevOffset;
		uint64_t tempMatrix = 0;
		for (int i = 1; i < 8; i++) {
			uint8_t row = (displayMatrix >> (i * 8)) & 0xFF;
			tempMatrix |= uint64_t(applyOffset(row, offsetDifference)) << (i * 8);
		}
		displayMatrix = tempMatrix;
		prevOffset = o;

		// Push latest row
		displayMatrix &= ~0xFFULL;	
		displayMatrix |= static_cast<uint64_t>(applyOffset(rowBuffer[readHead], o));
	}

	void generate() override {
		// One Dimensional Cellular Automata.
		uint8_t readRow = rowBuffer[readHead];
		uint8_t writeRow = 0;
		
		// Clip.
		// TODO: this is different from life!
		uint8_t west = readRow >> 1;
		uint8_t east = readRow << 1;

		switch (modeIndex) {
		case 1: {
			// Wrap.
			west = (readRow >> 1) | (readRow << 7);
			east = (readRow << 1) | (readRow >> 7);
			break;
		}

		case 2: {
			// Random.
			west |= random::get<bool>() << 7;
			east |= random::get<bool>();
			break;
		}

		default:
			// Clip.
			break;
		}

		for (int col = 0; col < 8; col++) {
			uint8_t westBit = (west >> col) & 1;
			uint8_t currentBit = (readRow >> col) & 1;
			uint8_t eastBit = (east >> col) & 1;

			uint8_t tag = (westBit << 2) | (currentBit << 1) | eastBit;
			uint8_t newBit = (rule >> tag) & 1;
			
			writeRow |= newBit << col;
		}
		rowBuffer[writeHead] = writeRow;
	}

	void pushSeed(bool w) override {
		uint8_t resetRow = randSeed ? random::get<uint8_t>() : seed;
		size_t head = readHead;
		if (w) {
			head = writeHead;
		}
		rowBuffer[head] = resetRow;
	}

	void inject(bool a, bool w) override {
		int head = readHead;
		if (w)
			head = writeHead;
		
		uint8_t row = rowBuffer[head];

		// Check if row is already full or empty
		if ((a & (row == UINT8_MAX)) | (!a & (row == 0)))
			return;

		uint8_t targetMask = row;
		if (a)
			targetMask = ~row;	// Flip row

		int targetCount = __builtin_popcount(targetMask);	// Count target bits
		int target = random::get<uint8_t>() % targetCount;	// Random target index

		// Find corresponding bit position, TODO: this is hard to read
		uint8_t bitMask;
		for (bitMask = 1; target || !(targetMask & bitMask); bitMask <<= 1) {
			if (targetMask & bitMask)
				target--;
		}

		row = a ? (row | bitMask) : (row & ~bitMask);
		rowBuffer[head] = row;
	}

	void updateRule() override {
		rule = static_cast<uint8_t>(clamp(ruleSelect + ruleCV, 0, UINT8_MAX));
		lookAndFeel->setRedrawBg();
	}

	void setRuleCV(float cv) override {
		int newCV = std::round(cv * 256);

		if (newCV == ruleCV)
			return;

		ruleCV = newCV;
		updateRule();
	}

	void setRuleSelect(int d, bool r) override {
		int newSelect = 0;

		if (r) {
			newSelect = defaultRule;
			return;
		}
		newSelect = static_cast<uint8_t>(ruleSelect + d);

		if (newSelect == ruleSelect)
			return;

		ruleSelect = newSelect;
		updateRule();
	}

	void setSeedSelect(int d, bool r) override {

		if (r) {
			seed = defaultSeed;
			seedSelect = seed;
			randSeed = false;
			lookAndFeel->setRedrawBg();
			return;
		}

		// Seed options are 256 + 1 (RAND) 
		seedSelect += d;

		if (seedSelect > 256)
			seedSelect -= 257;
		else if (seedSelect < 0)
			seedSelect += 257;

		randSeed = (seedSelect == 256);

		if (!randSeed)
			seed = static_cast<uint8_t>(seedSelect);

		//lookAndFeel->setRedrawBg();
	}

	void setModeSelect(int d, bool r) override {
		modeIndex = updateSelect(modeIndex, NUM_MODES, defaultMode, d, r);
		lookAndFeel->setRedrawBg();
	}

	void setBuffer(bool r) override {

		if (r) {
			rowBuffer = {};
			return;
		}
	}

	float getXVoltage() override {
		// Returns bottom row of Ouput Matrix as 'voltage' (0-1V).		
		uint8_t firstRow = displayMatrix & 0xFFULL;
		return firstRow * voltageScaler;
	}

	float getYVoltage() override {
		// Returns right column of ouput matrix as 'voltage' (0-1V).
		// Output matrix is flipped when drawn (right -> left, left <- right),
		uint64_t yMask = 0x0101010101010101ULL;
		uint64_t column = displayMatrix & yMask;
		uint8_t yColumn = static_cast<uint8_t>((column * 0x8040201008040201ULL) >> 56);
		return yColumn * voltageScaler;
	}

	bool getXPulse() override {
		// Returns true if bottom left cell state
		// of displayMatrix is alive.
		bool bottonLeftCellState = ((displayMatrix & 0xFFULL) >> 7) & 1;
		bool xPulse = false;

		if (displayMatrixUpdated && bottonLeftCellState)
			xPulse = true;

		return xPulse;
	}

	bool getYPulse() override {
		// Returns true if top right cell state
		// of displayMatrix is alive.
		bool topRightCellState = ((displayMatrix >> 56) & 0xFFULL) & 1;
		bool yPulse = false;

		if (displayMatrixUpdated && topRightCellState)
			yPulse = true;

		return yPulse;
	}

	// LED function
	float getModeLEDValue() override {
		return static_cast<float>(modeIndex) * modeScaler;
	}


	std::string getAlgoStr() override {
		return "WOLF";
	}

	std::string getRuleStr() override {
		//return std::to_string(rule);
		return lookAndFeel->formatIntForText(rule);
	}

	std::string getRuleSelectStr() override {
		//return std::to_string(rule);
		return lookAndFeel->formatIntForText(ruleSelect);
	}

	std::string getSeedStr() override {
		std::string str = "";
		if (randSeed) {
			str = "RAND";
		}
		else {
			lookAndFeel->drawWolfSeedDisplay(2, true, seed);
		}
		return str;
	}

	std::string getModeStr() override {
		return modeName[modeIndex];
	}

	void getSeedBg() override {
		if (randSeed) {
			lookAndFeel->drawTextBg(2);
			
		}
		else {
			lookAndFeel->drawWolfSeedDisplay(2, false, seed);
		}
	}

	/*
	void onReset() override {
		//readHead = 0;
		//writeHead = 1;

		displayMatrix = 0;
		rowBuffer.empty();
		matrixBuffer.empty();

		//ruleSelect = defaultRule;
		//seedSelect = defaultSeed;
		//modeIndex = defaultMode;
	}
	*/

private:
	static constexpr int NUM_MODES = 3;
	static constexpr int defaultMode = 1;
	int modeIndex = defaultMode;
	std::array<std::string, NUM_MODES> modeName{
		"CLIP",
		"WRAP",
		"RAND"
	};

	uint8_t defaultRule = 30;
	uint8_t ruleSelect = defaultRule;
	int ruleCV = 0;
	uint8_t rule = 0;
	
	uint8_t defaultSeed = 0x08;
	uint8_t seed = defaultSeed;
	int seedSelect = seed;
	bool randSeed = false;

	int prevOffset = 0;

	bool prevXbit = false;
	bool prevYbit = false;

	static constexpr float voltageScaler = 1.f / UINT8_MAX;
	static constexpr float modeScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);
};

class LifeAlgoithm : public Algorithm {
public:
	LifeAlgoithm() {
		matrixBuffer[readHead] = seeds[seedIndex].value;	// Init seed
	}

	// Helpers
	static inline void halfadder(uint8_t a, uint8_t b,
		uint8_t& sum, uint8_t& carry) {

		sum = a ^ b;
		carry = a & b;
	}

	static inline void fulladder(uint8_t a, uint8_t b, uint8_t c,
		uint8_t& sum, uint8_t& carry) {

		uint8_t t0, t1, t2;
		halfadder(a, b, t0, t1);
		halfadder(t0, c, sum, t2);
		carry = t2 | t1;
	}

	static inline uint8_t reverseRow(uint8_t row) {
		row = ((row & 0xF0) >> 4) | ((row & 0x0F) << 4);
		row = ((row & 0xCC) >> 2) | ((row & 0x33) << 2);
		row = ((row & 0xAA) >> 1) | ((row & 0x55) << 1);
		return row;
	}

	void getHorizontalNeighbours(uint8_t row, uint8_t& west, uint8_t& east) {

		switch (modeIndex) {
		case 0: {
			// Clip.
			west = row >> 1;
			east = row << 1;
			break;
		}

		case 3: {
			// Random.
			west = (row >> 1) | (random::get<bool>() << 7);
			east = (row << 1) | random::get<bool>();
			break;
		}

		default: // Wrap & klein bottle
			west = (row >> 1) | (row << 7);
			east = (row << 1) | (row >> 7);
			break;
		}
	}
	
	void update(int o) override {
		// Push current matrix to output matrix
		uint64_t tempMatrix = 0;
		for (int i = 0; i < 8; i++) {
			uint8_t row = (matrixBuffer[readHead] >> (i * 8)) & 0xFFULL;
			tempMatrix |= uint64_t(applyOffset(row, o)) << (i * 8);
		}
		displayMatrix = tempMatrix;

		// Count current living cells
		population = __builtin_popcountll(displayMatrix);

		displayMatrixUpdated = true;
	}

	void generate() override {
		// 2D cellular automata.
		// Based on parallel bitwise implementation by Tomas Rokicki, Paperclip Optimizer,
		// and Michael Abrash's (Graphics Programmer's Black Book, Chapter 17) padding method.
		//
		// Not optimal but efficent enough and readable.
		
		uint64_t readMatrix = matrixBuffer[readHead];
		uint64_t writeMatrix = 0;

		// Eight matrix rows + top & bottom padding
		std::array<uint8_t, 10> row{};

		// Fill rows from current matrix
		for (int i = 1; i < 9; i++)
			row[i] = (readMatrix >> ((i - 1) * 8)) & 0xFFULL;

		// Fill top & bottom padding rows
		switch (modeIndex) {
		case 0: {
			// Clip
			row[0] = 0;
			row[9] = 0;
			break;
		}

		case 2: {
			// Klein bottle
			row[0] = reverseRow(row[8]);
			row[9] = reverseRow(row[1]);
			break;
		}

		case 3: {
			// Random
			row[0] = random::get<uint8_t>();
			row[9] = random::get<uint8_t>();
			break;
		}

		default:
			// Wrap
			row[0] = row[8];
			row[9] = row[1];
			break;
		}

		for (int i = 1; i < 9; i++) {
			// Current row  - C,
 			// 8 neighbours - NW, N, NE, W, E, SW, S, SE
			uint8_t n = row[i - 1];
			uint8_t c = row[i];
			uint8_t s = row[i + 1];
			uint8_t nw = 0, ne = 0, w = 0, e = 0, sw = 0, se = 0;

			getHorizontalNeighbours(n, nw, ne);
			getHorizontalNeighbours(c, w, e);
			getHorizontalNeighbours(s, sw, se);

			// Parallel bitwise addition
			// What the helly

			// Sum north row
			uint8_t Nbit0 = 0, Nbit1 = 0;
			fulladder(nw, n, ne, Nbit0, Nbit1);   

			// Sum current row
			uint8_t Cbit0 = 0, Cbit1 = 0; 
			halfadder(w, e, Cbit0, Cbit1);

			// Sum south row
			uint8_t Sbit0 = 0, Sbit1 = 0;
			fulladder(sw, s, se, Sbit0, Sbit1);   

			// North row sum  + current row sum = north_current row sum
			// (Nbit1, Nbit0) + (Cbit1, Cbit0)  = NCbit2, NCbit0, NCbit1
			uint8_t NCbit0 = 0, carry1 = 0;
			fulladder(Nbit0, Cbit0, 0, NCbit0, carry1);
			uint8_t NCbit1 = 0, NCbit2 = 0;
			fulladder(Nbit1, Cbit1, carry1, NCbit1, NCbit2);

			// (north_current row sum)   + south row sum	 = full neighbour sum
			// (NCbit0, NCbit1, NCbit2)  + (0, Sbit1, Sbit0) = NCSbit3, NCSbit2, NCSbit1, NCSbit0
			uint8_t NCSbit0 = 0, carry2 = 0;
			fulladder(NCbit0, Sbit0, 0, NCSbit0, carry2);
			uint8_t NCSbit1 = 0, carry3 = 0;
			fulladder(NCbit1, Sbit1, carry2, NCSbit1, carry3);
			uint8_t NCSbit2 = 0, NCSbit3 = 0;
			fulladder(NCbit2, 0, carry3, NCSbit2, NCSbit3);

			// MSB <- -> LSB
			std::array<uint8_t, 9> alive{};
			alive[0] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & ~NCSbit1 & ~NCSbit0);	// 0 0000
			alive[1] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & ~NCSbit1 & NCSbit0);	// 1 0001
			alive[2] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & NCSbit1 & ~NCSbit0);	// 2 0010
			alive[3] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & NCSbit1 & NCSbit0);	// 3 0011
			alive[4] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & ~NCSbit1 & ~NCSbit0);	// 4 0100
			alive[5] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & ~NCSbit1 & NCSbit0);	// 5 0101
			alive[6] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & NCSbit1 & ~NCSbit0);	// 6 0110
			alive[7] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & NCSbit1 & NCSbit0);	// 7 0111
			alive[8] = static_cast<uint8_t>(NCSbit3 & ~NCSbit2 & ~NCSbit1 & ~NCSbit0);	// 8 1000

			// Apply rule
			uint8_t birth = 0;	
			uint8_t survival = 0;	

			for (int k = 0; k < 9; k++) {
				int birthIndex = k;
				int survivalIndex = k + 9;

				if (rules[ruleIndex].value & (1 << birthIndex))
					birth |= alive[k];

				if (rules[ruleIndex].value & (1 << survivalIndex))
					survival |= alive[k];
			}
			uint8_t nextRow = (c & survival) | ((~c) & birth);

			// Update
			writeMatrix |= static_cast<uint64_t>(nextRow) << ((i - 1) * 8);
		}
		matrixBuffer[writeHead] = writeMatrix;
	}

	void pushSeed(bool w) override {
		size_t head = readHead;
		if (w)
			head = writeHead;

		uint64_t resetMatrix = seeds[seedIndex].value;

		// Dynamic random seeds
		switch (seedIndex) {
		case 9: {
			// True random
			resetMatrix = random::get<uint64_t>();
			break;
		}

		case 8: {
			// Half desity random
			resetMatrix = random::get<uint64_t>() &
			random::get<uint64_t>();
			break;
		}

		case 7: {
			// Symmetrical / mirrored random
			uint32_t randomHalf = random::get<uint32_t>();
			uint64_t mirroredRandomHalf = 0;
			for (int i = 0; i < 4; i++) {
				uint8_t row = (randomHalf >> (i * 8)) & 0xFFUL;
				mirroredRandomHalf |= static_cast<uint64_t>(row) << ((i - 3) * -8);
			}
			resetMatrix = randomHalf | (mirroredRandomHalf << 32);
			break;
		}

		default:
			break;
		}

		matrixBuffer[head] = resetMatrix;
	}

	void inject(bool a, bool w) override {
		size_t head = readHead;
		if (w)
			head = writeHead;
		uint64_t matrix = matrixBuffer[head];

		// Check to see if row is already full or empty
		if ((a & (matrix == UINT64_MAX)) | (!a & (matrix == 0)))
			return;
		
		uint64_t targetMask = matrix;
		if (a)
			targetMask = ~matrix;	// Flip row

		int targetCount = __builtin_popcountll(targetMask);	// Count target bits
		int target = random::get<uint8_t>() % targetCount;	// Random target index

		// Find corresponding bit position
		uint64_t bitMask;
		for (bitMask = 1; target || !(targetMask & bitMask); bitMask <<= 1) {
			if (targetMask & bitMask)
				target--;
		}

		/*
		// TODO: Safer way to to inject?
		uint64_t bitMask = 1;
		while (true) {
			if (targetMask & bitMask) {
				if (target == 0)
					break;
				target--;
			}
			bitMask <<= 1;
		}
		*/
		matrix = a ? (matrix | bitMask) : (matrix & ~bitMask);
		matrixBuffer[head] = matrix;
	}

	void updateRule() override {
		ruleIndex = clamp(ruleSelect + ruleCV, 0, NUM_RULES - 1);
		lookAndFeel->setRedrawBg();
	}

	/* Setters */
	void setRuleCV(float cv) override { 
		int newCV = std::round(cv * NUM_RULES); 

		if (newCV == ruleCV)
			return;

		ruleCV = newCV;
		updateRule();
	
	}

	void setRuleSelect(int d, bool r) override {
		int newSelect = updateSelect(ruleSelect, 
			NUM_RULES, ruleDefault, d, r);

		if (newSelect == ruleSelect)
			return;

		ruleSelect = newSelect;
		updateRule();
	}

	void setSeedSelect(int d, bool r) override {
		seedIndex = updateSelect(seedIndex, 
			NUM_SEEDS, seedDefault, d, r);
		lookAndFeel->setRedrawBg();
	}

	void setModeSelect(int d, bool r) override {
		modeIndex = updateSelect(modeIndex, 
			NUM_MODES, modeDefault, d, r);
		lookAndFeel->setRedrawBg();
	}

	void setBuffer(bool r) override {

		if (r) {
			matrixBuffer = {};
			return;
		}
	}

	/* Getters */
	float getXVoltage() override {
		// Returns the population (number of alive cells) as voltage (0-1V).
		return population * xVoltageScaler;
	}

	float getYVoltage() override {
		// Returns the 64-bit number output matrix as voltage (0-1V).
		return displayMatrix * yVoltageScaler;
	}

	bool getXPulse() override {
		// True if population (number of alive cells) has grown.
		bool xPulse = false;

		if (displayMatrixUpdated && (population > prevPopulation))
			xPulse = true;

		prevPopulation = population;
		return xPulse;
	}

	bool getYPulse() override {
		// True if life becomes stagnant (no change occurs),
		// also true if output repeats while looping.

		// TODO: stangant = no change for 2 generations??
		bool yPulse = false;

		if (displayMatrixUpdated && (displayMatrix == prevOutputMatrix))
			yPulse = true;

		prevOutputMatrix = displayMatrix;
		return yPulse;
	}

	float getModeLEDValue() override {
		return static_cast<float>(modeIndex) * modesScaler;
	}

	std::string getAlgoStr() override {
		return "LIFE";
	}

	std::string getRuleStr() override {
		return rules[ruleIndex].name;
	}

	std::string getRuleSelectStr() override {
		return rules[ruleSelect].name;
	}

	std::string getSeedStr() override {
		return seeds[seedIndex].name;
	}

	std::string getModeStr() override {
		return modeName[modeIndex];
	}


private:
	struct Rule {
		std::string name;
		uint32_t value;
	};

	struct Seed {
		std::string name;
		uint64_t value;
	};

	static constexpr int NUM_MODES = 4;
	int modeDefault = 1;
	int modeIndex = modeDefault;
	std::array<std::string, NUM_MODES> modeName{
		"CLIP",	// A plane bounded by 0s.
		"WRAP",	// Donut-shaped torus.
		"BOTL",	// Klein bottle - If one pair of opposite edges are reversed.
		"RAND"	// Plane is bounded by randomness. 
	};

	static constexpr int NUM_RULES = 30;
	std::array<Rule, NUM_RULES> rules{ {
		// Rules from the Hatsya catagolue & LifeWiki
		{ "WALK", 0x1908U },			// Pedestrian Life			B38/S23
		{ "VRUS", 0x5848U },			// Virus					B36/S235
		{ "TREK", 0x22A08U },			// Star Trek				B3/S0248		
		{ "SQRT", 0x6848U },			// Sqrt Replicator			B36/S245		
		{ "SEED", 0x04U },				// Seeds					B2/S			
		{ "RUGS", 0x1CU },				// Serviettes				B234/S			
		{ "MOVE", 0x6948U },			// Move / Morley			B368/S245		
		{ "MESS", 0x3D9C8U },			// Stains					B3678/S235678   
		{ "MAZE", 0x3C30U },			// Mazectric non Static		B45/S1234 
		{ " LOW", 0x1408U },			// LowLife					B3/S13		
		{ "LONG", 0x4038U },			// LongLife					B345/S5			
		{ "LIFE", 0x1808U },			// Conway's Game of Life	B3/S23			
		{ "ICE9", 0x3C1E4U },			// Iceballs					B25678/S5678	
		{ "HUNY", 0x21908U },			// HoneyLife				B38/S238		
		{ "GNRL", 0x402U },				// Gnarl					B1/S1			
		{ " GEO", 0x3A9A8U },			// Geology					B3578/S24678	
		{ "GEMS", 0x2E0B8U },			// Gems						B3457/S4568		
		{ "FREE", 0x204U },				// Live Free or Die			B2/S0			
		{ "FORT", 0x3D5C8U },			// Castles					B3678/S135678   
		{ "FLOK", 0xC08U },				// Flock					B3/S12
		{ "DUPE", 0x154AAU },			// Replicator				B1357/S1357		
		{ " DOT", 0x1A08U },			// DotLife					B3/S023
		{ "DIRT", 0x1828U },			// Grounded Life			B35/S23
		{ "DIAM", 0x3C1E8U },			// Diamoeba					B35678/S5678	
		{ "DANC", 0x5018U },			// Dance					B34/S35			
		{ "CLOT", 0x3D988U },			// Coagulations				B378/S235678	
		{ "ACID", 0x2C08U },			// Corrosion of Conformity	B3/S124			
		{ " 3-4", 0x3018U },			// 3-4 Life					B34/S34			
		{ " 2X2", 0x4C48U },			// 2x2						B35/S125		
		{ "24/7", 0x3B1C8U },			// Day & Night				B3678/S34678	
	} };
	static constexpr int ruleDefault = 11;
	int ruleSelect = ruleDefault;
	int ruleCV = 0;
	int ruleIndex = 0;

	static constexpr int NUM_SEEDS = 30;
	std::array<Seed, NUM_SEEDS> seeds { {
		// Seeds from the Life Lexicon, Hatsya catagolue & LifeWiki
		{ "WING", 0x1824140C0000ULL },		// Wing									Rule: Life
		{ "WIND", 0x60038100000ULL },		// Sidewinder Spaceship, 				Rule: LowLife
		{ "VONG", 0x283C3C343000ULL },		// Castles Spaceship, 					Rule: Castles
		{ "VELP", 0x20700000705000ULL },	// Virus Spaceship, 					Rule: Virus
		{ "STEP", 0xC1830000000ULL },		// Stairstep Hexomino					Rule: Life, HoneyLife
		{ "SSSS", 0x4040A0A0A00ULL },		// Creeper Spaceship, 					Rule: LowLife
		{ "SENG", 0x2840240E0000ULL },		// Switch Engine						Rule: Life
		{ "RNDM", 0x66555566B3AAABB2ULL },	// Symmetrical / Mirrored Random,
		{ "RNDH", 0xEFA8EFA474577557ULL },	// Half Density Random,				
		{ " RND", random::get<uint64_t>() },// True Random,
		{ "NSEP", 0x70141E000000ULL },		// Nonomino Switch Engine Predecessor	Rule: Life
		{ "MWSS", 0x50088808483800ULL },	// Middleweight Spaceship,				Rule: Life, HoneyLife
		{ "MORB", 0x38386C44200600ULL },	// Virus Spaceship, 					Rule: Virus									// TODO: move left 1 bit
		{ "MOON", 0x1008081000000000ULL },	// Moon Spaceship, 						Rule: Live Free or Die, Seeds, Iceballs
		{ "LWSS", 0x1220223C00000000ULL },	// Lightweight Spaceship,				Rule: Life, HoneyLife 
		{ "JELY", 0x203038000000ULL },		// Jellyfish Spaceship,					Rule: Move, Sqrt Replicator
		{ "HAND", 0xC1634180000ULL },		// Handshake							Rule: Life, HoneyLife
		{ "G-BC", 0x1818000024A56600ULL },	// Glider-block Cycle,					Rule: Life 					
		{ " GSV", 0x10387C38D60010ULL },	// Xq4_gkevekgzx2 Spaceship, 			Rule: Day & Night
		{ "FUMA", 0x1842424224A5C3ULL },	// Fumarole,							Rule: Life  						
		{ "FLYR", 0x382010000000ULL },		// Glider,								Rule: Life, HoneyLife			
		{ "FIG8", 0x7070700E0E0E00ULL },	// Figure 8,							Rule: Life 
		{ "EPST", 0xBA7CEE440000ULL },		// Eppstein's Glider,					Rule: Stains
		{ "CRWL", 0x850001C0000ULL },		// Crawler,								Rule: 2x2
		{ "CHEL", 0x302C0414180000ULL },	// Herschel Descendant					Rule: Life
		{ "BORG", 0x40304838380000ULL },	// Xq6_5qqs Spaceship, 					Rule: Star Trek	
		{ "BFLY", 0x38740C0C0800ULL },		// Butterfly, 							Rule: Day & Night
		{ " B&G", 0x30280C000000ULL },		// Block and Glider						Rule: Life
		{ "34D3", 0x41E140C000000ULL },		// 3-4 Life Spaceship, 					Rule: 3-4 Life
		{ "34C3", 0x3C2464140000ULL },		// 3-4 Life Spaceship, 					Rule: 3-4 Life
	} };
	static constexpr int seedDefault = 9;
	int seedIndex = seedDefault;

	int population = 0;
	int prevPopulation = 0;
	uint64_t prevOutputMatrix = 0;
	bool prevYbit = false;

	static constexpr float xVoltageScaler = 1.f / 64.f;
	static constexpr float yVoltageScaler = 1.f / UINT64_MAX;
	static constexpr float modesScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);
};