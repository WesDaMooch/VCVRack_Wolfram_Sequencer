#include "wolfEngine.hpp"

const char WolfEngine::modeLabel[WolfEngine::NUM_MODES][5] = {
	"CLIP",
	"WRAP",
	"RAND"
};

WolfEngine::WolfEngine() {
	memcpy(engineLabel, "WOLF", 5);
	rowBuffer[readHead] = seed;
	updateDisplay(false);
}

void WolfEngine::updateDisplay(bool step, int length) {
	if (step) {
		advanceHeads(length);
		internalDisplayMatrix <<= 8;	// Shift matrix up
	}

	internalDisplayMatrix &= ~0xFFULL;
	internalDisplayMatrix |= rowBuffer[readHead];

	// Apply offset
	uint64_t tempMatrix = 0;
	for (int i = 0; i < 8; i++) {
		uint8_t row = (internalDisplayMatrix >> (i * 8)) & 0xFFULL;
		tempMatrix |= uint64_t(applyOffset(row, offset)) << (i * 8);
	}
	displayMatrix = tempMatrix;
	displayMatrixUpdated = true;
}

void WolfEngine::inject(int inject, bool sync) {
	int steps = std::abs(inject);

	for (int step = 0; step < steps; step++) {
		bool addCell = (inject > 0);
		int head = sync ? writeHead : readHead;

		uint8_t row = rowBuffer[head];

		// Check if row is already full or empty
		if ((addCell & (row == UINT8_MAX)) | (!addCell & (row == 0)))
			return;

		uint8_t targetMask = row;
		if (addCell)
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

		row = addCell ? (row | bitMask) : (row & ~bitMask);
		rowBuffer[head] = row;
	}
}

void WolfEngine::updateMenuParams(const EngineMenuParams& p) {
	// Rule
	int newRuleSelect = ruleSelect;
	if (p.menuReset[EngineMenuParams::RULE_RESET])
		newRuleSelect = ruleDefault;
	else if (p.menuDelta[EngineMenuParams::RULE_DELTA] != 0)
		newRuleSelect = static_cast<uint8_t>(ruleSelect + p.menuDelta[EngineMenuParams::RULE_DELTA]);
	setRuleSelect(newRuleSelect);

	// Seed
	int newSeedSelect = seedSelect;
	if (p.menuReset[EngineMenuParams::SEED_RESET])
		newSeedSelect = seedDefault;
	else if (p.menuDelta[EngineMenuParams::SEED_DELTA] != 0)
		newSeedSelect += p.menuDelta[EngineMenuParams::SEED_DELTA];
	setSeed(newSeedSelect);

	// Mode
	int newModeSelect = updateSelect(p.menuDelta[EngineMenuParams::MODE_DELTA],
		p.menuReset[EngineMenuParams::MODE_RESET],
		modeIndex, modeDefault, NUM_MODES);
	setMode(newModeSelect);
};

void WolfEngine::process(const EngineCoreParams& p,
	float* xOut, float* yOut, 
	bool* xPulse, bool* yPulse, 
	float* modeLED) {

	// Sequencer
	setRuleCv(p.ruleCv);

	bool refreshDisplay = p.step;
	generate = (rack::random::get<float>() < p.probability);

	bool injectOccured = (p.inject != 0);
	if (injectOccured && p.sync)
		injectPending += p.inject;

	// Non-sync inject
	if (injectOccured && !p.sync) {
		inject(p.inject, p.sync);
		refreshDisplay = true;
	}

	// Reset
	if (p.miniMenuChanged && p.sync)
		miniMenuChangePending = true;

	if (p.reset && p.sync)
		resetPending = true;

	if (((p.reset || p.miniMenuChanged) && !p.sync) || ((resetPending || miniMenuChangePending) && p.sync && p.step)) {
		if (generate) {
			int head = p.sync ? writeHead : readHead;
			uint8_t resetRow = randSeed ? rack::random::get<uint8_t>() : seed;
			rowBuffer[head] = resetRow;
			generate = false;
		}
		else if (!miniMenuChangePending) {
			if (p.sync) {
				writeHead = 0;
			}
			else {
				readHead = 0;
				writeHead = 1;
			}
		}
		resetPending = false;
		miniMenuChangePending = false;
		refreshDisplay = true;
	}

	// Generate
	if (generate && p.step) {
		// One Dimensional Cellular Automata
		uint8_t readRow = rowBuffer[readHead];
		uint8_t writeRow = 0;

		// Clip
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
		refreshDisplay = true;
	}

	// Sync inject
	if (injectPending && p.sync && p.step) {
		inject(injectPending, p.sync);
		injectPending = 0;
	}

	// Offset
	int newOffset = p.offset - 4;
	if ((!p.sync && (offset != newOffset)) || (p.sync && p.step)) {
		offset = newOffset;
		refreshDisplay = true;
	}

	// Update
	if (refreshDisplay)
		updateDisplay(p.step, p.length);

	// Render output
	// X - Returns bottom row of the display matrix scaled to 0-1	
	uint8_t firstRow = displayMatrix & 0xFFULL;
	*xOut = firstRow * voltageScaler;

	// Y - Returns right column of the display matrix scaled to 0-1
	// Output matrix is flipped when drawn (right -> left, left <- right)
	uint64_t yMask = 0x0101010101010101ULL;
	uint64_t column = displayMatrix & yMask;
	uint8_t yColumn = static_cast<uint8_t>((column * 0x8040201008040201ULL) >> 56);
	*yOut = yColumn * voltageScaler;

	// X Pulse - Returns true if bottom left cell state of displayMatrix is living
	bool bottonLeftCellState = ((displayMatrix & 0xFFULL) >> 7) & 1;
	if (displayMatrixUpdated && bottonLeftCellState)
		*xPulse = true;

	// Y Pulse - Returns true if top right cell state	of displayMatrix is living
	bool topRightCellState = ((displayMatrix >> 56) & 0xFFULL) & 1;
	if (displayMatrixUpdated && topRightCellState)
		*yPulse = true;

	// Mode LED brightness
	*modeLED = static_cast<float>(modeIndex) * modeScaler;

	displayMatrixUpdated = false;
}

