#include "ensemble.h"

#include <fstream>
#include <cstdlib>
#include <random>
#include <stack>

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
 * @param output_file The file to which to write the DFAs in json format
 */
void tree_random_ensemble(state_merger* merger, int nr_estimators, const std::string& output_file) {
    // Setup
    auto cmp = [](merge_tree* a, merge_tree* b) {
        return a->get_level() > b->get_level();
    };

    std::vector<std::vector<int>> E;
    std::priority_queue<merge_tree*, std::vector<merge_tree*>, decltype(cmp)> skipped_nodes(cmp);
    auto root = new merge_tree(nr_estimators);
    std::random_device rd;
    std::mt19937 gen(rd());
    bool is_reset = false;
    std::stack<merge_tree*> next_nodes;

    std::string json_filename = output_file +".random.json";
    std::ostringstream json_stream;
    json_stream << "{\n";

    std::cout << "Entering Phase I" << std::endl;
    // Phase I: First traversal allocation
    merge_tree* prev_node;
    next_nodes.push(root);
    while (!next_nodes.empty() && E.size() < nr_estimators) {
        merge_tree* node = next_nodes.top();
        next_nodes.pop();
        if (is_reset) {
            prev_node->revert_merges(merger);
            node->perform_merges(merger);
        } else {
            if (node->get_merge() != nullptr) {
                node->get_merge()->doref(merger);
            }
        }
        if (node->is_leaf(merger)) {
            E.push_back(node->get_index_path());
            merger->tojson();
            json_stream << " \"Automaton " << E.size() << "\": " << merger->json_output;
            if (E.size() != nr_estimators) json_stream <<",";
            json_stream << "\n";
            std::cout << "Adding DFA #" << E.size() << std::endl;
            is_reset = true;
        } else {
            node->initialize_children(merger);
            auto [skipped_children, selected_children] = node->allocate_live();
            for (merge_tree* child: skipped_children) {
                skipped_nodes.push(child);
            }
            for (merge_tree* child: selected_children) {
                next_nodes.push(child);
            }
            is_reset = false;
        }
        prev_node = node;
    }

    std::cout << "Entering Phase II" << std::endl;
    // Phase II: Allocation of remaining selections
    prev_node->revert_merges(merger);
    int m = nr_estimators-E.size();
    if (m > 0) {
        std::cout << "Remaining models: " << m << std::endl;
    } else {
        std::cout << "No more models needed" << std::endl;
    }

    while (m > 0) {
        if (skipped_nodes.empty()) {
            break;
        }
        merge_tree* node = skipped_nodes.top();
        skipped_nodes.pop();
        node->perform_merges(merger);
        while (!node->is_leaf(merger)) {
            node->initialize_children(merger);
            int nr_children = node->get_children().size();
            std::uniform_int_distribution<> dist(0, nr_children - 1);
            int allocation = dist(gen);
            int j = 0;
            while (j < nr_children && skipped_nodes.size() < m) {
                if (j == allocation) {
                    continue;
                }
                skipped_nodes.push(node->get_children()[j]);
                j++;
            }
            node = node->get_children()[allocation];
            node->get_merge()->doref(merger);
        }
        E.push_back(node->get_index_path());
        merger->tojson();
        json_stream << " \"Automaton " << E.size() << "\": " << merger->json_output;
        if (E.size() != nr_estimators) json_stream <<",";
        json_stream << "\n";
        std::cout << "Adding DFA #" << E.size() << std::endl;
        node->revert_merges(merger);
        m--;
    }

    json_stream << "}\n";
    std::ofstream json_out;
    json_out.open(json_filename);
    json_out << json_stream.str();
    json_out.close();

    delete root;
}