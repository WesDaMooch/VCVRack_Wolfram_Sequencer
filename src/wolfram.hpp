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

	// TODO: bundle update and reseters together?
	virtual void OutputMatrixStep() = 0;
	virtual void OutputMatrixPush() = 0;
	virtual void Generate() = 0;
	virtual void GenerateReset() = 0;

	virtual void RuleUpdate(int d, bool r) = 0;
	virtual void SeedUpdate(int d, bool r) = 0;
	virtual void ModeUpdate(int d, bool r) = 0;

	// Output functions
	virtual float GetXVoltage() = 0;
	virtual float GetYVoltage() = 0;

	// Drawing functions
	virtual void DrawRule(NVGcontext* vg, float fs) = 0;
	virtual void DrawSeedMenu(NVGcontext* vg, float fs) = 0;
	virtual void DrawModeMenu(NVGcontext* vg, float fs) = 0;

	// Common functions
	void SetReadHead(int r) {
		readHead = r;
	}

	void SetWriteHead(int w) {
		writeHead = w;
	}

	void AdvanceHeads(int s) {
		// Advance read and write heads
		readHead = writeHead;
		writeHead = (writeHead + 1) % s;
	}
	
	void SetOffset(int o) {
		offset = clamp(o, -4, 4);
	}

	uint8_t GetOutputMatrixRow(int i) {
		uint8_t row = (outputMatrix >> (i * 8)) & 0xFF;
		// Apply offset
		int shift = offset;
		if (shift < 0) {
			shift = -shift;
			row = (row << shift) | (row >> (8 - shift));
		}
		else if (shift > 0) {
			row = (row >> shift) | (row << (8 - shift));
		}
		return row;
	}

	uint64_t GetOutputMatrix() {
		uint64_t matrix = 0;
		for (int i = 0; i < 8; i++) {
			uint8_t row = GetOutputMatrixRow(i);
			//row &= 0xFF; // mask to ensure we don't spill extra bits?
			matrix |= static_cast<uint64_t>(row) << (i * 8);
		}
		return matrix;
	}

	/*
	bool GetXGate() {
		//x = row 
		//bool xGate = (x >> 7) & 1;
		return 0;
	}
	bool GetYGate() {
		return 0;
	}
	*/


protected:
	static constexpr size_t MAX_SEQUENCE_LENGTH = 64;
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer {};
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> frameBuffer {};
	uint64_t outputMatrix = 0;

	int readHead = 0;
	int writeHead = 1;
	
	int offset = 0;
	bool randSeed = false;
};

class WolfAlgoithm : public AlgorithmBase {
public:
	WolfAlgoithm() {
		// Init seed
		rowBuffer[readHead] = seed;
		OutputMatrixPush();
		
	}

	void OutputMatrixStep() override {
		// Shift matrix along
		outputMatrix <<= 8;
	}

	void OutputMatrixPush() override {
		// Push row to output matrix
		outputMatrix &= ~0xFFULL;		// Clear first row
		outputMatrix |= static_cast<uint64_t>(rowBuffer[readHead]);
	}

	void Generate() override {
		// One Dimensional Cellular Automata
		for (int column = 0; column < 8; column++) {

			uint8_t readRow = rowBuffer[readHead];
			//uint8_t writeRow = 0;

			int left = column - 1;
			int right = column + 1;
			int leftIndex = left;
			int rightIndex = right;

			// Wrap
			if (left < 0) { leftIndex = 7; }
			if (right > 7) { rightIndex = 0; }

			int leftCell = (readRow >> leftIndex) & 1;
			int cell = (readRow >> column) & 1;
			int rightCell = (readRow >> rightIndex) & 1;

			switch (mode) {
			case Mode::CLIP:
				if (left < 0) { leftCell = 0; }
				if (right > 7) { rightCell = 0; }
				break;
			case Mode::RAND:
				if (left < 0) { leftCell = random::get<bool>(); }
				if (right > 7) { rightCell = random::get<bool>(); }
				break;
			default:
				break;
			}

			int tag = 7 - ((leftCell << 2) | (cell << 1) | rightCell);

			bool ruleBit = (rule >> (7 - tag)) & 1;
			if (ruleBit) {
				rowBuffer[writeHead] |= (1 << column);
			}
			else {
				rowBuffer[writeHead] &= ~(1 << column);
			}
			//rowBuffer[writeHead] = writeRow;
		}

	}