void WolfEngine::reset() {
	for (int i = 0; i < MAX_SEQUENCE_LENGTH; i++)
		setBufferFrame(0, i);

	setBufferFrame(0, 0, true);
	setReadHead(0);
	setWriteHead(0);
	setRuleSelect(ruleDefault);
	setSeed(seedDefault);
	setMode(modeDefault);

	rowBuffer[readHead] = seed;
	updateDisplay(false);
}

// SETTERS
void WolfEngine::setBufferFrame(uint64_t newFrame, int index, 
	bool setDisplayMatrix) {

	if (setDisplayMatrix)
		internalDisplayMatrix = newFrame;
	else if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		rowBuffer[index] = static_cast<uint8_t>(newFrame);
}

void WolfEngine::setRuleSelect(int newRule) {
	ruleSelect = rack::clamp(newRule, 0, UINT8_MAX);
	onRuleChange();
}

void WolfEngine::setRuleCv(float newRuleCv) {
	ruleCv = static_cast<int>(newRuleCv * 256);
	onRuleChange();
}

void WolfEngine::onRuleChange() {
	rule = static_cast<uint8_t>(rack::clamp(ruleSelect + ruleCv, 0, UINT8_MAX));
}

void WolfEngine::setSeed(int newSeed) {
	if (newSeed == seedSelect)
		return;

	// Seeds are 256 + 1 (0 to 255 + RAND) 
	if (newSeed > 256)
		newSeed -= 257;
	else if (newSeed < 0)
		newSeed += 257;

	seedSelect = newSeed;
	randSeed = (seedSelect == 256);
	seed = static_cast<uint8_t>(seedSelect);
}

void WolfEngine::setMode(int newMode) {
	if (newMode == modeIndex)
		return;

	modeIndex = rack::clamp(newMode, 0, NUM_MODES - 1);
}

// GETTERS
uint64_t WolfEngine::getBufferFrame(int index, 
	bool getDisplayMatrix, 
	bool getDisplayMatrixSave) {

	if (getDisplayMatrix)
		return displayMatrix;
	else if (getDisplayMatrixSave)
		return internalDisplayMatrix;
	else if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		return static_cast<uint64_t>(rowBuffer[index]);
	else
		return 0;
}

int WolfEngine::getRuleSelect() {
	return ruleSelect;
}

int WolfEngine::getSeed() {
	return seedSelect;
}

int WolfEngine::getMode() {
	return modeIndex;
}

void WolfEngine::getRuleActiveLabel(char out[5]) {
	snprintf(out, 5, "%4d", rule);
}

void WolfEngine::getRuleSelectLabel(char out[5]) {
	snprintf(out, 5, "%4d", ruleSelect);
}

void WolfEngine::getSeedLabel(char out[5]) {
	memcpy(out, "    ", 5);
}

void WolfEngine::getModeLabel(char out[5]) {
	memcpy(out, modeLabel[modeIndex], 5);
}