#pragma once
#include "baseEngine.hpp"

class LifeEngine : public BaseEngine {
protected:
	struct Rule {
		char name[5];
		uint32_t value;
	};

	struct Seed {
		char name[5];
		uint64_t value;
	};

	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};

	static constexpr int NUM_MODES = 4;
	static const char modeNames[NUM_MODES][5];
	static constexpr int modeDefault = 1;
	int modeIndex = modeDefault;
	
	static const int NUM_RULES = 30;
	static const std::array<Rule, NUM_RULES> rule;
	static const int ruleDefault = 11;
	int ruleSelect = ruleDefault;
	int ruleCV = 0;
	int ruleIndex = 0;

	static const int NUM_SEEDS = 30;
	std::array<Seed, NUM_SEEDS> seed;
	static const int seedDefault = 9;
	int seedIndex = seedDefault;

	// TODO: could some of these be static in a function
	int population = 0;
	int prevPopulation = 0;
	uint64_t prevOutputMatrix = 0;
	bool prevYbit = false;

	static constexpr float xVoltageScaler = 1.f / 64.f;
	static constexpr float yVoltageScaler = 1.f / UINT64_MAX;
	static constexpr float modesScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);

	// EVENT
	void onRuleChange();

	// HELPERS
	static void halfadder(uint8_t a, uint8_t b,
		uint8_t& sum, uint8_t& carry);

	static void fulladder(uint8_t a, uint8_t b, uint8_t c,
		uint8_t& sum, uint8_t& carry);

	static uint8_t reverseRow(uint8_t row);

	void getHorizontalNeighbours(uint8_t row, uint8_t& west, uint8_t& east);

public:
	LifeEngine();

	// SEQUENCER
	void updateMatrix(int length, int offset, bool advance) override;
	void generate() override;
	void pushSeed(bool writeToNextRow) override;
	void inject(bool add, bool writeToNextRow) override;

	// UPDATERS
	void updateRule(int delta, bool reset) override;
	void updateSeed(int delta, bool reset) override;
	void updateMode(int delta, bool reset) override;

	// SETTERS
	void setBufferFrame(uint64_t newFrame, int index) override;
	void setRuleCV(float newCv) override;
	void setRule(int newRule) override;
	void setSeed(int newSeed) override;
	void setMode(int newMode) override;

	// GETTERS
	uint64_t getBufferFrame(int index) override;
	int getRuleSelect() override;
	int getRule() override;
	int getSeed() override;
	int getMode() override;
	float getXVoltage() override;
	float getYVoltage() override;
	bool getXPulse() override;
	bool getYPulse() override;
	float getModeLEDValue() override;
	void getRuleActiveLabel(char out[5]) override;
	void getRuleSelectLabel(char out[5]) override;
	void getSeedLabel(char out[5]) override;
	void getModeLabel(char out[5]) override;
};