#pragma once
#include "plugin.hpp"
#include <array>
#include <limits>

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
	
	bool randSeed = false;
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
		outputMatrix &= ~0xFFULL;	// Clear first row
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

	void RuleUpdate(int d, bool r) override {
		if (r) {
			rule = 30;
		}
		else {
			rule = static_cast<uint8_t>(rule + d);
		}
		
	}

	void SeedUpdate(int d, bool r) override {
		if (r) {
			seed = 8;
			seedSelect = seed;
			randSeed = false;
		}
		else {
			// Seed options are 256 + 1 (RAND) 
			seedSelect += d;
			if (seedSelect > 256)
				seedSelect -= 257;
			else if (seedSelect < 0)
				seedSelect += 257;

			randSeed = (seedSelect == 256);

			if (!randSeed) {
				seed = static_cast<uint8_t>(seedSelect);
			}
		}

	}

	void ModeUpdate(int d, bool r) override {
		if (r) {
			mode = Mode::WRAP;
		}
		else {
			int modeIndex = static_cast<int>(mode);
			modeIndex = (modeIndex + d + numModes) % numModes;
			mode = static_cast<Mode>(modeIndex);
		}

	}

	float GetXVoltage() override {
		// Returns bottom row of Ouput Matrix as voltage (0-10V).		
		uint8_t firstRow = outputMatrix & 0xFF;
		return firstRow * voltageScaler * 10.f;
	}

	float GetYVoltage() override {
		// Returns right column of Ouput Matrix as voltage (0-10V).
		//uint8_t col = 0;
		//for (int i = 0; i < 8; i++) {
		//	uint8_t row = GetOutputMatrixRow(i);
		//	col |= ((row & 0x01) << (7 - i));
		//}
		//return col * voltageScaler * 10.f;

		//return right col of m * voltageScaler * 10.f;
		return 0;
	}

	bool GetXGate() override {
		// Returns bottom left cell state of Ouput Matrix.
		//return (GetOutputMatrixRow(0) >> 7) & 1;
		return 0;
	}

	bool GetYGate() override {
		// Returns top right cell state of Ouput Matrix.
		//return GetOutputMatrixRow(7) & 1;
		return 0;
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

	int prevOffset = 0;

	static constexpr float voltageScaler = 1.f / std::numeric_limits<uint8_t>::max();
};

class LifeAlgoithm : public AlgorithmBase {
public:
	LifeAlgoithm() {
		
		// Define seeds.
		// Generic rand seed as randSeed will override behaviour.
		std::array<Vec, 1> rand{ Vec(0, 0) };
		seedValues[static_cast<int>(Seeds::RAND)] = GetSeed(rand);
		// Single dot
		std::array<Vec, 1> dot { Vec(3, 3) };
		seedValues[static_cast<int>(Seeds::DOT)] = GetSeed(dot);
		// 2x2 block
		std::array<Vec, 4> blok { Vec(3, 3), Vec(3, 4), Vec(4, 3), Vec(4, 4) };
		seedValues[static_cast<int>(Seeds::BLOK)] = GetSeed(blok);
		// Glider
		std::array<Vec, 5> flyr { Vec(3, 2), Vec(4, 3), Vec(3, 4), Vec(4, 4), Vec(2, 4) };
		seedValues[static_cast<int>(Seeds::FLYR)] = GetSeed(flyr);
		
		// Init seed.
		seed = seedValues[static_cast<int>(seeds)];
		frameBuffer[readHead] = seed;
	}

	void OutputMatrixStep() override {
		// Suppress behaviour.
	}

