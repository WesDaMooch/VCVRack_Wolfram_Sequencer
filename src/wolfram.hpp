#pragma once
#include "plugin.hpp"
#include <array>

// TODO: stop using virtuals, use Function pointer instead?
// TODO: If I want static constexpr need header and cpps for wolfram,
// might be a good idea


// MSB = left most
// LSB = right most

// Base class
class AlgorithmBase {
public:
	AlgorithmBase() {}
	~AlgorithmBase() {}

	// Common functions
	void SetReadHead(int r) {
		readHead = r;
	}

	void SetWriteHead(int w) {
		writeHead = w;
	}

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

	uint64_t GetOutputMatrix() { 
		return outputMatrix; 
	}

	// Algorithm specific functions
	virtual void OutputMatrixStep() = 0;
	virtual void OutputMatrixPush(int o) = 0;
	virtual void Generate() = 0;
	virtual void SeedReset(bool w) = 0;
	virtual void Inject(bool a, bool w) = 0;

	// Update functions
	virtual void RuleUpdate(int d, bool r) = 0;
	virtual void SeedUpdate(int d, bool r) = 0;
	virtual void ModeUpdate(int d, bool r) = 0;

	// Output functions
	virtual float GetXVoltage() = 0;
	virtual float GetYVoltage() = 0;
	virtual bool GetXGate() = 0;
	virtual bool GetYGate() = 0;

	// Drawing functions
	virtual void DrawRuleMenu(NVGcontext* vg, float fs) = 0;
	virtual void DrawSeedMenu(NVGcontext* vg, float fs) = 0;
	virtual void DrawModeMenu(NVGcontext* vg, float fs) = 0;

	// LED function
	virtual float GetModeLEDValue() = 0;

protected:
	static constexpr size_t MAX_SEQUENCE_LENGTH = 64;
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{};
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};

	uint64_t outputMatrix = 0;

	int readHead = 0;
	int writeHead = 1;
};

class WolfAlgoithm : public AlgorithmBase {
public:
	WolfAlgoithm() {
		// Init seed
		rowBuffer[readHead] = seed;
	}

	void OutputMatrixStep() override {
		// Shift matrix along
		outputMatrix <<= 8;
	}

	void OutputMatrixPush(int o) override {
		// Apply lastest offset
		int offsetDifference = o - prevOffset;
		uint64_t tempMatrix = 0;
		for (int i = 1; i < 8; i++) {
			uint8_t row = (outputMatrix >> (i * 8)) & 0xFF;
			tempMatrix |= uint64_t(ApplyOffset(row, offsetDifference)) << (i * 8);
		}
		outputMatrix = tempMatrix;
		prevOffset = o;

		// Push latest row
		outputMatrix &= ~0xFFULL;	
		outputMatrix |= static_cast<uint64_t>(ApplyOffset(rowBuffer[readHead], o));	
	}

