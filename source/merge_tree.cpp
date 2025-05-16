//
// Created by konto on 5/15/2025.
//

#include "merge_tree.h"

#include <random>

#include "state_merger.h"

/**
 * @brief Constructor to be used for the root. Creates the list of live selections
 *
 *
 * @param n the number of live selections. The list of live selections is set to [0,1,...,n-1]
 */
merge_tree::merge_tree(int n) {
    is_leaf = false;
    parent = nullptr;
    merge = nullptr;
    for (int i = 0; i < n; i++) {
        live.push_back(i);
    }
}

/**
 * @brief Constructor to be used for non-root nodes. The list of live selections is initially empty
 *
 * @param parent_node the node's parent
 * @param ref the last merge performed to reach the node
 * @param merger the state merger object one step before arriving to the current node
 */
merge_tree::merge_tree(merge_tree* parent_node, refinement* ref, state_merger* merger) {
    parent = parent_node;
    merge = ref;

    ref->doref(merger);
    refinement_vector* possible_merges = merger->get_possible_refinements_vector();
    is_leaf = possible_merges->empty();
    ref->undo(merger);
}

/**
 *
 * @return True if the current node is the root; false otherwise
 */
bool merge_tree::is_root() {
    return parent == nullptr;
}

/**
 * @brief Backtracks to the root in order to find the sequence of merges required from the original apta to arrive to the current node
 *
 * @return a vector containing the required merges in reverse order; the last entry should be performed first
 */
std::vector<refinement*> merge_tree::get_path() {
    std::vector<refinement*> path;
    merge_tree* current_node = this;
    while (current_node->merge != nullptr) {
        path.push_back(current_node->merge);
        current_node = current_node->parent;
    }
    return path;
}

/**
 * @brief Given the original merger, perform all necessary merges to arrive to the current node
 *
 * @param merger the original merger
 */
void merge_tree::perform_merges(state_merger* merger) {
    std::vector<refinement*> path = get_path();
    for (int i = path.size() - 1; i >= 0; i--) {
        refinement* next_merge = path[i];
        next_merge->doref(merger);
    }
}

/**
 * @brief Given the merger corresponding to the current node, reverts all merges to retrieve the original merger
 * @param merger the current merger
 */
void merge_tree::revert_merges(state_merger* merger) {
    std::vector<refinement*> path = get_path();
    for (int i = 0; i < path.size(); i++) {
        refinement* next_merge = path[i];
        next_merge->undo(merger);
    }
}

/**
 * @brief Creates the children of the current node. For every child, a check of whether it is a leaf is performed by the constructor.
 *
 * @param merger the original merger
 */
void merge_tree::initialize_children(state_merger* merger) {
    if (is_leaf)
        return;

    perform_merges(merger);

    refinement_vector* possible_merges = merger->get_possible_refinements_vector();
    for (int i = 0; i < possible_merges->size(); i++) {
        refinement* next_merge = possible_merges->at(i);
        merge_tree* child;
        child = new merge_tree(this, next_merge, merger);
        children.push_back(child);
    }

    revert_merges(merger);
}

/**
 * @brief Adds a live selection to the current node
 *
 * @param index The live selection to be added
 */
void merge_tree::add_live(int index) {
    live.push_back(index);
}

/**
 * @brief Checks if live selections have been allocated to the current node
 *
 * @return True if the current node has no live selections; false otherwise
 */
bool merge_tree::is_empty() {
    return live.empty();
}

/**
 * @brief Allocates the live selections of the current node to its children
 *
 * @return A vector containing all the empty children and a vector containing all the non-empty children
 */
std::pair<std::vector<merge_tree*>, std::vector<merge_tree*>> merge_tree::allocate_live() {
    std::map<int, int> allocation = generate_allocation();
    std::vector<merge_tree*> skipped_merges;
    std::vector<merge_tree*> selected_merges;

    for (auto it = allocation.begin(); it != allocation.end(); it++) {
        int selection = it->first;
        int child_ind = it->second;
        children[child_ind]->add_live(selection);
    }

    for (merge_tree* child : children) {
        if (child->is_empty()) {
            skipped_merges.push_back(child);
        } else {
            selected_merges.push_back(child);
        }
    }

    return std::make_pair(skipped_merges, selected_merges);
}

/**
 * @brief Generates a balanced allocation of the current node's live selections to its children, where each leaf can appear only once in the allocation
 * Note: In the case that all children are leaves and children.size()<live.size(), some live selections are skipped
 *
 * @return A map from the live selections to the children index, representing allocations
 */
std::map<int, int> merge_tree::generate_allocation() {
    std::map<int, int> allocation;

    int n_children = static_cast<int>(children.size());

    std::vector<int> leaf_indices;
    std::vector<int> non_leaf_indices;

    for (int i = 0; i < n_children; i++) {
        if (children[i]->get_is_leaf()) {
            leaf_indices.push_back(i);
        } else {
            non_leaf_indices.push_back(i);
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<int> shuffled_live = live;
    std::ranges::shuffle(shuffled_live, gen);

    if (non_leaf_indices.empty()) {
        std::ranges::shuffle(leaf_indices, gen);
        int count = std::min(static_cast<int>(shuffled_live.size()),static_cast<int>(leaf_indices.size()));
        for (int i = 0; i < count; i++) {
            allocation[shuffled_live[i]] = leaf_indices[i];
        }
    } else {
        std::vector<int> available_targets = non_leaf_indices;
        available_targets.insert(available_targets.end(), leaf_indices.begin(), leaf_indices.end());

        std::ranges::shuffle(available_targets, gen);

        while (static_cast<int>(available_targets.size()) < static_cast<int>(shuffled_live.size())) {
            std::ranges::shuffle(non_leaf_indices, gen);
            for (int i : non_leaf_indices) {
                available_targets.push_back(i);
                if (static_cast<int>(available_targets.size()) >= static_cast<int>(shuffled_live.size()))
                    break;
            }
        }

        for (int i = 0; i < static_cast<int>(shuffled_live.size()); ++i) {
            allocation[shuffled_live[i]] = available_targets[i];
        }
    }

    return allocation;
}


merge_tree::~merge_tree() {
    for (merge_tree* child : children) {
        delete child;
    }
}