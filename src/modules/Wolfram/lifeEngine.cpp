#include "lifeEngine.hpp"

LifeEngine::LifeEngine() {
	engineName = "LIFE";
	seeds[seedDefault].value = random::get<uint64_t>();
	matrixBuffer[readHead] = seeds[seedDefault].value;
}

// SEQUENCER
void LifeEngine::updateMatrix(int length, int offset, bool advance) {
	if (advance)
		advanceHeads(length);

	uint64_t tempMatrix = 0;
	for (int i = 0; i < 8; i++) {
		uint8_t row = (matrixBuffer[readHead] >> (i * 8)) & 0xFFULL;
		tempMatrix |= uint64_t(applyOffset(row, offset)) << (i * 8);
	}
	displayMatrix = tempMatrix;

	// Count living cells
	population = __builtin_popcountll(displayMatrix);
	displayMatrixUpdated = true;
}

void LifeEngine::generate() {
	// 2D cellular automata
	// Based on parallel bitwise implementation by Tomas Rokicki, Paperclip Optimizer,
	// and Michael Abrash's (Graphics Programmer's Black Book, Chapter 17) padding method
	// 
	// Not optimal but efficent enough and readable

	uint64_t readMatrix = matrixBuffer[readHead];
	uint64_t writeMatrix = 0;

	// Eight matrix rows + top & bottom padding
	std::array<uint8_t, 10> row{};

	// Fill rows from current matrix
	for (int i = 1; i < 9; i++)
		row[i] = (readMatrix >> ((i - 1) * 8)) & 0xFFULL;

	// Fill top & bottom padding rows
	if (modeIndex == 0) {
		// Clip
		row[0] = 0;
		row[9] = 0;
	}
	else if (modeIndex == 1) {
		// Wrap
		row[0] = row[8];
		row[9] = row[1];
	}
	else if (modeIndex == 2) {
		// Klein bottle
		row[0] = reverseRow(row[8]);
		row[9] = reverseRow(row[1]);
	}
	else if (modeIndex == 3) {
		// Random
		row[0] = random::get<uint8_t>();
		row[9] = random::get<uint8_t>();
	}

	for (int i = 1; i < 9; i++) {
		// Current row  - C,
		// 8 neighbours - NW, N, NE, W, E, SW, S, SE
		uint8_t n = row[i - 1];
		uint8_t c = row[i];
		uint8_t s = row[i + 1];
		uint8_t nw = 0, ne = 0, w = 0, e = 0, sw = 0, se = 0;

		getHorizontalNeighbours(n, nw, ne);
		getHorizontalNeighbours(c, w, e);
		getHorizontalNeighbours(s, sw, se);

		// Parallel bitwise addition
		// What the helly

		// Sum north row
		uint8_t Nbit0 = 0, Nbit1 = 0;
		fulladder(nw, n, ne, Nbit0, Nbit1);

		// Sum current row
		uint8_t Cbit0 = 0, Cbit1 = 0;
		halfadder(w, e, Cbit0, Cbit1);

		// Sum south row
		uint8_t Sbit0 = 0, Sbit1 = 0;
		fulladder(sw, s, se, Sbit0, Sbit1);

		// North row sum  + current row sum = north_current row sum
		// (Nbit1, Nbit0) + (Cbit1, Cbit0)  = NCbit2, NCbit0, NCbit1
		uint8_t NCbit0 = 0, carry1 = 0;
		fulladder(Nbit0, Cbit0, 0, NCbit0, carry1);
		uint8_t NCbit1 = 0, NCbit2 = 0;
		fulladder(Nbit1, Cbit1, carry1, NCbit1, NCbit2);

		// (north_current row sum)   + south row sum	 = full neighbour sum
		// (NCbit0, NCbit1, NCbit2)  + (0, Sbit1, Sbit0) = NCSbit3, NCSbit2, NCSbit1, NCSbit0
		uint8_t NCSbit0 = 0, carry2 = 0;
		fulladder(NCbit0, Sbit0, 0, NCSbit0, carry2);
		uint8_t NCSbit1 = 0, carry3 = 0;
		fulladder(NCbit1, Sbit1, carry2, NCSbit1, carry3);
		uint8_t NCSbit2 = 0, NCSbit3 = 0;
		fulladder(NCbit2, 0, carry3, NCSbit2, NCSbit3);

		// MSB <- -> LSB
		std::array<uint8_t, 9> alive{};
		alive[0] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & ~NCSbit1 & ~NCSbit0);	// 0 0000
		alive[1] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & ~NCSbit1 & NCSbit0);	// 1 0001
		alive[2] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & NCSbit1 & ~NCSbit0);	// 2 0010
		alive[3] = static_cast<uint8_t>(~NCSbit3 & ~NCSbit2 & NCSbit1 & NCSbit0);	// 3 0011
		alive[4] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & ~NCSbit1 & ~NCSbit0);	// 4 0100
		alive[5] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & ~NCSbit1 & NCSbit0);	// 5 0101
		alive[6] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & NCSbit1 & ~NCSbit0);	// 6 0110
		alive[7] = static_cast<uint8_t>(~NCSbit3 & NCSbit2 & NCSbit1 & NCSbit0);	// 7 0111
		alive[8] = static_cast<uint8_t>(NCSbit3 & ~NCSbit2 & ~NCSbit1 & ~NCSbit0);	// 8 1000

		// Apply rule
		uint8_t birth = 0;
		uint8_t survival = 0;

		for (int k = 0; k < 9; k++) {
			int birthIndex = k;
			int survivalIndex = k + 9;

			if (rules[ruleIndex].value & (1 << birthIndex))
				birth |= alive[k];

			if (rules[ruleIndex].value & (1 << survivalIndex))
				survival |= alive[k];
		}
		uint8_t nextRow = (c & survival) | ((~c) & birth);

		// Update
		writeMatrix |= static_cast<uint64_t>(nextRow) << ((i - 1) * 8);
	}
	matrixBuffer[writeHead] = writeMatrix;
}

