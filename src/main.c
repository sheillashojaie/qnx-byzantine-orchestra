#include "byzantine_orchestra.h"

musician_t musicians[MAX_MUSICIANS];
int num_musicians = 0;
double conductor_bpm = DEFAULT_BPM;
int conductor_chid;
int coids_to_musicians[MAX_MUSICIANS];
volatile bool program_running = true;

const char *musician_names[MAX_MUSICIANS] = {
    "Melody", "Harmony", "Bass", "Counter-Melody", "Rhythm"
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <num_musicians> <bpm>\n", argv[0]);
        return 1;
    }

    num_musicians = atoi(argv[1]);
    if (num_musicians < 1 || num_musicians > MAX_MUSICIANS) {
        printf("Number of musicians must be between 1 and %d\n", MAX_MUSICIANS);
        return 1;
    }

    // Set BPM from command-line argument
    conductor_bpm = atof(argv[2]);
    if (conductor_bpm <= 0) {
        printf("BPM must be a positive number\n");
        return 1;
    }

    // Prompt user to select a piece
    printf("Select a piece:\n");
    printf("1. Night on Bald Mountain\n");
    int piece_selection;
    scanf("%d", &piece_selection);

    const char *filename;
    switch (piece_selection) {
    case 1:
        filename = "../src/night_on_bald_mountain.txt";
        break;
    default:
        printf("Invalid choice\n");
        return 1;
    }

    // Read notes from the selected file
    const char *notes[MAX_MUSICIANS][MAX_NOTES];
    if (read_notes_from_file(filename, musician_names, notes) == -1) {
        printf("Failed to read notes from file\n");
        return 1;
    }

    printf("Byzantine Orchestra starting with %d musicians at %.1f BPM\n\n", num_musicians, conductor_bpm);

    // Create conductor channel
    conductor_chid = ChannelCreate(0);
    if (conductor_chid == -1) {
        perror("Could not create conductor channel");
        return 1;
    }

    // Initialize musicians and assign note sequences and names
    for (int i = 0; i < num_musicians; i++) {
        musicians[i].id = i;
        musicians[i].perceived_bpm = conductor_bpm;
        musicians[i].notes = notes[i];
        musicians[i].note_index = 0;
        musicians[i].name = musician_names[i];

        // Create individual channel for each musician
        musicians[i].chid = ChannelCreate(0);
        if (musicians[i].chid == -1) {
            perror("Could not create musician channel");
            // Clean up previously created channels
            for (int j = 0; j < i; j++) ChannelDestroy(musicians[j].chid);
            ChannelDestroy(conductor_chid);
            return 1;
        }

        // Pre-establish connection to conductor
        musicians[i].coid_to_conductor = ConnectAttach(0, 0, conductor_chid, 0, 0);
        if (musicians[i].coid_to_conductor == -1) {
            perror("Could not create connection to conductor");
            for (int j = 0; j <= i; j++) ChannelDestroy(musicians[j].chid);
            ChannelDestroy(conductor_chid);
            return 1;
        }

        // Pre-establish conductor's connection to musician
        coids_to_musicians[i] = ConnectAttach(0, 0, musicians[i].chid, 0, 0);
        if (coids_to_musicians[i] == -1) {
            perror("Conductor could not create connection to musician");
            // Clean up
            for (int j = 0; j <= i; j++) {
                ConnectDetach(coids_to_musicians[j]);
                ConnectDetach(musicians[j].coid_to_conductor);
                ChannelDestroy(musicians[j].chid);
            }
            ChannelDestroy(conductor_chid);
            return 1;
        }

        // Create musician thread
        pthread_create(&musicians[i].thread, NULL, musician_thread, &musicians[i]);
    }

    sleep(1);

    // Create conductor thread
    pthread_t conductor;
    pthread_create(&conductor, NULL, conductor_thread, NULL);
    pthread_join(conductor, NULL);

    // Signal threads to terminate
    program_running = false;
    sleep(1);

    // Clean up
    for (int i = 0; i < num_musicians; i++) {
        pthread_cancel(musicians[i].thread);
        ConnectDetach(musicians[i].coid_to_conductor);
        ConnectDetach(coids_to_musicians[i]);
        ChannelDestroy(musicians[i].chid);
    }
    ChannelDestroy(conductor_chid);

    return 0;
}
