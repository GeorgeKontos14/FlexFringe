//
// Created by konto on 5/15/2025.
//

#ifndef MERGE_TREE_H
#define MERGE_TREE_H

#include <vector>

#include "refinement.h"

class merge_tree;

class merge_tree {
private:
    std::vector<int> live;
    merge_tree* parent;
    std::vector<merge_tree*> children;
    refinement* merge;
    bool is_leaf;

public:
    merge_tree(int n);
    merge_tree(merge_tree* parent_node, refinement* ref, state_merger* merger);
    ~merge_tree();

    inline std::vector<int> get_live() { return live; }
    inline merge_tree* get_parent() { return parent; }
    inline std::vector<merge_tree*> get_children() { return children; }
    inline bool get_is_leaf() { return is_leaf; }

    bool is_root();
    bool is_empty();
    std::vector<refinement*> get_path();
    void add_live(int index);
    void perform_merges(state_merger* merger);
    void revert_merges(state_merger* merger);
    void initialize_children(state_merger* merger);
    std::pair<std::vector<merge_tree*>, std::vector<merge_tree*>> allocate_live();
    std::map<int, int> generate_allocation();
};



#endif //MERGE_TREE_H
