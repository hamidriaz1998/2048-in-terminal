#include "draw.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* sliding tile */
typedef struct tile {
  int x, y;
  int mx, my; /* coord modifiers */
  int val;    /* tile's value, power of two */
} Tile;

static const char *tile_str[] = {
    "        ", "   2    ", "   4    ", "   8    ", "   16   ", "   32   ",
    "   64   ", "  128   ", "  256   ", "  512   ", "  1024  ", "  2048  ",
    "  4096  ", "  8192  ", " 16384  ", " 32768  ", " 65536  ", " 131072 "};
static const char *empty_tile_str = "          ";

static const NCURSES_ATTR_T tile_attr[] = {
    COLOR_PAIR(1),          COLOR_PAIR(1), /* empty 2 */
    COLOR_PAIR(2),          COLOR_PAIR(3),
    COLOR_PAIR(4), /*  4  8  16 */
    COLOR_PAIR(5),          COLOR_PAIR(6),
    COLOR_PAIR(7),                                  /* 32 64 128 */
    COLOR_PAIR(1) | A_BOLD, COLOR_PAIR(2) | A_BOLD, /* 256  512 */
    COLOR_PAIR(3) | A_BOLD, COLOR_PAIR(4) | A_BOLD, /* 1024 2048 */
    COLOR_PAIR(5) | A_BOLD, COLOR_PAIR(6) | A_BOLD, /* 4096 8192 */
    COLOR_PAIR(7) | A_BOLD,                         /* 16384 */
    COLOR_PAIR(1) | A_BOLD,                         /* 32768 */
    COLOR_PAIR(2) | A_BOLD,                         /* 65536 */
    COLOR_PAIR(3) | A_BOLD,                         /* 131072 */
};

static const struct timespec tick_time = {.tv_sec = 0, .tv_nsec = 15000000};
static const struct timespec end_move_time = {.tv_sec = 0, .tv_nsec = 3000000};

// Undo/Redo animation timing - much slower for visibility
static const struct timespec undo_step_time = {
    .tv_sec = 0, .tv_nsec = 200000000}; // 0.2 seconds per step
static const struct timespec undo_pause_time = {
    .tv_sec = 0, .tv_nsec = 400000000}; // 0.4 seconds between phases
static const struct timespec undo_final_pause = {
    .tv_sec = 0, .tv_nsec = 600000000}; // 0.6 seconds at end

static WINDOW *board_win;
static WINDOW *stats_win;

int init_win(int board_size) {

  const int bwidth = TILE_WIDTH * board_size + 2;
  const int bheight = TILE_HEIGHT * board_size + 2;
  const int swidth = 13;
  const int min_sheight =
      23; // Minimum height to show all menu options including save/load
  const int sheight = (bheight - 2) > min_sheight ? (bheight - 2) : min_sheight;

  if (board_win) {
    delwin(board_win);
    board_win = NULL;
  }
  if (stats_win) {
    delwin(stats_win);
    stats_win = NULL;
  }
  clear();
  refresh();

  int scr_width, scr_height;
  getmaxyx(stdscr, scr_height, scr_width);

  int btop = (scr_height - bheight) / 2;
  int stop = btop + 1;

  int bleft;
  if (bwidth + swidth < scr_width)
    bleft = (scr_width - bwidth - swidth) / 2;
  else
    bleft = 0;
  int sleft = bleft + bwidth + 1;

  if (bheight > scr_height || bwidth > scr_width)
    return WIN_TOO_SMALL;

  board_win = newwin(bheight, bwidth, btop, bleft);
  stats_win = newwin(sheight, swidth, stop, sleft);
  if (!board_win || !stats_win) {
    endwin();
    exit(1);
  }
  wattrset(board_win, COLOR_PAIR(1));
  wborder(board_win, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER,
          ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);

  return WIN_OK;
}

void setup_screen(void) {
  initscr();
  start_color();
  noecho();
  cbreak();
  curs_set(0);
  set_escdelay(0);
  keypad(stdscr, true);

  init_pair(1, COLOR_WHITE, COLOR_BLACK);
  init_pair(2, COLOR_YELLOW, COLOR_BLACK);
  init_pair(3, COLOR_GREEN, COLOR_BLACK);
  init_pair(4, COLOR_BLUE, COLOR_BLACK);
  init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(6, COLOR_CYAN, COLOR_BLACK);
  init_pair(7, COLOR_RED, COLOR_BLACK);
}

void print_too_small(void) {
  static const char *msg = "TERMINAL TOO SMALL";
  static int mlen = 0;
  if (!mlen)
    mlen = strlen(msg);

  int width, height;
  getmaxyx(stdscr, height, width);
  int x = (width - mlen) / 2;
  x = x >= 0 ? x : 0;
  int y = height / 2;
  mvprintw(y, x, "%s", msg);
  refresh();
}