	void OutputMatrixPush(int o) override {
		// Push current matrix to output matrix.
		uint64_t tempMatrix = 0;
		for (int i = 0; i < 8; i++) {
			uint8_t row = (frameBuffer[readHead] >> (i * 8)) & 0xFF;
			tempMatrix |= uint64_t(ApplyOffset(row, o)) << (i * 8);
		}
		outputMatrix = tempMatrix;
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
		for (int i = 1; i < 9; i++) {
			row[i] = (readMatrix >> ((i - 1) * 8)) & 0xFF;
		}

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

		// Top row
		{
			int i = 1;
			int sumIndex = i - 1;
			sumBit0[sumIndex] = horizXor[i] ^ partialXor[i + 1];
			sumBit1[sumIndex] = (horizAnd[i] ^ partialAnd[i + 1]) ^ (horizXor[i] & partialXor[i + 1]);
			sumBit2[sumIndex] = (horizAnd[i] & partialAnd[i + 1]) | ((horizAnd[i] ^ partialAnd[i + 1]) & (horizXor[i] & partialXor[i + 1]));
		}

		// Middle rows
		for (int i = 2; i <= 7; i++) {
			int sumIndex = i - 1;
			sumBit0[sumIndex] = partialXor[i - 1] ^ horizXor[i] ^ partialXor[i + 1];
			uint8_t carryMid =
				((partialXor[i - 1] | partialXor[i + 1]) & horizXor[i]) |
				(partialXor[i - 1] & partialXor[i + 1]);
			sumBit1[sumIndex] =
				partialAnd[i - 1] ^ horizAnd[i] ^ partialAnd[i + 1] ^ carryMid;
			sumBit2[sumIndex] =
				((partialAnd[i - 1] | partialAnd[i + 1]) & (horizAnd[i] | carryMid)) |
				((partialAnd[i - 1] & partialAnd[i + 1]) | (horizAnd[i] & carryMid));
		}

		// Bottom row
		{
			int i = 8;
			int sumIndex = i - 1;
			sumBit0[sumIndex] = partialXor[i - 1] ^ horizXor[i - 1];
			sumBit1[sumIndex] = (partialAnd[i - 1] ^ horizAnd[i - 1]) ^ (partialXor[i - 1] & horizXor[i - 1]);
			sumBit2[sumIndex] = (partialAnd[i - 1] & horizAnd[i - 1]) |
				((partialAnd[i - 1] ^ horizAnd[i - 1]) & (partialXor[i - 1] & horizXor[i - 1]));
		}
				
		// Apply rule
		for (int i = 1; i < 9; i++) {
			int sumIndex = i - 1;

			uint8_t newRow = GetRule(sumBit0[sumIndex], sumBit1[sumIndex], sumBit2[sumIndex], row[i]);
			//uint8_t newRow = ~sumBit2[sumIndex] & sumBit1[sumIndex] & (sumBit0[sumIndex] | row[i]);
			writeFrame |= (uint64_t(newRow) << (sumIndex * 8));
		}
		frameBuffer[writeHead] = writeFrame;
	}

	void GenerateReset(bool w) override {
		uint64_t resetFrame = randSeed ? random::get<uint64_t>() : seed;
		size_t head = readHead;
		if (w) {
			head = writeHead;
		}
		frameBuffer[head] = resetFrame;
	}

	void RuleUpdate(int d, bool r) override {
		if (r) {
			rule = Rule::LIFE;
		}
		else {
			int ruleIndex = static_cast<int>(rule);
			ruleIndex = (ruleIndex + d + numRules) % numRules;
			rule = static_cast<Rule>(ruleIndex);
		}
	}

	void SeedUpdate(int d, bool r) override {
		//  use rule switch style to change seed
		if (r) {
			seeds = Seeds::DOT;
			seed = seedValues[static_cast<int>(seeds)];
			randSeed = false;
		}
		else {
			int seedIndex = static_cast<int>(seeds);
			seedIndex = (seedIndex + d + numSeeds) % numSeeds;
			seeds = static_cast<Seeds>(seedIndex);

			seed = seedValues[static_cast<int>(seeds)];
			randSeed = false;

			if (seeds == Seeds::RAND) {
				randSeed = true;
			}
		}
	}

	void ModeUpdate(int d, bool r) override {
		if (r) {
			mode = Mode::WRAP;
		}
		else {
			int modeIndex = static_cast<int>(mode);
			modeIndex = (modeIndex + d + numModes) % numModes;
			mode = static_cast<Mode>(modeIndex);
		}

	}

