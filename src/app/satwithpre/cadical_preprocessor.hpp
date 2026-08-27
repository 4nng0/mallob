
#pragma once

#include <atomic>
#include <filesystem>
#include <vector>

#include "app/sat/execution/solver_setup.hpp"
#include "app/sat/solvers/cadical.hpp"
#include "app/satwithpre/lrat_fixed_literal_patch.hpp"
#include "app/satwithpre/sat_preprocess_actor.hpp"
#include "data/job_description.hpp"
#include "scheduling/core_allocator.hpp"
#include "util/logger.hpp"
#include "util/params.hpp"
#include "util/sys/thread_pool.hpp"
#include <future>



class CadicalPreprocessor : public SatPreprocessActor {

    private:
    std::unique_ptr<Cadical> _cadical;
    std::atomic<bool> _interrupted{false};

    std::string proofPath() const {
        return _params.proofDirectory() + "/tmp/" + _name + "." + _proof_format;
    }

    public: 

    CadicalPreprocessor(const Parameters& params, const JobDescription& desc, const std::string& name, std::vector<int>&& formula) :
        SatPreprocessActor(params, name, std::move(formula)) {_proof_format = "lrat";}

    ~CadicalPreprocessor() {}

    void preprocessAsync() override {
        _fut_prepro = ProcessWideThreadPool::get().addTask([&]() {
            CoreAllocator::Allocation ca(1);

            SolverSetup setup;
            setup.logger = &Logger::getMainInstance();
            setup.numVars = nbInputVars();
            setup.numOriginalClauses = nbInputClauses();
            setup.solverType = 'p';
            setup.flavour = PortfolioSequence::PREPROCESS;
            setup.adaptiveImportManager = false; // no clause sharing in preprocessing
            _cadical.reset(new Cadical(setup));
            if (_interrupted) _cadical->setSolverInterrupt();
            if (_params.savePreprocessingProofs())
                _cadical->savePreproProof(proofPath());

            _cadical->diversify(0);
            for (int i = 0; i+2 < _input_cnf.size(); i++) {
                _cadical->addLiteral(_input_cnf[i]);
            }

            LOG(V2_INFO, "PREPRO running Cadical\n");
            int res = _cadical->solve(0, nullptr);
            if (_params.savePreprocessingProofs())
                _cadical->closePreproProof();
            LOG(V2_INFO, "PREPRO Cadical done, result %i\n", res);
            if (res == 10) {
                _model = _cadical->getSolution();
                _result = SAT;
            } else if (res == 20) {
                _result = UNSAT;
            } else {
                // fixes the missing proof lines, as collectSimplifiedFormula deletes literals falsefied by unit propagation. 
                if (_params.savePreprocessingProofs()) {
                    auto fixedLits = _cadical->getFixedLiterals();
                    if (fixedLits.empty()) {
                        LOG(V3_VERB, "PREPRO %s no fixed literals, proof already complete\n", getName());
                    } else {
                        long n = LratFixedLiteralPatch::apply(proofPath(), _input_cnf, fixedLits);
                        if (n < 0) LOG(V1_WARN, "[WARN] PREPRO %s could not patch proof \"%s\"\n",
                            getName(), proofPath().c_str());
                        else LOG(V2_INFO, "PREPRO %s %i fixed literals, %ld proof steps appended\n",
                            getName(), (int) fixedLits.size(), n);
                    }
                }
                _cadical->collectSimplifiedFormula();
                if (_cadical->hasPreprocessedFormula()) {
                    _output_cnf = std::move(_cadical->extractPreprocessedFormula());
                    int outVars = _output_cnf[_output_cnf.size() - 2];
                    int outClauses = _output_cnf[_output_cnf.size() - 1];
                    bool changed = outVars != nbInputVars() || outClauses != nbInputClauses()
                        || _output_cnf.size() != _input_cnf.size();
                    // theoretically possible, however unprobable, that the additons of variables and clauses lead 
                    // to the same numbers of each even though Cadical has found simplifications
                    if (!changed && _params.savePreprocessingProofs()) {
                        std::error_code ec;
                        changed = std::filesystem::file_size(proofPath(), ec) > 0 && !ec;
                    }
                    _result = changed ? SIMPLIFIED : NONE;
                } else {
                    _result = NONE;
                }
            }
        });
    }


    void interrupt() override {
        _interrupted = true;
        if (_cadical) _cadical->setSolverInterrupt();
    }


    void reconstructSolution(std::vector<int>& sol) override {
        _cadical->reconstructSolutionFromPreprocessing(sol);
    }

};