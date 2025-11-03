#pragma once
#include "plugin.hpp"
#include <array>

// TODO: stop using virtuals, use Function pointer instead?

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
			r = (r << shift) | (r >> (8 - shift));
		}
		else if (shift > 0) {
			r = (r >> shift) | (r << (8 - shift));
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
	virtual void GenerateReset(bool w) = 0;
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
	virtual void DrawRule(NVGcontext* vg, float fs) = 0;
	virtual void DrawSeedMenu(NVGcontext* vg, float fs) = 0;
	virtual void DrawModeMenu(NVGcontext* vg, float fs) = 0;

	// Light function
	virtual float GetModeValue() = 0;

protected:
	static constexpr size_t MAX_SEQUENCE_LENGTH = 64;
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{};
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> frameBuffer{};

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
		// Apply new offset
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
		uint8_t left = readRow >> 1;
		uint8_t right = readRow << 1;
		switch (mode) {
		case Mode::WRAP:
			left = (readRow >> 1) | (readRow << 7);
			right = (readRow << 1) | (readRow >> 7);
			break;
		case Mode::RAND:
			left |= random::get<bool>() << 7;
			right |= random::get<bool>();
			break;
		default:
			break;
		}

		for (int col = 0; col < 8; col++) {
			uint8_t leftBit = (left >> col) & 1;
			uint8_t thisBit = (readRow >> col) & 1;
			uint8_t rightBit = (right >> col) & 1;

			uint8_t tag = (leftBit << 2) | (thisBit << 1) | rightBit;
			uint8_t newBit = (rule >> tag) & 1;
			
			writeRow |= newBit << col;
		}
		rowBuffer[writeHead] = writeRow;
	}

	void GenerateReset(bool w) override {
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

		//rowBits = remove ? (rowBits & ~bitMask) : (rowBits | bitMask);
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
		// Output matrix is flipped when draw (right -> left, left <- right),
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
	void DrawRule(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, 0, "RULE", nullptr);
		char ruleString[5];
		snprintf(ruleString, sizeof(ruleString), "%4.3d", rule);	// cast rule to int?
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
		case Mode::RAND: 
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
		}
		else {
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
	}

	// Light function
	float GetModeValue() override {
		int modeIndex = static_cast<int>(mode); //const?
		return static_cast<float>(modeIndex) * numModesScaler;
	}

private:
	enum class Mode {
		WRAP,
		CLIP,
		RAND,
		MODE_LEN
	};

	Mode mode = Mode::WRAP;
	static constexpr int numModes = static_cast<int>(Mode::MODE_LEN);
	static constexpr float numModesScaler = 1.f / (static_cast<float>(numModes) - 1.f);

	uint8_t rule = 30;
	uint8_t seed = 8;
	int seedSelect = seed;
	bool randSeed = false;

	int prevOffset = 0;

	static constexpr float voltageScaler = 1.f / UINT8_MAX;
};

class LifeAlgoithm : public AlgorithmBase {
public:
	LifeAlgoithm() {
		// Define seeds.
		// Generic rand seeds as randSeed will override behaviour.
		/*
		seedValues[static_cast<int>(Seeds::MIRROR_RANDOM)]			= 0;
		seedValues[static_cast<int>(Seeds::HALF_RANDOM)]			= 0;
		seedValues[static_cast<int>(Seeds::RANDOM)]					= 0;
		// Spaceships
		seedValues[static_cast<int>(Seeds::GLIDER)]					= 0x382010000000ULL;
		seedValues[static_cast<int>(Seeds::LIGHTWEIGHT_SPACESHIP)]	= 0x1220223C00000000ULL;
		seedValues[static_cast<int>(Seeds::MIDDLEWEIGHT_SPACESHIP)] = 0x82240427C000000ULL;
		seedValues[static_cast<int>(Seeds::HEAVYEIGHT_SPACESHIP)]	= 0xC2140417E000000ULL;		// doesnt act like a spaceship, dont like that
		seedValues[static_cast<int>(Seeds::SHIP)]					= 0xE62543000000ULL;
		// Oscillator
		seedValues[static_cast<int>(Seeds::OCTAGON_II)]				= 0x1824428181422418ULL;
		seedValues[static_cast<int>(Seeds::FUMAROLE)]				= 0;
		seedValues[static_cast<int>(Seeds::GLIDER_BLOCK_CYCLE)]		= 0;
		seedValues[static_cast<int>(Seeds::BY_FLOPS)]				= 0;
		// Heptomino
		seedValues[static_cast<int>(Seeds::B_HEPTOMINO)]			= 0;
		seedValues[static_cast<int>(Seeds::C_HEPTOMINO)]			= 0;
		seedValues[static_cast<int>(Seeds::E_HEPTOMINO)]			= 0;
		seedValues[static_cast<int>(Seeds::F_HEPTOMINO)]			= 0;
		seedValues[static_cast<int>(Seeds::H_HEPTOMINO)]			= 0;
		// Coil
		seedValues[static_cast<int>(Seeds::CAP)]					= 0;
		// Still life
		seedValues[static_cast<int>(Seeds::DOT)]					= 0x8000000ULL;
		*/


		// Init seed.
		//seed = seedValues[static_cast<int>(seeds)];
		//frameBuffer[readHead] = seed;
		frameBuffer[readHead] = seeds[seedIndex].value;
	}