void LifeEngine::pushSeed(bool writeToNextRow) {
	int head = writeToNextRow ? writeHead : readHead;
	uint64_t resetMatrix = 0;

	if (seedIndex == 7) {		// Sparse / half density random 	
		resetMatrix = random::get<uint64_t>() & random::get<uint64_t>();
	}
	else if (seedIndex == 8) {	// Symmetrical / mirrored random
		uint32_t randomHalf = random::get<uint32_t>();
		uint64_t mirroredRandomHalf = 0;
		for (int i = 0; i < 4; i++) {
			uint8_t row = (randomHalf >> (i * 8)) & 0xFFUL;
			mirroredRandomHalf |= static_cast<uint64_t>(row) << ((i - 3) * -8);
		}
		resetMatrix = randomHalf | (mirroredRandomHalf << 32);
	}
	else if (seedIndex == 9) {	// True random
		resetMatrix = random::get<uint64_t>();
	}
	else {
		resetMatrix = seeds[seedIndex].value;
	}

	matrixBuffer[head] = resetMatrix;
}

void LifeEngine::inject(bool add, bool writeToNextRow) {
	// TODO: this dont work!
	int head = writeToNextRow ? writeHead : readHead;
	uint64_t tempMatrix = matrixBuffer[head];

	// Check to see if row is already full or empty
	if ((add & (tempMatrix == UINT64_MAX)) | (!add & (tempMatrix == 0)))
		return;

	uint64_t targetMask = tempMatrix;
	if (add)
		targetMask = ~tempMatrix;	// Flip row

	int targetCount = __builtin_popcountll(targetMask);	// Count target bits
	int target = random::get<uint8_t>() % targetCount;	// Random target index

	// Find corresponding bit position
	uint64_t bitMask;
	for (bitMask = 1; target || !(targetMask & bitMask); bitMask <<= 1) {
		if (targetMask & bitMask)
			target--;
	}

	/*
	uint8_t bitMask = 1;
	for (int i = 0; i < 8; i++, bitMask <<= 1) {
		if (targetMask & bitMask) {
			if (target == 0) break;
			target--;
		}
	}
	*/

	tempMatrix = add ? (tempMatrix | bitMask) : (tempMatrix & ~bitMask);
	matrixBuffer[head] = tempMatrix;
}

// UPDATERS
void LifeEngine::updateRule(int delta, bool reset) {
	if (reset) {
		ruleSelect = ruleDefault;
	}
	else {
		int newSelect = updateSelect(ruleSelect,
			NUM_RULES, ruleDefault, delta, reset);

		if (newSelect == ruleSelect)
			return;

		ruleSelect = newSelect;
	}
	onRuleChange();
}

void LifeEngine::updateSeed(int delta, bool reset) {
	seedIndex = updateSelect(seedIndex,
		NUM_SEEDS, seedDefault, delta, reset);
}

