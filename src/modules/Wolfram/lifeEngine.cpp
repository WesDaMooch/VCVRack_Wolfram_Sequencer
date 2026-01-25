#include "lifeEngine.hpp"

const char LifeEngine::modeNames[LifeEngine::NUM_MODES][5] = {
	"CLIP",	// A plane bounded by 0s
	"WRAP",	// Donut-shaped torus
	"BOTL",	// Klein bottle - One pair of opposite edges are reversed
	"RAND"	// Plane is bounded by randomness
};

const std::array<LifeEngine::Rule, LifeEngine::NUM_RULES> LifeEngine::rule{ {
	// Rules from the Hatsya catagolue & LifeWiki
	{ "WALK", 0x1908U },			// Pedestrian Life			B38/S23
	{ "VRUS", 0x5848U },			// Virus					B36/S235
	{ "TREK", 0x22A08U },			// Star Trek				B3/S0248		
	{ "SQRT", 0x6848U },			// Sqrt Replicator			B36/S245		
	{ "SEED", 0x04U },				// Seeds					B2/S			
	{ "RUGS", 0x1CU },				// Serviettes				B234/S			
	{ "MOVE", 0x6948U },			// Move / Morley			B368/S245		
	{ "MESS", 0x3D9C8U },			// Stains					B3678/S235678   
	{ "MAZE", 0x3C30U },			// Mazectric non Static		B45/S1234 
	{ " LOW", 0x1408U },			// LowLife					B3/S13		
	{ "LONG", 0x4038U },			// LongLife					B345/S5			
	{ "LIFE", 0x1808U },			// Conway's Game of Life	B3/S23			
	{ "ICE9", 0x3C1E4U },			// Iceballs					B25678/S5678	
	{ "HUNY", 0x21908U },			// HoneyLife				B38/S238		
	{ "GNRL", 0x402U },				// Gnarl					B1/S1			
	{ " GEO", 0x3A9A8U },			// Geology					B3578/S24678	
	{ "GEMS", 0x2E0B8U },			// Gems						B3457/S4568		
	{ "FREE", 0x204U },				// Live Free or Die			B2/S0			
	{ "FORT", 0x3D5C8U },			// Castles					B3678/S135678   
	{ "FLOK", 0xC08U },				// Flock					B3/S12
	{ "DUPE", 0x154AAU },			// Replicator				B1357/S1357		
	{ " DOT", 0x1A08U },			// DotLife					B3/S023
	{ "DIRT", 0x1828U },			// Grounded Life			B35/S23
	{ "DIAM", 0x3C1E8U },			// Diamoeba					B35678/S5678	
	{ "DANC", 0x5018U },			// Dance					B34/S35			
	{ "CLOT", 0x3D988U },			// Coagulations				B378/S235678	
	{ "ACID", 0x2C08U },			// Corrosion of Conformity	B3/S124			
	{ " 3-4", 0x3018U },			// 3-4 Life					B34/S34			
	{ " 2X2", 0x4C48U },			// 2x2						B35/S125		
	{ "24/7", 0x3B1C8U },			// Day & Night				B3678/S34678	
} };

