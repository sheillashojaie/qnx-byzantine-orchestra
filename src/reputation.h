#ifndef REPUTATION_H
#define REPUTATION_H

void initialize_reputation_system();
void cleanup_reputation_system();
void update_reputation(int musician_id, double behaviour_score);
void process_reputation_votes();
void cast_reputation_vote(int voter_id, int target_id, bool is_negative);
bool should_blacklist_musician(int musician_id);
void blacklist_musician(int musician_id);
double calculate_behaviour_score(double reported_bpm, double expected_bpm);
void decay_all_reputations();
bool is_musician_trusted(int musician_id);
void print_reputation_status();

#endif
