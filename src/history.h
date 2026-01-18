#ifndef HISTORY_H
#define HISTORY_H

#include "common.h"

/* Initialize history */
void history_init(History *history);

/* Save current game state to history */
void history_save_state(History *history, const Board *board,
                        const Stats *stats);

/* Undo last move - returns true if successful */
bool history_undo(History *history, Board *board, Stats *stats);

/* Redo last undone move - returns true if successful */
bool history_redo(History *history, Board *board, Stats *stats);

/* Clear all history */
void history_clear(History *history);

/* Check if undo is possible */
bool history_can_undo(const History *history);

/* Check if redo is possible */
bool history_can_redo(const History *history);

/* Get number of undo steps available */
int history_undo_count(const History *history);

/* Get number of redo steps available */
int history_redo_count(const History *history);

#endif