	void OutputMatrixStep() override {
		// Suppress behaviour.
	}

	void OutputMatrixPush(int o) override {
		// Push current matrix to output matrix.
		uint64_t tempMatrix = 0;
		for (int i = 0; i < 8; i++) {
			uint8_t row = (frameBuffer[readHead] >> (i * 8)) & 0xFFULL;
			tempMatrix |= uint64_t(ApplyOffset(row, o)) << (i * 8);
		}
		outputMatrix = tempMatrix;

		// Count the current number of living cells.
		population = __builtin_popcountll(outputMatrix);
	}

	void Generate() override {
		// Conway's Game of Life.
		// Base on efficent implementation by paperclip optimizer
		// and Michael Abrash's (Graphics Programmer's Black Book, Chapter 17) padding method. 
		
		uint64_t readMatrix = frameBuffer[readHead];
		uint64_t writeFrame = 0;

		// Eight matrix rows + top & bottom padding rows 
		std::array<uint8_t, 10> row{};

		// Fill rows
		for (int i = 1; i < 9; i++)
			row[i] = (readMatrix >> ((i - 1) * 8)) & 0xFFULL;

		// Fill top & bottom padding rows
		switch (mode) {
		case Mode::WRAP:
			row[0] = row[8];
			row[9] = row[1];
			break;
		case Mode::RAND:
			row[0] = random::get<uint8_t>();
			row[9] = random::get<uint8_t>();
			break;
		default:
			// Clip
			row[0] = 0;
			row[9] = 0;
			break;
		}
		
		std::array<uint8_t, 10> horizXor{};
		std::array<uint8_t, 10> horizAnd{};
		std::array<uint8_t, 10> partialXor{};
		std::array<uint8_t, 10> partialAnd{};

		for (int i = 0; i < 10; i++) {
			// Clip
			uint8_t left = row[i] >> 1;
			uint8_t right = row[i] << 1;
			
			switch (mode) {
			case Mode::WRAP:
				left = (row[i] >> 1) | (row[i] << 7);
				right = (row[i] << 1) | (row[i] >> 7);
				break;
			case Mode::RAND:
				left |= (random::get<bool>() << 7);
				right |= random::get<bool>();
				break;
			default:
				break;
			}
			
			horizXor[i] = left ^ right;
			horizAnd[i] = left & right;
			partialXor[i] = horizXor[i] ^ row[i];
			partialAnd[i] = horizAnd[i] | (horizXor[i] & row[i]);
		}
		
		// Parallel bit-wise addition
		std::array<uint8_t, 8>  sumBit0{};
		std::array<uint8_t, 8>  sumBit1{};
		std::array<uint8_t, 8>  sumBit2{};

		for (int i = 1; i <= 8; ++i) {
			int sumIndex = i - 1;
			sumBit0[sumIndex] = partialXor[i - 1] ^ horizXor[i] ^ partialXor[i + 1];
			uint8_t carry = ((partialXor[i - 1] | partialXor[i + 1]) & horizXor[i]) |
				(partialXor[i - 1] & partialXor[i + 1]);
			sumBit1[sumIndex] = partialAnd[i - 1] ^ horizAnd[i] ^ partialAnd[i + 1] ^ carry;
			sumBit2[sumIndex] = ((partialAnd[i - 1] | partialAnd[i + 1]) & (horizAnd[i] | carry)) |
				((partialAnd[i - 1] & partialAnd[i + 1]) | (horizAnd[i] & carry));
		}

		// Apply rule
		for (int i = 1; i < 9; i++) {
			int sumIndex = i - 1;

			uint8_t newRow = GetRule(sumBit0[sumIndex], sumBit1[sumIndex], sumBit2[sumIndex], row[i]);
			writeFrame |= (uint64_t(newRow) << (sumIndex * 8));
		}
		frameBuffer[writeHead] = writeFrame;
	}

	void GenerateReset(bool w) override {
		//
		uint64_t resetFrame = seeds[seedIndex].value;

		// Dynamic random seeds.
		switch (seedIndex) {
		case 0:
			// S random
			resetFrame = random::get<uint32_t>();
			break;
		case 1:
			// Half random
			resetFrame = 0;
			break;
		case 2:
			// True random
			resetFrame = random::get<uint64_t>();
			break;
		default:
			break;
		}

		size_t head = readHead;
		if (w)
			head = writeHead;

		frameBuffer[head] = resetFrame;
	}

