#include <stdio.h>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <random>

#include "refinement.h"
#include "greedy.h"
#include "parameters.h"

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
    // TODO: Parallelize(?)
    // TODO: Find heuristic for creating as different DFAs as possible
    for(int i = 0; i < nr_estimators; ++i) {
        std::cout << "Building random estimator "<< i << std::endl;

        // Build the initial APTA again using the input data
        // TODO: Faster/safer way
        the_apta = new apta();
        merger->get_dat()->add_traces_to_apta(the_apta);
        state_merger* merger_clone = new state_merger(merger->get_dat(), merger->get_eval(), the_apta);
        the_apta->set_context(merger_clone);

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