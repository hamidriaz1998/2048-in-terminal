#include "save.h"
#include "common.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PATH_LEN 512
#define MAGIC_NUMBER 0x32303438 // "2048" in hex

static char save_dir[PATH_LEN] = "";
static int legacy_fd = -1;
static bool auto_save_enabled = false;

// Internal function declarations
static int init_save_dir(void);
static int get_legacy_filename(char *filename);
static int get_slot_filename(int slot, char *filename);
static bool validate_save_data(const SaveData *data);
static int write_save_data(const char *filename, const SaveData *data);
static int read_save_data(const char *filename, SaveData *data);
static void create_save_data(const Board *board, const Stats *stats,
                             const History *history, const char *description,
                             SaveData *save_data);

// Legacy load function (maintains compatibility)
int load_game(Board *board, Stats *stats, History *history) {
  char filename[PATH_LEN];

  if (get_legacy_filename(filename) == -1)
    return -1;

  legacy_fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (legacy_fd == -1)
    return -1;

  if (flock(legacy_fd, LOCK_EX | LOCK_NB) != -1)
    auto_save_enabled = true;

  // Try to read legacy format first
  struct {
    int score;
    int max_score;
    int board_size;
    Board board;
  } legacy_data;

  ssize_t bytes_read = read(legacy_fd, &legacy_data, sizeof(legacy_data));

  if (bytes_read == sizeof(legacy_data)) {
    // Legacy format found
    stats->score = legacy_data.score;
    stats->max_score = legacy_data.max_score;
    stats->board_size = legacy_data.board_size;
    stats->game_over = false;
    stats->auto_save = auto_save_enabled;
    stats->points = 0;
    *board = legacy_data.board;

    // Initialize empty history for legacy saves
    history->current = -1;
    history->size = 0;

    // Validate the loaded data
    if (stats->score >= 0 && stats->max_score >= 0 &&
        stats->board_size >= MIN_BOARD_SIZE &&
        stats->board_size <= MAX_BOARD_SIZE &&
        board->size == stats->board_size) {
      return 0;
    }
  }

  // Try enhanced format
  lseek(legacy_fd, 0, SEEK_SET);
  SaveData save_data;
  if (read_save_data(filename, &save_data) == 0) {
    *board = save_data.board;
    *stats = save_data.stats;
    *history = save_data.history;
    stats->auto_save = auto_save_enabled;
    return 0;
  }

  if (!auto_save_enabled)
    close(legacy_fd);
  return -1;
}

// Legacy save function (maintains compatibility)
int save_game(const Board *board, const Stats *stats, const History *history) {
  if (legacy_fd == -1 || !auto_save_enabled)
    return -1;

  char filename[PATH_LEN];
  if (get_legacy_filename(filename) == -1)
    return -1;

  SaveData save_data;
  create_save_data(board, stats, history, "Auto-save", &save_data);

  int result = write_save_data(filename, &save_data);

  close(legacy_fd);
  legacy_fd = -1;
  return result;
}

// Enhanced save function with slot support
int save_game_slot(const Board *board, const Stats *stats,
                   const History *history, int slot, const char *description) {
  if (validate_save_slot(slot) != 0)
    return -1;

  if (init_save_dir() != 0)
    return -1;

  char filename[PATH_LEN];
  if (get_slot_filename(slot, filename) != 0)
    return -1;

  SaveData save_data;
  create_save_data(board, stats, history, description, &save_data);

  return write_save_data(filename, &save_data);
}

// Enhanced load function with slot support
int load_game_slot(Board *board, Stats *stats, History *history, int slot) {
  if (validate_save_slot(slot) != 0)
    return -1;

  char filename[PATH_LEN];
  if (get_slot_filename(slot, filename) != 0)
    return -1;

  SaveData save_data;
  if (read_save_data(filename, &save_data) != 0)
    return -1;

  *board = save_data.board;
  *stats = save_data.stats;
  *history = save_data.history;

  return 0;
}

// List all available save slots
int list_save_slots(char descriptions[MAX_SAVE_SLOTS][64],
                    long timestamps[MAX_SAVE_SLOTS]) {
  if (init_save_dir() != 0)
    return -1;

  int count = 0;

  for (int i = 0; i < MAX_SAVE_SLOTS; i++) {
    char filename[PATH_LEN];
    if (get_slot_filename(i, filename) != 0)
      continue;

    SaveData save_data;
    if (read_save_data(filename, &save_data) == 0) {
      strncpy(descriptions[i], save_data.description, 63);
      descriptions[i][63] = '\0';
      timestamps[i] = save_data.timestamp;
      count++;
    } else {
      descriptions[i][0] = '\0';
      timestamps[i] = 0;
    }
  }

  return count;
}