	void Generate() override {
		// One Dimensional Cellular Automata.
		uint8_t readRow = rowBuffer[readHead];
		uint8_t writeRow = 0;
		
		// Clip
		// TODO: this is different from life!
		uint8_t west = readRow >> 1;
		uint8_t east = readRow << 1;
		switch (mode) {
		case Mode::WRAP:
			west = (readRow >> 1) | (readRow << 7);
			east = (readRow << 1) | (readRow >> 7);
			break;
		case Mode::RANDOM:
			west |= random::get<bool>() << 7;
			east |= random::get<bool>();
			break;
		default:
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

	void RuleUpdate(int d, bool r) override {
		if (r) {
			rule = 30;
			return;
		}
		// TODO: is the wrapped because of the cast, is this a good way to do things
		rule = static_cast<uint8_t>(rule + d);
	}

	void SeedUpdate(int d, bool r) override {
		if (r) {
			seed = 8;
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
			mode = Mode::WRAP;
			return;
		}

		int modeIndex = static_cast<int>(mode);
		modeIndex = (modeIndex + d + numModes) % numModes;
		mode = static_cast<Mode>(modeIndex);
	}

	float GetXVoltage() override {
		// Returns bottom row of Ouput Matrix as 'voltage' (0-1V).		
		uint8_t firstRow = outputMatrix & 0xFFULL;
		return firstRow * voltageScaler;
	}

	float GetYVoltage() override {
		// Returns right column of ouput matrix as 'voltage' (0-1V).
		// Output matrix is flipped when drawn (right -> left, left <- right),
		uint64_t yMask = 0x0101010101010101ULL;
		uint64_t column = outputMatrix & yMask;
		uint8_t yColumn = static_cast<uint8_t>((column * 0x8040201008040201ULL) >> 56);
		return yColumn * voltageScaler;
	}

	bool GetXGate() override {
		// Returns bottom left cell state of ouput matrix.
		uint8_t firstRow = outputMatrix & 0xFFULL;
		return (firstRow >> 7) & 1;
	}

	bool GetYGate() override {
		// Returns top right cell state of ouput matrix.
		uint8_t lastRow = (outputMatrix >> 56) & 0xFFULL;
		return lastRow & 1;
	}

	// Drawing functions
	void DrawRuleMenu(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, 0, "RULE", nullptr);
		char ruleString[5];
		snprintf(ruleString, sizeof(ruleString), "%4.3d", rule);
		nvgText(vg, 0, fs, ruleString, nullptr);
	}

	void DrawModeMenu(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, fs, "MODE", nullptr);
		switch (mode) {
		case Mode::WRAP: 
			nvgText(vg, 0, fs * 2, "WRAP", nullptr);
			break;
		case Mode::CLIP: 
			nvgText(vg, 0, fs * 2, "CLIP", nullptr);
			break;
		case Mode::RANDOM: 
			nvgText(vg, 0, fs * 2, "RAND", nullptr);
			break;
		default: 
			break;
		}
	}

	void DrawSeedMenu(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, fs, "SEED", nullptr);

		if (randSeed) {
			nvgText(vg, 0, fs * 2, "RAND", nullptr);
			return;
		}
		float cellPos = fs * 0.5f;
		float cellSize = fs * 0.25f;
		for (int col = 0; col < 8; col++) {
			if ((seed >> (7 - col)) & 1) {
				nvgBeginPath(vg);
				nvgCircle(vg, (cellPos * col) + cellSize, fs * 2.5f, cellSize);
				nvgFill(vg);
			}
		}

	}

	// LED function
	float GetModeLEDValue() override {
		int modeIndex = static_cast<int>(mode);
		return static_cast<float>(modeIndex) * numModesScaler;
	}

private:
	enum class Mode {
		WRAP,
		CLIP,
		RANDOM,
		MODE_LEN
	};

	Mode mode = Mode::WRAP;
	static constexpr int numModes = static_cast<int>(Mode::MODE_LEN);
	static constexpr float numModesScaler = 1.f / (static_cast<float>(numModes) - 1.f);

	uint8_t rule = 30;
	uint8_t seed = 0x08;
	int seedSelect = seed;
	bool randSeed = false;

	int prevOffset = 0;

	static constexpr float voltageScaler = 1.f / UINT8_MAX;
};

class LifeAlgoithm : public AlgorithmBase {
public:
	LifeAlgoithm() {
		matrixBuffer[readHead] = seeds[seedIndex].seedInt;
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
		switch (mode) {
		case Mode::CLIP:
			west = row << 1;  
			east = row >> 1;  
			break;
		case Mode::RANDOM:
			west = (row << 1) | (random::get<bool>());     
			east = (row >> 1) | (random::get<bool>() << 7);
			break;
		default: // Wrap & klein bottle
			west = (row << 1) | (row >> 7);
			east = (row >> 1) | (row << 7);
			break;
		}
	}
	
	// Algoithm specific functions.
	void OutputMatrixStep() override {
		// Suppress behaviour.
	}

	void OutputMatrixPush(int o) override {
		// Push current matrix to output matrix.
		uint64_t tempMatrix = 0;
		for (int i = 0; i < 8; i++) {
			uint8_t row = (matrixBuffer[readHead] >> (i * 8)) & 0xFFULL;
			tempMatrix |= uint64_t(ApplyOffset(row, o)) << (i * 8);
		}
		outputMatrix = tempMatrix;

		// Count the current number of living cells.
		population = __builtin_popcountll(outputMatrix);
	}