	void GenerateReset() override {
		uint8_t resetRow = randSeed ? random::get<uint8_t>() : seed;
		rowBuffer[readHead] = resetRow;
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
			const int numModes = static_cast<int>(Mode::MODE_LEN);
			int modeIndex = static_cast<int>(mode);
			modeIndex = (modeIndex + d + numModes) % numModes;
			mode = static_cast<Mode>(modeIndex);
		}

	}

	float GetXVoltage() override {
		//rowIndex = clamp(rowIndex, 0, 7);
		//return applyOffset((outputMatrix >> (rowIndex * 8)) & 0xFF);
		//uint8_t x = getOutputGridRow();
		
		return GetOutputMatrixRow(0) * voltageScaler * 10.f;
	}

	float GetYVoltage() override {

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

private:
	enum class Mode {
		WRAP,
		CLIP,
		RAND,
		MODE_LEN
	};

	Mode mode = Mode::WRAP;

	uint8_t rule = 30;
	uint8_t seed = 8;
	int seedSelect = seed;

	const float voltageScaler = 1.f / std::numeric_limits<uint8_t>::max();
};

class LifeAlgoithm : public AlgorithmBase {
public:
	LifeAlgoithm() {
		// Init seed
		frameBuffer[readHead] = seed;
		OutputMatrixPush();
	}

	void OutputMatrixStep() override {
		// Suppress behaviour
	}

	void OutputMatrixPush() override {
		// Push frame to output matrix
		outputMatrix = frameBuffer[readHead];
	}

	void Generate() override {
		// Conways Game of Life.
		uint64_t readFrame = frameBuffer[readHead];

		for (int row = 0; row < 8; row++) {
			for (int column = 0; column < 8; column++) {
				bool writeCell = false;
				int aliveCount = 0;

				int bitIndex = row * 8 + column;
				bool cell = (readFrame >> bitIndex) & 1ULL;

				// Clip
				if (row > 0) {
					aliveCount += (readFrame >> (bitIndex - 8)) & 1ULL;						// Up
					if (column > 0) aliveCount += (readFrame >> (bitIndex - 8 - 1)) & 1ULL;	// Up-left
					if (column < 7) aliveCount += (readFrame >> (bitIndex - 8 + 1)) & 1ULL; // Up-right
				}
				if (row < 7) {
					aliveCount += (readFrame >> (bitIndex + 8)) & 1ULL;						// Down
					if (column > 0) aliveCount += (readFrame >> (bitIndex + 8 - 1)) & 1ULL; // Down-left
					if (column < 7) aliveCount += (readFrame >> (bitIndex + 8 + 1)) & 1ULL; // Down-right
				}
				if (column > 0)
					aliveCount += (readFrame >> (bitIndex - 1)) & 1ULL;						// Left
				if (column < 7)
					aliveCount += (readFrame >> (bitIndex + 1)) & 1ULL;						// Right

				// Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction
				// Any live cell with two or three live neighbours lives on to the next generation
				// Any live cell with fewer than two live neighbours dies, as if by underpopulation
				// Any live cell with more than three live neighbours dies, as if by overpopulation
				
				//if (cell && (aliveCount == 2 || aliveCount == 3)) { writeCell = true; }
				//else if (!cell && aliveCount == 3) { writeCell = true; }

				if (cell && (aliveCount == 2 || aliveCount == 3)) { writeCell = true; }
				else if (!cell && aliveCount == 3) { writeCell = true; }

				frameBuffer[writeHead] &= ~(1ULL << bitIndex);
				frameBuffer[writeHead] |= (uint64_t(writeCell) << bitIndex);
			}
		}
	}

	void GenerateReset() override {
		uint64_t resetFrame = randSeed ? random::get<uint64_t>() : seed;
		frameBuffer[readHead] = resetFrame;
	}

	void RuleUpdate(int d, bool r) override {
		if (r) {
			//rule = 30;
		}
		else {
			//rule = static_cast<uint8_t>(rule + d);
		}
		
	}

	void SeedUpdate(int d, bool r) override {
		if (r) {

			randSeed = false;
		}
		else {

		}
	}

