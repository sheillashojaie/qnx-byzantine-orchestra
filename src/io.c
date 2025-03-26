#include <byzantine_orchestra.h>

int parse_arguments(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <num_musicians> <bpm>\n", argv[0]);
		return -1;
	}

    num_musicians = atoi(argv[1]);
    if (num_musicians < MIN_MUSICIANS || num_musicians > MAX_MUSICIANS) {
        printf("Number of musicians must be between %d and %d\n",
               MIN_MUSICIANS, MAX_MUSICIANS);
        return -1;
    }

	conductor_bpm = atof(argv[2]);
	if (conductor_bpm <= 0) {
		printf("BPM must be a positive number\n");
		return -1;
	}

	return 0;
}

const char* select_piece() {
	printf("Select a piece:\n");
	printf("1. O Fortuna\n");
	int piece_selection;
	scanf("%d", &piece_selection);

	switch (piece_selection) {
	case 1:
		return "../src/o_fortuna.txt";
	default:
		printf("Invalid choice\n");
		return NULL;
	}
}

int read_notes_from_file(const char *filename,
		const char *musician_names[MAX_MUSICIANS],
		const char *notes[MAX_MUSICIANS][MAX_NOTES]) {

	FILE *file = fopen(filename, "r");

	if (!file) {
		perror("Could not open file");
		return -1;
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
