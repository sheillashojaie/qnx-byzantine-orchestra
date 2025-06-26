#include <byzantine_orchestra.h>

typedef struct {
    char note[20];
    double bpm;
    double timestamp;
    int musician_id;
    bool was_blacklisted;
} note_event_t;

typedef struct {
    note_event_t events[MAX_HISTORY];
    int count;
    double start_time;
    double max_deviation_percent;
} visualization_t;

// ANSI escape codes for distinct musician colours
const char *COLOURS[] = {
    "\033[38;5;226m", // Yellow
    "\033[38;5;214m", // Orange
    "\033[38;5;210m", // Coral
    "\033[38;5;198m", // Hot Pink
    "\033[38;5;201m", // Pink
    "\033[38;5;207m", // Magenta
    "\033[38;5;213m", // Light Pink
};

const char *BLACKLIST_COLOUR = "\033[38;5;252m"; //  Grey
const char *RESET_COLOUR = "\033[0m";

visualization_t viz = { 0 };
volatile bool viz_running = true;

void play_note_with_viz(musician_t *musician) {
    play_note(musician);
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
    // If history is full, shift events to make room for new event
    if (viz.count >= MAX_HISTORY) {
        memmove(viz.events, viz.events + 1,
                sizeof(note_event_t) * (MAX_HISTORY - 1));
        viz.count = MAX_HISTORY - 1;
    }

    // Store the new note event
    strncpy(viz.events[viz.count].note, note,
            sizeof(viz.events[viz.count].note) - 1);
    viz.events[viz.count].note[sizeof(viz.events[viz.count].note) - 1] = '\0';
    viz.events[viz.count].bpm = bpm;
    viz.events[viz.count].timestamp = time(NULL) - viz.start_time;
    viz.events[viz.count].musician_id = musician_id;
    viz.events[viz.count].was_blacklisted = musicians[musician_id].is_blacklisted;

    viz.count++;
}

void* visualization_thread(void *arg) {
    while (viz_running && program_running) {
        printf("\033[2J\033[H"); // Clear screen, move cursor to top left
        draw_visualization(target_bpm, time(NULL) - viz.start_time);
        usleep(REFRESH_INTERVAL_MS * 1000); // Wait before next frame
    }
    viz_running = false;
    return NULL;
}