	void ModeUpdate(int d, bool r) override {
		if (r) {
			mode = Mode::WRAP;
		}
		else {
			const int numModes = static_cast<int>(Mode::MODE_LEN);
			int modeIndex = static_cast<int>(mode);
			modeIndex = (modeIndex + d + numModes) % numModes;
			mode = static_cast<Mode>(modeIndex);
		}

	}

	float GetXVoltage() override {

		//return row * voltageScaler * 10.f;
		return outputMatrix * voltageScaler * 10.f;
	}

	float GetYVoltage() override {

		return 0;
	}

	// Drawing functions
	void DrawRule(NVGcontext* vg, float fs) override {
		nvgText(vg, 0, 0, "RULE", nullptr);
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
	}

private:
	enum class Mode {
		WRAP,
		CLIP,
		RAND,
		MODE_LEN
	};

	Mode mode = Mode::WRAP;

	uint64_t seed = 123456;

	const float voltageScaler = 1.f / std::numeric_limits<uint64_t>::max();
};

// Dispatcher 
class WEngine {
public:
	WEngine() {
		algorithms[0] = &wolf;
		algorithms[1] = &life;

		// Default alogrithm
		activeAlogrithm = algorithms[algorithmIndex];
	}
	
	void algoithmUpdate(int delta) {
		algorithmIndex = (algorithmIndex + delta + MAX_ALGORITHMS) % MAX_ALGORITHMS;
		activeAlogrithm = algorithms[algorithmIndex];
	}

	void algoithmReset() {
		algorithmIndex = 0;
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

	// Algoithm functions
	void outputMatrixStep() { activeAlogrithm->OutputMatrixStep(); }
	void outputMatrixPush() { activeAlogrithm->OutputMatrixPush(); }
	void generate() { activeAlogrithm->Generate(); }
	void generateReset() { activeAlogrithm->GenerateReset(); }

	void ruleUpdate(int delta, bool reset) { activeAlogrithm->RuleUpdate(delta, reset); }
	void seedUpdate(int delta, bool reset) { activeAlogrithm->SeedUpdate(delta, reset); }
	void modeUpdate(int delta, bool reset) { activeAlogrithm->ModeUpdate(delta, reset); }

	float getXVoltage() { return activeAlogrithm->GetXVoltage(); }
	float getYVoltage() { return activeAlogrithm->GetYVoltage(); }

	// Drawing functions
	void drawRule(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawRule(vg, fontSize); }
	void drawModeMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawModeMenu(vg, fontSize); }
	void drawSeedMenu(NVGcontext* vg, float fontSize) { activeAlogrithm->DrawSeedMenu(vg, fontSize); }

	// Common functions
	void setReadHead(int readHead) { activeAlogrithm->SetReadHead(readHead); }
	void setWriteHead(int writeHead) { activeAlogrithm->SetReadHead(writeHead); }
	void advanceHeads(int sequenceLength) { activeAlogrithm->AdvanceHeads(sequenceLength); }
	void setOffset(int offset) { activeAlogrithm->SetOffset(offset); }
	uint8_t getOutputMatrixRow(int index) { return activeAlogrithm->GetOutputMatrixRow(index); }
	uint64_t getOutputMatrix() { return activeAlogrithm->GetOutputMatrix(); }
	
	//bool getXGate() { return activeAlogrithm->GetXGate(); }
	//bool getYGate() { return activeAlogrithm->GetYGate(); }

private:
	WolfAlgoithm wolf;
	LifeAlgoithm life;

	int algorithmIndex = 0;

	static constexpr int MAX_ALGORITHMS = 2;
	std::array<AlgorithmBase*, MAX_ALGORITHMS> algorithms;
	AlgorithmBase* activeAlogrithm;

	// uint64_t outputMatrix = 0 here?
	// int offset = 0 here?
};


