#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#define MAX_BOARD_SIZE 5
#define MIN_BOARD_SIZE 3
#define MAX_BOARD_TILES (MAX_BOARD_SIZE * MAX_BOARD_SIZE)
#define MAX_HISTORY 50

/* Each tile is represented as power of two,
 * empty tile is 0 */
typedef struct board {
  int tiles[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
  int size;
} Board;

typedef struct stats {
  int score;
  int points; /* points for the last slide */
  int max_score;
  bool game_over;
  bool auto_save;
  int board_size;
} Stats;

typedef struct coord {
  int x, y;
} Coord;

typedef enum dir { UP, DOWN, LEFT, RIGHT } Dir;

/* Game state for undo/redo functionality */
typedef struct game_state {
  Board board;
  Stats stats;
} GameState;

/* History management for undo/redo */
typedef struct history {
  GameState states[MAX_HISTORY];
  int current;
  int size;
} History;

/* Enhanced save file structure with version and metadata */
typedef struct save_data {
  int version;          /* Save file format version */
  long timestamp;       /* When the game was saved */
  int play_time;        /* Total play time in seconds */
  Board board;          /* Current board state */
  Stats stats;          /* Current game statistics */
  History history;      /* Undo/redo history */
  char description[64]; /* Optional save description */
} SaveData;

#define SAVE_VERSION 1
#define MAX_SAVE_SLOTS 10

#endif
