#pragma once
#include "baseEngine.hpp"

class LifeEngine : public BaseEngine {
public:
	LifeEngine();

	// SEQUENCER
	void updateMatrix(int length, int offset, bool advance) override;
	void generate() override;
	void pushSeed(bool writeToNextRow) override;
	void inject(bool add, bool writeToNextRow) override;

	// UPDATERS
	void updateRule(int delta, bool reset) override;
	void updateSeed(int delta, bool reset) override;
	void updateMode(int delta, bool reset) override;

	// SETTERS
	void setBufferFrame(uint64_t frame, int index) override;
	void setRuleCV(float cv) override;
	void setRule(int newRule) override;
	void setSeed(int newSeed) override;
	void setMode(int newMode) override;

	// GETTERS
	uint64_t getBufferFrame(int index) override;
	int getRuleSelect() override;
	int getRule() override;
	int getSeed() override;
	int getMode() override;
	float getXVoltage() override;
	float getYVoltage() override;
	bool getXPulse() override;
	bool getYPulse() override;
	float getModeLEDValue() override;
	std::string getRuleName() override;
	std::string getRuleSelectName() override;
	std::string getSeedName() override;
	std::string getModeName() override;

protected:
	struct Rule {
		std::string name;
		uint32_t value;
	};

	struct Seed {
		std::string name;
		uint64_t value;
	};

	std::array<uint64_t, MAX_SEQUENCE_LENGTH> matrixBuffer{};

	static constexpr int NUM_MODES = 4;
	static constexpr int modeDefault = 1;
	int modeIndex = modeDefault;
	std::array<std::string, NUM_MODES> modeNames{
		"CLIP",	// A plane bounded by 0s
		"WRAP",	// Donut-shaped torus
		"BOTL",	// Klein bottle - One pair of opposite edges are reversed
		"RAND"	// Plane is bounded by randomness
	};


	static constexpr int NUM_RULES = 30;
	static constexpr int ruleDefault = 11;
	int ruleSelect = ruleDefault;
	int ruleCV = 0;
	int ruleIndex = 0;
	std::array<Rule, NUM_RULES> rules{ {
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

	static constexpr int NUM_SEEDS = 30;
	static constexpr int seedDefault = 9;
	int seedIndex = seedDefault;
	std::array<Seed, NUM_SEEDS> seeds{ {
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

	int population = 0;
	int prevPopulation = 0;
	uint64_t prevOutputMatrix = 0;
	bool prevYbit = false;

	static constexpr float xVoltageScaler = 1.f / 64.f;
	static constexpr float yVoltageScaler = 1.f / UINT64_MAX;
	static constexpr float modesScaler = 1.f / (static_cast<float>(NUM_MODES) - 1.f);

	// EVENT
	void onRuleChange();

	// HELPERS
	static void halfadder(uint8_t a, uint8_t b,
		uint8_t& sum, uint8_t& carry);

	static void fulladder(uint8_t a, uint8_t b, uint8_t c,
		uint8_t& sum, uint8_t& carry);

	// TODO: pass row ref?
	static uint8_t reverseRow(uint8_t row);

	void getHorizontalNeighbours(uint8_t row, uint8_t& west, uint8_t& east);
};