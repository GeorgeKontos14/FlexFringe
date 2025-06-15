#include "ensemble.h"

#include <fstream>
#include <cstdlib>
#include <random>
#include <stack>
#include <unordered_set>

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
 * @brief Creates an ensemble by independently performing random sequences of merges
 *
 * @param merger the state merger object that includes the original APTA
 * @param nr_estimators the number of DFAs to construct
 * @param output_file the file in which to write the output automata
 */
void random_walk_ensemble(state_merger* merger, int nr_estimators, std::string& output_file) {
    int E = 0;
    std::random_device rd;
    std::mt19937 gen(rd());

    std::string json_filename = output_file +".final.random.json";
    std::ostringstream json_stream;
    json_stream << "{\n";

    std::vector<refinement*> path;
    while (E < nr_estimators) {
        refinement_vector* refs = merger->get_possible_refinements_vector();
        while (!refs->empty()) {
            std::uniform_int_distribution<> dist(0, refs->size() - 1);
            int random_index = dist(gen);
            refinement* ref = refs->at(random_index);
            path.push_back(ref);
            ref->doref(merger);
            refs = merger->get_possible_refinements_vector();
        }

        E++;
        merger->tojson();
        json_stream << " \"Automaton " << E << "\": " << merger->json_output;
        if (E != nr_estimators) json_stream <<",";
        json_stream << "\n";
        std::cout << "Adding DFA #" << E << std::endl;

        for (int i = path.size() - 1; i >= 0; --i) {
            refinement* ref = path[i];
            ref->undo(merger);
        }
        path.clear();
    }

    json_stream << "}\n";
    std::ofstream json_out;
    json_out.open(json_filename);
    json_out << json_stream.str();
    json_out.close();
}

/**
 * @brief Creates an ensemble by performing a balanced merge tree exploration
 *
 * @param merger the state merger object that includes the original APTA
 * @param nr_estimators the number of DFAs to construct
 * @param output_file the file in which to write the output automata
 */
void tree_balanced_ensemble(state_merger* merger, int nr_estimators, const std::string& output_file) {
    // Setup
    auto cmp = [](merge_tree* a, merge_tree* b) {
        return a->get_level() > b->get_level();
    };

    int E = 0;
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

    auto init_children = [](merge_tree* node, state_merger* merger)
    {
        node->initialized = true;
        refinement_vector* possible_merges = merger->get_possible_refinements_vector();
        if (possible_merges->empty()) {
            node->is_leaf = true;
        } else {
            for (int i = 0; i < possible_merges->size(); ++i) {
                refinement* ref = possible_merges->at(i);
                auto child = new merge_tree(node, ref);
                node->children.push_back(child);
            }
        }
    };
    merge_tree* node;

    while (E < nr_estimators && !next_nodes.empty()) {
        // Pop the next node to process
        node = next_nodes.top();
        next_nodes.pop();

        // Transform the APTA to the corresponding state
        if (!node->is_root) {
            if (is_reset)
                prev_node->transform(node, merger);
            else
                node->get_merge()->doref(merger);
        }

        // Initialize the node's children if necessary
        if (!node->initialized) init_children(node, merger);

        // If the node is a leaf, add the DFA to the ensemble and mark the merges as performed
        if (node->is_leaf) {
            E++;
            merger->tojson();
            json_stream << " \"Automaton " << E << "\": " << merger->json_output;
            if (E != nr_estimators) json_stream <<",";
            json_stream << "\n";
            std::cout << "Adding DFA #" << E << std::endl;
            is_reset = true;

        } else { // Allocate the live selections across the children of the node and add all children with live selections to the stack
            node->allocate_live();
            for (int index: node->get_selected_children()) next_nodes.push(node->children[index]);
            is_reset = false;
        }

        prev_node = node; // Remember the last node in the next iteration in order to transform the apta
    }


    json_stream << "}\n";
    std::ofstream json_out;
    json_out.open(json_filename);
    json_out << json_stream.str();
    json_out.close();

    delete root;
}

