#include "baseEngine.hpp"

BaseEngine::BaseEngine() = default;
BaseEngine::~BaseEngine() = default;

// SEQUENCER
void BaseEngine::tick() {
    displayMatrixUpdated = false;
}

void BaseEngine::advanceHeads(int length) {
    // TODO: this might be wrong or unsafe, could go out of range?
    readHead = writeHead;
    writeHead = (writeHead + 1) % length;
}

// SETTERS
void BaseEngine::setReadHead(int newReadHead) {
    readHead = rack::clamp(newReadHead, 0, MAX_SEQUENCE_LENGTH - 1);
}


void BaseEngine::setWriteHead(int newWriteHead) { 
	writeHead = rack::clamp(newWriteHead, 0, MAX_SEQUENCE_LENGTH - 1);
}

// GETTERS
int BaseEngine::getReadHead() { 
	return readHead; 
}

int BaseEngine::getWriteHead() { 
	return writeHead; 
}

void BaseEngine::getEngineLabel(char out[5]) {
    memcpy(out, engineLabel, 5);
}

// HELPERS
uint8_t BaseEngine::applyOffset(uint8_t row, int offset) {
    int shift = rack::clamp(offset, -4, 4);

    if (shift < 0) {
        shift = -shift;
        row = ((row << shift) | (row >> (8 - shift))) & 0xFF;
    }
    else if (shift > 0) {
        row = ((row >> shift) | (row << (8 - shift))) & 0xFF;
    }

    return row;
}

int BaseEngine::updateSelect(int value, int MAX_VALUE,
	int defaultValue, int delta, bool reset) {
	if (reset)
		return defaultValue;

	return (value + delta + MAX_VALUE) % MAX_VALUE;
}