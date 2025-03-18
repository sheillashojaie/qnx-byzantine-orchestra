#include "byzantine_orchestra.h"

musician_t musicians[MAX_MUSICIANS];
int num_musicians = 0;
double conductor_bpm = DEFAULT_BPM;
int conductor_chid;
int coids_to_musicians[MAX_MUSICIANS];
volatile bool program_running = true;
int byzantine_count = 0;

const char *musician_names[MAX_MUSICIANS] = { "Melody", "Harmony", "Bass",
		"Counter-Melody", "Rhythm" };

int main(int argc, char *argv[]) {
	srand(time(NULL)); // Seed random number

	if (parse_arguments(argc, argv) != 0) {
		return 1;
	}

	const char *filename = select_piece();
	if (!filename) {
		return 1;
	}

	const char *notes[MAX_MUSICIANS][MAX_NOTES];
	if (read_notes_from_file(filename, musician_names, notes) == -1) {
		printf("Could not read notes from file\n");
		return 1;
	}

	assign_byzantine_musicians();

	printf("Byzantine Orchestra starting with %d musicians at %.1f BPM\n\n",
			num_musicians, conductor_bpm);

	conductor_chid = ChannelCreate(0);
	if (conductor_chid == -1) {
		perror("Could not create conductor channel");
		return 1;
	}

	if (initialize_musicians(notes) != 0) {
		cleanup_resources();
		return 1;
	}

	pthread_t conductor;
	pthread_attr_t attr;
	struct sched_param param;

	pthread_attr_init(&attr);

	// Set scheduling policy to real-time and set conductor priority
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	param.sched_priority = 50;

	pthread_attr_setschedparam(&attr, &param);
	pthread_create(&conductor, &attr, conductor_thread, NULL);
	pthread_attr_destroy(&attr);
	pthread_join(conductor, NULL);

	program_running = false;

	cleanup_resources();
	return 0;
}
