#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "Evaluate.cpp"
#include "Random.cpp"

using namespace std;
using Array = vector <vector <int>>;

namespace {

constexpr int rows = 8;
constexpr int cols = 14;
constexpr int digits = 10;
constexpr int reheat_after = 8000;
constexpr int elite_archive_capacity = 16;
constexpr int elite_restart_choices = 12;
constexpr int elite_restart_mutations = 6;
constexpr int random_restart_percent = 20;
constexpr int local_improvement_rounds = 1;
constexpr int local_improvement_probes = 2;
constexpr int local_population_size = 8;
constexpr int tournament_size = 3;
constexpr int local_elite_keep = 2;
constexpr int local_stable_keep = 4;
constexpr int population_refresh_count = 3;
constexpr int crossover_percent_progress = 5;
constexpr int crossover_percent_mid_stagnation = 20;
constexpr int crossover_percent_high_stagnation = 45;
constexpr int immigrant_percent_progress = 3;
constexpr int immigrant_percent_mid_stagnation = 8;
constexpr int immigrant_percent_high_stagnation = 16;
constexpr int migration_pool_capacity = 16;
constexpr int migration_interval = 2400;
constexpr int migration_import_count = 1;
constexpr int migration_import_choices = 8;
constexpr int newborn_tenure_progress = 6;
constexpr int newborn_tenure_mid_stagnation = 12;
constexpr int newborn_tenure_high_stagnation = 18;
constexpr int newborn_tenure_score_divisor = 700;
constexpr int newborn_tenure_score_bonus_cap = 12;
constexpr int newborn_tenure_total_cap = 30;
constexpr double exploration_temperature_progress = 0.20;
constexpr double exploration_temperature_mid_stagnation = 0.60;
constexpr double exploration_temperature_high_stagnation = 1.20;
constexpr double exploration_diversity_bonus_scale = 0.03;

struct Individual {
    Array board;
    Fitness fitness;
    int protected_turns = 0;
};

using EliteEntry = Individual;

struct MigrantEntry {
    Array board;
    Fitness fitness;
    size_t source_tag = 0;
};

mutex state_lock;
Array global_best_solution(rows, vector <int> (cols, 0));
Fitness global_best_fitness;
vector <EliteEntry> elite_archive;
vector <MigrantEntry> migration_pool;
chrono::steady_clock::time_point run_deadline = chrono::steady_clock::time_point::max();

void publish_global_best(const Array & board, const Fitness & fitness);

Array random_board() {
    Array board(rows, vector <int> (cols));

    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            board[i][j] = random(digits);
        }
    }

    return board;
}

void mutate_one_cell(Array & board) {
    const int x = random(rows);
    const int y = random(cols);
    const int old_digit = board[x][y];

    int new_digit = old_digit;
    while(new_digit == old_digit) {
        new_digit = random(digits);
    }
    board[x][y] = new_digit;
}

void mutate_swap_cells(Array & board) {
    for(int attempt = 0; attempt < 8; ++attempt) {
        const int x1 = random(rows);
        const int y1 = random(cols);
        const int x2 = random(rows);
        const int y2 = random(cols);

        if((x1 != x2 || y1 != y2) && board[x1][y1] != board[x2][y2]) {
            swap(board[x1][y1], board[x2][y2]);
            return;
        }
    }

    mutate_one_cell(board);
}

void mutate_patch(Array & board) {
    const int height = 1 + random(2);
    const int width = 2 + random(2);
    const int start_x = random(rows - height + 1);
    const int start_y = random(cols - width + 1);

    bool changed = false;
    for(int dx = 0; dx < height; ++dx) {
        for(int dy = 0; dy < width; ++dy) {
            const int x = start_x + dx;
            const int y = start_y + dy;
            const int old_digit = board[x][y];
            int new_digit = old_digit;

            while(new_digit == old_digit) {
                new_digit = random(digits);
            }
            board[x][y] = new_digit;
            changed = true;
        }
    }

    if(!changed) {
        mutate_one_cell(board);
    }
}

int digit_macro_cycle_size_for(int stagnant_steps) {
    const int roll = random(100);
    if(stagnant_steps < reheat_after / 3) {
        if(roll < 70) {
            return 2;
        }
        if(roll < 95) {
            return 3;
        }
        return 4;
    }

    if(stagnant_steps < (2 * reheat_after) / 3) {
        if(roll < 35) {
            return 2;
        }
        if(roll < 75) {
            return 3;
        }
        return 4;
    }

    if(roll < 20) {
        return 2;
    }
    if(roll < 60) {
        return 3;
    }
    return 4;
}

