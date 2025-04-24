#ifndef VISUALIZATION_H
#define VISUALIZATION_H

int initialize_visualization();
void add_note_event(const char* note, double bpm, int musician_id);
void play_note_with_viz(musician_t *musician, double reported_bpm);
void* visualization_thread(void *arg);
bool is_within_bounds(int x, int y);
bool is_overwritable_char(char c);
void draw_musician_line(char display[DISPLAY_HEIGHT][DISPLAY_WIDTH + 1],
                        int colour_map[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                        int x1, int y1, int x2, int y2,
                        char symbol, int musician_id);
void draw_visualization(double target_bpm, double elapsed_seconds);

#endif
