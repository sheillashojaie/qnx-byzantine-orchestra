#ifndef MUSICIAN_H
#define MUSICIAN_H

void* musician_thread(void* arg);
void update_musician_bpm(musician_t *musician, bool byzantine_timing);
void play_note(musician_t *musician);
double add_variance(double bpm, deviation_type_t deviation_type);
void cast_reputation_votes(musician_t *voter);

#endif
