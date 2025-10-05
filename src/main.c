#include "board.h"
#include "draw.h"
#include "history.h"
#include "save.h"
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static sigset_t all_signals;
static Board board;
static Stats stats = {.auto_save = false, .game_over = false, .board_size = 4};
static History history;

static int show_menu(void);

static void sig_handler(int __attribute__((unused)) sig_no) {
  sigprocmask(SIG_BLOCK, &all_signals, NULL);
  save_game(&board, &stats);
  endwin();
  exit(0);
}

static int show_menu(void) {
  setup_screen();

  int width, height;
  getmaxyx(stdscr, height, width);

  int selected = 0;
  const char *options[] = {"3x3 Mini", "4x4 Classic", "5x5 Extended"};
  const int board_sizes[] = {3, 4, 5};
  int num_options = 3;

  while (1) {
    clear();

    // Title
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(height / 2 - 5, (width - 4) / 2, "2048");
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(COLOR_PAIR(1));
    mvprintw(height / 2 - 3, (width - 19) / 2, "Choose board size:");

    // Menu options
    for (int i = 0; i < num_options; i++) {
      if (i == selected) {
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(height / 2 - 1 + i, (width - strlen(options[i]) - 4) / 2,
                 ">> %s <<", options[i]);
        attroff(COLOR_PAIR(3) | A_BOLD);
      } else {
        attron(COLOR_PAIR(1));
        mvprintw(height / 2 - 1 + i, (width - strlen(options[i])) / 2, "%s",
                 options[i]);
      }
    }

    attron(COLOR_PAIR(1));
    mvprintw(height / 2 + 4, (width - 32) / 2, "Use arrow keys/j/k and ENTER");
    mvprintw(height / 2 + 5, (width - 16) / 2, "Press Q to quit");

    refresh();

    int ch = getch();
    switch (ch) {
    case KEY_UP:
    case 'k':
    case 'K':
      selected = (selected - 1 + num_options) % num_options;
      break;
    case KEY_DOWN:
    case 'j':
    case 'J':
      selected = (selected + 1) % num_options;
      break;
    case '\n':
    case '\r':
    case KEY_ENTER:
      return board_sizes[selected];
    case 'q':
    case 'Q':
      endwin();
      exit(0);
    }
  }
}

int main(void) {
  const struct timespec addtile_time = {.tv_sec = 0, .tv_nsec = 100000000};
  bool show_animations = 1;
  bool terminal_too_small;
  int board_size;

  if (!isatty(fileno(stdout)) || !isatty(fileno(stdin))) {
    exit(1);
  }

  srand(time(NULL));

  sigfillset(&all_signals);
  sigdelset(&all_signals, SIGKILL);
  sigdelset(&all_signals, SIGSTOP);
  sigdelset(&all_signals, SIGTSTP);
  sigdelset(&all_signals, SIGCONT);
  signal(SIGINT, sig_handler);
  signal(SIGABRT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGHUP, sig_handler);

  sigprocmask(SIG_BLOCK, &all_signals, NULL);

  // Show menu to select board size
  board_size = show_menu();
  stats.board_size = board_size;

  // Initialize history
  history_init(&history);

  if (load_game(&board, &stats) != 0 || board.size != board_size) {
    board_start(&board, board_size);
    stats.score = 0;
    stats.max_score = 0;
    stats.board_size = board_size;
  }

  // Save initial state to history
  history_save_state(&history, &board, &stats);

  setup_screen();
  if (init_win(board_size) == WIN_TOO_SMALL) {
    terminal_too_small = true;
    print_too_small();
  } else {
    terminal_too_small = false;
    draw(&board, &stats);
  }

  sigprocmask(SIG_UNBLOCK, &all_signals, NULL);

  int ch;
  while ((ch = getch()) != 'q' && ch != 'Q') {
    Dir dir;
    Board new_board;
    Board moves;

    sigprocmask(SIG_BLOCK, &all_signals, NULL);

    if (terminal_too_small && ch != KEY_RESIZE)
      goto next;

    switch (ch) {
    case KEY_UP:
    case 'k':
    case 'K':
      dir = UP;
      break;
    case KEY_DOWN:
    case 'j':
    case 'J':
      dir = DOWN;
      break;
    case KEY_LEFT:
    case 'h':
    case 'H':
      dir = LEFT;
      break;
    case KEY_RIGHT:
    case 'l':
    case 'L':
      dir = RIGHT;
      break;

    /* restart */
    case 'r':
    case 'R':
      stats.score = 0;
      stats.game_over = false;
      board_start(&board, stats.board_size);
      history_clear(&history);
      history_save_state(&history, &board, &stats);
      draw(&board, &stats);
      goto next;

    /* undo */
    case 'u':
      if (history_undo(&history, &board, &stats)) {
        draw(&board, &stats);
      }
      goto next;

    /* redo */
    case 'U':
    case 'y':
      if (history_redo(&history, &board, &stats)) {
        draw(&board, &stats);
      }
      goto next;

    /* toggle animations */
    case 'a':
    case 'A':
      show_animations = !show_animations;
      goto next;

    /* terminal resize */
    case KEY_RESIZE:
      if (init_win(stats.board_size) == WIN_TOO_SMALL) {
        terminal_too_small = true;
        print_too_small();
      } else {
        terminal_too_small = false;
        draw(&board, &stats);
      }
      goto next;
    default:
      goto next;
    }

    if (stats.game_over)
      goto next;

    stats.points = board_slide(&board, &new_board, &moves, dir);

    if (stats.points >= 0) {
      // Save state before making the move
      history_save_state(&history, &board, &stats);

      draw(NULL, &stats); /* show +points */
      if (show_animations)
        draw_slide(&board, &moves, dir);

      board = new_board;
      stats.score += stats.points;
      if (stats.score > stats.max_score)
        stats.max_score = stats.score;
      draw(&board, &stats);

      nanosleep(&addtile_time, NULL);
      board_add_tile(&board, false);
      draw(&board, NULL);
      /* didn't slide, check if game's over */
    } else if (!board_can_slide(&board)) {
      stats.game_over = true;
      draw(&board, &stats);
    }
    flushinp();
  next:
    sigprocmask(SIG_UNBLOCK, &all_signals, NULL);
  }

  /* block all signals before saving */
  sigprocmask(SIG_BLOCK, &all_signals, NULL);
  endwin();

  if (stats.game_over) {
    board_start(&board, stats.board_size);
    stats.score = 0;
  }

  save_game(&board, &stats);
  return 0;
}
