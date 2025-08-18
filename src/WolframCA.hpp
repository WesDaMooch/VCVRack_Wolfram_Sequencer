// Wolfram Cellular Automata Brain
#pragma once
//#include <rack.hpp>
#include <array>

class CellularAutomata
{
public:
	CellularAutomata() {
		// Empty all buffers
		circularBuffer.fill(0);
		outputBuffer.fill(0);
		// Init seed
		circularBuffer[readRow] = seed;
		//outputBuffer[readRow] = seed;
	}

	void step() {
		bool generateFlag = false;
		int randomNum = 50;
		if (randomNum < chance) {
			generateFlag = true;
		}

		// Messy Logic
		if (reset_flag && generateFlag) {
			circularBuffer[writeRow] = seed;
			reset_flag = false;
		}
		else {
			// Generate random number (0 - 100)
			
			if (generateFlag) {
				generateRow();
			}
		}

		// Advance row write head
		readRow = writeRow;
		writeRow = (writeRow + 1) % sequenceLength;
	}

	void reset() {
		reset_flag = true;
	}

	void setRule(uint8_t passedRule) {
		rule = passedRule;
		reset();
	}

	void setChance(float passedChance) {
		chance = passedChance;
	}

	float getVoltageX() {
		//return outputBuffer[passedRow];
		return circularBuffer[readRow];
	}

	float getDisplayRow(int passedOffset) {
		// Returns readRow - offset, wrapped around sequence lenght
		int rowIndex = (readRow - passedOffset + sequenceLength) % sequenceLength;
		return circularBuffer[rowIndex];
	}

private:

	static constexpr std::size_t MAX_ROWS = 64;
	std::array<uint8_t, MAX_ROWS> circularBuffer = {};
	// Could use rack RingBuffer?
	std::array<uint8_t, 8> outputBuffer = {};

	int readRow = 0;
	int writeRow = 1;
	int sequenceLength = 8;

	uint8_t rule = 30;
	uint8_t seed = 8;	// 00001000

	float chance = 100;

	bool reset_flag = false;

	void generateRow() {

		for (int i = 0; i < 8; i++) {

			int left_index = i - 1;
			int right_index = i + 1;

			if (left_index < 0) { left_index = 7; }
			if (right_index > 7) { right_index = 0; }

			int left_cell = (circularBuffer[readRow] >> left_index) & 1;
			int cell = (circularBuffer[readRow] >> i) & 1;
			int right_cell = (circularBuffer[readRow] >> right_index) & 1;

			// Generate tag
			int tag = 7 - ((left_cell << 2) | (cell << 1) | right_cell);

			int ruleBit = (rule >> (7 - tag)) & 1;
			if (ruleBit > 0) {
				circularBuffer[writeRow] |= (1 << i);
			}
			else {
				circularBuffer[writeRow] &= ~(1 << i);
			}
		}
	}
};