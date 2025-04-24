#include <byzantine_orchestra.h>

typedef struct {
	char note[20];
	double bpm;
	double timestamp;
	int musician_id;
} note_event_t;

typedef struct {
	note_event_t events[MAX_HISTORY];
	int count;
	double start_time;
	double max_deviation_percent;
} visualization_t;

// ANSI escape codes for distinct musician colours
const char *COLOURS[] = { "\033[38;5;226m", // Yellow
		"\033[38;5;214m", // Orange
		"\033[38;5;210m", // Coral
		"\033[38;5;198m", // Hot Pink
		"\033[38;5;201m", // Pink
		"\033[38;5;207m", // Magenta
		"\033[38;5;213m", // Pale Pink
		};

const char *RESET_COLOUR = "\033[0m";

visualization_t viz = { 0 };
volatile bool viz_running = true;

void play_note_with_viz(musician_t *musician, double reported_bpm) {
    play_note(musician, reported_bpm);

    // Log the note
    add_note_event(musician->notes[musician->note_index], musician->perceived_bpm, musician->id);
}

int initialize_visualization() {
	viz.count = 0;
	viz.max_deviation_percent = BYZANTINE_MAX_DEVIATION * 100;
	viz.start_time = time(NULL);

	pthread_t viz_thread;
	if (pthread_create(&viz_thread, NULL, visualization_thread, NULL) != 0) {
		perror("Could not create visualization thread");
		return -1;
	}
	pthread_detach(viz_thread);

	return 0;
}

void add_note_event(const char *note, double bpm, int musician_id) {
	if (viz.count >= MAX_HISTORY) {
		memmove(viz.events, viz.events + 1,
				sizeof(note_event_t) * (MAX_HISTORY - 1)); // Shift older events
		viz.count = MAX_HISTORY - 1;
	}

	// Store the new note event
	strncpy(viz.events[viz.count].note, note,
			sizeof(viz.events[viz.count].note) - 1);
	viz.events[viz.count].note[sizeof(viz.events[viz.count].note) - 1] = '\0';
	viz.events[viz.count].bpm = bpm;
	viz.events[viz.count].timestamp = time(NULL) - viz.start_time;
	viz.events[viz.count].musician_id = musician_id;

	viz.count++;
}

void* visualization_thread(void *arg) {
	(void) arg;
	while (viz_running && program_running) {
		printf("\033[2J\033[H"); // Clear screen, move cursor to top left
		usleep(REFRESH_INTERVAL_MS * 1000); // Wait before next frame
	}
	viz_running = false;
	return NULL;
}

bool is_within_bounds(int x, int y) {
    return x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT;
}

bool is_overwritable_char(char c) {
    return c == ' ' || c == '.';
}

void draw_musician_line(char display[DISPLAY_HEIGHT][DISPLAY_WIDTH + 1],
                        int colour_map[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                        int x1, int y1, int x2, int y2,
                        char symbol, int musician_id) {
    int delta_x = abs(x2 - x1);
    int delta_y = abs(y2 - y1);
    int step_x = (x1 < x2) ? 1 : -1;
    int step_y = (y1 < y2) ? 1 : -1;
    int error = delta_x - delta_y; // Initial error value for Bresenham's algorithm

    while (true) {
        if (is_within_bounds(x1, y1) && is_overwritable_char(display[y1][x1])) {
            display[y1][x1] = symbol;
            if (colour_map[y1][x1] == -1) {
                colour_map[y1][x1] = musician_id; // Assign colour only if unset
            }
        }

        if (x1 == x2 && y1 == y2) break; // End of line

        int double_error = 2 * error;
        if (double_error > -delta_y) {
            error -= delta_y;
            x1 += step_x;
        }
        if (double_error < delta_x) {
            error += delta_x;
            y1 += step_y;
        }
    }
}

void draw_visualization(double target_bpm, double elapsed_seconds) {

	printf("QNX Byzantine Orchestra - Target BPM: %.1f\n", target_bpm);

	char display[DISPLAY_HEIGHT][DISPLAY_WIDTH + 1];
	int colour_map[DISPLAY_HEIGHT][DISPLAY_WIDTH];

	// Initialize display and colour map
	for (int y = 0; y < DISPLAY_HEIGHT; y++) {
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
			display[y][x] = ' ';
			colour_map[y][x] = -1;
		}
		display[y][DISPLAY_WIDTH] = '\0';
	}

	// Print display
	for (int y = 0; y < DISPLAY_HEIGHT; y++) {
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
			printf("%c", display[y][x]);
		}
		printf("\n");
	}
}
