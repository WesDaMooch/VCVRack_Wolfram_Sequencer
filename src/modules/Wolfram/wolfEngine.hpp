#pragma once
#include "baseEngine.hpp"

class WolfEngine : public BaseEngine {
public:
	WolfEngine();

	// SEQUENCER
	void updateMatrix(int length, int offset, bool advance) override;
	void generate() override;
	void pushSeed(bool writeToNextRow) override;
	void inject(bool add, bool writeToNextRow) override;

	// SETTERS
	void setBufferFrame(uint64_t frame, int index) override;
	void setRule(int newRule) override;
	void setSeed(int newSeed) override;
	void setMode(int newMode) override;
	void setRuleCV(float cv) override;

	// UPDATERS
	void updateRule(int delta, bool reset) override;
	void updateSeed(int delta, bool reset) override;
	void updateMode(int delta, bool reset) override;

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
	std::string getRuleName() override;
	std::string getRuleSelectName() override;
	std::string getSeedName() override;
	std::string getModeName() override;

protected:
	std::array<uint8_t, MAX_SEQUENCE_LENGTH> rowBuffer{};

	static constexpr int NUM_MODES = 3;
	static constexpr int defaultMode = 1;
	int modeIndex = defaultMode;
	std::array<std::string, NUM_MODES> modeNames{
		"CLIP",
		"WRAP",
		"RAND"
	};

	static constexpr uint8_t defaultRule = 30;
	uint8_t ruleSelect = defaultRule;
	int ruleCV = 0;
	uint8_t rule = 0;

	static constexpr uint8_t defaultSeed = 0x08;
	uint8_t seed = defaultSeed;
	int seedSelect = seed;
	bool randSeed = false;

	int prevOffset = 0;
	bool prevXbit = false;
	bool prevYbit = false;

	static constexpr float voltageScaler = 1.f / UINT8_MAX;
	static constexpr float modeScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);

	// EVENT
	void onRuleChange();
	void onSeedChange();
};