	void Generate() override {
		// 2D cellular automata.
		// Based on parallel bitwise implementation by Tomas Rokicki, Paperclip Optimizer
		// and Michael Abrash's (Graphics Programmer's Black Book, Chapter 17) padding method.
		//
		// Not optimal but efficent enough and readable.
		
		uint64_t readMatrix = matrixBuffer[readHead];
		uint64_t writeMatrix = 0;

		// Eight matrix rows + top & bottom padding.
		std::array<uint8_t, 10> row{};

		// Fill rows from current matrix.
		for (int i = 1; i < 9; i++)
			row[i] = (readMatrix >> ((i - 1) * 8)) & 0xFFULL;

		// Fill top & bottom padding rows.
		switch (mode) {
		case Mode::CLIP:
			row[0] = 0;
			row[9] = 0;
			break;
		case Mode::KlEIN_BOTTLE:
			row[0] = reverseRow(row[8]);
			row[9] = reverseRow(row[1]);
			break;
		case Mode::RANDOM:
			row[0] = random::get<uint8_t>();
			row[9] = random::get<uint8_t>();
			break;
		default:
			// Wrap
			row[0] = row[8];
			row[9] = row[1];
			break;
		}

		for (int i = 1; i < 9; i++) {
			// Current row  - C,
 			// 8 neighbours - NW, N, NE, W, E, SW, S, SE.
			uint8_t n = row[i - 1];
			uint8_t c = row[i];
			uint8_t s = row[i + 1];
			uint8_t nw = 0, ne = 0, w = 0, e = 0, sw = 0, se = 0;

			getHorizontalNeighbours(n, nw, ne);
			getHorizontalNeighbours(c, w, e);
			getHorizontalNeighbours(s, sw, se);

			// Parallel bitwise addition.
			// What the helly.

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

			// MSB <- -> LSB.
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

			// Apply rule.
			uint8_t birth = 0;	
			uint8_t survival = 0;	

			for (int k = 0; k < 9; k++) {
				int birthIndex = k;
				int survivalIndex = k + 9;

				if (rules[ruleIndex].ruleInt & (1 << birthIndex))
					birth |= alive[k];

				if (rules[ruleIndex].ruleInt & (1 << survivalIndex))
					survival |= alive[k];
			}
			uint8_t nextRow = (c & survival) | ((~c) & birth);

			// Update.
			writeMatrix |= static_cast<uint64_t>(nextRow) << ((i - 1) * 8);
		}
		matrixBuffer[writeHead] = writeMatrix;
	}