void mutate_digit_permutation(Array & board, int cycle_size) {
    vector <int> pool(digits);
    iota(pool.begin(), pool.end(), 0);
    for(int idx = 0; idx < cycle_size; ++idx) {
        const int pick = idx + random(digits - idx);
        swap(pool[idx], pool[pick]);
    }

    int mapping[digits];
    for(int digit = 0; digit < digits; ++digit) {
        mapping[digit] = digit;
    }
    for(int idx = 0; idx < cycle_size; ++idx) {
        mapping[pool[idx]] = pool[(idx + 1) % cycle_size];
    }

    bool changed = false;
    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            const int mapped_digit = mapping[board[i][j]];
            if(mapped_digit != board[i][j]) {
                board[i][j] = mapped_digit;
                changed = true;
            }
        }
    }

    if(!changed) {
        mutate_one_cell(board);
    }
}

void mutate_digit_macro(Array & board, int stagnant_steps) {
    mutate_digit_permutation(board, digit_macro_cycle_size_for(stagnant_steps));
}

void apply_search_move(Array & board, int stagnant_steps) {
    const int roll = random(100);
    if(stagnant_steps < reheat_after / 2) {
        if(roll < 70) {
            mutate_one_cell(board);
        } else if(roll < 90) {
            mutate_swap_cells(board);
        } else if(roll < 95) {
            mutate_patch(board);
        } else {
            mutate_digit_macro(board, stagnant_steps);
        }
    } else {
        if(roll < 35) {
            mutate_one_cell(board);
        } else if(roll < 60) {
            mutate_swap_cells(board);
        } else if(roll < 80) {
            mutate_patch(board);
        } else {
            mutate_digit_macro(board, stagnant_steps);
        }
    }
}

void apply_refinement_move(Array & board) {
    if(random(100) < 70) {
        mutate_one_cell(board);
    } else {
        mutate_swap_cells(board);
    }
}

void local_improve(Array & board, Fitness & fitness) {
    for(int round = 0; round < local_improvement_rounds; ++round) {
        Array best_trial = board;
        Fitness best_trial_fitness = fitness;
        bool improved = false;

        for(int probe = 0; probe < local_improvement_probes; ++probe) {
            Array trial = board;
            apply_refinement_move(trial);
            const Fitness trial_fitness = evaluate(trial);
            if(trial_fitness > best_trial_fitness) {
                best_trial = move(trial);
                best_trial_fitness = trial_fitness;
                improved = true;
            }
        }

        if(!improved) {
            break;
        }

        board = move(best_trial);
        fitness = best_trial_fitness;
    }
}

void sort_population(vector <Individual> & population) {
    stable_sort(population.begin(), population.end(), [] (const Individual & lhs, const Individual & rhs) {
        return lhs.fitness > rhs.fitness;
    });
}

int board_distance(const Array & lhs, const Array & rhs) {
    int distance = 0;
    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            if(lhs[i][j] != rhs[i][j]) {
                ++distance;
            }
        }
    }

    return distance;
}

int min_population_distance(const Array & board, const vector <Individual> & population, int excluded_index = -1) {
    int best_distance = rows * cols;
    for(int idx = 0; idx < static_cast <int> (population.size()); ++idx) {
        if(idx == excluded_index) {
            continue;
        }
        best_distance = min(best_distance, board_distance(board, population[idx].board));
    }

    return best_distance;
}

int tournament_pick(const vector <Individual> & population) {
    int best_index = random(static_cast <int> (population.size()));
    for(int pick = 1; pick < tournament_size; ++pick) {
        const int candidate_index = random(static_cast <int> (population.size()));
        if(population[candidate_index].fitness > population[best_index].fitness) {
            best_index = candidate_index;
        }
    }

    return best_index;
}

int tournament_pick_excluding(const vector <Individual> & population, int excluded_index) {
    if(static_cast <int> (population.size()) <= 1) {
        return excluded_index;
    }

    int picked_index = excluded_index;
    while(picked_index == excluded_index) {
        picked_index = tournament_pick(population);
    }

    return picked_index;
}

