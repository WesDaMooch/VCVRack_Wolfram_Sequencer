#pragma once
#include "plugin.hpp"
#include <array>

class LookAndFeel {
public:
	// Setters
	void setDrawingContext(NVGcontext* context) { vg2 = context; }

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
				cellCirclePos[i].x = (cellSpacing * c) + (cellSpacing * 0.5f) + padding;
				cellCirclePos[i].y = (cellSpacing * r) + (cellSpacing * 0.5f) + padding;

				cellSquarePos[i].x = (cellSpacing * c) + (cellSpacing * 0.5f) - (squareCellSize * 0.5f) + padding;
				cellSquarePos[i].y = (cellSpacing * r) + (cellSpacing * 0.5f) - (squareCellSize * 0.5f) + padding;
			}
		}

	};

	void setLookIndex(int i) { lookIndex = i; }
	void setFeelIndex(int i) { feelIndex = i; }

	// Getters
	int getLookIndex() { return lookIndex; }
	int getFeelIndex() { return feelIndex; }

	NVGcolor* getBackgroundColour() { return &looks[lookIndex][0]; }
	NVGcolor* getPrimaryColour() { return &looks[lookIndex][1]; }
	NVGcolor* getSecondaryColour() { return &looks[lookIndex][2]; }

	// Drawing
	
	void drawText(const char text[5], const int row) {
		// Draw four character row of text
		nvgFillColor(vg2, *getPrimaryColour());
		nvgText(vg2, textPos[row].x, textPos[row].y, text, nullptr);
	}

	std::string formatIntForText(const int value) {
		char t[5];
		snprintf(t, sizeof(t), "%4.3d", value);
		return std::string(t);
	}

	void drawText2(const std::string& str, const int row) {
		// Draw four character row of text.
		std::string outputStr;

		if (str.size() >= 4) {
			outputStr = str.substr(str.size() - 4);
		}
		else {
			outputStr = std::string(4 - str.size(), ' ') + str;
		}

		nvgFillColor(vg2, *getPrimaryColour());
		nvgText(vg2, textPos[row].x, textPos[row].y, outputStr.c_str(), nullptr);
	}

	void drawTextBackground(const int row) {
		// Draw one row of four square text character backgrounds.

		float textBgBevel = 3.f;
		nvgFillColor(vg2, *getSecondaryColour());

		nvgBeginPath(vg2);
		for (int col = 0; col < 4; col++) {

			int i = row * 4 + col;
			nvgRoundedRect(vg2, textBgPos[i].x, textBgPos[i].y, 
				textBgSize, textBgSize, textBgBevel);
		}
		nvgFill(vg2);
	}

	void drawSeedRect(const int row, const bool state, const uint8_t seed) {

		if (state) {
			// Draw ON seed rectangles.
			float halfFontSize = fontSize * 0.5f;
			float rectSize = halfFontSize - 2.f;

			nvgFillColor(vg2, *getPrimaryColour());
			nvgBeginPath(vg2);
			for (int col = 0; col < 8; col++) {
				bool cell = (seed >> (7 - col)) & 1;
				if (cell) {
					nvgRoundedRect(vg2, (halfFontSize * col) + (halfFontSize * 0.5f) - (rectSize * 0.5f) + padding,
						(halfFontSize * 4) + (halfFontSize * 0.5f) - (rectSize * 0.5f) + padding,
						rectSize, (rectSize * 2.f) + 2.f, 3.f);
				}

			}
			nvgFill(vg2);
		}
		else {
			// Draw seed rectangle backgrounds.
			float halfFontSize = fontSize * 0.5f;
			float rectSize = halfFontSize - 2.f;

			nvgFillColor(vg2, *getSecondaryColour());
			nvgBeginPath(vg2);
			for (int col = 0; col < 8; col++) {
				bool cell = (seed >> (7 - col)) & 1;
				if (!cell) {
					nvgRoundedRect(vg2, (halfFontSize * col) + (halfFontSize * 0.5f) - (rectSize * 0.5f) + padding,
						(halfFontSize * 4) + (halfFontSize * 0.5f) - (rectSize * 0.5f) + padding,
						rectSize, (rectSize * 2.f) + 2.f, 3.f);
				}

			}
			nvgFill(vg2);
		}
	}

	void drawCell(float col, float row, bool state) {
		// Draw cell.
		int i = row * 8 + col;
		nvgFillColor(vg2, state ? *getPrimaryColour() : *getSecondaryColour());

		switch (feelIndex) {
		case 1: {
			// Square.
			nvgRoundedRect(vg2, cellSquarePos[i].x, cellSquarePos[i].y,
				squareCellSize, squareCellSize, squareCellBevel);
			break;
		}

		default:	
			// Circle.
			nvgCircle(vg2, cellCirclePos[i].x, cellCirclePos[i].y, circleCellSize);
			break;
		}
	}

