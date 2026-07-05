
#pragma once

#include <atomic>
#include <vector>

#include "app/sat/execution/solver_setup.hpp"
#include "app/sat/solvers/cadical.hpp"
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
                _cadical->savePreproProof(_params.proofDirectory() + "/tmp." + _name + "." + _proof_format);

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
                _cadical->collectSimplifiedFormula();
                if (_cadical->hasPreprocessedFormula()) {
                    _output_cnf = std::move(_cadical->extractPreprocessedFormula()); 
                    _result = SIMPLIFIED;
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