#include "baseEngine.hpp"

BaseEngine::BaseEngine() = default;
BaseEngine::~BaseEngine() = default;

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
uint8_t BaseEngine::applyOffset(uint8_t inputRow, int inputOffset) {
    int shift = rack::clamp(inputOffset, -4, 4);

    if (shift < 0) {
        shift = -shift;
        inputRow = ((inputRow << shift) | (inputRow >> (8 - shift))) & 0xFF;
    }
    else if (shift > 0) {
        inputRow = ((inputRow >> shift) | (inputRow << (8 - shift))) & 0xFF;
    }

    return inputRow;
}