void LifeEngine::updateMode(int delta, bool reset) {
	modeIndex = updateSelect(modeIndex,
		NUM_MODES, modeDefault, delta, reset);
}

// SETTERS
void LifeEngine::setBufferFrame(uint64_t frame, int index) {
	if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		matrixBuffer[index] = frame;
	else if (index == -1)
		displayMatrix = frame;
}

void LifeEngine::setRuleCV(float cv) {
	int newCV = std::round(cv * NUM_RULES);

	if (newCV == ruleCV)
		return;

	ruleCV = newCV;
	onRuleChange();
}

void LifeEngine::setRule(int newRule) {
	ruleSelect = newRule;
	onRuleChange();
}

void LifeEngine::setSeed(int newSeed) {
	seedIndex = newSeed;
}

void LifeEngine::setMode(int newMode) {
	modeIndex = newMode;
}

// GETTERS
uint64_t LifeEngine::getBufferFrame(int index) {
	if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		return matrixBuffer[index];
	else if (index == -1)
		return displayMatrix;
	else
		return 0;
}

int LifeEngine::getRuleSelect() { 
	return ruleSelect; 
}

int LifeEngine::getRule() { 
	return ruleIndex; 
}

int LifeEngine::getSeed() { 
	return seedIndex; 
}

int LifeEngine::getMode() { 
	return modeIndex; 
}

float LifeEngine::getXVoltage() {
	// Returns the population (number of alive cells) scaled to 0-1
	return population * xVoltageScaler;
}

float LifeEngine::getYVoltage() {
	// Returns the 64-bit number display matrix scaled to 0-1
	return displayMatrix * yVoltageScaler;
}

bool LifeEngine::getXPulse() {
	// True if population (number of alive cells) has grown.
	bool xPulse = false;

	if (displayMatrixUpdated && (population > prevPopulation))
		xPulse = true;

	prevPopulation = population;
	return xPulse;
}

bool LifeEngine::getYPulse() {
	// True if life becomes stagnant (no change occurs),
	// also true if output repeats while looping.
	bool yPulse = false;

	if (displayMatrixUpdated && (displayMatrix == prevOutputMatrix))
		yPulse = true;

	prevOutputMatrix = displayMatrix;
	return yPulse;
}

float LifeEngine::getModeLEDValue() { 
	return static_cast<float>(modeIndex) * modesScaler; 
}

std::string LifeEngine::getRuleName() { 
	return rules[ruleIndex].name; 
}

std::string LifeEngine::getRuleSelectName() { 
	return rules[ruleSelect].name; 
}

std::string LifeEngine::getSeedName() { 
	return seeds[seedIndex].name; 
}

std::string LifeEngine::getModeName() { 
	return modeNames[modeIndex]; 
}

// EVENT
void LifeEngine::onRuleChange() {
	ruleIndex = rack::clamp(ruleSelect + ruleCV, 0, NUM_RULES - 1);
}

// HELPERS
void LifeEngine::halfadder(uint8_t a, uint8_t b,
	uint8_t& sum, uint8_t& carry) {
	sum = a ^ b;
	carry = a & b;
}

void LifeEngine::fulladder(uint8_t a, uint8_t b, uint8_t c,
	uint8_t& sum, uint8_t& carry) {

	uint8_t t0, t1, t2;
	halfadder(a, b, t0, t1);
	halfadder(t0, c, sum, t2);
	carry = t2 | t1;
}

// TODO: pass row ref?
uint8_t LifeEngine::reverseRow(uint8_t row) {
	row = ((row & 0xF0) >> 4) | ((row & 0x0F) << 4);
	row = ((row & 0xCC) >> 2) | ((row & 0x33) << 2);
	row = ((row & 0xAA) >> 1) | ((row & 0x55) << 1);
	return row;
}

void LifeEngine::getHorizontalNeighbours(uint8_t row, uint8_t& west, uint8_t& east) {
	if (modeIndex == 0) {
		// Clip
		west = row >> 1;
		east = row << 1;
	}
	else if (modeIndex == 1 || modeIndex == 2) {
		// Wrap & klein bottle
		west = (row >> 1) | (row << 7);
		east = (row << 1) | (row >> 7);
	}
	else if (modeIndex == 3) {
		// Random
		west = (row >> 1) | (random::get<bool>() << 7);
		east = (row << 1) | random::get<bool>();
	}
}