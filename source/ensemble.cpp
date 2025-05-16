#include "ensemble.h"

#include <fstream>
#include <cstdlib>
#include <random>

#include "refinement.h"
#include "greedy.h"

#include "merge_tree.h"

/** todo: work in progress */

refinement_list* greedy(state_merger* merger){
    std::cerr << "starting greedy merging" << std::endl;
    merger->get_eval()->initialize_after_adding_traces(merger);

    auto* all_refs = new refinement_list();

    refinement* best_ref = merger->get_best_refinement();
    while( best_ref != nullptr ){
        std::cout << " ";
        best_ref->print_short();
        std::cout << " ";
        std::cout.flush();

        best_ref->doref(merger);
        all_refs->push_back(best_ref);
        best_ref = merger->get_best_refinement();
    }
    std::cout << "no more possible merges" << std::endl;
    return all_refs;
};

void bagging(state_merger* merger, std::string output_file, int nr_estimators){
    std::cerr << "starting bagging" << std::endl;
    for(int i = 0; i < nr_estimators; ++i){
        refinement_list* all_refs = greedy(merger);

        for(refinement_list::reverse_iterator it = all_refs->rbegin(); it != all_refs->rend(); ++it){
            (*it)->undo(merger);
        }
        for(refinement_list::iterator it = all_refs->begin(); it != all_refs->end(); ++it){
            (*it)->erase();
        }
        delete all_refs;
    }
    std::cerr << "ended bagging" << std::endl;
};

/**
 * @Brief Ensemble that chooses random state merges without a suitability metric
 *
 * @param merger The state merger object containing the initial APTA
 * @param nr_estimators The number of random DFAs to generate
 * @return A vector of size nr_estimators containing all the final random automata
 */
std::vector<state_merger*> random_dfa(state_merger* merger, int nr_estimators){
    std::cerr << "starting random DFA" << std::endl;

    // Initialize objects used in the loops
    refinement_list* refs_list;
    refinement* selected_ref;
    apta* the_apta;

    // Initialize the random number generator
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<state_merger*> mergers;

    // Each loop iteration creates a new random estimator
    for(int i = 0; i < nr_estimators; ++i) {
        std::cout << "Building random estimator "<< i << std::endl;

        // Build the initial APTA again using the input data
        state_merger* merger_clone = merger->copy();

        // Generate all the possible changes to the initial APTA
        refs_list = merger_clone->get_possible_refinements_list();

        // Perform (feasible) merges until a minimal automaton is reached
        while (!refs_list->empty()) {
            // Randomly select a refinement
            std::uniform_int_distribution<> dist(0, refs_list->size() - 1);
            int random_index = dist(gen);
            auto it = refs_list->begin();
            std::advance(it, random_index);
            selected_ref = *it;

            // Print the selected refinement
            std::cout << " ";
            selected_ref->print_short();
            std::cout << " ";
            std::cout.flush();

            // Perform the refinement on the automaton
            selected_ref->doref(merger_clone);

            delete selected_ref;
            delete refs_list;

            // Generate all the possible changes to the new APTA
            refs_list = merger_clone->get_possible_refinements_list();
        }
        std::cout << "no more possible merges" << std::endl;

        mergers.push_back(merger_clone);
    }

    return mergers;
}

/**
 * @brief Ensemble that chooses different merge sequences by creating a tree-like structure
 * In case nr_estimators is larger than the number of possible minimal automata, fewer automata are returned
 *
 * @param merger The state merger object containing the initial APTA
 * @param nr_estimators The number of random DFAs to generate
 * @return A vector containing all the final random automata.
 */
std::vector<state_merger*> tree_random_ensemble(state_merger* merger, int nr_estimators) {
    // Setup
    std::vector<state_merger*> E;
    std::queue<merge_tree*> skipped_nodes;
    auto root = new merge_tree(nr_estimators);
    std::random_device rd;
    std::mt19937 gen(rd());

    std::cout << "Entering Phase I" << std::endl;
    // Phase I: First traversal allocation
    std::vector<merge_tree*> level = {root};
    while (!level.empty() && E.size() < nr_estimators) {
        std::vector<merge_tree*> next_level;
        for (merge_tree* node : level) {
            if (node->get_is_leaf()) {
                state_merger* cloned_merger = merger->copy();
                node->perform_merges(cloned_merger);
                E.push_back(cloned_merger);
            } else {
                node->initialize_children(merger);
                auto [skipped_children, selected_children] = node->allocate_live();

                for (merge_tree* child : skipped_children) {
                    skipped_nodes.push(child);
                }

                for (merge_tree* child : selected_children) {
                    next_level.push_back(child);
                }
            }
        }
        level = next_level;
    }

    std::cout << "Entering Phase II" << std::endl;
    // Phase II: Allocation of remaining selections
    int m = nr_estimators - E.size();
    while(m > 0) {
        if (skipped_nodes.empty()) {
            break;
        }
        merge_tree* current_node = skipped_nodes.front();
        skipped_nodes.pop();
        while (!current_node->get_is_leaf()) {
            current_node->initialize_children(merger);
            int nr_children = current_node->get_children().size();
            std::uniform_int_distribution<> dist(0, nr_children - 1);
            int allocation = dist(gen);
            int j = 0;
            while (j < nr_children && skipped_nodes.size() < m) {
                if (j == allocation) {
                    continue;
                }
                skipped_nodes.push(current_node->get_children()[j]);
                j++;
            }
            current_node = current_node->get_children()[allocation];
        }
        state_merger* merger_clone = merger->copy();
        current_node->perform_merges(merger_clone);
        E.push_back(merger_clone);
        m--;
    }

    delete root;

    return E;
}
