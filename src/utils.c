#include "byzantine_orchestra.h"
#include <ctype.h>

double add_normal_variance(double bpm) {
	// Add a small variance (+/- 5%)
	double variance = ((rand() % 1000) / 10000.0) - 0.05;
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

	char line[256];
	int musician_index = 0;

	while (fgets(line, sizeof(line), file)) {
		if (musician_index >= MAX_MUSICIANS) {
			break;
		}

		int i = strlen(line) - 1;
		while (i >= 0
				&& (line[i] == ' ' || line[i] == '\n' || line[i] == '\r'
						|| line[i] == '\t')) {
			line[i] = '\0';
			i--;
		}

		char *p = line;
		int note_index = 0;

		while (*p && note_index < MAX_NOTES) {
			while (*p && isspace(*p))
				p++;
			if (!*p)
				break;  // End of line

			// Start of token
			char *start = p;

			// End of token
			while (*p && !isspace(*p))
				p++;

			// Null-terminate
			if (*p)
				*p++ = '\0';

			notes[musician_index][note_index] = strdup(start);
			note_index++;
		}

		musician_index++;
	}

	fclose(file);
	// Return the number of musicians with notes
	return musician_index;
}
