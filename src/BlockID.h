#ifndef CC_BLOCKID_H
#define CC_BLOCKID_H
#include "Core.h" /* TODO: Remove this include when we move to external defines */
/* List of all core/standard block IDs
   Copyright 2014-2019 ClassiCube | Licensed under BSD-3
*/

enum BLOCKID {
	/* Classic blocks */
	BLOCK_AIR = 0,
	BLOCK_STONE = 1,
	BLOCK_GRASS = 2,
	BLOCK_DIRT = 3,
	BLOCK_COBBLE = 4,
	BLOCK_WOOD = 5,
	BLOCK_SAPLING = 6,
	BLOCK_BEDROCK = 7,
	BLOCK_WATER = 8,
	BLOCK_STILL_WATER = 9,
	BLOCK_LAVA = 10,
	BLOCK_STILL_LAVA = 11,
	BLOCK_SAND = 12,
	BLOCK_GRAVEL = 13,
	BLOCK_GOLD_ORE = 14,
	BLOCK_IRON_ORE = 15,
	BLOCK_COAL_ORE = 16,
	BLOCK_LOG = 17,
	BLOCK_LEAVES = 18,
	BLOCK_SPONGE = 19,
	BLOCK_GLASS = 20,
	BLOCK_RED = 21,
	BLOCK_ORANGE = 22,
	BLOCK_YELLOW = 23,
	BLOCK_LIME = 24,
	BLOCK_GREEN = 25,
	BLOCK_TEAL = 26,
	BLOCK_AQUA = 27,
	BLOCK_CYAN = 28,
	BLOCK_BLUE = 29,
	BLOCK_INDIGO = 30,
	BLOCK_VIOLET = 31,
	BLOCK_MAGENTA = 32,
	BLOCK_PINK = 33,
	BLOCK_BLACK = 34,
	BLOCK_GRAY = 35,
	BLOCK_WHITE = 36,
	BLOCK_DANDELION = 37,
	BLOCK_ROSE = 38,
	BLOCK_BROWN_SHROOM = 39,
	BLOCK_RED_SHROOM = 40,
	BLOCK_GOLD = 41,
	BLOCK_IRON = 42,
	BLOCK_DOUBLE_SLAB = 43,
	BLOCK_SLAB = 44,
	BLOCK_BRICK = 45,
	BLOCK_TNT = 46,
	BLOCK_BOOKSHELF = 47,
	BLOCK_MOSSY_ROCKS = 48,
	BLOCK_OBSIDIAN = 49,

	/* CPE blocks */
	BLOCK_COBBLE_SLAB = 50,
	BLOCK_ROPE = 51,
	BLOCK_SANDSTONE = 52,
	BLOCK_SNOW = 53,
	BLOCK_FIRE = 54,
	BLOCK_LIGHT_PINK = 55,
	BLOCK_FOREST_GREEN = 56,
	BLOCK_BROWN = 57,
	BLOCK_DEEP_BLUE = 58,
	BLOCK_TURQUOISE = 59,
	BLOCK_ICE = 60,
	BLOCK_CERAMIC_TILE = 61,
	BLOCK_MAGMA = 62,
	BLOCK_PILLAR = 63,
	BLOCK_CRATE = 64,
	BLOCK_STONE_BRICK = 65,

	/* Max block ID used in original classic */
	BLOCK_MAX_ORIGINAL = BLOCK_OBSIDIAN,
	/* Number of blocks in original classic. */
	BLOCK_ORIGINAL_COUNT = (BLOCK_MAX_ORIGINAL + 1),
	/* Max block ID used in original classic plus CPE blocks. */
	BLOCK_MAX_CPE = BLOCK_STONE_BRICK,
	/* Number of blocks in original classic plus CPE blocks. */
	BLOCK_CPE_COUNT = (BLOCK_MAX_CPE + 1),

#ifdef EXTENDED_BLOCKS
	BLOCK_MAX_DEFINED = 0x2FF,
#else
	BLOCK_MAX_DEFINED = 0xFF,
#endif
	BLOCK_COUNT = (BLOCK_MAX_DEFINED + 1)
};
#endif
