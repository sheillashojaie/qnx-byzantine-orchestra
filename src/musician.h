#ifndef MUSICIAN_H
#define MUSICIAN_H

void* musician_thread(void *arg);
void update_musician_bpm(musician_t *musician, bool byzantine_timing);
double get_reported_bpm(musician_t *musician, bool report_honestly);
void play_note(musician_t *musician, double reported_bpm);
double add_variance(double bpm, deviation_type_t deviation_type);

#endif
