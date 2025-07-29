// Wolfram Cellular Automata Brain
#pragma once
//#include <rack.hpp>
#include <array>


struct CellularAutomata
{
	int readRow = 0;
	int writeRow = 1;

	std::array<uint8_t, 8> circularBuffer = {};

	uint8_t rule = 30;
	uint8_t seed = 8;	// 00001000

	CellularAutomata() {
		// Init seed
		circularBuffer[readRow] = seed;	
	}

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
			if (ruleBit == 1) {
				circularBuffer[writeRow] |= (1 << i);
			}
			else {
				circularBuffer[writeRow] &= ~(1 << i);
			}
		}
		// Advance row write head
		readRow = writeRow;
		writeRow = (writeRow + 1) % 8;
	}

	void reset() {
		// Place seed on reset
		readRow = seed;
	}

	void setRule(uint8_t newRule) {
		rule = newRule;
		reset();
	}

	float getRow(int rowOffset = 0) {
		// Return row as 0 - 255
		return circularBuffer[readRow - rowOffset];
	}

};