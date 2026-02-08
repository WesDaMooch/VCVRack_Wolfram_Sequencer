#pragma once
#include "baseEngine.hpp"

class WolfEngine : public BaseEngine {
public:
	WolfEngine();

	void update(bool advance, int length=8) override;

	void process(const EngineParameters& p,
		float* xOut, float* yOut, 
		bool* xPulse, bool* yPulse, 
		float* modeLED) override;

	void reset() override;
	
	// SETTERS
	void setBufferFrame(uint64_t newFrame, int index, bool setDisplayMatrix=false) override;
	void setRule(int newRule, float newRuleCv) override;
	void setSeed(int newSeed) override;
	void setMode(int newMode) override;

	// GETTERS
	uint64_t getBufferFrame(int index, bool getDisplayMatrix=false, bool getDisplayMatrixSave=false) override;
	int getRuleSelect() override;
	int getSeed() override;
	int getMode() override;

	void getRuleActiveLabel(char out[5]) override;
	void getRuleSelectLabel(char out[5]) override;
	void getSeedLabel(char out[5]) override;
	void getModeLabel(char out[5]) override;

protected:
	void inject(int inject, bool sync) override;

	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{}; //TODO: rename buffer
	uint64_t internalDisplayMatrix = 0;

	static const int NUM_MODES = 3;
	static const char modeNames[NUM_MODES][5]; // TODO: rename modeLabel
	static const int modeDefault = 1;
	int modeIndex = modeDefault;

	static const uint8_t ruleDefault = 30;
	uint8_t ruleSelect = ruleDefault;
	uint8_t rule = 0;

	static const int NUM_SEEDS = 256;
	static const uint8_t seedDefault = 0x08;
	uint8_t seed = seedDefault;
	int seedSelect = seed;
	bool randSeed = false;

	int prevOffset = 0;
	bool prevXbit = false;
	bool prevYbit = false;

	static constexpr  float voltageScaler = 1.f / UINT8_MAX;
	static constexpr  float modeScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);
};