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
		musicians[i].is_first_chair = (i == 0);

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
	deviation_type_t deviation_type;

	if (musician->is_byzantine && byzantine_timing) {
		deviation_type = DEVIATION_BYZANTINE;
	} else if (musician->is_first_chair) {
		deviation_type = DEVIATION_FIRST_CHAIR;
	} else {
		deviation_type = DEVIATION_NORMAL;
	}

	musician->perceived_bpm = add_variance(conductor_bpm, deviation_type);
}

double get_reported_bpm(musician_t *musician, bool report_honestly) {
	if (musician->is_first_chair) {
		if (musician->is_byzantine) {
			// First chair, if Byzantine, uses first chair deviation for reporting
			return add_variance(conductor_bpm, DEVIATION_FIRST_CHAIR);
		} else {
			// First chair, if not Byzantine, reports its actual perceived BPM
			return musician->perceived_bpm;
		}
	} else if (report_honestly) {
		return musician->perceived_bpm;
	} else {
		return add_variance(conductor_bpm, DEVIATION_NORMAL);
	}
}

void play_note(musician_t *musician, double reported_bpm) {
	const char *status = musician->is_byzantine ? "[BYZANTINE]" : "";

	if (musician->is_first_chair && musician->is_byzantine) {
		status = "[FIRST CHAIR, BYZANTINE]";
	} else if (musician->is_first_chair) {
		status = "[FIRST CHAIR]";
	} else if (musician->is_byzantine) {
		status = "[BYZANTINE]";
	}

	if (musician->is_byzantine || musician->is_first_chair) {
		printf("%s %s: Playing %s at %.1f BPM (reporting: %.1f BPM)\n",
				musician->name, status, musician->notes[musician->note_index],
				musician->perceived_bpm, reported_bpm);
	} else {
		printf("%s: Playing %s at %.1f BPM\n", musician->name,
				musician->notes[musician->note_index], musician->perceived_bpm);
	}
}

double add_variance(double bpm, deviation_type_t deviation_type) {
	double variance = 0.0;

	switch (deviation_type) {
	case DEVIATION_NORMAL:
		// Normal variance: +/- BPM_TOLERANCE
		variance = ((rand() % 1000) / 10000.0) - BPM_TOLERANCE;
		break;

	case DEVIATION_BYZANTINE:
		// Byzantine variance: at least BPM_TOLERANCE, at most BYZANTINE_MAX_DEVIATION
		if (rand() % 2) {
			// Too fast
			variance = BPM_TOLERANCE
					+ (BYZANTINE_MAX_DEVIATION - BPM_TOLERANCE)
							* (rand() % 1000) / 1000.0;
		} else {
			// Too slow
			variance = -BPM_TOLERANCE
					- (BYZANTINE_MAX_DEVIATION - BPM_TOLERANCE)
							* (rand() % 1000) / 1000.0;
		}
		break;

	case DEVIATION_FIRST_CHAIR:
		// First chair variance: +/- FIRST_CHAIR_MAX_DEVIATION
		variance = ((rand() % 400) / 10000.0) - FIRST_CHAIR_MAX_DEVIATION;
		break;

	default:
		variance = 0.0;
		break;
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
