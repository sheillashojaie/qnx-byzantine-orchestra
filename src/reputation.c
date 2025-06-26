#include <byzantine_orchestra.h>

static reputation_vote_t vote_buffer[MAX_MUSICIANS * MAX_MUSICIANS];
static int vote_count = 0;
pthread_mutex_t reputation_mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_reputation_system() {
    pthread_mutex_lock(&reputation_mutex);

    for (int i = 0; i < num_musicians; i++) {
        musicians[i].reputation = INITIAL_REPUTATION;
        musicians[i].is_blacklisted = false;
        musicians[i].last_reported_bpm = conductor_bpm;
        musicians[i].blacklist_time = 0;
    }

    vote_count = 0;

    pthread_mutex_unlock(&reputation_mutex);
}

void cleanup_reputation_system() {
    if (pthread_mutex_trylock(&reputation_mutex) == 0) {
        pthread_mutex_unlock(&reputation_mutex);
    }

    usleep(10000);

    pthread_mutex_destroy(&reputation_mutex);
}

double calculate_behaviour_score(double reported_bpm, double expected_bpm) {
    double deviation = fabs(reported_bpm - expected_bpm) / expected_bpm;

    if (deviation <= BPM_TOLERANCE) {
        return GOOD_BEHAVIOR_REWARD;
    } else if (deviation <= BYZANTINE_MAX_DEVIATION) {
        return -BAD_BEHAVIOR_PENALTY * (deviation / BYZANTINE_MAX_DEVIATION);
    } else {
        return -EXTREME_BEHAVIOR_PENALTY;
    }
}

void update_reputation(int musician_id, double behaviour_score) {
    if (musician_id < 0 || musician_id >= num_musicians) return;

    pthread_mutex_lock(&reputation_mutex);

    musician_t *musician = &musicians[musician_id];

    musician->reputation += behaviour_score;

    if (musician->reputation > MAX_REPUTATION) {
        musician->reputation = MAX_REPUTATION;
    } else if (musician->reputation < MIN_REPUTATION) {
        musician->reputation = MIN_REPUTATION;
    }

    // Check for blacklisting
    if (should_blacklist_musician(musician_id) && !musician->is_blacklisted) {
        blacklist_musician(musician_id);
    }

    pthread_mutex_unlock(&reputation_mutex);
}

void cast_reputation_vote(int voter_id, int target_id, bool is_negative) {
    if (voter_id < 0 || voter_id >= num_musicians ||
        target_id < 0 || target_id >= num_musicians ||
        voter_id == target_id || musicians[voter_id].is_blacklisted) {
        return;
    }

    pthread_mutex_lock(&reputation_mutex);

    if (vote_count < MAX_MUSICIANS * MAX_MUSICIANS) {
        vote_buffer[vote_count].voter_id = voter_id;
        vote_buffer[vote_count].target_id = target_id;
        vote_buffer[vote_count].is_negative = is_negative;
        vote_buffer[vote_count].timestamp = time(NULL);
        vote_count++;
    }

    pthread_mutex_unlock(&reputation_mutex);
}

void process_reputation_votes() {
    if (vote_count == 0) return;

    pthread_mutex_lock(&reputation_mutex);

    int negative_votes[MAX_MUSICIANS] = {0};
    int positive_votes[MAX_MUSICIANS] = {0};
    int total_voters = 0;

    // Count non-blacklisted voters
    for (int i = 0; i < num_musicians; i++) {
        if (!musicians[i].is_blacklisted) {
            total_voters++;
        }
    }

    if (total_voters == 0) {
        vote_count = 0;
        pthread_mutex_unlock(&reputation_mutex);
        return;
    }

    // Process votes
    for (int i = 0; i < vote_count; i++) {
        int voter = vote_buffer[i].voter_id;
        int target = vote_buffer[i].target_id;

        // Only count votes from non-blacklisted musicians
        if (!musicians[voter].is_blacklisted) {
            if (vote_buffer[i].is_negative) {
                negative_votes[target]++;
            } else {
                positive_votes[target]++;
            }
        }
    }

    // Apply reputation changes based on consensus
    for (int i = 0; i < num_musicians; i++) {
        if (musicians[i].is_blacklisted) continue;

        double negative_ratio = (double)negative_votes[i] / total_voters;
        double positive_ratio = (double)positive_votes[i] / total_voters;

        if (negative_ratio >= CONSENSUS_THRESHOLD) {
            musicians[i].reputation -= BAD_BEHAVIOR_PENALTY;
            printf("Consensus negative vote against %s (%.1f%% voted negative)\n",
                   musicians[i].name, negative_ratio * 100);
        } else if (positive_ratio >= CONSENSUS_THRESHOLD) {
            musicians[i].reputation += GOOD_BEHAVIOR_REWARD;
        }

        if (musicians[i].reputation > MAX_REPUTATION) {
            musicians[i].reputation = MAX_REPUTATION;
        } else if (musicians[i].reputation < MIN_REPUTATION) {
            musicians[i].reputation = MIN_REPUTATION;
        }

        // Check for blacklisting
        if (should_blacklist_musician(i) && !musicians[i].is_blacklisted) {
            blacklist_musician(i);
        }
    }

    vote_count = 0;

    pthread_mutex_unlock(&reputation_mutex);
}

bool should_blacklist_musician(int musician_id) {
    if (musician_id < 0 || musician_id >= num_musicians) return false;

    musician_t *musician = &musicians[musician_id];

    // First chair musicians have a lower threshold
    double threshold = musician->is_first_chair ? FIRST_CHAIR_THRESHOLD : BLACKLIST_THRESHOLD;

    return musician->reputation <= threshold;
}

void blacklist_musician(int musician_id) {
    if (musician_id < 0 || musician_id >= num_musicians) return;

    musician_t *musician = &musicians[musician_id];

    if (!musician->is_blacklisted) {
        musician->is_blacklisted = true;
        musician->blacklist_time = time(NULL);

        const char *status = musician->is_first_chair ? "[FIRST CHAIR]" : "";
        printf("*** %s %s has been BLACKLISTED (reputation: %.1f) ***\n",
               musician->name, status, musician->reputation);
    }
}

void decay_all_reputations() {
    pthread_mutex_lock(&reputation_mutex);

    for (int i = 0; i < num_musicians; i++) {
        if (!musicians[i].is_blacklisted) {
            musicians[i].reputation *= REPUTATION_DECAY_RATE;

            if (musicians[i].reputation < MIN_REPUTATION) {
                musicians[i].reputation = MIN_REPUTATION;
            }
        }
    }

    pthread_mutex_unlock(&reputation_mutex);
}

bool is_musician_trusted(int musician_id) {
    if (musician_id < 0 || musician_id >= num_musicians) return false;

    return !musicians[musician_id].is_blacklisted &&
           musicians[musician_id].reputation >= BLACKLIST_THRESHOLD;
}

void print_reputation_status() {
    pthread_mutex_lock(&reputation_mutex);

    printf("\nReputation Status\n");
    for (int i = 0; i < num_musicians; i++) {
        const char *status = "";
        if (musicians[i].is_blacklisted) {
            status = "[BLACKLISTED]";
        } else if (musicians[i].is_first_chair) {
            status = "[FIRST CHAIR]";
        } else if (musicians[i].is_byzantine) {
            status = "[BYZANTINE]";
        }

        printf("%s %s: %.1f reputation\n",
               musicians[i].name, status, musicians[i].reputation);
    }

    pthread_mutex_unlock(&reputation_mutex);
}