LifeEngine::LifeEngine() {
	seed = { {
		// Seeds from the Life Lexicon, Hatsya catagolue & LifeWiki
		{ "WING", 0x1824140C0000ULL },		// Wing									Rule: Life
		{ "WIND", 0x60038100000ULL },		// Sidewinder Spaceship					Rule: LowLife
		{ "VONG", 0x283C3C343000ULL },		// Castles Spaceship 					Rule: Castles
		{ "VELP", 0x20700000705000ULL },	// Virus Spaceship	 					Rule: Virus
		{ "STEP", 0xC1830000000ULL },		// Stairstep Hexomino					Rule: Life, HoneyLife
		{ "SSSS", 0x4040A0A0A00ULL },		// Creeper Spaceship 					Rule: LowLife
		{ "SENG", 0x2840240E0000ULL },		// Switch Engine						Rule: Life
		{ "RNDS", 0xEFA8EFA474577557ULL },	// Sparse Random / Half Density		
		{ "RNDM", 0x66555566B3AAABB2ULL },	// Symmetrical / Mirrored Random
		{ " RND", 0xFFFF81006618243CULL },	// True Random
		{ "NSEP", 0x70141E000000ULL },		// Nonomino Switch Engine Predecessor	Rule: Life
		{ "MWSS", 0x50088808483800ULL },	// Middleweight Spaceship				Rule: Life, HoneyLife
		{ "MORB", 0x38386C44200600ULL },	// Virus Spaceship	 					Rule: Virus
		{ "MOON", 0x1008081000000000ULL },	// Moon Spaceship 						Rule: Live Free or Die, Seeds, Iceballs
		{ "LWSS", 0x1220223C00000000ULL },	// Lightweight Spaceship				Rule: Life, HoneyLife 
		{ "JELY", 0x203038000000ULL },		// Jellyfish Spaceship					Rule: Move, Sqrt Replicator
		{ "HAND", 0xC1634180000ULL },		// Handshake							Rule: Life, HoneyLife
		{ "G-BC", 0x1818000024A56600ULL },	// Glider-block Cycle					Rule: Life 					
		{ " GSV", 0x10387C38D60010ULL },	// Xq4_gkevekgzx2 Spaceship 			Rule: Day & Night
		{ "FUMA", 0x1842424224A5C3ULL },	// Fumarole								Rule: Life  						
		{ "FLYR", 0x382010000000ULL },		// Glider								Rule: Life, HoneyLife			
		{ "FIG8", 0x7070700E0E0E00ULL },	// Figure 8								Rule: Life 
		{ "EPST", 0xBA7CEE440000ULL },		// Eppstein's Glider					Rule: Stains
		{ "CRWL", 0x850001C0000ULL },		// Crawler								Rule: 2x2
		{ "CHEL", 0x302C0414180000ULL },	// Herschel Descendant					Rule: Life
		{ "BORG", 0x40304838380000ULL },	// Xq6_5qqs Spaceship 					Rule: Star Trek	
		{ "BFLY", 0x38740C0C0800ULL },		// Butterfly 							Rule: Day & Night
		{ " B&G", 0x30280C000000ULL },		// Block and Glider						Rule: Life
		{ "34D3", 0x41E140C000000ULL },		// 3-4 Life Spaceship 					Rule: 3-4 Life
		{ "34C3", 0x3C2464140000ULL },		// 3-4 Life Spaceship 					Rule: 3-4 Life
	} };

	memcpy(engineLabel, "LIFE", 5);
	seed[seedDefault].value = random::get<uint64_t>();
	matrixBuffer[readHead] = seed[seedDefault].value;
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

			if (rule[ruleIndex].value & (1 << birthIndex))
				birth |= alive[k];

			if (rule[ruleIndex].value & (1 << survivalIndex))
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

	if (seedIndex == 7) {
		// Sparse / half density random 
		resetMatrix = random::get<uint64_t>() & random::get<uint64_t>();
	}
	else if (seedIndex == 8) {
		// Symmetrical / mirrored random
		uint32_t randomHalf = random::get<uint32_t>();
		uint64_t mirroredRandomHalf = 0;
		for (int i = 0; i < 4; i++) {
			uint8_t row = (randomHalf >> (i * 8)) & 0xFFUL;
			mirroredRandomHalf |= static_cast<uint64_t>(row) << ((i - 3) * -8);
		}
		resetMatrix = randomHalf | (mirroredRandomHalf << 32);
	}
	else if (seedIndex == 9) {
		// True random
		resetMatrix = random::get<uint64_t>();
	}
	else {
		resetMatrix = seed[seedIndex].value;
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
void LifeEngine::setBufferFrame(uint64_t newFrame, int index) {
	if ((index >= 0) && (index < MAX_SEQUENCE_LENGTH))
		matrixBuffer[index] = newFrame;
	else if (index == -1)
		displayMatrix = newFrame;
}

void LifeEngine::setRuleCV(float newCv) {
	int cv = std::round(newCv * NUM_RULES);

	if (cv == ruleCV)
		return;

	ruleCV = cv;
	onRuleChange();
}

void LifeEngine::setRule(int newRule) {
	ruleSelect = rack::clamp(newRule, 0, NUM_RULES - 1);
	onRuleChange();
}

void LifeEngine::setSeed(int newSeed) {
	seedIndex = rack::clamp(newSeed, 0, NUM_SEEDS - 1);
}

void LifeEngine::setMode(int newMode) {
	modeIndex = rack::clamp(newMode, 0, NUM_MODES - 1);
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
	// Returns the population (number of alive cells) scaled to 0 - 1
	return population * xVoltageScaler;
}

float LifeEngine::getYVoltage() {
	// Returns the 64-bit number display matrix scaled to 0 - 1
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

void LifeEngine::getRuleActiveLabel(char out[5]) {
	memcpy(out, rule[ruleIndex].name, 5);
}

void LifeEngine::getRuleSelectLabel(char out[5]) {
	memcpy(out, rule[ruleSelect].name, 5);
}

void LifeEngine::getSeedLabel(char out[5]) {
	memcpy(out, seed[seedIndex].name, 5);
}

void LifeEngine::getModeLabel(char out[5]) {
	memcpy(out, modeNames[modeIndex], 5);
}