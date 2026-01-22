#include "wolfEngine.hpp"

WolfEngine::WolfEngine() {
	engineName = "WOLF";
	rowBuffer[readHead] = seed;
}

// SEQUENCER
void WolfEngine::updateMatrix(int length, int offset, bool advance) {
	if (advance) {
		advanceHeads(length);
		displayMatrix <<= 8;	// Shift matrix up
	}

	// Apply lastest offset
	int offsetDifference = offset - prevOffset;
	uint64_t tempMatrix = 0;

	for (int i = 1; i < 8; i++) {
		uint8_t row = (displayMatrix >> (i * 8)) & 0xFF;
		tempMatrix |= uint64_t(applyOffset(row, offsetDifference)) << (i * 8);
	}
	displayMatrix = tempMatrix;
	prevOffset = offset;

	// Push latest row
	displayMatrix &= ~0xFFULL;
	displayMatrix |= static_cast<uint64_t>(applyOffset(rowBuffer[readHead], offset));
	displayMatrixUpdated = true;
}

void WolfEngine::generate() {
	// One Dimensional Cellular Automata
	uint8_t readRow = rowBuffer[readHead];
	uint8_t writeRow = 0;

	// Clip
	// TODO: this is different from life!
	uint8_t left = readRow >> 1;
	uint8_t right = readRow << 1;

	if (modeIndex == 1) {
		// Wrap
		left = (readRow >> 1) | (readRow << 7);
		right = (readRow << 1) | (readRow >> 7);
	}
	else if (modeIndex == 2) {
		// Random
		left |= random::get<bool>() << 7;
		right |= random::get<bool>();
	}

	for (int col = 0; col < 8; col++) {
		uint8_t leftBit = (left >> col) & 1;
		uint8_t currentBit = (readRow >> col) & 1;
		uint8_t rightBit = (right >> col) & 1;

		uint8_t tag = (leftBit << 2) | (currentBit << 1) | rightBit;
		uint8_t newBit = (rule >> tag) & 1;

		writeRow |= newBit << col;
	}
	rowBuffer[writeHead] = writeRow;
}

void WolfEngine::pushSeed(bool writeToNextRow) {
	int head = writeToNextRow ? writeHead : readHead;
	uint8_t resetRow = randSeed ? random::get<uint8_t>() : seed;
	rowBuffer[head] = resetRow;
}

void WolfEngine::inject(bool add, bool writeToNextRow) {
	int head = writeToNextRow ? writeHead : readHead;
	uint8_t row = rowBuffer[head];

	// Check if row is already full or empty
	if ((add & (row == UINT8_MAX)) | (!add & (row == 0)))
		return;

	uint8_t targetMask = row;
	if (add)
		targetMask = ~row;	// Flip row

	int targetCount = __builtin_popcount(targetMask);	// Count target bits
	int target = random::get<uint8_t>() % targetCount;	// Random target index

	// Find corresponding bit position, TODO: this is hard to read
	uint8_t bitMask = 1;
	for (int i = 0; i < 8; i++, bitMask <<= 1) {
		if (targetMask & bitMask) {
			if (target == 0)
				break;
			target--;
		}
	}

	row = add ? (row | bitMask) : (row & ~bitMask);
	rowBuffer[head] = row;
}

// SETTERS
void WolfEngine::setBufferFrame(uint64_t frame, int index) {
	if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		rowBuffer[index] = static_cast<uint8_t>(frame);
	else if (index == -1)
		displayMatrix = frame;
}

void WolfEngine::setRule(int newRule) {
	ruleSelect = static_cast<uint8_t>(newRule);
	onRuleChange();
}

void WolfEngine::setSeed(int newSeed) {
	seedSelect = newSeed;
	onSeedChange();
}

void WolfEngine::setMode(int newMode) {
	modeIndex = newMode;
}

void WolfEngine::setRuleCV(float cv) {
	int newCV = std::round(cv * 256);

	if (newCV == ruleCV)
		return;

	ruleCV = newCV;
	onRuleChange();
}

// UPDATERS
void WolfEngine::updateRule(int delta, bool reset) {
	if (reset) {
		ruleSelect = defaultRule;
	}
	else {
		uint8_t newSelect = static_cast<uint8_t>(ruleSelect + delta);

		if (newSelect == ruleSelect)
			return;

		ruleSelect = newSelect;
	}
	onRuleChange();
}

void WolfEngine::updateSeed(int delta, bool reset) {
	if (reset) {
		seed = defaultSeed;
		seedSelect = seed;
		randSeed = false;
		return;
	}
	seedSelect += delta;
	onSeedChange();
}

void WolfEngine::updateMode(int delta, bool reset) {
	modeIndex = updateSelect(modeIndex, NUM_MODES, defaultMode, delta, reset);
}

// GETTERS
uint64_t WolfEngine::getBufferFrame(int index) {
	if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		return static_cast<uint64_t>(rowBuffer[index]);
	else if (index == -1)
		return displayMatrix;
	else
		return 0;
}

int WolfEngine::getRuleSelect() { 
	return ruleSelect; 
}

int WolfEngine::getRule() { 
	return rule; 
}

int WolfEngine::getSeed() { 
	return seedSelect; 
}

int WolfEngine::getMode() { 
	return modeIndex; 
}

float WolfEngine::getXVoltage() {
	// Returns bottom row of the display matrix scaled to 0-1	
	uint8_t firstRow = displayMatrix & 0xFFULL;
	return firstRow * voltageScaler;
}

float WolfEngine::getYVoltage() {
	// Returns right column of the display matrix scaled to 0-1
	// Output matrix is flipped when drawn (right -> left, left <- right),
	uint64_t yMask = 0x0101010101010101ULL;
	uint64_t column = displayMatrix & yMask;
	uint8_t yColumn = static_cast<uint8_t>((column * 0x8040201008040201ULL) >> 56);
	return yColumn * voltageScaler;
}

bool WolfEngine::getXPulse() {
	// Returns true if bottom left cell state of displayMatrix is living

	bool bottonLeftCellState = ((displayMatrix & 0xFFULL) >> 7) & 1;
	bool xPulse = false;

	if (displayMatrixUpdated && bottonLeftCellState)
		xPulse = true;

	return xPulse;
}

bool WolfEngine::getYPulse() {
	// Returns true if top right cell state	of displayMatrix is living
	bool topRightCellState = ((displayMatrix >> 56) & 0xFFULL) & 1;
	bool yPulse = false;

	if (displayMatrixUpdated && topRightCellState)
		yPulse = true;

	return yPulse;
}

float WolfEngine::getModeLEDValue() { 
	return static_cast<float>(modeIndex) * modeScaler; 
}

std::string WolfEngine::getRuleName() { 
	return std::to_string(rule); 
}

std::string WolfEngine::getRuleSelectName() { 
	return std::to_string(ruleSelect); 
}

std::string WolfEngine::getSeedName() { 
	return ""; 
}

std::string WolfEngine::getModeName() { 
	return modeNames[modeIndex]; 
}

// EVENT
void WolfEngine::onRuleChange() {
	rule = static_cast<uint8_t>(rack::clamp(ruleSelect + ruleCV, 0, UINT8_MAX));
}

void WolfEngine::onSeedChange() {
	// Seed options are 256 + 1 (RAND) 
	// Wrap seedSelect
	if (seedSelect > 256)
		seedSelect -= 257;
	else if (seedSelect < 0)
		seedSelect += 257;

	// Find randSeed
	randSeed = (seedSelect == 256);
	// Set seed
	if (!randSeed)
		seed = static_cast<uint8_t>(seedSelect);
}
