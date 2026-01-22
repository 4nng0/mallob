//
// Created by Anna Goerth on 07.12.25.
//
// satsuma/include/preprocessor.h

// Copyright 2025 Markus Anders
// This file is part of satsuma 1.2.
// See LICENSE for extended copyright information.

#ifndef SATSUMA_SATSUMA_H
#define SATSUMA_SATSUMA_H

#define SATSUMA_VERSION_MAJOR 1
#define SATSUMA_VERSION_MINOR 2

#include "cnf2wl.h"
#include <iostream>

class cnf;
class profiler;
class proof_veripb;

namespace satsuma {

    /**
     * \brief The satsuma preprocessor.
     *
     */
    class preprocessor {
        bool        entered_output_file = false;
        std::string output_filename     = "";
        std::vector<int> preprocessed_formula;
        bool      save_as_formula = false;

        // further modules
        std::ostream* log         = &std::clog; /**< logging */
        proof_veripb* my_proof    = nullptr;    /**< proof logging */
        profiler* my_profiler = nullptr;    /**< profiling */

        // default configuration
        // modes
        bool struct_only = false;
        bool graph_only   = false;

        // general limits
        long graph_component_size_limit = 20000000; //< hard limit for component size

        // limits for special detection
        int row_orbit_limit        = 64;
        int row_column_orbit_limit = 64;
        int johnson_orbit_limit    = 64;

        // routines
        bool optimize_generators = true;
        bool preprocess_cnf      = false;
        bool preprocess_cnf_subsume = false;
        bool hypergraph_macros   = true;
        bool binary_clauses      = false;

        // limits for generator optimization
        int  opt_optimize_passes = 64;
        int  opt_addition_limit  = 196;
        int  opt_conjugate_limit = 256;
        bool opt_reopt = false;

        // options for breaking constraints
        int break_depth          = 512;

        long absolute_support_limit = 2 * 256 * 1024 * 1024; // we want no more than 2 GB worth of symmetries
        long split_limit = 1024*1024*16;

        // dejavu settings
        bool dejavu_print           = false;
        bool dejavu_prefer_dfs      = false;
        int  dejavu_budget_limit    = -1; // <0 means no limits
        int  dejavu_backtrack_limit = 64;

        /**
            Compute a symmetry breaking predicate for the given formula.

            @param formula The given CNF formula.
        */
        void generate_symmetry_predicate(cnf& formula);


    public:

        /**
        * \brief Returns a reference to the preprocessed formula, if it has been written yet.
        */
       std::vector<int>&& extractPreprocessedFormula();

        void set_struct_only(bool use_only_struct);

        void set_graph_only(bool use_only_struct);

        void set_optimize_generators(bool use_optimize_generators);

        void output_file(std::string& outfile);

       void set_save_as_Formula(bool save);

		bool hasPreprocessedFormula();

        int get_row_orbit_limit() const;

        void set_row_orbit_limit(int rowOrbitLimit);

        int get_row_column_orbit_limit() const;

        void set_row_column_orbit_limit(int rowColumnOrbitLimit);

        int get_johnson_orbit_limit() const;

        void set_johnson_orbit_limit(int johnsonOrbitLimit);

        int get_break_depth() const;

        void set_break_depth(int breakDepth);

        void set_opt_passes(int passes);

        void set_opt_conjugations(int conjugations);

        void set_opt_random(int random);

        void set_opt_reopt(bool reopt);

        void set_dejavu_print(bool print);

        void set_dejavu_backtrack_limit(int limit);

        void set_component_size_limit(int limit);

        void set_absolute_support_limit(int limit);

        void set_split_limit(int limit);

        void set_dejavu_prefer_dfs(bool prefer_dfs);

        void set_preprocess_cnf_subsume(bool preprocessCNFsubsume);

        void set_preprocess_cnf(bool preprocessCNF);

        void set_hypergraph_macros(bool hypergraphMacros);

        void set_binary_clauses(bool binaryClauses);

        void set_proof(proof_veripb* my_proof);

        void set_profiler(profiler* my_profiler);

        void set_log_output(std::ostream* new_logout);

        /**
         * \brief Main entry point for preprocessing a formula.
         *
         * @param formula The input CNF-like formula.
         */
        void preprocess(cnf2wl& formula);
    };
}

#endif //SATSUMA_SATSUMA_H