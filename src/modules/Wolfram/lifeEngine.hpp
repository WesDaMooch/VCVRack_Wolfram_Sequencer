#pragma once
#include "baseEngine.hpp"

class LifeEngine : public BaseEngine {
public:
	LifeEngine();

	void updateDisplay(bool advance, int length = 8) override;
	void updateMenuParams(const EngineMenuParams& p) override;

	void process(const EngineCoreParams& p,
		float* xOut, float* yOut,
		bool* xPulse, bool* yPulse,
		float* modeLED) override;

	void reset() override;

	// Setters
	void setBufferFrame(uint64_t newFrame, int index, 
		bool setDisplayMatrix = false) override;

	void setRuleSelect(int newRule) override;
	void setRuleCv(float newRuleCv) override;
	void setSeed(int newSeed) override;
	void setMode(int newMode) override;

	// Getters
	uint64_t getBufferFrame(int index, 
		bool getDisplayMatrix = false,
		bool getDisplayMatrixSave = false) override;

	int getRuleSelect() override;
	int getSeed() override;
	int getMode() override;
	void getRuleActiveLabel(char out[5]) override;
	void getRuleSelectLabel(char out[5]) override;
	void getSeedLabel(char out[5]) override;
	void getModeLabel(char out[5]) override;

protected:
	struct Rule {
		char label[5];
		uint32_t value;
	};

	struct Seed {
		char label[5];
		uint64_t value;
	};

	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};

	static constexpr int NUM_MODES = 4;
	static const char modeLabel[NUM_MODES][5];
	static constexpr int modeDefault = 1;
	int modeIndex = modeDefault;

	static constexpr  int NUM_RULES = 30;
	static const std::array<Rule, NUM_RULES> rule;
	static constexpr  int ruleDefault = 11;
	int ruleSelect = ruleDefault;
	int ruleCv = 0;
	int ruleIndex = 0;

	static constexpr int NUM_SEEDS = 30;
	static const std::array<Seed, NUM_SEEDS> seed;
	static constexpr int seedDefault = 9;
	int seedIndex = seedDefault;

	int population = 0;
	int prevPopulation = 0;
	uint64_t prevOutputMatrix = 0;
	bool prevYbit = false;

	static constexpr float xVoltageScaler = 1.f / 64.f;
	static constexpr float yVoltageScaler = 1.f / UINT64_MAX;
	static constexpr float modesScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);

	void inject(int inject, bool sync) override;
	void onRuleChange() override;

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

	static uint8_t reverseRow(uint8_t row);
	void getHorizontalNeighbours(uint8_t row, uint8_t& west, uint8_t& east);
};