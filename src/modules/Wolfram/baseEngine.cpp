#include "baseEngine.hpp"

BaseEngine::BaseEngine() = default;
BaseEngine::~BaseEngine() = default;

void BaseEngine::tick() { 
	displayMatrixUpdated = false; 
}

// SETTERS
void BaseEngine::setReadHead(int newReadHead) { 
	readHead = newReadHead; 
}

// SEQUENCER
void BaseEngine::setWriteHead(int newWriteHead) { 
	writeHead = newWriteHead; 
}

int BaseEngine::getReadHead() { 
	return readHead; 
}

int BaseEngine::getWriteHead() { 
	return writeHead; 
}

std::string& BaseEngine::getEngineString() { 
	return engineName; 
}

void BaseEngine::advanceHeads(int length) {
    // TODO: this might be wrong or unsafe, could go out of range?
    readHead = writeHead;
    writeHead = (writeHead + 1) % length;
}

// HELPER
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