static void draw_stats(const Stats *stats);
static void draw_board(const Board *board);
/* 'top' and 'left' are window coords of upper-left tile's corner */
static void draw_tile(int top, int left, int val);

void draw(const Board *board, const Stats *stats) {
  if (board) {
    draw_board(board);
    if (stats && stats->game_over) {
      wattron(board_win, A_BOLD | COLOR_PAIR(1));
      mvwprintw(board_win, TILE_HEIGHT * 2, (TILE_WIDTH * board->size - 8) / 2,
                "GAME OVER");
      wattroff(board_win, A_BOLD);
    }
    wrefresh(board_win);
  }
  if (stats) {
    draw_stats(stats);
    wrefresh(stats_win);
  }
}

static void draw_board(const Board *board) {
  for (int y = 0; y < board->size; y++) {
    for (int x = 0; x < board->size; x++) {
      /* convert board position to window coords */
      int xc = TILE_WIDTH * x + 1;
      int yc = TILE_HEIGHT * y + 1;
      draw_tile(yc, xc, board->tiles[y][x]);
    }
  }
}

static void draw_stats(const Stats *stats) {
  wattron(stats_win, COLOR_PAIR(2));
  mvwprintw(stats_win, 1, 1, "Score");
  mvwprintw(stats_win, 4, 1, "Best Score");

  if (stats->points > 0) {
    wattron(stats_win, COLOR_PAIR(3));
    mvwprintw(stats_win, 1, 7, "%+6d", stats->points);
  } else {
    mvwprintw(stats_win, 1, 7, "       ");
  }

  if (!stats->auto_save) {
    wattron(stats_win, COLOR_PAIR(1));
    mvwprintw(stats_win, 8, 1, "Autosave is");
    wattron(stats_win, COLOR_PAIR(7));
    mvwprintw(stats_win, 9, 9, "OFF");
  }

  wattron(stats_win, COLOR_PAIR(1));
  mvwprintw(stats_win, 2, 1, "%8d", stats->score);
  mvwprintw(stats_win, 5, 1, "%8d", stats->max_score);
  mvwprintw(stats_win, 12, 2, "ndo");
  mvwprintw(stats_win, 13, 2, "edo(");
  mvwprintw(stats_win, 14, 2, "ave");
  mvwprintw(stats_win, 15, 2, "oad(");
  mvwprintw(stats_win, 16, 2, "nimations");
  mvwprintw(stats_win, 17, 2, "estart");
  mvwprintw(stats_win, 18, 2, "uit");

  wattron(stats_win, COLOR_PAIR(4));
  mvwaddch(stats_win, 12, 1, 'U');

  wattron(stats_win, COLOR_PAIR(6));
  mvwaddch(stats_win, 13, 1, 'R');
  mvwaddch(stats_win, 13, 6, 'y');
  mvwaddch(stats_win, 13, 7, ')');

  wattron(stats_win, COLOR_PAIR(2));
  mvwaddch(stats_win, 14, 1, 'S');

  wattron(stats_win, COLOR_PAIR(3));
  mvwaddch(stats_win, 15, 1, 'L');
  mvwaddch(stats_win, 15, 5, 'G');
  mvwaddch(stats_win, 15, 6, ')');

  wattron(stats_win, COLOR_PAIR(5));
  mvwaddch(stats_win, 16, 1, 'A');

  wattron(stats_win, COLOR_PAIR(3));
  mvwaddch(stats_win, 17, 1, 'R');

  wattron(stats_win, COLOR_PAIR(7));
  mvwaddch(stats_win, 18, 1, 'Q');
}

static void draw_tile(int top, int left, int val) {
  int right = left + TILE_WIDTH - 1;
  int bottom = top + TILE_HEIGHT - 1;
  int center = (top + bottom) / 2;

  /* draw empty tile */
  if (val == 0) {
    for (int y = top; y <= bottom; y++)
      mvwprintw(board_win, y, left, "%s", empty_tile_str);
    return;
  }

  wattrset(board_win, tile_attr[val]);

  /* erase tile except it's border */
  for (int y = top + 1; y < bottom; y++)
    mvwprintw(board_win, y, left + 1, "%s", tile_str[0]);

  /* draw corners */
  mvwaddch(board_win, top, left, ACS_ULCORNER);
  mvwaddch(board_win, top, right, ACS_URCORNER);
  mvwaddch(board_win, bottom, left, ACS_LLCORNER);
  mvwaddch(board_win, bottom, right, ACS_LRCORNER);

  /* draw lines */
  mvwhline(board_win, top, left + 1, ACS_HLINE, TILE_WIDTH - 2);
  mvwhline(board_win, bottom, left + 1, ACS_HLINE, TILE_WIDTH - 2);
  mvwvline(board_win, top + 1, left, ACS_VLINE, TILE_HEIGHT - 2);
  mvwvline(board_win, top + 1, right, ACS_VLINE, TILE_HEIGHT - 2);

  /* draw number */
  mvwprintw(board_win, center, left + 1, "%s", tile_str[val]);
}

