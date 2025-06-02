
#ifndef _ENSEMBLE_H_
#define _ENSEMBLE_H_

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <list>
#include "state_merger.h"
#include "refinement.h"


void bagging(state_merger* merger, std::string output_file, int nr_estimators);
void random_walk_ensemble(state_merger* merger, int nr_estimators, std::string& output_file);
void tree_balanced_ensemble(state_merger* merger, int nr_estimators, const std::string& output_file);
void tree_pruning_ensemble(state_merger* merger, int nr_estimators, const std::string& output_file);

#endif /* _ENSEMBLE_H_ */