	float GetXVoltage() override {
		// TODO:

		// number of alive cells / 64;
		// xVoltageScaler
		return 0;
	}

	float GetYVoltage() override {
		// Could return the ampount of change 
		// Gate could return if the value has increased
		// need sequenceLength...
		
		//int prevReadHead = (readHead - 1) % sequenceLength;
		//float change = frameBuffer[readHead] - frameBuffer[prevReadHead];

		// outputMatrix (int) as voltage
		float normalizedMatrix = outputMatrix * yVoltageScaler;
		return normalizedMatrix * 10.f;
	}

	bool GetXGate() override {
		return false;
	}

	bool GetYGate() override {
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
		switch (seeds) {
		case Seeds::DOT:
			nvgText(vg, 0, fs * 2, " DOT", nullptr);
			break;
		case Seeds::BLOK:
			nvgText(vg, 0, fs * 2, "BLOK", nullptr);
			break;
		case Seeds::FLYR:
			nvgText(vg, 0, fs * 2, "FLYR", nullptr);
			break;
		case Seeds::RAND:
			nvgText(vg, 0, fs * 2, "RAND", nullptr);
			break;
		default:
			break;
		}
	}

	// Light function
	float GetModeValue() override {
		int modeIndex = static_cast<int>(mode); //const?
		return static_cast<float>(modeIndex) * numModesScaler;
	}

	// Internal algoithm specific functions
	uint8_t GetRule(uint8_t bit0, uint8_t bit1, uint8_t bit2, uint8_t row) {
		// Takes a three bit sum of alive cells per eacg row & the row itself, 
		// applies selected rule and returns the row generation.

		// Bool for each alive count (0-7)
		uint8_t alive0 = ~bit0 & ~bit1 & ~bit2;
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
		case Rule::SEED:
			// Seeds (B2/S) ** got a glider I think!
			birth = alive2;
			survival = 0; //?
			break;
		default:
			// Game of Life (B3/S23) *
			birth = alive3;
			survival = alive2 | alive3;
			break;
		}

		return birth | (row & survival);;
	}

	template<size_t SIZE>
	uint64_t GetSeed(std::array<rack::math::Vec, SIZE>& cellCoordinates) {
		// 
		uint64_t m = 0;

		for (auto& coordinates : cellCoordinates) {
			int x = static_cast<int>(coordinates.x);
			int y = static_cast<int>(coordinates.y);

			// Check bounds
			if (x < 0 || x >= 8 || y < 0 || y >= 8)
				continue;

			int index = y * 8 + x;

			m |= (uint64_t(1) << index);
		}
		return m;
	}

private:
	enum class Rule {
		LIFE,	// cool
		HIGH,	// cool (bit like life)
		DUPE,	// cool
		R2X2,	// bit boring 
		R34,	// v cool
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

	enum class Seeds {
		RAND,
		DOT,		
		BLOK,
		FLYR,
		SEED_LEN	
	};

	enum class Mode {
		WRAP,
		CLIP,
		RAND,
		MODE_LEN
	};

	Mode mode = Mode::WRAP;
	static constexpr int numModes = static_cast<int>(Mode::MODE_LEN);
	static constexpr float numModesScaler = 1.f / (static_cast<float>(numModes) - 1.f);

	Rule rule = Rule::LIFE;
	static constexpr int numRules = static_cast<int>(Rule::RULE_LEN);
	
	Seeds seeds = Seeds::DOT;
	static constexpr int numSeeds = static_cast<int>(Seeds::SEED_LEN);
	std::array<uint64_t, numSeeds> seedValues{};
	uint64_t seed = 0;

	const float xVoltageScaler = 1.f / 64.f;
	const float yVoltageScaler = 1.f / std::numeric_limits<uint64_t>::max();
};

// Dispatcher 
class WEngine {
public:
	WEngine() {
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