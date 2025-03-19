#include "byzantine_orchestra.h"

int parse_arguments(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <num_musicians> <bpm>\n", argv[0]);
		return 1;
	}

	num_musicians = atoi(argv[1]);
	if (num_musicians < 3 || num_musicians > MAX_MUSICIANS) {
		printf("Number of musicians must be between 3 and %d\n", MAX_MUSICIANS);
		return 1;
	}

	conductor_bpm = atof(argv[2]);
	if (conductor_bpm <= 0) {
		printf("BPM must be a positive number\n");
		return 1;
	}

	return 0;
}

const char* select_piece() {
	printf("Select a piece:\n");
	printf("1. Night on Bald Mountain\n");
	int piece_selection;
	scanf("%d", &piece_selection);

	switch (piece_selection) {
	case 1:
		return "../src/night_on_bald_mountain.txt";
	default:
		printf("Invalid choice\n");
		return NULL;
	}
}

int initialize_musicians(const char *notes[MAX_MUSICIANS][MAX_NOTES]) {
	for (int i = 0; i < num_musicians; i++) {
		musicians[i].id = i;
		musicians[i].perceived_bpm = conductor_bpm;
		musicians[i].notes = notes[i];
		musicians[i].note_index = 0;
		musicians[i].name = musician_names[i];

		musicians[i].chid = ChannelCreate(0);
		if (musicians[i].chid == -1) {
			perror("Could not create musician channel");
			return -1;
		}

		musicians[i].coid_to_conductor = ConnectAttach(0, 0, conductor_chid, 0,
				0);
		if (musicians[i].coid_to_conductor == -1) {
			perror("Could not create connection to conductor");
			return -1;
		}

		coids_to_musicians[i] = ConnectAttach(0, 0, musicians[i].chid, 0, 0);
		if (coids_to_musicians[i] == -1) {
			perror("Conductor could not create connection to musician");
			return -1;
		}

		pthread_create(&musicians[i].thread, NULL, musician_thread,
				&musicians[i]);
	}
	return 0;
}

void assign_byzantine_musicians() {
	// 1 to fewer than half of musicians
	byzantine_count = 1 + (rand() % (num_musicians / 2));

	for (int i = 0; i < num_musicians; i++) {
		musicians[i].is_byzantine = false;
	}

	for (int i = 0; i < byzantine_count; i++) {
		int index;
		do {
			index = rand() % num_musicians;
		} while (musicians[index].is_byzantine);

		musicians[index].is_byzantine = true;
		printf("%s will be a byzantine musician\n", musician_names[index]);
	}
}

void update_musician_bpm(musician_t *musician, bool byzantine_timing) {
    if (byzantine_timing) {
        musician->perceived_bpm = add_byzantine_variance(conductor_bpm);
    } else {
        musician->perceived_bpm = add_normal_variance(conductor_bpm);
    }
}

double get_reported_bpm(musician_t *musician, bool report_honestly) {
    if (report_honestly) {
        return musician->perceived_bpm;
    } else {
        return add_normal_variance(conductor_bpm);
    }
}

void play_note(musician_t *musician, double reported_bpm) {
    const char *status = musician->is_byzantine ? "[BYZANTINE]" : "";

    if (musician->is_byzantine) {
        printf("%s %s: Playing %s at %.1f BPM (reporting: %.1f BPM)\n",
               musician->name, status,
               musician->notes[musician->note_index],
               musician->perceived_bpm, reported_bpm);
    } else {
        printf("%s: Playing %s at %.1f BPM\n",
               musician->name,
               musician->notes[musician->note_index],
               musician->perceived_bpm);
    }
}

double add_normal_variance(double bpm) {
	// Add a small variance (+/- 5%)
	double variance = ((rand() % 1000) / 10000.0) - BPM_TOLERANCE;
	return bpm * (1.0 + variance);
}

double add_byzantine_variance(double bpm) {
	// Add deviation at least BPM_TOLERANCE and at most BYZANTINE_MAX_DEVIATION
	double max_deviation = BYZANTINE_MAX_DEVIATION;
	double variance;

	if (rand() % 2) {
		// Too fast
		variance = BPM_TOLERANCE
				+ (max_deviation - BPM_TOLERANCE) * (rand() % 1000) / 1000.0;
	} else {
		// Too slow
		variance = -BPM_TOLERANCE
				- (max_deviation - BPM_TOLERANCE) * (rand() % 1000) / 1000.0;
	}

	return bpm * (1.0 + variance);
}

int read_notes_from_file(const char *filename,
	const char *musician_names[MAX_MUSICIANS],
	const char *notes[MAX_MUSICIANS][MAX_NOTES]) {

	FILE *file = fopen(filename, "r");

	if (!file) {
		perror("Failed to open file");
		return -1;
	}

	for (int i = 0; i < MAX_MUSICIANS; i++) {
		for (int j = 0; j < MAX_NOTES; j++) {
			notes[i][j] = NULL;
		}
	}

	char line[256];
	int musician_index = 0;

	while (fgets(line, sizeof(line), file)) {
		int i = strlen(line) - 1;
		while (i >= 0 && (line[i] == ' ' || line[i] == '\n' || line[i] == '\r')) {
			line[i] = '\0';
			i--;
		}

		char *p = line;
		int note_index = 0;

		while (*p && note_index < MAX_NOTES) {
			while (*p && isspace(*p))
				p++;
			if (!*p)
				break; // End of line
			// Start of token
			char *start = p;
			// End of token
			while (*p && !isspace(*p))
				p++;
			// Null-terminate
			if (*p)
				*p++ = '\0';

			char *note_copy = strdup(start);
			if (note_copy == NULL) {
				perror("Could not allocate memory for note");
				// Clean up already allocated memory
				free_notes_memory(notes);
				fclose(file);
				return -1;
			}
			notes[musician_index][note_index] = note_copy;
			note_index++;
		}

		musician_index++;
	}

	fclose(file);
	// Return the number of musicians with notes
	return musician_index;
}

void free_notes_memory(const char *notes[MAX_MUSICIANS][MAX_NOTES]) {
	for (int i = 0; i < MAX_MUSICIANS; i++) {
		for (int j = 0; j < MAX_NOTES; j++) {
			if (notes[i][j] != NULL) {
				free((void*) notes[i][j]);
			}
		}
	}
}

void cleanup_resources() {
	for (int i = 0; i < num_musicians; i++) {
		pthread_cancel(musicians[i].thread);
		ConnectDetach(musicians[i].coid_to_conductor);
		ConnectDetach(coids_to_musicians[i]);
		ChannelDestroy(musicians[i].chid);
	}
	ChannelDestroy(conductor_chid);
}