int random_pick_excluding(int size, int excluded_index) {
    int picked_index = excluded_index;
    while(picked_index == excluded_index) {
        picked_index = random(size);
    }

    return picked_index;
}

int immigrant_percent_for(int stagnant_steps) {
    if(stagnant_steps < reheat_after / 3) {
        return immigrant_percent_progress;
    }
    if(stagnant_steps < (2 * reheat_after) / 3) {
        return immigrant_percent_mid_stagnation;
    }
    return immigrant_percent_high_stagnation;
}

int newborn_tenure_for(int stagnant_steps, const Fitness & fitness) {
    int base_tenure = newborn_tenure_progress;
    if(stagnant_steps < reheat_after / 3) {
        base_tenure = newborn_tenure_progress;
    } else if(stagnant_steps < (2 * reheat_after) / 3) {
        base_tenure = newborn_tenure_mid_stagnation;
    } else {
        base_tenure = newborn_tenure_high_stagnation;
    }

    const int score_bonus = min(newborn_tenure_score_bonus_cap,
        max(0, fitness.prefix_score) / newborn_tenure_score_divisor);

    return min(newborn_tenure_total_cap, base_tenure + score_bonus);
}

int crossover_percent_for(int stagnant_steps) {
    if(stagnant_steps < reheat_after / 3) {
        return crossover_percent_progress;
    }
    if(stagnant_steps < (2 * reheat_after) / 3) {
        return crossover_percent_mid_stagnation;
    }
    return crossover_percent_high_stagnation;
}

int crossover_post_mutations_for(int stagnant_steps) {
    if(stagnant_steps < reheat_after / 2) {
        return 1;
    }
    if(stagnant_steps < (3 * reheat_after) / 4) {
        return 2;
    }
    return 3;
}

double exploration_temperature_for(int stagnant_steps) {
    if(stagnant_steps < reheat_after / 3) {
        return exploration_temperature_progress;
    }
    if(stagnant_steps < (2 * reheat_after) / 3) {
        return exploration_temperature_mid_stagnation;
    }
    return exploration_temperature_high_stagnation;
}

bool is_protected_tail_slot(const vector <Individual> & population, int index) {
    return index >= local_stable_keep && population[index].protected_turns > 0;
}

void age_population(vector <Individual> & population) {
    for(int idx = local_stable_keep; idx < static_cast <int> (population.size()); ++idx) {
        if(population[idx].protected_turns > 0) {
            --population[idx].protected_turns;
        }
    }
}

int worst_replaceable_tail_index(const vector <Individual> & population) {
    for(int idx = static_cast <int> (population.size()) - 1; idx >= local_stable_keep; --idx) {
        if(!is_protected_tail_slot(population, idx)) {
            return idx;
        }
    }

    return -1;
}

vector <int> replaceable_tail_indices(const vector <Individual> & population) {
    vector <int> indices;
    for(int idx = local_stable_keep; idx < static_cast <int> (population.size()); ++idx) {
        if(!is_protected_tail_slot(population, idx)) {
            indices.push_back(idx);
        }
    }

    return indices;
}

void place_individual(vector <Individual> & population, int index, Array board, const Fitness & fitness, int stagnant_steps) {
    population[index] = {move(board), fitness, 0};
    if(index >= local_stable_keep) {
        population[index].protected_turns = newborn_tenure_for(stagnant_steps, fitness);
    }
}

int pick_crossover_mate(const vector <Individual> & population, int parent_index, int stagnant_steps) {
    if(stagnant_steps < reheat_after / 2) {
        return tournament_pick_excluding(population, parent_index);
    }

    if(stagnant_steps < (3 * reheat_after) / 4) {
        return random_pick_excluding(static_cast <int> (population.size()), parent_index);
    }

    const int diverse_pool_begin = static_cast <int> (population.size()) / 2;
    const int offset = random(static_cast <int> (population.size()) - diverse_pool_begin);
    const int picked_index = diverse_pool_begin + offset;
    if(picked_index == parent_index) {
        return random_pick_excluding(static_cast <int> (population.size()), parent_index);
    }
    return picked_index;
}

