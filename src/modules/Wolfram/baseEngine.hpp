#pragma once
#include "../../plugin.hpp"
#include <array>
#include <cstdint>
;
static constexpr int MAX_SEQUENCE_LENGTH = 64;

struct EngineParameters {
	enum MenuDelta {
		RULE_DELTA,
		SEED_DELTA,
		MODE_DELTA,
		DELTA_LEN
	};
	enum MenuReset {
		RULE_RESET,
		SEED_RESET,
		MODE_RESET,
		RESET_LEN
	};
	std::array<int, DELTA_LEN> menuDelta{};
	std::array<bool, RESET_LEN> menuReset{};

	// Core params
	float ruleCv = 0.f;
	float probability = 0.f;
	int length = 0;
	int offset = 0;
	int inject = 0;
	bool step = false;
	bool reset = false;
	bool miniMenuChanged = false;
	bool sync = false;
};

// EngineUiLayer
struct EngineStateSnapshot {
	// Used to take a snapshot of the engine's current values,
	// to be safely read by the UI and patch autosaver
	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};
	uint64_t display = 0;
	uint64_t displaySave = 0;
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

class BaseEngine {
public:
	BaseEngine();
	virtual ~BaseEngine();

	virtual void update(bool advance, int length=8) = 0;

	virtual void process(const EngineParameters& p,
		float* xOut, float* yOut, 
		bool* xPulse, bool* yPulse, 
		float* modeLED) = 0;

	virtual void reset() = 0;

	// Save setters
	void setReadHead(int newReadHead);
	void setWriteHead(int newWriteHead);
	virtual void setBufferFrame(uint64_t newFrame, int index, bool setDisplayMatrix=false) = 0;
	virtual void setRule(int newRule, float newRuleCv) = 0;
	virtual void setSeed(int newSeed) = 0;
	virtual void setMode(int newMode) = 0;

	// Save Getters 
	int getReadHead();
	int getWriteHead();
	virtual uint64_t getBufferFrame(int index, bool getDisplayMatrix=false, bool getDisplayMatrixSave=false) = 0;
	virtual int getRuleSelect() = 0;
	virtual int getSeed() = 0;
	virtual int getMode() = 0;

	// UI Getters
	void getEngineLabel(char out[5]);
	virtual void getRuleActiveLabel(char out[5]) = 0;
	virtual void getRuleSelectLabel(char out[5]) = 0;
	virtual void getSeedLabel(char out[5]) = 0;
	virtual void getModeLabel(char out[5]) = 0;

protected:
	virtual void inject(int inject, bool sync) = 0;

	uint64_t displayMatrix = 0;
	bool displayMatrixUpdated = false;

	int readHead = 0;
	int writeHead = 1;
	int offset = 0;
	int injectPending = 0;
	bool generate = false;
	bool resetPending = false;
	bool miniMenuChangePending = false;
	char engineLabel[5] = "BASE";

	// HELPERS
	inline void advanceHeads(int length) {
		// TODO: this might be wrong or unsafe, could go out of range?
		readHead = writeHead;
		writeHead = (writeHead + 1) % length;
	}

	inline void resetHeads(bool read, bool write) {
		if (read)
			readHead = 0;
		if (write)
			writeHead = 1;
	}

	inline int updateSelect(int delta, int reset,
		int value, int defaultValue, int maxValue) {
		if (reset)
			return defaultValue;

		return (value + delta + maxValue) % maxValue;
	}

	uint8_t applyOffset(uint8_t inputRow, int inputOffset);
};