#pragma once
#include "../../plugin.hpp"
#include <array>
#include <cstdint>

static constexpr int MAX_SEQUENCE_LENGTH = 64;

struct EngineStateSnapshot {
	// Used to take a snapshot of the engine's current values,
	// to be safely read by the UI and patch autosaver
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};
	uint64_t display = 0;
	int readHead = 0;
	int writeHead = 0;
	int ruleSelect = 0;
	int seed = 0;
	int mode = 0;
	char engineLabel[5]{};
	char ruleActiveLabel[5]{};
	char ruleSelectLabel[5]{};
	char seedLabel[5]{};
	char modeLabel[5]{};
};

// TODO: rename getXString to getXLabel
class BaseEngine {
protected:
	uint64_t displayMatrix = 0;
	bool displayMatrixUpdated = false;
	int readHead = 0;
	int writeHead = 1;
	char engineLabel[5] = "BASE";

	// SEQUENCER
	void advanceHeads(int length);

	// HELPERS
	uint8_t applyOffset(uint8_t row, int offset);

	// TODO: value, delta, reset, defaultValue, maxValue
	int updateSelect(int value, int MAX_VALUE,
		int defaultValue, int delta, bool reset);

public:
	BaseEngine();
	virtual ~BaseEngine();

	// SEQUENCER
	void tick();
	virtual void updateMatrix(int length, int offset, bool advance) = 0;
	virtual void generate() = 0;
	virtual void pushSeed(bool writeToNextRow) = 0;
	virtual void inject(bool add, bool writeToNextRow) = 0;

	// UPDATERS
	virtual void updateRule(int delta, bool reset) = 0;
	virtual void updateSeed(int delta, bool reset) = 0;
	virtual void updateMode(int delta, bool reset) = 0;

	// SETTERS
	void setReadHead(int newReadHead);
	void setWriteHead(int newWriteHead);
	virtual void setBufferFrame(uint64_t newFrame, int index) = 0;
	virtual void setRuleCV(float newCV) = 0;
	virtual void setRule(int newRule) = 0; // set rule select
	virtual void setSeed(int newSeed) = 0;
	virtual void setMode(int newMode) = 0;

	// GETTERS
	int getReadHead();
	int getWriteHead();
	void getEngineLabel(char out[5]);
	virtual uint64_t getBufferFrame(int index) = 0;
	virtual int getRuleSelect() = 0;
	virtual int getRule() = 0; // not needed?
	virtual int getSeed() = 0;
	virtual int getMode() = 0;
	virtual float getXVoltage() = 0;
	virtual float getYVoltage() = 0;
	virtual bool getXPulse() = 0;
	virtual bool getYPulse() = 0;
	virtual float getModeLEDValue() = 0;
	virtual void getRuleActiveLabel(char out[5]) = 0;
	virtual void getRuleSelectLabel(char out[5]) = 0;
	virtual void getSeedLabel(char out[5]) = 0;
	virtual void getModeLabel(char out[5]) = 0;
};