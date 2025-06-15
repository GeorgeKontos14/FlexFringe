//
// Created by konto on 5/15/2025.
//

#ifndef MERGE_TREE_H
#define MERGE_TREE_H

#include <unordered_set>
#include <vector>

#include "refinement.h"

class merge_tree;

class merge_tree {
private:
    static int id_counter;
    std::vector<int> live;
    merge_tree* parent;
    refinement* merge;
    int level;
    int id;
    std::vector<int> idpath;
    std::vector<int> selected_children;
    std::vector<int> skipped_children;

public:
    merge_tree(int n);
    merge_tree(merge_tree* parent_node, refinement* ref);
    ~merge_tree();
    std::vector<merge_tree*> children;
    bool pruned;
    bool initialized;
    bool is_leaf;
    bool is_root;

    inline std::vector<int> get_live() { return live; }
    inline merge_tree* get_parent() { return parent; }
    inline std::vector<merge_tree*> get_children() { return children; }
    inline int get_level() { return level; }
    inline refinement* get_merge() { return merge; }
    inline int get_id() const { return id; }
    inline std::vector<int> get_id_path() { return idpath; }
    inline std::vector<int> get_selected_children() { return selected_children; }
    inline std::vector<int> get_skipped_children() { return skipped_children; }

    bool is_empty();
    std::vector<refinement*> get_path();
    std::vector<refinement*> get_path(int nr_steps);
    void add_live(int index);
    void perform_merges(state_merger* merger);
    void perform_merges(state_merger* merger, int nr_steps);
    void revert_merges(state_merger* merger);
    void revert_merges(state_merger* merger, int nr_steps);
    void initialize_children(state_merger* merger);
    void allocate_live();
    merge_tree* pivot();
    void update_children();
    void transform(merge_tree* other, state_merger* merger);
    std::map<int, int> generate_allocation();
};

#endif //MERGE_TREE_H