void crossover_rectangle(Array & child, const Array & donor) {
    const int height = 1 + random(3);
    const int width = 2 + random(4);
    const int start_x = random(rows - height + 1);
    const int start_y = random(cols - width + 1);

    for(int dx = 0; dx < height; ++dx) {
        for(int dy = 0; dy < width; ++dy) {
            child[start_x + dx][start_y + dy] = donor[start_x + dx][start_y + dy];
        }
    }
}

void crossover_row_band(Array & child, const Array & donor) {
    const int band_height = 1 + random(3);
    const int start_x = random(rows - band_height + 1);

    for(int x = start_x; x < start_x + band_height; ++x) {
        child[x] = donor[x];
    }
}

Array make_crossover_child(const Array & base, const Array & donor, int stagnant_steps) {
    Array child = base;
    if(random(100) < 60) {
        crossover_rectangle(child, donor);
    } else {
        crossover_row_band(child, donor);
    }

    const int mutation_count = crossover_post_mutations_for(stagnant_steps);
    for(int mutation = 0; mutation < mutation_count; ++mutation) {
        apply_search_move(child, reheat_after);
    }

    return child;
}

void insert_into_elite_archive(const Array & board, const Fitness & fitness) {
    lock_guard <mutex> lock(state_lock);

    elite_archive.push_back({board, fitness});
    stable_sort(elite_archive.begin(), elite_archive.end(), [] (const EliteEntry & lhs, const EliteEntry & rhs) {
        return lhs.fitness > rhs.fitness;
    });

    if(static_cast <int> (elite_archive.size()) > elite_archive_capacity) {
        elite_archive.resize(elite_archive_capacity);
    }
}

Array elite_restart_board() {
    lock_guard <mutex> lock(state_lock);
    if(elite_archive.empty()) {
        return random_board();
    }

    const int choices = min(static_cast <int> (elite_archive.size()), elite_restart_choices);
    return elite_archive[random(choices)].board;
}

Array restart_seed_board() {
    if(random(100) < random_restart_percent) {
        return random_board();
    }

    return elite_restart_board();
}

Individual make_seed_individual(bool use_restart_seed) {
    Array board = use_restart_seed ? restart_seed_board() : random_board();
    if(use_restart_seed) {
        for(int mutation = 0; mutation < elite_restart_mutations; ++mutation) {
            apply_search_move(board, reheat_after);
        }
    }

    Fitness fitness = evaluate(board);
    if(use_restart_seed) {
        local_improve(board, fitness);
    }

    return {move(board), fitness};
}

Individual make_immigrant_individual(int stagnant_steps) {
    const bool use_restart_seed = stagnant_steps >= reheat_after / 2 || random(100) < 40;
    Individual immigrant = make_seed_individual(use_restart_seed);

    int digit_macro_count = 0;
    if(stagnant_steps < reheat_after / 3) {
        if(random(100) < 35) {
            digit_macro_count = 1;
        }
    } else if(stagnant_steps < (2 * reheat_after) / 3) {
        digit_macro_count = 1;
    } else {
        digit_macro_count = 1 + random(2);
    }

    if(digit_macro_count == 0) {
        return immigrant;
    }

    for(int iter = 0; iter < digit_macro_count; ++iter) {
        mutate_digit_macro(immigrant.board, stagnant_steps);
    }
    immigrant.fitness = evaluate(immigrant.board);
    local_improve(immigrant.board, immigrant.fitness);
    immigrant.protected_turns = 0;
    return immigrant;
}

void export_migrant(const Individual & individual, size_t source_tag) {
    lock_guard <mutex> lock(state_lock);

    for(auto & entry : migration_pool) {
        if(entry.source_tag == source_tag) {
            if(individual.fitness > entry.fitness) {
                entry.board = individual.board;
                entry.fitness = individual.fitness;
            }
            stable_sort(migration_pool.begin(), migration_pool.end(), [] (const MigrantEntry & lhs, const MigrantEntry & rhs) {
                return lhs.fitness > rhs.fitness;
            });
            return;
        }
    }

    migration_pool.push_back({individual.board, individual.fitness, source_tag});
    stable_sort(migration_pool.begin(), migration_pool.end(), [] (const MigrantEntry & lhs, const MigrantEntry & rhs) {
        return lhs.fitness > rhs.fitness;
    });
    if(static_cast <int> (migration_pool.size()) > migration_pool_capacity) {
        migration_pool.resize(migration_pool_capacity);
    }
}

