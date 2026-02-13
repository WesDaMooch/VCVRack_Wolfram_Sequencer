#include "baseEngine.hpp"

BaseEngine::BaseEngine() = default;
BaseEngine::~BaseEngine() = default;

// Setters
void BaseEngine::setReadHead(size_t newReadHead) {
    readHead = rack::clamp(newReadHead, 0, MAX_SEQUENCE_LENGTH - 1);
}

void BaseEngine::setWriteHead(size_t newWriteHead) {
    writeHead = rack::clamp(newWriteHead, 0, MAX_SEQUENCE_LENGTH - 1);
}

// Getters
int BaseEngine::getReadHead() {
    return static_cast<int>(readHead);
}

int BaseEngine::getWriteHead() {
    return static_cast<int>(writeHead);
}

void BaseEngine::getEngineLabel(char out[5]) {
    memcpy(out, engineLabel, 5);
}

// Helpers
uint8_t BaseEngine::applyOffset(uint8_t inputRow, int inputOffset) {
    int shift = inputOffset % 7;
    if (shift > 4)  
        shift -= 7;
    if (shift < -4) 
        shift += 7;

    if (shift < 0) {
        shift = -shift;
        inputRow = ((inputRow << shift) | (inputRow >> (8 - shift))) & 0xFF;
    }
    else if (shift > 0) {
        inputRow = ((inputRow >> shift) | (inputRow << (8 - shift))) & 0xFF;
    }
    return inputRow;
}

