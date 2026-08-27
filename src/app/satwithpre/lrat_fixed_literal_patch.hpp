
#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


class LratFixedLiteralPatch {

public:
    // Appends the missing steps to the (closed, text-format) proof at proofPath.
    // fixedLits holds the root-level fixed literals in their true polarity.
    // Returns the number of appended strengthening steps, or -1 on I/O error.
    static long apply(const std::string& proofPath, const std::vector<int>& inputCnf,
            const std::vector<int>& fixedLits) {

        if (fixedLits.empty()) return 0;

        std::unordered_set<int> fixedSet(fixedLits.begin(), fixedLits.end());
        std::unordered_set<int> fixedVars;
        for (int lit : fixedLits) fixedVars.insert(std::abs(lit));

        // Only clauses over a fixed variable can be affected at all. Everything
        // else is parsed and thrown away right again, so peak memory stays tied
        // to that subset rather than to the size of the proof.
        auto relevant = [&](const std::vector<int>& clause) {
            for (int lit : clause) if (fixedVars.count(std::abs(lit))) return true;
            return false;
        };

        std::unordered_map<int64_t, std::vector<int>> alive; // id -> literals
        int64_t maxId = 0;

        // The original clauses carry ids 1..n in order of addition.
        {
            std::vector<int> clause;
            for (size_t i = 0; i + 2 < inputCnf.size(); i++) {
                int lit = inputCnf[i];
                if (lit != 0) {clause.push_back(lit); continue;}
                maxId++;
                if (relevant(clause)) alive[maxId] = clause;
                clause.clear();
            }
        }

        // Replay the proof: additions extend the clause set, deletions shrink it.
        {
            std::ifstream in(proofPath);
            if (!in.good()) return -1;
            std::string line;
            std::vector<int> lits;
            while (std::getline(in, line)) {
                const char* p = line.c_str();
                char* end;
                int64_t id = std::strtoll(p, &end, 10);
                if (end == p) continue; // blank or unparsable line
                p = end;
                while (*p == ' ') p++;
                if (id > maxId) maxId = id;
                if (*p == 'd') {
                    p++;
                    while (true) {
                        int64_t v = std::strtoll(p, &end, 10);
                        if (end == p || v == 0) break;
                        p = end;
                        alive.erase(v);
                    }
                    continue;
                }
                lits.clear();
                while (true) {
                    int64_t v = std::strtoll(p, &end, 10);
                    if (end == p || v == 0) break;
                    p = end;
                    lits.push_back((int) v);
                }
                if (relevant(lits)) alive[id] = lits; // hints after the 0 are irrelevant here
            }
        }

        // A fixed literal's unit clause is itself over a fixed variable, so it is
        // among the clauses we kept; its id is the antecedent for every literal
        // it falsifies.
        std::unordered_map<int, int64_t> unitId;
        for (const auto& [id, clause] : alive)
            if (clause.size() == 1) unitId[clause[0]] = id;

        std::ofstream out(proofPath, std::ios::app);
        if (!out.good()) return -1;

        std::vector<int64_t> satisfiedIds;
        std::vector<int64_t> hints;
        std::vector<int> shortened;
        long nbStrengthened = 0;

        for (const auto& [id, clause] : alive) {

            bool satisfied = false;
            for (int lit : clause) if (fixedSet.count(lit)) {satisfied = true; break;}
            if (satisfied) {satisfiedIds.push_back(id); continue;}

            // Mirrors Proof::flush_clause (lib/cadical/src/proof.cpp:469-482):
            // walk the literals in clause order, collecting the unit id behind
            // each falsified one and keeping the rest, then close the antecedent
            // chain with the clause itself. That is also the propagation order a
            // checker replays -- the units force the removed literals, after
            // which the original clause is fully falsified.
            shortened.clear();
            hints.clear();
            bool complete = true;
            for (int lit : clause) {
                if (!fixedSet.count(-lit)) {shortened.push_back(lit); continue;}
                auto it = unitId.find(-lit);
                if (it == unitId.end()) {complete = false; break;} // no antecedent: leave alone
                hints.push_back(it->second);
            }
            if (!complete || hints.empty()) continue;

            int64_t newId = ++maxId;
            out << newId;
            for (int lit : shortened) out << ' ' << lit;
            out << " 0";
            for (int64_t hint : hints) out << ' ' << hint;
            out << ' ' << id << " 0\n";
            out << newId << " d " << id << " 0\n";
            nbStrengthened++;
        }

        // Deliberately last: the satisfied clauses include the unit clauses that
        // the strengthening steps above cite as antecedents, and a checker needs
        // those still present when it verifies them.
        if (!satisfiedIds.empty()) {
            out << maxId << " d";
            for (int64_t id : satisfiedIds) out << ' ' << id;
            out << " 0\n";
        }

        return nbStrengthened;
    }
};