vector <Individual> import_migrants(size_t source_tag) {
    lock_guard <mutex> lock(state_lock);

    vector <int> candidate_indices;
    const int candidate_limit = min(static_cast <int> (migration_pool.size()), migration_import_choices);
    for(int idx = 0; idx < candidate_limit; ++idx) {
        if(migration_pool[idx].source_tag != source_tag) {
            candidate_indices.push_back(idx);
        }
    }

    vector <Individual> migrants;
    while(!candidate_indices.empty() && static_cast <int> (migrants.size()) < migration_import_count) {
        const int pick = random(static_cast <int> (candidate_indices.size()));
        const int entry_index = candidate_indices[pick];
        migrants.push_back({migration_pool[entry_index].board, migration_pool[entry_index].fitness});
        candidate_indices[pick] = candidate_indices.back();
        candidate_indices.pop_back();
    }

    return migrants;
}

bool apply_migration(vector <Individual> & population, size_t source_tag, int stagnant_steps) {
    const vector <Individual> migrants = import_migrants(source_tag);
    if(migrants.empty()) {
        return false;
    }

    sort_population(population);
    bool changed = false;
    for(const Individual & migrant : migrants) {
        const int replace_index = worst_replaceable_tail_index(population);
        if(replace_index < 0) {
            break;
        }

        if(migrant.fitness > population[replace_index].fitness) {
            place_individual(population, replace_index, migrant.board, migrant.fitness, stagnant_steps);
            changed = true;
        }
    }

    if(changed) {
        sort_population(population);
    }

    return changed;
}

bool try_insert_exploration_candidate(vector <Individual> & population, Array candidate, const Fitness & candidate_fitness, int stagnant_steps) {
    if(static_cast <int> (population.size()) <= local_stable_keep) {
        return false;
    }

    vector <int> replaceable_indices = replaceable_tail_indices(population);
    if(replaceable_indices.empty()) {
        return false;
    }

    const int slot_index = replaceable_indices[random(static_cast <int> (replaceable_indices.size()))];
    const int diversity = min_population_distance(candidate, population, slot_index);

    if(diversity <= 4 && candidate_fitness <= population[slot_index].fitness) {
        return false;
    }

    const double current_value = annealing_value(population[slot_index].fitness);
    const double candidate_value = annealing_value(candidate_fitness)
        + static_cast <double> (diversity) * exploration_diversity_bonus_scale;

    if(!update(current_value, candidate_value, exploration_temperature_for(stagnant_steps))) {
        return false;
    }

    place_individual(population, slot_index, move(candidate), candidate_fitness, stagnant_steps);
    return true;
}

void refresh_population(vector <Individual> & population, int stagnant_steps) {
    sort_population(population);
    int refreshed = 0;
    for(int idx = static_cast <int> (population.size()) - 1; idx >= local_stable_keep && refreshed < population_refresh_count; --idx) {
        if(is_protected_tail_slot(population, idx)) {
            continue;
        }

        Individual seed = make_seed_individual(true);
        place_individual(population, idx, move(seed.board), seed.fitness, stagnant_steps);
        insert_into_elite_archive(population[idx].board, population[idx].fitness);
        publish_global_best(population[idx].board, population[idx].fitness);
        ++refreshed;
    }
    sort_population(population);
}

void publish_global_best(const Array & board, const Fitness & fitness) {
    lock_guard <mutex> lock(state_lock);
    if(fitness <= global_best_fitness) {
        return;
    }

    global_best_solution = board;
    global_best_fitness = fitness;
    cout << "New global best(" << this_thread::get_id() << "): "
         << "prefix=" << fitness.prefix_score
         << ", frontier=" << fitness.frontier_hits
         << ", cover_9999=" << fitness.cover_9999
         << endl;
}

}  // namespace

void print_board(const Array & board) {
    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            cout << board[i][j];
        }
        cout << '\n';
    }
}

Fitness best_fitness_snapshot() {
    lock_guard <mutex> lock(state_lock);
    return global_best_fitness;
}

Array best_solution_snapshot() {
    lock_guard <mutex> lock(state_lock);
    return global_best_solution;
}

void set_run_time_budget(chrono::seconds budget) {
    run_deadline = chrono::steady_clock::now() + budget;
}

bool time_budget_expired() {
    return chrono::steady_clock::now() >= run_deadline;
}

