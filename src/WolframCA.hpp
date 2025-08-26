// Wolfram Cellular Automata Brain
#pragma once

#include <array>
//#include <rack.hpp>

class CellularAutomata
{
public:
	CellularAutomata() {
		// Empty buffer
		internalCircularBuffer.fill(0);
		outputCircularBuffer.fill(0);
		// Init seed
		internalCircularBuffer[internalReadHead] = seed;
		outputCircularBuffer[outputReadHead] = seed;
	}

	void step() {
		bool generateFlag = false;
		// Generate random number (0 - 100)
		int randomNum = 50;
		if (randomNum < chance) {
			generateFlag = true;
		}

		// Messy Logic
		if (reset_flag && generateFlag) {
			internalCircularBuffer[internalWriteHead] = seed;
			reset_flag = false;
		}
		else {
			if (generateFlag) {
				generateRow();
			}
		}

		// Copy internal buffer to output buffer
		outputCircularBuffer[outputWriteHead] = internalCircularBuffer[internalWriteHead];
		// Push right bit to Y output array or uint8_t...
		
		// Advance output read and write heads
		outputReadHead = outputWriteHead;
		outputWriteHead = (outputWriteHead + 1) % 8;	// could do a & 7 thing?

		// Advance internal read and write heads
		internalReadHead = internalWriteHead;
		internalWriteHead = (internalWriteHead + 1) % sequenceLength;
	}

	void reset() {
		reset_flag = true;
	}

	void setRule(uint8_t passedRule) {
		rule = passedRule;
		reset();
	}

	void setSequenceLength(float passedSequenceLength) {
		sequenceLength = passedSequenceLength;
	}

	void setChance(float passedChance) {
		chance = passedChance;
	}

	float getVoltageX() {
		return outputCircularBuffer[outputReadHead];
	}

	float getDisplayRow(int passedOffset) {
		int rowIndex = (outputReadHead - passedOffset + 8) % 8;		// could do a & 7 thing?
		return outputCircularBuffer[rowIndex];
	}

private:

	static constexpr std::size_t MAX_ROWS = 64;
	std::array<uint8_t, MAX_ROWS> internalCircularBuffer = {};
	std::array<uint8_t, 8> outputCircularBuffer = {};

	int internalReadHead = 0;
	int internalWriteHead = 1;
	int outputReadHead = internalReadHead;
	int outputWriteHead = internalWriteHead;

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

			int left_cell = (internalCircularBuffer[internalReadHead] >> left_index) & 1;
			int cell = (internalCircularBuffer[internalReadHead] >> i) & 1;
			int right_cell = (internalCircularBuffer[internalReadHead] >> right_index) & 1;

			// Generate tag
			int tag = 7 - ((left_cell << 2) | (cell << 1) | right_cell);

			int ruleBit = (rule >> (7 - tag)) & 1;
			if (ruleBit > 0) {
				internalCircularBuffer[internalWriteHead] |= (1 << i);
			}
			else {
				internalCircularBuffer[internalWriteHead] &= ~(1 << i);
			}
		}
	}
};