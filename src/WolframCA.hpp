// Wolfram Cellular Automata Brain
#pragma once
//#include <rack.hpp>
#include <array>


struct CellularAutomata
{
	//uint8_t rule = 30;

	std::array<int, 8> rule = { 0, 0, 0, 1, 1, 1, 1, 0 };

	int readRow = 0;
	int writeRow = 1;
	std::array<int, 64> circularBuffer = {};

	CellularAutomata() {
		// Init seed
		circularBuffer[readRow * 8 + 4] = 1;
	}

	void generateRow() {
		for (int i = 0; i < 8; i++) {
			// Make cleaner, bit shifting?
			int tag = 0;

			int left_index = i - 1;
			int right_index = i + 1;

			if (left_index < 0) { left_index = 7; }
			if (right_index > 7) { right_index = 0; }

			int left_cell = circularBuffer[readRow * 8 + left_index] * 100; 
			int cell = circularBuffer[readRow * 8 + i] * 10;
			int right_cell = circularBuffer[readRow * 8 + right_index];

			int sum = left_cell + cell + right_cell;

			if (sum == 111) {
				tag = 0;
			}
			else if (sum == 110) {
				tag = 1;
			}
			else if (sum == 101) {
				tag = 2;
			}
			else if (sum == 100) {
				tag = 3;
			}
			else if (sum == 11) {
				tag = 4;
			}
			else if (sum == 10) {
				tag = 5;
			}
			else if (sum == 1) {
				tag = 6;
			}
			else {
				tag = 7;
			}
			circularBuffer[writeRow * 8 + i] = rule[tag];
		}
		// Advance row write head
		readRow = writeRow;
		writeRow = (writeRow + 1) % 8;
	}
};