#ifndef VISUALIZATION_H
#define VISUALIZATION_H

int initialize_visualization();
void add_note_event(const char* note, double bpm, int musician_id);
void play_note_with_viz(musician_t *musician, double reported_bpm);
void cleanup_visualization();
void* visualization_thread(void *arg);
cleanup_visualization();

#endif
