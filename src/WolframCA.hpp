// Wolfram Cellular Automata Brain
#pragma once

#include <array>
//#include <rack.hpp>


//WHY IS THIS A CLASS NOT STRUCT

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
		bool generateFlag = random::get<float>() < chance;

		//if (reset_flag && generateFlag) {
		//	internalCircularBuffer[internalWriteHead] = seed;
		//}
		//else if (reset_flag) {
		//	internalReadHead = 0;
		//}
		//else
		//{
		//	generateRow();
		//}

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
		
		// Advance output read and write heads
		outputReadHead = outputWriteHead;
		outputWriteHead = (outputWriteHead + 1) & 7;

		// Advance internal read and write heads
		internalReadHead = internalWriteHead;
		internalWriteHead = (internalWriteHead + 1) % sequenceLength;
	}

	void reset() {
		reset_flag = true;
		//internalCircularBuffer[internalWriteHead] = seed;
	}

	void changeRule(int change) {
		rule = static_cast<uint8_t>((rule + change + 256) % 256);
		reset();
	}

	void changeSeed(int change) {
		seed = static_cast<uint8_t>((seed + change + 256) % 256);
	}

	void setSequenceLength(float passedSequenceLength) {
		sequenceLength = passedSequenceLength;
	}

	void setChance(float passedChance) {
		chance = passedChance;
	}

	void setOffset(float passedOffset) {
		offset = passedOffset;
	}

	void inject() {
		int pos = 3;
		int displayPos = (pos + offset + 8) & 7;
		internalCircularBuffer[internalReadHead] |= (1 << pos);
		// I want inject to be indepent from offset
		// work out what the offset is for it with offset applied
		outputCircularBuffer[outputReadHead] |= (1 << displayPos);
	}

	uint8_t getRow(int index = 0) {
		size_t rowIndex = (outputReadHead - index + 8) & 7;
		return applyOffset(outputCircularBuffer[rowIndex]);
	}

	uint8_t getColumn() {
		uint8_t col = 0;
		for (int i = 0; i < 8; i++) {
			size_t row = getRow(i);
			col |= ((row & 0x01) << (7 - i));
		}
		return col;
	}

	uint8_t getRule() {
		return rule;
	}

	uint8_t getSeed() {
		return seed;
	}


private:

	constexpr static size_t MAX_ROWS = 64;
	std::array<uint8_t, MAX_ROWS> internalCircularBuffer = {};
	std::array<uint8_t, 8> outputCircularBuffer = {};

	int internalReadHead = 0;
	int internalWriteHead = 1;
	int outputReadHead = internalReadHead;
	int outputWriteHead = internalWriteHead;

	int sequenceLength = 8;

	uint8_t rule = 30;
	uint8_t seed = 8;	// 00001000
	float chance = 1;
	int offset = 0;		

	bool reset_flag = false;

	uint8_t applyOffset(uint8_t row) {
		int shift = offset;
		if (shift < 0) {
			shift = -shift;
			return (row << shift) | (row >> (8 - shift));
		}
		else if (shift > 0) {
			return (row >> shift) | (row << (8 - shift));
		}
		return row;
	}


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