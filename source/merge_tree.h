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
    static int id_counter;
    std::vector<int> live;
    merge_tree* parent;
    std::vector<merge_tree*> children;
    refinement* merge;
    int level;
    int id;
    std::vector<int> index_path;
    std::vector<int> ancestors;

public:
    merge_tree(int n);
    merge_tree(merge_tree* parent_node, refinement* ref, int merge_ind);
    ~merge_tree();

    inline std::vector<int> get_live() { return live; }
    inline merge_tree* get_parent() { return parent; }
    inline std::vector<merge_tree*> get_children() { return children; }
    inline int get_level() { return level; }
    inline refinement* get_merge() { return merge; }
    inline int get_id() { return id; }
    inline std::vector<int> get_index_path() { return index_path; }
    inline std::vector<int> get_ancestors() { return ancestors; }

    bool is_root();
    bool is_empty();
    bool is_leaf(state_merger* merger);
    std::vector<refinement*> get_path();
    std::vector<refinement*> get_path(int nr_steps);
    void add_live(int index);
    void perform_merges(state_merger* merger);
    void perform_merges(state_merger* merger, int nr_steps);
    void revert_merges(state_merger* merger);
    void revert_merges(state_merger* merger, int nr_steps);
    void initialize_children(state_merger* merger);
    std::pair<std::vector<merge_tree*>, std::vector<merge_tree*>> allocate_live();
    std::map<int, int> generate_allocation();
    std::pair<int, int> find_common_ancestor(merge_tree* other);
};



#endif //MERGE_TREE_H
