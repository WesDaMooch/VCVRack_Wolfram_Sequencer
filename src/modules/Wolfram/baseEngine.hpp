#pragma once
#include "../../plugin.hpp"
#include <array>
#include <string>

// TODO: clamp setters

class BaseEngine {
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

	virtual void setBufferFrame(uint64_t frame, int index) = 0;
	virtual void setRuleCV(float newCV) = 0;
	virtual void setRule(int newRule) = 0;
	virtual void setSeed(int newSeed) = 0;
	virtual void setMode(int newMode) = 0;

	// GETTERS
	int getReadHead();
	int getWriteHead();
	std::string& getEngineString();

	virtual uint64_t getBufferFrame(int index) = 0;
	virtual int getRuleSelect() = 0;
	virtual int getRule() = 0;
	virtual int getSeed() = 0;
	virtual int getMode() = 0;
	virtual float getXVoltage() = 0;
	virtual float getYVoltage() = 0;
	virtual bool getXPulse() = 0;
	virtual bool getYPulse() = 0;
	virtual float getModeLEDValue() = 0;

	virtual std::string getRuleSelectName() = 0;
	virtual std::string getRuleName() = 0;
	virtual std::string getSeedName() = 0;
	virtual std::string getModeName() = 0;

protected:
	static constexpr int MAX_SEQUENCE_LENGTH = 64;
	uint64_t displayMatrix = 0;
	bool displayMatrixUpdated = false;
	int readHead = 0;
	int writeHead = 1;
	std::string engineName = "BASE";

	void advanceHeads(int length);
	uint8_t applyOffset(uint8_t row, int offset);

	// HELPERS
	int updateSelect(int value, int MAX_VALUE,
		int defaultValue, int delta, bool reset);
};