// Delete a save slot
int delete_save_slot(int slot) {
  if (validate_save_slot(slot) != 0)
    return -1;

  char filename[PATH_LEN];
  if (get_slot_filename(slot, filename) != 0)
    return -1;

  return unlink(filename);
}

// Get next available slot
int get_next_available_slot(void) {
  char descriptions[MAX_SAVE_SLOTS][64];
  long timestamps[MAX_SAVE_SLOTS];

  list_save_slots(descriptions, timestamps);

  for (int i = 0; i < MAX_SAVE_SLOTS; i++) {
    if (descriptions[i][0] == '\0')
      return i;
  }

  return -1; // All slots full
}

// Quick save (slot 0)
int quick_save(const Board *board, const Stats *stats, const History *history) {
  return save_game_slot(board, stats, history, 0, "Quick Save");
}

// Quick load (slot 0)
int quick_load(Board *board, Stats *stats, History *history) {
  return load_game_slot(board, stats, history, 0);
}

// Get filename for a save slot
const char *get_save_slot_filename(int slot) {
  static char filename[PATH_LEN];
  if (get_slot_filename(slot, filename) == 0)
    return filename;
  return NULL;
}

// Validate save slot number
int validate_save_slot(int slot) {
  return (slot >= 0 && slot < MAX_SAVE_SLOTS) ? 0 : -1;
}

// Internal functions

static int init_save_dir(void) {
  if (save_dir[0] != '\0')
    return 0; // Already initialized

  char *home = getenv("HOME");
  if (!home || strlen(home) > PATH_LEN - 20)
    return -1;

  snprintf(save_dir, sizeof(save_dir), "%s/.2048_saves", home);

  // Create directory if it doesn't exist
  struct stat st;
  if (stat(save_dir, &st) != 0) {
    if (mkdir(save_dir, 0755) != 0)
      return -1;
  }

  return 0;
}

static int get_legacy_filename(char *filename) {
  char *home = getenv("HOME");
  if (!home || strlen(home) > PATH_LEN - 10)
    return -1;

  snprintf(filename, PATH_LEN, "%s/.2048", home);
  return 0;
}

static int get_slot_filename(int slot, char *filename) {
  if (init_save_dir() != 0)
    return -1;

  snprintf(filename, PATH_LEN, "%s/slot_%d.save", save_dir, slot);
  return 0;
}

static bool validate_save_data(const SaveData *data) {
  if (data->version != SAVE_VERSION)
    return false;

  if (data->stats.score < 0 || data->stats.max_score < 0 ||
      data->stats.board_size < MIN_BOARD_SIZE ||
      data->stats.board_size > MAX_BOARD_SIZE ||
      data->board.size != data->stats.board_size)
    return false;

  if (data->history.current >= MAX_HISTORY ||
      data->history.size > MAX_HISTORY ||
      data->history.current >= data->history.size)
    return false;

  // Validate board tiles
  for (int y = 0; y < data->board.size; y++) {
    for (int x = 0; x < data->board.size; x++) {
      int tile = data->board.tiles[y][x];
      if (tile < 0 || tile > 17) // Max tile is 2^17 = 131072
        return false;
    }
  }

  return true;
}

static int write_save_data(const char *filename, const SaveData *data) {
  FILE *file = fopen(filename, "wb");
  if (!file)
    return -1;

  // Write magic number for file format identification
  uint32_t magic = MAGIC_NUMBER;
  if (fwrite(&magic, sizeof(magic), 1, file) != 1) {
    fclose(file);
    return -1;
  }

  // Write save data
  size_t written = fwrite(data, sizeof(SaveData), 1, file);
  fclose(file);

  return (written == 1) ? 0 : -1;
}

static int read_save_data(const char *filename, SaveData *data) {
  FILE *file = fopen(filename, "rb");
  if (!file)
    return -1;

  // Read and verify magic number
  uint32_t magic;
  if (fread(&magic, sizeof(magic), 1, file) != 1 || magic != MAGIC_NUMBER) {
    fclose(file);
    return -1;
  }

  // Read save data
  size_t read_bytes = fread(data, sizeof(SaveData), 1, file);
  fclose(file);

  if (read_bytes != 1 || !validate_save_data(data))
    return -1;

  return 0;
}

static void create_save_data(const Board *board, const Stats *stats,
                             const History *history, const char *description,
                             SaveData *save_data) {
  memset(save_data, 0, sizeof(SaveData));

  save_data->version = SAVE_VERSION;
  save_data->timestamp = time(NULL);
  save_data->play_time = 0; // TODO: implement play time tracking
  save_data->board = *board;
  save_data->stats = *stats;
  save_data->history = *history;

  if (description) {
    strncpy(save_data->description, description, 63);
    save_data->description[63] = '\0';
  } else {
    strcpy(save_data->description, "Game Save");
  }
}