protected:
	// Params
	NVGcontext* vg2;
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
	

	// Looks
	static constexpr int NUM_COLOURS = 3;
	static constexpr int NUM_LOOKS = 3;
	int lookIndex = 0;
	std::array<std::array<NVGcolor, NUM_COLOURS>, NUM_LOOKS> looks { {

		{ nvgRGB(58, 16, 19), nvgRGB(228, 7, 7), nvgRGB(78, 12, 9) },		// Default
		{ nvgRGB(37, 59, 99), nvgRGB(205, 254, 254), nvgRGB(39, 70, 153) },	// Oled
		{ nvgRGB(18, 18, 18), SCHEME_YELLOW, nvgRGB(0, 0, 0) },				// VCV Rack

	} };

	// Feels
	int feelIndex = 0;
	float circleCellSize = 5.f;
	float squareCellSize = 10.f;
	float squareCellBevel = 1.f;
};


class AlgorithmBase {
public:
	AlgorithmBase() {}
	~AlgorithmBase() {}

	// Common functions
	void SetReadHead(int h) { readHead = h; }
	void SetWriteHead(int h) { writeHead = h; }
	uint64_t GetDisplayMatrix() { return displayMatrix; }

	void AdvanceHeads(int s) {
		// Advance read and write heads
		// TODO: could use if, would be quicker?
		readHead = writeHead;
		writeHead = (writeHead + 1) % s;
	}

	uint8_t ApplyOffset(uint8_t r, int o) {
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

	// Common drawing functions.
	void SetLookAndFeel(LookAndFeel* l) { lookAndFeel = l; }
	
	// Algorithm specific functions.
	virtual void Tick() = 0;
	virtual void Step(int s) { AdvanceHeads(s); };
	virtual void Update(int o) = 0;
	virtual void Generate() = 0;
	virtual void SeedReset(bool w) = 0;
	virtual void Inject(bool a, bool w) = 0;

	// Update functions
	virtual void SetRuleCV(float cv) = 0;
	virtual void RuleUpdate(int d, bool r) = 0;
	virtual void SeedUpdate(int d, bool r) = 0;
	virtual void ModeUpdate(int d, bool r) = 0;

	// Output functions
	virtual float GetXVoltage() = 0;
	virtual float GetYVoltage() = 0;
	virtual bool GetXPulse() = 0;
	virtual bool GetYPulse() = 0;

	// Drawing functions
	virtual void DrawMenuText(int page, bool displayRule) = 0;

	virtual void DrawMenuBackground(int page, bool displayRule) {
		int rows = 4;
		if (displayRule)
			rows = 2;

		for (int row = 0; row < rows; row++) {
			lookAndFeel->drawTextBackground(row);
		}
	}

	// LED function
	virtual float GetModeLEDValue() = 0;
	virtual std::string GetRuleString() = 0;

protected:
	static constexpr size_t MAX_SEQUENCE_LENGTH = 64;
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{};
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};

	uint64_t displayMatrix = 0;
	bool displayMatrixUpdated = false;

	int readHead = 0;
	int writeHead = 1;

	// Drawing
	LookAndFeel* lookAndFeel;
	float padding = 0;
	float fontSize = 0;
};

class WolfAlgoithm : public AlgorithmBase {
public:
	WolfAlgoithm() {
		// Init seed.
		rowBuffer[readHead] = seed;
	}

	void Tick() override { 
		// Set rule.
		rule = static_cast<uint8_t>(clamp(ruleSelect + ruleCV, 0, UINT8_MAX));
		displayMatrixUpdated = false; 
	}

	void Step(int s) override {
		AdvanceHeads(s);
		displayMatrix <<= 8;	// Shift matrix along (up).
	}

