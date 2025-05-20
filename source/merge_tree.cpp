//
// Created by konto on 5/15/2025.
//

#include "merge_tree.h"

#include <random>

#include "state_merger.h"

int merge_tree::id_counter = 0;

/**
 * @brief Constructor to be used for the root. Creates the list of live selections
 *
 *
 * @param n the number of live selections. The list of live selections is set to [0,1,...,n-1]
 */
merge_tree::merge_tree(int n) {
    parent = nullptr;
    merge = nullptr;
    for (int i = 0; i < n; i++) {
        live.push_back(i);
    }
    level = 0;
    id = id_counter++;
}

/**
 * @brief Constructor to be used for non-root nodes. The list of live selections is initially empty
 *
 * @param parent_node the node's parent
 * @param ref the last merge performed to reach the node
 * @param merge_ind the index of the merge that was followed from the parent to arrive to this node
 */
merge_tree::merge_tree(merge_tree* parent_node, refinement* ref, int merge_ind) {
    parent = parent_node;
    merge = ref;
    level = parent->get_level() + 1;
    id = id_counter++;
    ancestors = parent->get_ancestors();
    ancestors.push_back(parent_node->get_id());
    index_path = parent_node->get_index_path();
    index_path.push_back(merge_ind);
}

/**
 *
 * @return True if the current node is the root; false otherwise
 */
bool merge_tree::is_root() {
    return parent == nullptr;
}

/**
 *
 * @param merger The state merger in the state of the current node
 * @return True if no merges are possible; false otherwise
 */
bool merge_tree::is_leaf(state_merger* merger) {
    refinement_vector* possible_merges = merger->get_possible_refinements_vector();
    return possible_merges->empty();
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
 * @brief Backtracks a given amount of steps to find the partial sequence of merges that arrives to the current node
 *
 * @param nr_steps The amount of levels up the tree to climb
 * @return a vector containing the last nr_steps merges in reverse order; the last entry should be performed first
 */
std::vector<refinement*> merge_tree::get_path(int nr_steps) {
    std::vector<refinement*> path;
    merge_tree* current_node = this;
    while (current_node->merge != nullptr && path.size() < nr_steps) {
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
 * @brief Given a merger at some intermediate state, perform the given number of merges to arrive to the current node
 *
 * @param merger the intermediate merger
 * @param nr_steps the number of merges to perform
 */
void merge_tree::perform_merges(state_merger* merger, int nr_steps) {
    std::vector<refinement*> path = get_path(nr_steps);
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
 * @brief Given the merger corresponding to the current node, reverts a given amount of merges
 *
 * @param merger the current merges
 * @param nr_steps the number of merges to revert
 */
void merge_tree::revert_merges(state_merger* merger, int nr_steps) {
    std::vector<refinement*> path = get_path(nr_steps);
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
    refinement_vector* possible_merges = merger->get_possible_refinements_vector();
    for (int i = 0; i < possible_merges->size(); i++) {
        refinement* next_merge = possible_merges->at(i);
        merge_tree* child;
        child = new merge_tree(this, next_merge, i);
        children.push_back(child);
    }
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
 * @brief Generates a balanced allocation of the current node's live selections to its children
 *
 * @return A map from the live selections to the children index, representing allocations
 */
std::map<int, int> merge_tree::generate_allocation() {
    std::map<int, int> allocation;

    int n_children = static_cast<int>(children.size());
    if (n_children == 0 || live.empty()) {
        return allocation;
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<int> shuffled_live = live;
    std::ranges::shuffle(shuffled_live, gen);

    // Assign live elements in round-robin to shuffled child indices
    std::vector<int> child_indices(n_children);
    std::iota(child_indices.begin(), child_indices.end(), 0);
    std::ranges::shuffle(child_indices, gen);

    for (size_t i = 0; i < shuffled_live.size(); ++i) {
        int child_index = child_indices[i % n_children]; // round-robin with random start
        allocation[shuffled_live[i]] = child_index;
    }

    return allocation;
}

/**
 * @brief finds the distance of the current node and a different node from their deepest common ancestor
 *
 * @param other a different node of the same tree
 * @return the distance from this to the common ancestor; the distance from other to the common ancestor
 */
std::pair<int, int> merge_tree::find_common_ancestor(merge_tree* other) {
    std::vector<int> other_ancestors = other->get_ancestors();
    int i = static_cast<int>(ancestors.size())-1;
    int j = static_cast<int>(other_ancestors.size())-1;

    int this_level = level;
    int other_level = other->get_level();

    while (this_level > other_level) {
        --i;
        --this_level;
    }
    while (this_level < other_level) {
        --j;
        --other_level;
    }

    while (i >= 0 && j >= 0)
    {
        if (ancestors[i] == other_ancestors[j]) {
            int steps_this = static_cast<int>(ancestors.size() - i - 1);
            int steps_other = static_cast<int>(other_ancestors.size() - j - 1);
            return std::make_pair(steps_this, steps_other);
        }
        --i;
        --j;
    }
    return std::make_pair(-1, -1);
}


merge_tree::~merge_tree() {
    for (merge_tree* child : children) {
        delete child;
    }
}