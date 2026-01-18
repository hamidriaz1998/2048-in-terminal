#include "history.h"
#include <string.h>

void history_init(History *history) {
  memset(history, 0, sizeof(History));
  history->current = -1;
  history->size = 0;
}

void history_save_state(History *history, const Board *board,
                        const Stats *stats) {
  // If we're not at the end of history, we need to truncate forward history
  if (history->current < history->size - 1) {
    history->size = history->current + 1;
  }

  // Move to next position
  history->current++;

  // If we've reached maximum history, shift everything back
  if (history->current >= MAX_HISTORY) {
    for (int i = 0; i < MAX_HISTORY - 1; i++) {
      history->states[i] = history->states[i + 1];
    }
    history->current = MAX_HISTORY - 1;
  }

  // Save current state
  history->states[history->current].board = *board;
  history->states[history->current].stats = *stats;

  // Update size
  if (history->size < MAX_HISTORY) {
    history->size = history->current + 1;
  } else {
    history->size = MAX_HISTORY;
  }
}

bool history_undo(History *history, Board *board, Stats *stats) {
  if (!history_can_undo(history)) {
    return false;
  }

  // Move back in history
  history->current--;

  // Restore state
  *board = history->states[history->current].board;
  *stats = history->states[history->current].stats;

  return true;
}

bool history_redo(History *history, Board *board, Stats *stats) {
  if (!history_can_redo(history)) {
    return false;
  }

  // Move forward in history
  history->current++;

  // Restore state
  *board = history->states[history->current].board;
  *stats = history->states[history->current].stats;

  return true;
}

void history_clear(History *history) { history_init(history); }

bool history_can_undo(const History *history) { return history->current > 0; }

bool history_can_redo(const History *history) {
  return history->current < history->size - 1;
}

int history_undo_count(const History *history) {
  return history->current;
}

int history_redo_count(const History *history) {
  return history->size - history->current - 1;
}
