//
// Created by Anna Goerth on 07.12.25.
//

// satsuma/include/cnf2wl.h

// Copyright 2025 Markus Anders
// This file is part of satsuma 1.2.
// See LICENSE for extended copyright information.

#ifndef SATSUMA_CNF2WL_H
#define SATSUMA_CNF2WL_H

#include <string>
#include <vector>
#include <utility>
#include <charconv>
#include <bitset>
namespace dejavu {
    namespace ds {
        class markset;
    }
}

struct CNF2WLImpl;

class cnf2wl {
private:
    std::vector<std::pair<int, int>> clauses_pt;
    std::vector<std::pair<int, int>> clauses_watches;
    std::vector<int> clauses;
    std::vector<std::vector<int>>    literal_used_list;
    std::vector<std::vector<int>>    variable_watches_clause;

    std::vector<int> assignment;

    int number_of_variables = 0;
    int units_applied = 0;
    int redundant_removed = 0;
    int subsumptions_found = 0;
    bool conflict = false;
    std::vector<int>    units;

    std::unique_ptr<CNF2WLImpl> marksets;

    /*dejavu::ds::markset in_units;
    dejavu::ds::markset clause_satisfied;
    dejavu::ds::markset found_literal;
    dejavu::ds::markset test_redundant;
    dejavu::ds::markset test_for_subsumption;*/


    // --- Private Helper Methods (Short accessors/getters remain here as inline) ---

    // Macro for HASH0
    #define HASH0(x) (abs((x*417) % 63 + x>0?1:0))

    /**
     * @brief Sets a 'satisfied' flag.
     */
    void update_satisfied(int clause_number);

    /**
     * @brief Adds a watcher. Inline because it is very short.
     */
    inline void add_watch(int variable, int watch_clause) {
        variable_watches_clause[abs(variable)].push_back(watch_clause);
    }

    /**
     * @brief Removes a watcher (implemented in .cpp, as it's more complex).
     */
    void remove_watch(int variable, int watch_pos);

    /**
     * @brief Queues a unit clause (implemented in .cpp).
     */
    void queue_units(int literal, int reason_clause);

    /**
     * @brief Dequeues the next unit clause (implemented in .cpp).
     */
    int dequeue_next_unit();

    /**
     * @brief Initializes watches (implemented in .cpp).
     */
    void initialize_watches(int clause_number);

    /**
     * @brief Updates watches (implemented in .cpp).
     */
    void update_watches(int clause_number, int from_variable, int from_pos);

    /**
     * @brief Computes a clause signature (implemented in .cpp).
     */
    unsigned long clause_signature(int cl);

public:
    // --- Public Methods (Prototypes for complex logic) ---

    cnf2wl();
    ~cnf2wl();
    void dynamicReserve(int n);
    void reserve(int n, int m);
    void mark_literal_uses();
    int mark_subsumed_clauses();
    void add_clause(std::vector<int>& clause);
    int propagate();
    void assign_literal(int literal);
    void reset_assignment();
    void dimacs_output_clauses(std::ostream& out);
    void dimacs_output_clauses(FILE* out);
    void clear();

    // --- Public Accessors (Inline, as they are short) ---

    /**
     * @brief Checks if a literal has been marked as used.
     */
    bool is_literal_marked_used(int lit);

    /**
     * @brief Returns the assigned value of the literal.
     */
    int assigned(int literal);
    /**
    * @brief Returns the literal at a specific clause position. Inline because it is very short.
    */
    inline int literal_at_clause_pos(int c, int i) {
        return clauses[clauses_pt[c].first + i];
    }

    /**
     * @brief Counts satisfied clauses. Inline.
     */
int satisfied_clauses();


    /**
     * @brief Returns the clause size. Inline because it is very short.
     */
    inline int clause_size(int c) {
        return clauses_pt[c].second - clauses_pt[c].first;
    }


    /**
     * @brief Checks if a specific clause is satisfied.
     */
    bool is_satisfied(int clause);

    // Simple getters
    inline int n_total_clause_size() { return clauses.size(); }
    inline int n_len() { return clauses.size(); }
    inline int n_clauses() { return clauses_pt.size(); }
    inline int n_redundant_clauses() { return redundant_removed; }
    inline int n_variables() { return number_of_variables; }
    inline bool is_conflicting() { return conflict; }
};

#endif //SATSUMA_CNF2WL_H