#ifndef VISUALIZATION_H
#define VISUALIZATION_H

int initialize_visualization();
void add_note_event(const char* note, double bpm, int musician_id);
void play_note_with_viz(musician_t *musician);
void* visualization_thread(void *arg);
void draw_musician_line(char display[DISPLAY_HEIGHT][DISPLAY_WIDTH + 1],
                        int colour_map[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                        int x1, int y1, int x2, int y2,
                        int musician_id);
int map_time_to_x(double t, double start_time, int display_time_range, int stretch_factor);
int map_bpm_to_y(double deviation_percent, double max_deviation_percent);
void draw_visualization(double target_bpm, double elapsed_seconds);

#endif