/* Passed to qsort */
static int sort_left(const void *l, const void *r);
static int sort_right(const void *l, const void *r);
static int sort_up(const void *l, const void *r);
static int sort_down(const void *l, const void *r);

void draw_slide(const Board *board, const Board *moves, Dir dir) {
  Tile tiles[MAX_BOARD_TILES]; /* sliding tiles */
  int tiles_n = 0;

  for (int y = 0; y < board->size; y++) {
    for (int x = 0; x < board->size; x++) {
      if (moves->tiles[y][x] == 0)
        continue;
      Tile tile;
      int step = moves->tiles[y][x];
      /* convert board position to window coords */
      tile.x = x * TILE_WIDTH + 1;
      tile.y = y * TILE_HEIGHT + 1;
      tile.val = board->tiles[y][x];

      switch (dir) {
      case UP:
        tile.mx = 0;
        tile.my = -1 * step;
        break;
      case DOWN:
        tile.mx = 0;
        tile.my = 1 * step;
        break;
      case LEFT:
        tile.mx = -2 * step;
        tile.my = 0;
        break;
      case RIGHT:
        tile.mx = 2 * step;
        tile.my = 0;
        break;
      }
      tiles[tiles_n++] = tile;
    }
  }

  int (*sort)(const void *, const void *);
  switch (dir) {
  case LEFT:
    sort = sort_left;
    break;
  case RIGHT:
    sort = sort_right;
    break;
  case UP:
    sort = sort_up;
    break;
  case DOWN:
    sort = sort_down;
    break;
  default:
    exit(1);
    break;
  }
  /* sort sliding tiles according to direction */
  qsort(tiles, tiles_n, sizeof(Tile), sort);

  nanosleep(&tick_time, NULL);
  for (int tick = 1; tick <= 3; tick++) {
    for (int t = 0; t < tiles_n; t++) {
      /* erase */
      draw_tile(tiles[t].y, tiles[t].x, 0);
      /* move */
      tiles[t].x += tiles[t].mx;
      tiles[t].y += tiles[t].my;
      /* redraw */
      draw_tile(tiles[t].y, tiles[t].x, tiles[t].val);
    }
    wrefresh(board_win);
    nanosleep(&tick_time, NULL);
  }
  nanosleep(&end_move_time, NULL);
}

static void draw_tile_with_attr(int top, int left, int val,
                                NCURSES_ATTR_T attr) {
  int right = left + TILE_WIDTH - 1;
  int bottom = top + TILE_HEIGHT - 1;
  int center = (top + bottom) / 2;

  /* draw empty tile */
  if (val == 0) {
    wattrset(board_win, COLOR_PAIR(1));
    for (int y = top; y <= bottom; y++)
      mvwprintw(board_win, y, left, "%s", empty_tile_str);
    return;
  }

  wattrset(board_win, attr);

  /* erase tile except it's border */
  for (int y = top + 1; y < bottom; y++)
    mvwprintw(board_win, y, left + 1, "%s", tile_str[0]);

  /* draw corners */
  mvwaddch(board_win, top, left, ACS_ULCORNER);
  mvwaddch(board_win, top, right, ACS_URCORNER);
  mvwaddch(board_win, bottom, left, ACS_LLCORNER);
  mvwaddch(board_win, bottom, right, ACS_LRCORNER);

  /* draw lines */
  mvwhline(board_win, top, left + 1, ACS_HLINE, TILE_WIDTH - 2);
  mvwhline(board_win, bottom, left + 1, ACS_HLINE, TILE_WIDTH - 2);
  mvwvline(board_win, top + 1, left, ACS_VLINE, TILE_HEIGHT - 2);
  mvwvline(board_win, top + 1, right, ACS_VLINE, TILE_HEIGHT - 2);

  /* draw number */
  mvwprintw(board_win, center, left + 1, "%s", tile_str[val]);
}