/**
 * @brief Creates an ensemble by performing a balanced merge tree exploration with branch pruning
 *
 * @param merger the state merger object that includes the original APTA
 * @param nr_estimators the number of DFAs to construct
 * @param output_file the file in which to write the output automata
 */
void tree_pruning_ensemble(state_merger* merger, int nr_estimators, const std::string& output_file) {
    struct refinement_ptr_hash {
        std::size_t operator()(const refinement* r) const {
            return r->hash();
        }
    };

    struct refinement_ptr_equal {
        bool operator()(const refinement* a, const refinement* b) const {
            if (a == b) return true;  // Fast path for identical pointers
            if (!a || !b) return false;  // Handle null pointers

            if (typeid(*a) != typeid(*b)) return false;

            if (auto ea = dynamic_cast<const extend_refinement*>(a)) {
                auto eb = dynamic_cast<const extend_refinement*>(b);
                return ea->red->get_number() == eb->red->get_number();
            }

            if (auto ma = dynamic_cast<const merge_refinement*>(a)) {
                auto mb = dynamic_cast<const merge_refinement*>(b);
                return ma->red->get_number() == mb->red->get_number() && ma->blue->get_number() == mb->blue->get_number();
            }

            if (auto sa = dynamic_cast<const split_refinement*>(a)) {
                auto sb = dynamic_cast<const split_refinement*>(b);
                return sa->red->get_number() == sb->red->get_number();
            }

            return false;
        }
    };

    auto init_children = [](merge_tree* node, state_merger* merger,
                                const std::unordered_set<refinement*, refinement_ptr_hash, refinement_ptr_equal>& performed)
    {
        node->initialized = true;
        refinement_vector* possible_merges = merger->get_possible_refinements_vector();
        if (possible_merges->empty()) {
            node->is_leaf = true;
        } else {
            for (int i = 0; i < possible_merges->size(); ++i) {
                refinement* ref = possible_merges->at(i);
                if (!performed.contains(ref)) {
                    auto child = new merge_tree(node, ref);
                    node->children.push_back(child);
                }
            }
        }
    };

    int E = 0;

    auto root = new merge_tree(nr_estimators);
    bool is_reset = false;
    std::stack<merge_tree*> next_nodes;
    std::unordered_set<refinement*, refinement_ptr_hash, refinement_ptr_equal> memory;

    std::string json_filename = output_file +".random.json";
    std::ostringstream json_stream;
    json_stream << "{\n";

    merge_tree* node;
    merge_tree* prev_node;
    next_nodes.push(root);

    while (E < nr_estimators && !next_nodes.empty()) {
        // Pop the next node to process
        node = next_nodes.top();
        next_nodes.pop();

        // Transform the APTA to the corresponding state
        if (!node->is_root) {
            if (is_reset)
                prev_node->transform(node, merger);
            else
                node->get_merge()->doref(merger);
        }

        // Initialize the node's children if necessary
        if (!node->initialized) init_children(node, merger, memory);

        // If the node is a leaf, add the DFA to the ensemble and mark the merges as performed
        if (node->is_leaf) {
            E++;
            std::vector<refinement*> path = node->get_path();
            for (refinement* ref: path) {
                memory.insert(ref);
            }
            merger->tojson();
            json_stream << " \"Automaton " << E << "\": " << merger->json_output;
            if (E != nr_estimators) json_stream <<",";
            json_stream << "\n";
            std::cout << "Adding DFA #" << E << std::endl;
            is_reset = true;

        } else if (node->children.empty()) { // If all the children of the current node are pruned, pivot to a skipped node and add it to the stack
            is_reset = true;
        } else { // Allocate the live selections across the children of the node and add all children with live selections to the stack
            node->allocate_live();
            for (int index: node->get_selected_children()) next_nodes.push(node->children[index]);
            is_reset = false;
        }

        prev_node = node; // Remember the last node in the next iteration in order to transform the apta
    }

    json_stream << "}\n";
    std::ofstream json_out;
    json_out.open(json_filename);
    json_out << json_stream.str();
    json_out.close();

    delete root;
}