	void Inject(bool a, bool w) override {
		int head = readHead;
		if (w)
			head = writeHead;
		uint64_t matrix = frameBuffer[head];

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
		frameBuffer[head] = matrix;
	}

	void RuleUpdate(int d, bool r) override {
		if (r) {
			rule = Rule::LIFE;
			return;
		}

		int ruleIndex = static_cast<int>(rule);
		ruleIndex = (ruleIndex + d + numRules) % numRules;
		rule = static_cast<Rule>(ruleIndex);
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
	void DrawRule(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, 0, "RULE", nullptr);
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
		case Mode::RAND:
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

	// Light function
	float GetModeValue() override {
		const int modeIndex = static_cast<int>(mode);
		return static_cast<float>(modeIndex) * numModesScaler;
	}

	// Internal algoithm specific functions
	uint8_t GetRule(uint8_t bit0, uint8_t bit1, uint8_t bit2, uint8_t row) {
		// Takes a three bit sum of alive cells per each row & the row itself, 
		// applies selected rule and returns the row generation.



		rules[ruleIndex].birth;
		rules[ruleIndex].survival;

		// old working code...

		// Bool for each alive count (0-7)
		// uint8_t alive0 = ~bit0 & ~bit1 & ~bit2;
		uint8_t alive1 = bit0 & ~bit1 & ~bit2;
		uint8_t alive2 = ~bit0 & bit1 & ~bit2;
		uint8_t alive3 = bit0 & bit1 & ~bit2;
		uint8_t alive4 = ~bit0 & ~bit1 & bit2;
		uint8_t alive5 = bit0 & ~bit1 & bit2;
		uint8_t alive6 = ~bit0 & bit1 & bit2;
		uint8_t alive7 = bit0 & bit1 & bit2;

		// Birth & survival condtions
		uint8_t birth = 0;
		uint8_t survival = 0;


		switch (rule) {
		case Rule::HIGH:
			// High Life (B36/S23) *
			birth = alive3 | alive6;
			survival = alive2 | alive3;
			break;
		case Rule::DUPE:
			// Replicator (B1357/S1357) *
			birth = alive1 | alive3 | alive5 | alive7;
			survival = alive1 | alive3 | alive5 | alive7;
			break;
		case Rule::R2X2:
			// 2x2 (B36/S125) *? maybe a bit boring
			birth = alive3 | alive6;
			survival = alive1 | alive2 | alive5;
			break;
		case Rule::R34:
			// 34 Life (B34/S34) **
			birth = alive3 | alive4;
			survival = alive3 | alive4;
			break;
		case Rule::WES:

			birth = alive1 | alive3;
			survival = alive3;
			break;
		case Rule::SEED:
			// Seeds (B2/S) ** got a glider I think!
			birth = alive2;
			survival = 0;
			break;
		default:
			// Game of Life (B3/S23) *
			birth = alive3;
			survival = alive2 | alive3;
			break;
		}
		return birth | (row & survival);
	}

private:
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

	enum class Mode {
		CLIP,
		WRAP,
		// BOTL	Klein bottle 
		// CROS Cross-surface
		// ORB	Sphere
		RAND,
		MODE_LEN
	};

	Mode mode = Mode::WRAP;
	static constexpr int numModes = static_cast<int>(Mode::MODE_LEN);
	static constexpr float numModesScaler = 1.f / (static_cast<float>(numModes) - 1.f);

	Rule rule = Rule::LIFE;
	static constexpr int numRules = static_cast<int>(Rule::RULE_LEN);

	struct Rule2 {
		char displayName[5];
		uint8_t birth;
		uint8_t survival;
		// TODO: hmmm this
		// Alive data (uint8_t, 00100010 = 1 and 5 alive in the hood
	};
	static constexpr int numRules2 = 3;	// 10?
	std::array<Rule2, numRules2> rules { {
		{ "LIFE", 0x08, 0x0C },		// Conway's game of life B3/S23  0x08, 0x0C 
		{ "DUPE", 0xAA, 0xAA },		// Replicator B1357/S1357
		{ "SEED", 0x04, 0 }
	} };
	static constexpr int ruleDefault = 0;
	int ruleIndex = seedDefault;


	struct Seed {
		char displayName[5];
		uint64_t value;
	};
	static constexpr int numSeeds = 28;
	std::array<Seed, numSeeds> seeds { {
		// Patterns from the Life Lexicon.
		// Random - generic seeds as values are dynamic.
		{ "SRND", 0x0ULL },					// Symmetrical random
		{ "2RND", 0x0ULL },					// Half random		could be quarter random
		{ " RND", 0x0ULL },					// True random 
		// Spaceship.
		{ "FLYR", 0x382010000000ULL },		// Glider 
		{ "LWSS", 0x1220223C00000000ULL },	// Lightweight spaceship 
		{ "MWSS", 0x82240427C000000ULL },	// Middleweight spaceship 
		// Heptomino.
		{ "BHEP", 0x2C3810000000ULL },		// B-heptomino 
		{ "CHEP", 0x1C38100000ULL },		// C-heptomino 
		{ "EHEP", 0x1C30180000ULL },		// E-heptomino 
		{ "FHEP", 0x3010101C0000ULL },		// F-heptomino
		{ "HHEP", 0x30101C080000ULL },		// H-heptomino
		// Misc.
		{ " BIT", 0x8000000ULL },			// Lonely living cell
		{ "BLOK", 0x1818000000ULL },		// Still life 4x4
		{ "BUNY", 0x4772200000ULL },		// Rabbits
		{ " B&G", 0x30280C000000ULL },		// Block and glider
		{ "CENT", 0xC38100000ULL },			// Century
		{ "CHEL", 0x302C0414180000ULL },	// Herschel descendant
		{ " CUP", 0xC1410D0D2161030ULL },	// Cuphook
		{ "FIG8", 0x7070700E0E0E00ULL },	// Figure 8
		{ "FUMA", 0x1842424224A5C3ULL },	// Fumarole
		{ "G-BC", 0x1818000024A56600ULL },	// Glider-block cycle
		{ "HAND", 0xC1634180000ULL },		// Handshake
		{ "NSEP", 0x70141E000000ULL },		// Nonomino switch engine predecessor
		{ "PARV", 0xE1220400000ULL },		// Multum in parvo
		{ "SENG", 0x2840240E0000ULL },		// Switch engine
		{ "STEP", 0xC1830000000ULL },		// Stairstep hexomino
		{ "WING", 0x1824140C0000ULL },		// Wing
		{ "WORD", 0x24A56600001818ULL }		// Rephaser
	} };
	static constexpr int seedDefault = 3;
	int seedIndex = seedDefault;

	int population = 0;
	int prevPopulation = 0;

	static constexpr float xVoltageScaler = 1.f / 64.f;
	static constexpr float yVoltageScaler = 1.f / UINT64_MAX;
};

// Dispatcher 
class WolframEngine {
public:
	WolframEngine() {
		algorithms[0] = &wolf;
		algorithms[1] = &life;

		// Default alogrithm
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

	// Common algoithm specific functions
	void setReadHead(int readHead) { activeAlogrithm->SetReadHead(readHead); }
	void setWriteHead(int writeHead) { activeAlogrithm->SetReadHead(writeHead); }
	void advanceHeads(int sequenceLength) { activeAlogrithm->AdvanceHeads(sequenceLength); }
	uint64_t getOutputMatrix() { return activeAlogrithm->GetOutputMatrix(); }

	// Algoithm specific functions
	void outputMatrixStep() { activeAlogrithm->OutputMatrixStep(); }
	void outputMatrixPush() { activeAlogrithm->OutputMatrixPush(offset); }
	void generate() { activeAlogrithm->Generate(); }
	void generateReset(bool write = false) { activeAlogrithm->GenerateReset(write); }
	void inject(bool add, bool write = false) { activeAlogrithm->Inject(add, write); }

	void ruleUpdate(int delta, bool reset) { activeAlogrithm->RuleUpdate(delta, reset); }
	void seedUpdate(int delta, bool reset) { activeAlogrithm->SeedUpdate(delta, reset); }
	void modeUpdate(int delta, bool reset) { activeAlogrithm->ModeUpdate(delta, reset); }

	float getXVoltage() { return activeAlogrithm->GetXVoltage(); }
	float getYVoltage() { return activeAlogrithm->GetYVoltage(); }
	bool getXGate() { return activeAlogrithm->GetXGate(); }
	bool getYGate() { return activeAlogrithm->GetYGate(); }

	// Drawing functions
	void drawRule(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawRule(vg, fontSize); }
	void drawModeMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawModeMenu(vg, fontSize); }
	void drawSeedMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawSeedMenu(vg, fontSize); }

	// Light function
	float getModeValue() { return activeAlogrithm->GetModeValue(); }

private:
	WolfAlgoithm wolf;
	LifeAlgoithm life;

	static constexpr int MAX_ALGORITHMS = 2;
	int algorithmIndex = 1;
	std::array<AlgorithmBase*, MAX_ALGORITHMS> algorithms;
	AlgorithmBase* activeAlogrithm;

	int offset = 0;
};