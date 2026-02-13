#pragma once
#include "baseEngine.hpp"

class WolfEngine : public BaseEngine {
public:
	WolfEngine();

	void updateDisplay(bool advance, size_t length = 8) override;
	void updateMenuParams(const EngineMenuParams& p) override;

	void process(const EngineCoreParams& p,
		float* xOut, float* yOut, 
		bool* xPulse, bool* yPulse, 
		float* modeLED) override;

	void reset() override;
	
	// Save setters
	void setBufferFrame(uint64_t newFrame, int index, 
		bool setDisplayMatrix=false) override;

	void setRuleSelect(int newRule) override;
	void setRuleCv(float newRuleCv) override;
	void setSeed(int newSeed) override;
	void setMode(int newMode) override;

	// Save getters 
	uint64_t getBufferFrame(int index, 
		bool getDisplayMatrix = false, 
		bool getDisplayMatrixSave = false) override;

	int getRuleSelect() override;
	int getSeed() override;
	int getMode() override;

	// UI getters
	void getRuleActiveLabel(char out[5]) override;
	void getRuleSelectLabel(char out[5]) override;
	void getSeedLabel(char out[5]) override;
	void getModeLabel(char out[5]) override;

protected:
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{};
	uint64_t internalDisplayMatrix = 0;

	static constexpr int NUM_MODES = 3;
	static const char modeLabel[NUM_MODES][5];
	static constexpr int modeDefault = 1;
	int modeIndex = modeDefault;

	static constexpr uint8_t ruleDefault = 30;
	int ruleSelect = ruleDefault;
	int ruleCv = 0;
	uint8_t rule = 0;

	static constexpr int NUM_SEEDS = 256;
	static constexpr uint8_t seedDefault = 0x08;
	uint8_t seed = seedDefault;
	int seedSelect = seed;
	bool randSeed = false;

	int prevOffset = 0;
	bool prevXbit = false;
	bool prevYbit = false;

	static constexpr  float voltageScaler = 1.f / UINT8_MAX;
	static constexpr  float modeScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);

	void inject(int inject, bool sync) override;
	void onRuleChange() override;
};