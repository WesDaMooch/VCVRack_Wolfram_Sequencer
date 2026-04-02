// algoEngine.cpp
// Part of the Modular Mooch Wolfram module (VCV Rack)
//
// GitHub: https://github.com/WesDaMooch/Modular-Mooch-VCV
// 
// Copyright (c) 2026 Wesley Lawrence Leggo-Morrell
// License: GPL-3.0-or-later

#include "algoEngine.hpp"

AlgoEngine::AlgoEngine() = default;
AlgoEngine::~AlgoEngine() = default;

// Setters
void AlgoEngine::setReadHead(size_t newReadHead) {
    readHead = rack::clamp(newReadHead, 0, MAX_SEQUENCE_LENGTH - 1);
}

void AlgoEngine::setWriteHead(size_t newWriteHead) {
    writeHead = rack::clamp(newWriteHead, 0, MAX_SEQUENCE_LENGTH - 1);
}

// Getters
int AlgoEngine::getReadHead() {
    return static_cast<int>(readHead);
}

int AlgoEngine::getWriteHead() {
    return static_cast<int>(writeHead);
}

void AlgoEngine::getEngineLabel(char out[5]) {
    memcpy(out, engineLabel, 5);
}

// Helpers
uint8_t AlgoEngine::applyOffset(uint8_t inputRow, int inputOffset) {
    int shift = inputOffset % 8;
    if (shift > 3)
        shift -= 8;

    if (shift < 0) {
        shift = -shift;
        inputRow = ((inputRow << shift) | (inputRow >> (8 - shift))) & 0xFF;
    }
    else if (shift > 0) {
        inputRow = ((inputRow >> shift) | (inputRow << (8 - shift))) & 0xFF;
    }
    return inputRow;
}

