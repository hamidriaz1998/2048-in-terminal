#ifndef DRAW_H
#define DRAW_H

#include "common.h"

#define TILE_WIDTH 10
#define TILE_HEIGHT 5

#define WIN_OK 0
#define WIN_TOO_SMALL -1

/* Set up screen, keyboard, colors */
void setup_screen(void);

/* (Re-)initialize board and stats windows.
 * Returns WIN_OK or WIN_TOO_SMALL if terminal's too small.
 * exit(1) on error */
int init_win(int board_size);

/* Print the 'TERMINAL IS TOO SMALL' message */
void print_too_small(void);

/* Set history pointer for display (called once during init) */
void set_history_display(const History *history);

/* Draw board and stats. Both can be omitted if NULL is passed */
void draw(const Board *board, const Stats *stats);

/* Draw history info (undo/redo counts) */
void draw_history_info(const History *history);

/* Draw sliding animation. 'moves' must hold distance (positive int) for
 * each sliding tile and 0 for static and empty tiles */
void draw_slide(const Board *board, const Board *moves, Dir dir);

/* Draw undo/redo animation between two board states */
void draw_undo_redo(const Board *from_board, const Board *to_board,
                    bool is_undo);

/* Display undo/redo status message temporarily */
void draw_undo_redo_status(const char *action);

#endif