	void Update(int o) override {
		displayMatrixUpdated = true;

		// Apply lastest offset.
		int offsetDifference = o - prevOffset;
		uint64_t tempMatrix = 0;
		for (int i = 1; i < 8; i++) {
			uint8_t row = (displayMatrix >> (i * 8)) & 0xFF;
			tempMatrix |= uint64_t(ApplyOffset(row, offsetDifference)) << (i * 8);
		}
		displayMatrix = tempMatrix;
		prevOffset = o;

		// Push latest row.
		displayMatrix &= ~0xFFULL;	
		displayMatrix |= static_cast<uint64_t>(ApplyOffset(rowBuffer[readHead], o));
	}

	void Generate() override {
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

	void SeedReset(bool w) override {
		uint8_t resetRow = randSeed ? random::get<uint8_t>() : seed;
		size_t head = readHead;
		if (w) {
			head = writeHead;
		}
		rowBuffer[head] = resetRow;
	}

	void Inject(bool a, bool w) override {
		int head = readHead;
		if (w)
			head = writeHead;
		
		uint8_t row = rowBuffer[head];

		// Check to see if row is already full or empty
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

	void SetRuleCV(float cv) override { ruleCV = std::round(cv * 256); }

	void RuleUpdate(int d, bool r) override {
		if (r) {
			ruleSelect = defaultRule;
			return;
		}
		ruleSelect = static_cast<uint8_t>(ruleSelect + d);
	}

	void SeedUpdate(int d, bool r) override {
		if (r) {
			seed = defaultSeed;
			seedSelect = seed;
			randSeed = false;
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
	}

	void ModeUpdate(int d, bool r) override {
		if (r) {
			modeIndex = modeDefault;
			return;
		}
		modeIndex = (modeIndex + d + NUM_MODES) % NUM_MODES;
	}

	float GetXVoltage() override {
		// Returns bottom row of Ouput Matrix as 'voltage' (0-1V).		
		uint8_t firstRow = displayMatrix & 0xFFULL;
		return firstRow * voltageScaler;
	}

	float GetYVoltage() override {
		// Returns right column of ouput matrix as 'voltage' (0-1V).
		// Output matrix is flipped when drawn (right -> left, left <- right),
		uint64_t yMask = 0x0101010101010101ULL;
		uint64_t column = displayMatrix & yMask;
		uint8_t yColumn = static_cast<uint8_t>((column * 0x8040201008040201ULL) >> 56);
		return yColumn * voltageScaler;
	}

	bool GetXPulse() override {
		// Returns true if bottom left cell state
		// of displayMatrix is alive.
		bool bottonLeftCellState = ((displayMatrix & 0xFFULL) >> 7) & 1;
		bool xPulse = false;

		if (displayMatrixUpdated && bottonLeftCellState)
			xPulse = true;

		return xPulse;
	}

	bool GetYPulse() override {
		// Returns true if top right cell state
		// of displayMatrix is alive.
		bool topRightCellState = ((displayMatrix >> 56) & 0xFFULL) & 1;
		bool yPulse = false;

		if (displayMatrixUpdated && topRightCellState)
			yPulse = true;

		return yPulse;
	}

	// Drawing functions
	void DrawMenuBackground(int page, bool displayRule) override {
		int rows = 4;
		if (displayRule)
			rows = 2;

		for (int row = 0; row < rows; row++) {
			if (!randSeed && (page == 0) && (row == 2))
				// Seed
				lookAndFeel->drawSeedRect(row, false, seed);
			else
				lookAndFeel->drawTextBackground(row);
		}
	}
	
	void DrawMenuText(int page, bool displayRule) override {
		if (displayRule) {
			lookAndFeel->drawText2("RULE", 0);
			lookAndFeel->drawText2(lookAndFeel->formatIntForText(rule), 1);
			return;
		}

		std::string pageName;
		std::string pageData;

		switch (page) {
		case 0: {
			pageName = "SEED";
			if (randSeed) {
				pageData = "RAND";
				break;
			}
			lookAndFeel->drawSeedRect(2, true, seed);
			break;
		}

		case 2: {
			pageName = "MODE";
			pageData = modeName[modeIndex];
			break;
		}

		default:
			break;
		}

		lookAndFeel->drawText2(pageName, 1);
		lookAndFeel->drawText2(pageData, 2);
	}

	// LED function
	float GetModeLEDValue() override {
		return static_cast<float>(modeIndex) * modeScaler;
	}

	std::string GetRuleString() override {
		std::string ruleString = std::to_string(rule);
		return ruleString;
	}

private:
	static constexpr int NUM_MODES = 3;
	static constexpr int modeDefault = 1;
	int modeIndex = modeDefault;
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

class LifeAlgoithm : public AlgorithmBase {
public:
	LifeAlgoithm() {
		// Init seed.
		matrixBuffer[readHead] = seeds[seedIndex].value;
	}

	// Helper functions
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
	
	// Algoithm specific functions
	void Tick() override {
		ruleIndex = clamp(ruleSelect + ruleCV, 0, NUM_RULES - 1);
		displayMatrixUpdated = false;
	}

	void Update(int o) override {
		displayMatrixUpdated = true;

		// Push current matrix to output matrix
		uint64_t tempMatrix = 0;
		for (int i = 0; i < 8; i++) {
			uint8_t row = (matrixBuffer[readHead] >> (i * 8)) & 0xFFULL;
			tempMatrix |= uint64_t(ApplyOffset(row, o)) << (i * 8);
		}
		displayMatrix = tempMatrix;

		// Count current living cells
		population = __builtin_popcountll(displayMatrix);
	}

	void Generate() override {
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

	void SeedReset(bool w) override {
		size_t head = readHead;
		if (w)
			head = writeHead;

		uint64_t resetMatrix = seeds[seedIndex].value;

		// Dynamic random seeds.
		switch (seedIndex) {
		case 9:
			// True random.
			resetMatrix = random::get<uint64_t>();
			break;
		case 8:
			// Half desity random.
			resetMatrix = random::get<uint64_t>() & 
				random::get<uint64_t>();
			break;
		case 7: {
			// Symmetrical / mirrored random.
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

	void Inject(bool a, bool w) override {
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

	void SetRuleCV(float cv) override { ruleCV = std::round(cv * NUM_RULES); }

	void RuleUpdate(int d, bool r) override {
		if (r) {
			ruleSelect = ruleDefault;
			return;
		}
		ruleSelect = (ruleSelect + d + NUM_RULES) % NUM_RULES;
	}

	void SeedUpdate(int d, bool r) override {
		if (r) {
			seedIndex = seedDefault;
			return;
		}
		seedIndex = (seedIndex + d + NUM_SEEDS) % NUM_SEEDS;
	}

	void ModeUpdate(int d, bool r) override {
		if (r) {
			modeIndex = modeDefault;
			return;
		}
		modeIndex = (modeIndex + d + NUM_MODES) % NUM_MODES;
	}

	float GetXVoltage() override {
		// Returns the population (number of alive cells) as voltage (0-1V).
		return population * xVoltageScaler;
	}

	float GetYVoltage() override {
		// Returns the 64-bit number output matrix as voltage (0-1V).
		return displayMatrix * yVoltageScaler;
	}

	bool GetXPulse() override {
		// True if population (number of alive cells) has grown.
		bool xPulse = false;

		if (displayMatrixUpdated && (population > prevPopulation))
			xPulse = true;

		prevPopulation = population;
		return xPulse;
	}

	bool GetYPulse() override {
		// True if life becomes stagnant (no change occurs),
		// also true if output repeats while looping.

		// TODO: stangant = no change for 2 generations??
		bool yPulse = false;

		if (displayMatrixUpdated && (displayMatrix == prevOutputMatrix))
			yPulse = true;

		prevOutputMatrix = displayMatrix;
		return yPulse;
	}

	// Drawing functions
	void DrawMenuText(int page, bool displayRule) override {
		if (displayRule) {
			lookAndFeel->drawText2("RULE", 0);
			lookAndFeel->drawText2(rules[ruleIndex].name, 1);
			return;
		}

		std::string pageName;
		std::string pageData;

		switch (page) {
		case 0: {
			pageName = "SEED";
			pageData = seeds[seedIndex].name;
			break;
		}

		case 2: {
			pageName = "MODE";
			pageData = modeName[modeIndex];
			break;
		}

		default:
			break;
		}

		lookAndFeel->drawText2(pageName, 1);
		lookAndFeel->drawText2(pageData, 2);
	}

	// LED function
	float GetModeLEDValue() override {
		return static_cast<float>(modeIndex) * modesScaler;
	}

	std::string GetRuleString() override {
		return rules[ruleIndex].name;
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
	static constexpr float modesScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);
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
};