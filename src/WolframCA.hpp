// Wolfram Cellular Automata Brain
#pragma once
//#include <rack.hpp>
#include <array>

class CellularAutomata
{
public:
	// Make private!
	std::array<uint8_t, 8> outputBuffer = {};

	CellularAutomata() {
		// Fill all buffers with 0?

		// Init seed
		circularBuffer[readRow] = seed;
	}

	void step(float passedChance) {

		if (reset_flag) {
			circularBuffer[writeRow] = seed;
			reset_flag = false;
		}
		else {
			// Generate random number (0 - 100)
			int randomNum = 50;
			if (randomNum < passedChance) {
				generateRow();
			}
		}

		// Dont want to use 'back()'
		for (std::size_t i = 0; i < outputBuffer.size() - 1; i++) {
			outputBuffer[i] = outputBuffer[i + 1];
		}
		outputBuffer.back() = circularBuffer[writeRow];

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

	float getRow(int passedRow) {
		return outputBuffer[passedRow];
	}

private:

	static constexpr std::size_t MAX_ROWS = 64;
	std::array<uint8_t, MAX_ROWS> circularBuffer = {};

	int readRow = 0;
	int writeRow = 1;
	int sequenceLength = 8;

	uint8_t rule = 30;
	uint8_t seed = 8;	// 00001000

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