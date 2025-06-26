#include <byzantine_orchestra.h>

musician_t musicians[MAX_MUSICIANS];
int num_musicians = 0;
double conductor_bpm = DEFAULT_BPM;
double target_bpm = DEFAULT_BPM;
int conductor_chid;
int coids_to_musicians[MAX_MUSICIANS];
volatile bool program_running = true;
int byzantine_count = 0;

const char *musician_names[MAX_MUSICIANS] = {
    "Melody", "Harmony", "Bass", "Counter-Melody",
    "Percussion", "Fill", "Rhythm"
};

int main(int argc, char *argv[]) {
    srand(time(NULL)); // Seed random number

    program_running = true;
    viz_running = true;

    if (parse_arguments(argc, argv) != 0) {
        return -1;
    }

    const char *filename = select_piece();
    if (!filename) {
        return -1;
    }

    if (initialize_orchestra(filename) != 0) {
        printf("Could not initialize orchestra\n");
        cleanup_resources();
        return -1;
    }

    if (initialize_visualization() != 0) {
        printf("Could not initialize visualization\n");
        cleanup_resources();
        return -1;
    }

    pthread_t conductor;
    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);

    // Set scheduling policy to real-time and set conductor priority
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = PRIORITY_CONDUCTOR;

    pthread_attr_setschedparam(&attr, &param);
    if (pthread_create(&conductor, &attr, conductor_thread, NULL) != 0) {
        perror("Could not create conductor thread");
        pthread_attr_destroy(&attr);
        cleanup_resources();
        return -1;
    }
    pthread_attr_destroy(&attr);

    pthread_join(conductor, NULL);

    program_running = false;
    viz_running = false;

    usleep(200000);

    print_reputation_status();

    cleanup_resources();
    return 0;
}