	void SeedReset(bool w) override {
		size_t head = readHead;
		if (w)
			head = writeHead;

		uint64_t resetMatrix = seeds[seedIndex].seedInt;

		// Dynamic random seeds.
		switch (seedIndex) {
		case 0:
			// TODO: S random
			resetMatrix = random::get<uint32_t>();
			break;
		case 1:
			// TODO: Half random
			resetMatrix = 0;
			break;
		case 2:
			// True random
			resetMatrix = random::get<uint64_t>();
			break;
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

	void RuleUpdate(int d, bool r) override {
		if (r) {
			//rule = Rule::LIFE;
			ruleIndex = ruleDefault;
			return;
		}
		ruleIndex = (ruleIndex + d + numRules) % numRules;
	}

	void SeedUpdate(int d, bool r) override {
		if (r) {
			seedIndex = seedDefault;
			return;
		}
		seedIndex = (seedIndex + d + numSeeds) % numSeeds;
	}

	void ModeUpdate(int d, bool r) override {
		if (r) {
			mode = Mode::WRAP;
			return;
		}
			
		int modeIndex = static_cast<int>(mode);
		modeIndex = (modeIndex + d + numModes) % numModes;
		mode = static_cast<Mode>(modeIndex);
	}

	float GetXVoltage() override {
		// Returns the population (number of alive cells) as voltage (0-1V).
		return population * xVoltageScaler;
	}

	float GetYVoltage() override {
		// Returns the 64-bit number output matrix as voltage (0-1V).
		return outputMatrix * yVoltageScaler;
	}

	bool GetXGate() override {
		// True if population (number of alive cells) has grown.
		bool growth = false;
		if (population > prevPopulation)
			growth = true;
		prevPopulation = population;
		return growth;
	}

	bool GetYGate() override {
		// TODO: Think what this out should be
		return false;
	}

	// Drawing functions
	void DrawRuleMenu(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, 0, "RULE", nullptr);
		nvgText(vg, 0, fs, rules[ruleIndex].displayName, nullptr);

		/*
		switch (rule) {
		case Rule::LIFE:
			nvgText(vg, 0, fs, "LIFE", nullptr);
			break;
		case Rule::HIGH:
			nvgText(vg, 0, fs, "HIGH", nullptr);
			break;
		case Rule::DUPE:
			nvgText(vg, 0, fs, "DUPE", nullptr);
			break;
		case Rule::R2X2:
			nvgText(vg, 0, fs, " 2X2", nullptr);
			break;
		case Rule::R34:
			nvgText(vg, 0, fs, "  34", nullptr);
			break;
		case Rule::WES:
			nvgText(vg, 0, fs, " WES", nullptr);
			break;
		case Rule::SEED:
			nvgText(vg, 0, fs, "SEED", nullptr);
			break;
		default:
			nvgText(vg, 0, fs, "NOPE", nullptr);
			break;
		}
		*/
	}

	void DrawModeMenu(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, fs, "MODE", nullptr);
		switch (mode) {
		case Mode::WRAP:
			nvgText(vg, 0, fs * 2, "WRAP", nullptr);
			break;
		case Mode::KlEIN_BOTTLE:
			nvgText(vg, 0, fs * 2, "BOTL", nullptr);
			break;
		case Mode::CLIP:
			nvgText(vg, 0, fs * 2, "CLIP", nullptr);
			break;
		case Mode::RANDOM:
			nvgText(vg, 0, fs * 2, "RAND", nullptr);
			break;
		default:
			break;
		}
	}

	void DrawSeedMenu(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, fs, "SEED", nullptr);
		nvgText(vg, 0, fs * 2, seeds[seedIndex].displayName, nullptr);
	}

	// LED function
	float GetModeLEDValue() override {
		const int modeIndex = static_cast<int>(mode);
		return static_cast<float>(modeIndex) * numModesScaler;
	}