void draw_musician_line(char display[DISPLAY_HEIGHT][DISPLAY_WIDTH + 1],
        int colour_map[DISPLAY_HEIGHT][DISPLAY_WIDTH], int x1, int y1, int x2,
        int y2, int musician_id) {
    // Absolute differences in x and y
    int delta_x = abs(x2 - x1);
    int delta_y = abs(y2 - y1);

    // Which direction to step in x and y
    int step_x = (x1 < x2) ? 1 : -1;
    int step_y = (y1 < y2) ? 1 : -1;

    int error = delta_x - delta_y; // Initial error value for Bresenham's algorithm

    while (true) {
        // Check is within bounds and is an overwritable character
        if (x1 >= 0 && x1 < DISPLAY_WIDTH && y1 >= 0 && y1 < DISPLAY_HEIGHT
                && (display[y1][x1] == ' ' || display[y1][x1] == '.')) {
            display[y1][x1] = '.';
            if (colour_map[y1][x1] == -1) {
                colour_map[y1][x1] = musician_id; // Assign colour only if unset
            }
        }

        if (x1 == x2 && y1 == y2) // End of line
            break;

        // Bresenham's algorithm to connect the two points
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

// Plotting helpers

int map_time_to_x(double t, double start_time, int display_time_range,
        int stretch_factor) {
    double visible_duration = display_time_range * stretch_factor;
    double time_offset = t - start_time;
    double normalized_time = time_offset / visible_duration;
    int x = (int) (normalized_time * (DISPLAY_WIDTH - 1) * stretch_factor);
    return x;
}

int map_bpm_to_y(double bpm, double min_bpm, double max_bpm) {
    double range = max_bpm - min_bpm;
    if (range == 0) return DISPLAY_HEIGHT / 2;
    double normalized = (bpm - min_bpm) / range;
    int y = DISPLAY_HEIGHT - 1 - (int)(normalized * (DISPLAY_HEIGHT - 1));
    if (y < 0) y = 0;
    if (y >= DISPLAY_HEIGHT) y = DISPLAY_HEIGHT - 1;
    return y;
}

void draw_visualization(double target_bpm, double elapsed_seconds) {
    int display_time_range = 20;
    double start_time = elapsed_seconds - display_time_range;
    if (start_time < 0)
        start_time = 0;

    int stretch_factor = 2;
    double min_bpm = MIN_BPM - 20;
    double max_bpm = MAX_BPM + 20;

    printf("QNX Byzantine Orchestra - BPM: %.1f\n", conductor_bpm);

    // Print legend with reputation scores and blacklist status
    for (int i = 0; i < num_musicians; i++) {
        const char *musician_name = musician_names[i];
        const char *status = "";
        const char *colour = COLOURS[i % (sizeof(COLOURS) / sizeof(COLOURS[0]))];

        if (musicians[i].is_blacklisted) {
            status = "[BLACKLISTED]";
            colour = BLACKLIST_COLOUR;
        } else if (musicians[i].is_byzantine && musicians[i].is_first_chair) {
            status = "[FIRST CHAIR, BYZANTINE]";
        } else if (musicians[i].is_first_chair) {
            status = "[FIRST CHAIR]";
        } else if (musicians[i].is_byzantine) {
            status = "[BYZANTINE]";
        }

        printf("%s%s%s %s (%.1f rep)%s  ",
               colour, musician_name, RESET_COLOUR, status,
               musicians[i].reputation, RESET_COLOUR);
    }
    printf("\n");

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

    int last_x[MAX_MUSICIANS];
    int last_y[MAX_MUSICIANS];
    bool has_last_pos[MAX_MUSICIANS] = { false };

    for (int i = 0; i < viz.count; i++) {
        double t = viz.events[i].timestamp;
        if (t < start_time) // Skip events outside display window
            continue;

        // Calculate positions
        int x = map_time_to_x(t, start_time, display_time_range, stretch_factor);
        int y = map_bpm_to_y(viz.events[i].bpm, min_bpm, max_bpm);
        int m_id = viz.events[i].musician_id;

        // Draw connecting line including old notes from before blacklisting
        if (has_last_pos[m_id]) {
            draw_musician_line(display, colour_map, last_x[m_id], last_y[m_id],
                    x, y, m_id);
        }

        // Update last position
        last_x[m_id] = x;
        last_y[m_id] = y;
        has_last_pos[m_id] = true;

        // Display note
        for (int j = 0; j < strlen(viz.events[i].note); j++) {
            int px = x + j;
            if (px < DISPLAY_WIDTH) {
                display[y][px] = viz.events[i].note[j];
                colour_map[y][px] = m_id;
            }
        }
    }

    // Print display
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        double bpm_at_y = max_bpm - ((double)y / (DISPLAY_HEIGHT - 1)) * (max_bpm - min_bpm);
        printf("%5.1f ", bpm_at_y);

        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            char c = display[y][x];
            int m_id = colour_map[y][x];

            if (m_id >= 0) {

                const char *colour = musicians[m_id].is_blacklisted ?
                    BLACKLIST_COLOUR :
                    COLOURS[m_id % (sizeof(COLOURS) / sizeof(COLOURS[0]))];

                printf("%s%c%s", colour, c, RESET_COLOUR);
            } else {
                printf("%c", c);
            }
        }
        printf("\n");
    }

    for (int i = 0; i < DISPLAY_WIDTH - 16; i++)
        printf(" ");
    printf("%.1fs\n", elapsed_seconds);
}
