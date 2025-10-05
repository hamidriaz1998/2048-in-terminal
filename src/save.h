#ifndef SAVE_H
#define SAVE_H

#include "common.h"

/* Enhanced save/load functions with multiple slots and history support */

/* Auto-save/load functions (existing behavior) */
int load_game(Board *board, Stats *stats, History *history);
int save_game(const Board *board, const Stats *stats, const History *history);

/* Manual save/load functions with multiple slots */
int save_game_slot(const Board *board, const Stats *stats,
                   const History *history, int slot, const char *description);
int load_game_slot(Board *board, Stats *stats, History *history, int slot);

/* Save file management */
int list_save_slots(char descriptions[MAX_SAVE_SLOTS][64],
                    long timestamps[MAX_SAVE_SLOTS]);
int delete_save_slot(int slot);
int get_next_available_slot(void);

/* Quick save/load (slot 0) */
int quick_save(const Board *board, const Stats *stats, const History *history);
int quick_load(Board *board, Stats *stats, History *history);

/* Utility functions */
const char *get_save_slot_filename(int slot);
int validate_save_slot(int slot);

#endif