private:
	struct Rule {
		char displayName[5];
		uint32_t ruleInt;
	};

	struct Seed {
		char displayName[5];
		uint64_t seedInt;
	};

	/*
	enum class Rule {
		// idk
		LIFE,	// cool
		HIGH,	// cool (bit like life)
		DUPE,	// cool
		R2X2,	// bit boring, 
		R34,	// v cool
		WES, 
		SEED,	// v cool

		// Amoeba (S1358/B357), might not work on 8x8 board *
		// Assimilation (S4567/B345), might be fun with rand mode? *
		// Coagulations (S235678/B378), pretty cool *
		// Coral (S45678/B3), pretty cool, but goes semi-static fast
		// Day & Night (S34678/B3678), cool *
		// Diamoeba (S5678/B35678), cool but goes semi-static fast
		// Flakes (S012345678/B3), also known as Life without Death (LwoD), cool but semi-static
		// Gnarl (S1/B1), pretty mental and cool *
		// InverseLife (S34678/B0123478), ye kinda cool *
		// Long life (S5/B345), cool but might do dead on a small grid *
		// Maze (S12345/B3), cool but goes semi-static fast
		// Mazectric (S1234/B3), cool but goes semi-static fast
		// Mazectric non static (S1234/B45), cool *
		// Corrosion of Conformity (S124/B3), check rule, cool ish *
		// Move (S245/B368), do kinda like it *
		// Pseudo life (S238/B357), could include bit it might be so similar to life *
		// Seeds (2) (/B2), might be cool? * 
		// Serviettes (/B234), ye p cool *
		// Stains (S235678/B3678), too static maybe 
		// WalledCities (S2345/B45678), cool but will it work on small grid

		RULE_LEN
	};
	*/

	enum class Mode {
		CLIP,			// A plane bounded by 0s.
		WRAP,			// Donut-shaped torus.
		KlEIN_BOTTLE,	// If one pair of opposite edges are reversed.	
		RANDOM,			// Plane is bounded by randomness. 
		MODE_LEN
		// TODO: More modes to add from Golly
		// Cross surface - if both pairs of opposite edges are reversed.
		// Sphere - if adjacent edges are joined rather than opposite edges.
	};

	Mode mode = Mode::WRAP;
	static constexpr int numModes = static_cast<int>(Mode::MODE_LEN);
	static constexpr float numModesScaler = 1.f / (static_cast<float>(numModes) - 1.f);

	static constexpr int numRules = 22;	// 30?
	std::array<Rule, numRules> rules{ {
			// Rules from the Hatsya catagolue & LifeWiki.
			{ "24/7", 0x3B1C8U },		// Day & Night				B3678/S34678	*
			{ " 2X2", 0x4C48U },		// 2x2						B35/S125		*
			{ " 3-4", 0x3018U },		// 3-4 Life					B34/S34			*
			{ "ACID", 0x2C08U },		// Corrosion of Conformity	B3/S124			*
			//{ "CLOT", 0x188, 0x1EC },	// Coagulations				B378/S235678	
			{ "DIAM", 0x3C1E8U },		// Diamoeba					B35678/S5678	*
			{ "DUPE", 0x154AAU },		// Replicator				B1357/S1357		*
			{ "FLOK", 0xC08U },			// Flock					B3/S12
			{ "FREE", 0x204U },			// Live Free or Die			B2/S0			*
			{ "GNRL", 0x402U },			// Gnarl					B1/S1			*
			{ "LIFE", 0x1808U },		// Conway's Game of Life	B3/S23			*
			{ "MAZE", 0x7C88U },		// Maze with mice			B37/S12345		Mazectric non static old:B45/S1234 <- better
			{ "MESS", 0x3D9C8U },		// Stains					B3678/S235678   *
			{ "MOVE", 0x6948U },		// Move	/ Morley			B368/S245		*
			{ "SEED", 0x04U },			// Seeds					B2/S			*
			{ "RUGS", 0x1CU },			// Serviettes				B234/S			*
			{ "SQRT", 0x6848U },		// Sqrt Replicator			B36/S245		*
			{ "LONG", 0x4038U },		// LongLife					B345/S5			*
			{ " LOW", 0x21948U },		// LowDeath					B368/S238		*
			{ "HUNY", 0x21908U },		// HoneyLife				B38/S238		
			{ "ICE9", 0x3C1E4U },		// Iceballs					B25678/S5678	*
			{ "DIRT", 0x1828U },		// Grounded Life			B35/S23
			{ "TREK", 0x22A08U },		// Star Trek				B3/S0248		*

			// looks cool
			// Shoots and Roots (??) - good ship https://conwaylife.com/wiki/OCA:Shoots_and_Roots
			// Dance p cool tbh https://conwaylife.com/wiki/OCA:Dance
			// Land Rush could b cool https://conwaylife.com/wiki/OCA:Land_Rush
			// Geo might be ok https://conwaylife.com/wiki/OCA:Geology
			// Virus, has some ship https://conwaylife.com/wiki/OCA:Virus
			
			// hmmm
			// Castles https://conwaylife.com/wiki/OCA:Castles

			// HighLife nah
			// Gems ye?
			// DryLife meh
			// EightLife meh
			// DotLife maybe
			//{ "WALK", 0x1908U },		// Pedestrian Life			B38/S23			Bit to much like Life
	} };
	static constexpr int ruleDefault = 0;
	int ruleIndex = ruleDefault;

	// TODO: Add some seeds from over rule sets
	static constexpr int numSeeds = 40;
	std::array<Seed, numSeeds> seeds { {
		// Patterns from the Life Lexicon & LifeWiki.
		// Random - generic seeds as values are dynamic.
   		// Soup, junk, ash - https://conwaylife.com/wiki/Soup
		{ " RND", 0x0ULL },					// True random
		{ "RNDM", 0x0ULL },					// Mirror / symmetrical random,			
		{ "RND2", 0x0ULL },					// Half random - could be quarter random
		// Spaceship.
		{ "FLYR", 0x382010000000ULL },		// Glider,								Rule: Life, LowDeath, HoneyLife			
		{ "LWSS", 0x1220223C00000000ULL },	// Lightweight spaceship,				Rule: Life, LowDeath, HoneyLife 
		{ "MWSS", 0x82240427C000000ULL },	// Middleweight spaceship,	TODO: make face down			Rule: Life, LowDeath, HoneyLife
		// Heptomino.
		{ "BHEP", 0x2C3810000000ULL },		// B-heptomino   // TODO: R hep?
		{ "CHEP", 0x1C38100000ULL },		// C-heptomino 
		{ "EHEP", 0x1C30180000ULL },		// E-heptomino 
		{ "FHEP", 0x3010101C0000ULL },		// F-heptomino
		{ "HHEP", 0x30101C080000ULL },		// H-heptomino
		// Misc.
		{ " BIT", 0x8000000ULL },			// Lonely living cell - meh
		{ "BLOK", 0x1818000000ULL },		// Still life 4x4		BORING, good with dupe
		{ "BUNY", 0x4772200000ULL },		// Rabbits
		{ " B&G", 0x30280C000000ULL },		// Block and glider
		{ "CENT", 0xC38100000ULL },			// Century
		{ "CHEL", 0x302C0414180000ULL },	// Herschel descendant
		{ " CUP", 0xC1410D0D2161030ULL },	// Cuphook
		{ "FIG8", 0x7070700E0E0E00ULL },	// Figure 8,							Rule: Life 
		{ "FUMA", 0x1842424224A5C3ULL },	// Fumarole,							Rule: Life  						
		{ "G-BC", 0x1818000024A56600ULL },	// Glider-block cycle,					Rule: Life 					
		{ "HAND", 0xC1634180000ULL },		// Handshake							Rule: Life, HoneyLife
		{ "NSEP", 0x70141E000000ULL },		// Nonomino switch engine predecessor
		{ "PARV", 0xE1220400000ULL },		// Multum in parvo
		{ "SENG", 0x2840240E0000ULL },		// Switch engine
		{ "STEP", 0xC1830000000ULL },		// Stairstep hexomino					Rule: Life, HoneyLife
		{ "WING", 0x1824140C0000ULL },		// Wing
		{ "WORD", 0x24A56600001818ULL },	// Rephaser
		// Cross
		// Oct

		{ "EPST", 0xBA7CEE440000ULL },		// Eppstein's glider,					Rule: Stains
		{ "JELY", 0x203038000000ULL },		// Jellyfish spaceship,					Rule: Move, Sqrt replicator
		{ "CRWL", 0x850001C0000ULL },		// Crawler,								Rule: 2x2
		{ " GSV", 0x10387C38D60010ULL },	// Xq4_gkevekgzx2 spaceship, 			Rule: Day & Night
		{ "BFLY", 0x38740C0C0800ULL },		// Butterfly, 							Rule: Day & Night
		{ "MOON", 0x1824000000ULL },		// Moon spaceship, 						Rule: Live Free or Die, Seeds, Iceballs
		{ "34C3", 0x3C2464140000ULL },		// 3-4 Life spaceship, 					Rule: 3-4 Life
		{ "34D3", 0x41E140C000000ULL },		// 3-4 Life spaceship, 					Rule: 3-4 Life
		{ "BORG", 0x40304838380000ULL },	// xq6_5qqs spaceship, 					Rule: Star Trek


		//LowLife seeds: https://conwaylife.com/wiki/OCA:LowLife

		// Could add
		//{ "XQ14", 0x10387C38D60010ULL },	// xq14_6q5tl spaceship, 				Rule: Amoeba b357s1358

		// Nah
		//{ "RGUN", 0x90048B88B0A000ULL },    // Rotating gun,						Rule: Pedestrian life DONT WORK
		//{ "BLOB", 0x20BD3CFF3C3C0000ULL },	// Diamoeba Oscillator					Rule: Diamoeba
		//{ "SHIP", 0x3018183000000000ULL },	// Ship,								Rule: Corrosion of Conformity
	} };
	static constexpr int seedDefault = 0;
	int seedIndex = seedDefault;

	int population = 0;
	int prevPopulation = 0;

	static constexpr float xVoltageScaler = 1.f / 64.f;
	static constexpr float yVoltageScaler = 1.f / UINT64_MAX;
};