/*
struct WolframEngine;

// Each algorithm's function signature
using GetCellFunction = bool (*)(WolframEngine&, int x, int y);
//using GetRowFunction = void (*)(WolframEngine&, int x, int y);
//using GetFrameFunction = void (*)(WolframEngine&, int x, int y);
using GenerateFunction = void (*)(WolframEngine&, int readHead, int writeHead);
using GenerateResetFunction = void (*)(WolframEngine&, int readHead);
using PushToOutputMatixFunction = void (*)(WolframEngine&, int readHead);
using RuleUpdateFunction = void (*)(WolframEngine&, int delta);
using RuleResetFunction = void (*)(WolframEngine&);
// InitSeedFunction??
using SeedUpdateFunction = void (*)(WolframEngine&, int delta);
using SeedResetFunction = void (*)(WolframEngine&);
//using ModeUpdateFunction = void (*)(WolframEngine&, int delta);
//using ModeResetFunction = void (*)(WolframEngine&);

static bool wolfGetCell(WolframEngine&, int, int);
static void wolfGenerate(WolframEngine&, int, int);
static void wolfGenerateReset(WolframEngine&, int);
static void wolfPushToOutputMatrix(WolframEngine&, int);
static void wolfRuleUpdate(WolframEngine&, int);
static void wolfRuleReset(WolframEngine&);
static void wolfSeedUpdate(WolframEngine&, int);
static void wolfSeedReset(WolframEngine&);

static bool lifeGetCell(WolframEngine&, int, int);
static void lifeGenerate(WolframEngine&, int, int);
static void lifeGenerateReset(WolframEngine&, int);
static void lifePushToOutputMatrix(WolframEngine&, int);
static void lifeRuleUpdate(WolframEngine&, int);
static void lifeRuleReset(WolframEngine&);
static void lifeSeedUpdate(WolframEngine&, int);
static void lifeSeedReset(WolframEngine&);


struct WolframEngine {
	enum class Algorithm { WOLF, LIFE };
	enum class Mode { WRAP, CLIP, RAND };

	Algorithm algorithm = Algorithm::WOLF;
	Mode mode = Mode::WRAP;

	uint64_t outputMatrix = 0;

	static constexpr size_t MAX_SEQUENCE_LENGTH = 64;
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> oneDimensionalBuffer{};
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> twoDimensionalBuffer{};

	bool randSeed = false;

	// Wolf variables
	uint8_t ruleWolf = 30;
	uint8_t seedWolf = 8;
	int seedSelectWolf = seedWolf;

	// Life variables

	// Function pointers
	GetCellFunction getCellFunction = nullptr;
	GenerateFunction generateFunction = nullptr;
	GenerateResetFunction generateResetFunction = nullptr;
	PushToOutputMatixFunction pushToOutputMatixFunction = nullptr;
	RuleUpdateFunction ruleUpdateFunction = nullptr;
	RuleResetFunction ruleResetFunction = nullptr;
	SeedUpdateFunction seedUpdateFunction = nullptr;
	SeedResetFunction seedResetFunction = nullptr;

	// Fast inline dispatch
	inline bool getCell(int x, int y) { return getCellFunction(*this, x, y); }
	inline void generate(int r, int w) { generateFunction(*this, r, w); }
	inline void generateReset(int r) { generateResetFunction(*this, r); }
	inline void pushToOutputMatrix(int r) { pushToOutputMatixFunction(*this, r); }
	inline void ruleUpdate(int d) { ruleUpdateFunction(*this, d); }
	inline void ruleReset() { ruleResetFunction(*this); }
	inline void seedUpdate(int d) { seedUpdateFunction(*this, d); }
	inline void seedReset() { seedResetFunction(*this); }

	void setAlgorithm(Algorithm algo) {
		algorithm = algo;
		switch (algo) {
		case Algorithm::WOLF:
			getCellFunction = wolfGetCell;
			generateFunction = wolfGenerate;
			generateResetFunction = wolfGenerateReset;
			pushToOutputMatixFunction = wolfPushToOutputMatrix;
			ruleUpdateFunction = wolfRuleUpdate;
			ruleResetFunction = wolfRuleReset;
			seedUpdateFunction = wolfSeedUpdate;
			seedResetFunction = wolfSeedReset;
			break;
		case Algorithm::LIFE:
			getCellFunction = lifeGetCell;
			generateFunction = lifeGenerate;
			generateResetFunction = lifeGenerateReset;
			pushToOutputMatixFunction = lifePushToOutputMatrix;
			ruleUpdateFunction = lifeRuleUpdate;
			ruleResetFunction = lifeRuleReset;
			seedUpdateFunction = lifeSeedUpdate;
			seedResetFunction = lifeSeedReset;
			break;
		}
	}
};
*/

