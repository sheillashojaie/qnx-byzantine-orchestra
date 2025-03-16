#ifndef BYZANTINE_H
#define BYZANTINE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <time.h>

#define MAX_MUSICIANS 5
#define DEFAULT_BPM 60
#define MAX_NOTES 100

typedef struct {
    int type;        // 1 = pulse, 2 = report
    int musician_id;
    double reported_bpm;
} pulse_msg_t;

typedef struct {
    int id;
    pthread_t thread;
    double perceived_bpm;
    int chid;
    int coid_to_conductor;
    const char **notes;
    int note_index;
    const char *name;
} musician_t;

extern musician_t musicians[MAX_MUSICIANS];
extern int num_musicians;
extern double conductor_bpm;
extern int conductor_chid;
extern int coids_to_musicians[MAX_MUSICIANS];
extern volatile bool program_running;
extern const char *musician_names[MAX_MUSICIANS];

void* conductor_thread(void *arg);
void* musician_thread(void *arg);
double add_normal_variance(double bpm);
int read_notes_from_file(const char *filename,
        const char *musician_names[MAX_MUSICIANS],
        const char *notes[MAX_MUSICIANS][MAX_NOTES]);

#endif