class AlgoithmEngine {
public:
	// Algoithm dispatcher.
	AlgoithmEngine() {
		algorithms[0] = &wolf;
		algorithms[1] = &life;

		// Default alogrithm.
		activeAlogrithm = algorithms[algorithmIndex];

		outputMatrixPush();
	}

	// Common functions
	void setOffset(int newOffset) { 
		offset = newOffset;
	}

	void algoithmUpdate(int delta, bool reset) {
		if (reset) {
			algorithmIndex = 0;
		}
		else {
			algorithmIndex = (algorithmIndex + delta + MAX_ALGORITHMS) % MAX_ALGORITHMS;
		}
		activeAlogrithm = algorithms[algorithmIndex];
	}

	void drawAlgoithmMenu(NVGcontext* vg, float fontSize) {
		nvgText(vg, 0, fontSize, "ALGO", nullptr);
		switch (algorithmIndex) {
		case 0:
			nvgText(vg, 0, fontSize * 2, "WOLF", nullptr);
			break;
		case 1:
			nvgText(vg, 0, fontSize * 2, "LIFE", nullptr);
			break;
		default:
			break;
		}		
	}

	// Common algoithm specific functions.
	void setReadHead(int readHead) { activeAlogrithm->SetReadHead(readHead); }
	void setWriteHead(int writeHead) { activeAlogrithm->SetWriteHead(writeHead); }
	void advanceHeads(int sequenceLength) { activeAlogrithm->AdvanceHeads(sequenceLength); }
	uint64_t getOutputMatrix() { return activeAlogrithm->GetOutputMatrix(); }