void search() {
    const size_t thread_tag = hash <thread::id> {}(this_thread::get_id());
    vector <Individual> population;
    population.reserve(local_population_size);
    for(int idx = 0; idx < local_population_size; ++idx) {
        population.push_back(make_seed_individual(false));
        insert_into_elite_archive(population.back().board, population.back().fitness);
        publish_global_best(population.back().board, population.back().fitness);
    }
    sort_population(population);
    export_migrant(population.front(), thread_tag);

    Array local_best = population.front().board;
    Fitness local_best_fitness = population.front().fitness;
    int stagnant_steps = 0;

    long long iter = 0;
    while(!time_budget_expired()) {
        sort_population(population);
        age_population(population);
        Array candidate;
        Fitness candidate_fitness;
        Fitness reference_fitness;
        int replacement_index = -1;

        if(random(100) < immigrant_percent_for(stagnant_steps)) {
            Individual immigrant = make_immigrant_individual(stagnant_steps);
            candidate = move(immigrant.board);
            candidate_fitness = immigrant.fitness;
            const int replaceable_tail_index = worst_replaceable_tail_index(population);
            reference_fitness = replaceable_tail_index >= 0 ? population[replaceable_tail_index].fitness : population.back().fitness;
        } else {
            const int parent_index = tournament_pick(population);
            const Fitness parent_fitness = population[parent_index].fitness;

            candidate = population[parent_index].board;
            reference_fitness = parent_fitness;
            replacement_index = parent_index;

            if(random(100) < crossover_percent_for(stagnant_steps)) {
                const int other_parent_index = pick_crossover_mate(population, parent_index, stagnant_steps);
                candidate = make_crossover_child(population[parent_index].board, population[other_parent_index].board, stagnant_steps);
                if(population[other_parent_index].fitness < reference_fitness) {
                    reference_fitness = population[other_parent_index].fitness;
                    replacement_index = other_parent_index;
                }
            } else {
                apply_search_move(candidate, stagnant_steps);
            }

            candidate_fitness = evaluate(candidate);
            if(candidate_fitness >= reference_fitness
                && (candidate_fitness.prefix_score > reference_fitness.prefix_score
                    || stagnant_steps >= reheat_after / 2)) {
                local_improve(candidate, candidate_fitness);
            }
        }

        bool inserted = false;
        const int worst_tail_index = worst_replaceable_tail_index(population);
        if(worst_tail_index >= 0 && candidate_fitness > population[worst_tail_index].fitness) {
            place_individual(population, worst_tail_index, move(candidate), candidate_fitness, stagnant_steps);
            inserted = true;
        } else if(replacement_index >= 0
            && !is_protected_tail_slot(population, replacement_index)
            && candidate_fitness > population[replacement_index].fitness) {
            place_individual(population, replacement_index, move(candidate), candidate_fitness, stagnant_steps);
            inserted = true;
        } else if(try_insert_exploration_candidate(population, move(candidate), candidate_fitness, stagnant_steps)) {
            inserted = true;
        }

        if(inserted) {
            sort_population(population);
        }

        if(population.front().fitness > local_best_fitness) {
            local_best = population.front().board;
            local_best_fitness = population.front().fitness;
            stagnant_steps = 0;
            insert_into_elite_archive(local_best, local_best_fitness);
            export_migrant(population.front(), thread_tag);
            publish_global_best(local_best, local_best_fitness);
        } else {
            ++stagnant_steps;
        }

        if((iter + 1) % migration_interval == 0) {
            export_migrant(population.front(), thread_tag);
            if(apply_migration(population, thread_tag, stagnant_steps) && population.front().fitness > local_best_fitness) {
                local_best = population.front().board;
                local_best_fitness = population.front().fitness;
                stagnant_steps = 0;
                insert_into_elite_archive(local_best, local_best_fitness);
                publish_global_best(local_best, local_best_fitness);
            }
        }

        if(stagnant_steps >= reheat_after) {
            refresh_population(population, stagnant_steps);
            export_migrant(population.front(), thread_tag);
            if(population.front().fitness > local_best_fitness) {
                local_best = population.front().board;
                local_best_fitness = population.front().fitness;
                insert_into_elite_archive(local_best, local_best_fitness);
                publish_global_best(local_best, local_best_fitness);
            }
            stagnant_steps = 0;
        }

        ++iter;
    }
}

void repeat() {
    search();
}
