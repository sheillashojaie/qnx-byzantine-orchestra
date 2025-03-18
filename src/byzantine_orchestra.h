#ifndef BYZANTINE_H
#define BYZANTINE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <time.h>
#include <ctype.h>

#define MAX_MUSICIANS 5
#define DEFAULT_BPM 60
#define MAX_NOTES 100
#define BPM_TOLERANCE 0.05
#define BYZANTINE_MAX_DEVIATION 0.10

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
	bool is_byzantine;
} musician_t;

extern musician_t musicians[MAX_MUSICIANS];
extern int num_musicians;
extern double conductor_bpm;
extern int conductor_chid;
extern int coids_to_musicians[MAX_MUSICIANS];
extern volatile bool program_running;
extern const char *musician_names[MAX_MUSICIANS];
extern int byzantine_count;

void* conductor_thread(void *arg);
void* musician_thread(void *arg);
int parse_arguments(int argc, char *argv[]);
const char* select_piece();
int initialize_musicians(const char *notes[MAX_MUSICIANS][MAX_NOTES]);
void assign_byzantine_musicians();
double add_normal_variance(double bpm);
double add_byzantine_variance(double bpm);
int read_notes_from_file(const char *filename,
const char *musician_names[MAX_MUSICIANS],
const char *notes[MAX_MUSICIANS][MAX_NOTES]);
void free_notes_memory(const char *notes[MAX_MUSICIANS][MAX_NOTES]);
void cleanup_resources();

#endif