	// Algoithm specific functions.
	void outputMatrixStep() { activeAlogrithm->OutputMatrixStep(); }
	void outputMatrixPush() { activeAlogrithm->OutputMatrixPush(offset); }
	void generate() { activeAlogrithm->Generate(); }
	void seedReset(bool write = false) { activeAlogrithm->SeedReset(write); }
	void inject(bool add, bool write = false) { activeAlogrithm->Inject(add, write); }

	void ruleUpdate(int delta, bool reset) { activeAlogrithm->RuleUpdate(delta, reset); }
	void seedUpdate(int delta, bool reset) { activeAlogrithm->SeedUpdate(delta, reset); }
	void modeUpdate(int delta, bool reset) { activeAlogrithm->ModeUpdate(delta, reset); }

	float getXVoltage() { return activeAlogrithm->GetXVoltage(); }
	float getYVoltage() { return activeAlogrithm->GetYVoltage(); }
	bool getXGate() { return activeAlogrithm->GetXGate(); }
	bool getYGate() { return activeAlogrithm->GetYGate(); }

	// Drawing functions
	void drawRuleMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawRuleMenu(vg, fontSize); }
	void drawModeMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawModeMenu(vg, fontSize); }
	void drawSeedMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawSeedMenu(vg, fontSize); }

	// LED function
	float getModeLEDValue() { return activeAlogrithm->GetModeLEDValue(); }

private:
	WolfAlgoithm wolf;
	LifeAlgoithm life;

	static constexpr int MAX_ALGORITHMS = 2;
	int algorithmIndex = 1;
	std::array<AlgorithmBase*, MAX_ALGORITHMS> algorithms;
	AlgorithmBase* activeAlogrithm;

	int offset = 0;
};