void draw_undo_redo(const Board *from_board, const Board *to_board,
                    bool is_undo) {
  if (!board_win)
    return;

  // Show status message at start of animation
  if (stats_win) {
    wattron(stats_win, COLOR_PAIR(7) | A_BOLD);
    mvwprintw(stats_win, 7, 1, "%-10s", is_undo ? "UNDO" : "REDO");
    wattroff(stats_win, COLOR_PAIR(7) | A_BOLD);
    wrefresh(stats_win);
  }

  // Draw the starting state
  for (int y = 0; y < from_board->size; y++) {
    for (int x = 0; x < from_board->size; x++) {
      int yc = y * TILE_HEIGHT + 1;
      int xc = x * TILE_WIDTH + 1;
      draw_tile(yc, xc, from_board->tiles[y][x]);
    }
  }
  wrefresh(board_win);
  nanosleep(&undo_pause_time, NULL);

  // Create smooth transition by highlighting changed tiles
  for (int step = 1; step <= 6; step++) {
    for (int y = 0; y < from_board->size; y++) {
      for (int x = 0; x < from_board->size; x++) {
        int from_val = from_board->tiles[y][x];
        int to_val = to_board->tiles[y][x];
        int yc = y * TILE_HEIGHT + 1;
        int xc = x * TILE_WIDTH + 1;

        if (from_val != to_val) {
          if (step <= 3) {
            // First half: highlight old value with increasing intensity
            if (from_val != 0) {
              NCURSES_ATTR_T attr;
              if (is_undo) {
                if (step == 1)
                  attr = COLOR_PAIR(4) | A_BOLD;
                else if (step == 2)
                  attr = COLOR_PAIR(4) | A_BOLD | A_REVERSE;
                else
                  attr = COLOR_PAIR(4) | A_BOLD | A_BLINK;
              } else {
                if (step == 1)
                  attr = COLOR_PAIR(3) | A_BOLD;
                else if (step == 2)
                  attr = COLOR_PAIR(3) | A_BOLD | A_REVERSE;
                else
                  attr = COLOR_PAIR(3) | A_BOLD | A_BLINK;
              }
              draw_tile_with_attr(yc, xc, from_val, attr);
            } else {
              draw_tile(yc, xc, 0);
            }
          } else {
            // Second half: fade in new value with decreasing intensity
            if (to_val != 0) {
              NCURSES_ATTR_T attr;
              if (is_undo) {
                if (step == 4)
                  attr = COLOR_PAIR(4) | A_BOLD | A_BLINK;
                else if (step == 5)
                  attr = COLOR_PAIR(4) | A_BOLD;
                else
                  attr = tile_attr[to_val];
              } else {
                if (step == 4)
                  attr = COLOR_PAIR(3) | A_BOLD | A_BLINK;
                else if (step == 5)
                  attr = COLOR_PAIR(3) | A_BOLD;
                else
                  attr = tile_attr[to_val];
              }
              draw_tile_with_attr(yc, xc, to_val, attr);
            } else {
              draw_tile(yc, xc, 0);
            }
          }
        } else {
          // Unchanged tile - draw normally with subtle dimming in middle steps
          if (from_val != 0) {
            NCURSES_ATTR_T attr = (step == 3 || step == 4)
                                      ? (tile_attr[from_val] | A_DIM)
                                      : tile_attr[from_val];
            draw_tile_with_attr(yc, xc, from_val, attr);
          } else {
            draw_tile(yc, xc, 0);
          }
        }
      }
    }
    wrefresh(board_win);
    if (step == 3) {
      nanosleep(&undo_pause_time, NULL); // Longer pause between phases
    } else {
      nanosleep(&undo_step_time, NULL); // Standard step timing
    }
  }

  // Final draw with normal attributes
  for (int y = 0; y < to_board->size; y++) {
    for (int x = 0; x < to_board->size; x++) {
      int yc = y * TILE_HEIGHT + 1;
      int xc = x * TILE_WIDTH + 1;
      draw_tile(yc, xc, to_board->tiles[y][x]);
    }
  }
  wrefresh(board_win);

  // Clear status message after animation
  if (stats_win) {
    mvwprintw(stats_win, 7, 1, "          ");
    wrefresh(stats_win);
  }

  nanosleep(&undo_final_pause, NULL); // Final pause to appreciate the result
}

void draw_undo_redo_status(const char *action) {
  if (!stats_win)
    return;

  wattron(stats_win, COLOR_PAIR(7) | A_BOLD);
  mvwprintw(stats_win, 7, 1, "%-10s", action);
  wattroff(stats_win, COLOR_PAIR(7) | A_BOLD);
  wrefresh(stats_win);

  // Brief non-blocking display
  struct timespec short_pause = {.tv_sec = 0, .tv_nsec = 500000000};
  nanosleep(&short_pause, NULL);
  mvwprintw(stats_win, 7, 1, "          ");
  wrefresh(stats_win);
}

static int sort_left(const void *l, const void *r) {
  return ((Tile *)l)->x - ((Tile *)r)->x;
}
static int sort_right(const void *l, const void *r) {
  return ((Tile *)r)->x - ((Tile *)l)->x;
}
static int sort_up(const void *l, const void *r) {
  return ((Tile *)l)->y - ((Tile *)r)->y;
}
static int sort_down(const void *l, const void *r) {
  return ((Tile *)r)->y - ((Tile *)l)->y;
}
