
#pragma once

#include "app/sat/parse/serialized_formula_parser.hpp"
#include "data/job_description.hpp"
#include "util/logger.hpp"
#include "util/params.hpp"
#include "util/sys/proc.hpp"
#include "util/sys/tmpdir.hpp"
#include <vector>
#include <filesystem>

class SatPreprocessActor {

public:
    SatPreprocessActor(const Parameters& params, const std::string& name, std::vector<int>&& inputCnf) :
        _params(params), _name(name), _input_cnf(std::move(inputCnf)) {}

    virtual void preprocessAsync() = 0;
    enum PreprocessActorResult {PENDING, SAT, UNSAT, SIMPLIFIED, ERROR, NONE};
    virtual bool isDonePreprocessing() const {return _result != PENDING;}
    virtual PreprocessActorResult getPreprocessingResult() const {return _result;}
    std::string getPreprocessingResultAsString() const {
        switch (_result) {
        case NONE: return "NONE";
        case PENDING: return "PENDING";
        case SAT: return "SAT";
        case UNSAT: return "UNSAT";
        case SIMPLIFIED: return "SIMPLIFIED";
        default: return "ERROR";
        }
    }
    virtual std::vector<int>&& getPreprocessedFormula() {
        return std::move(_output_cnf);
    }
    virtual std::vector<int>&& getModel() {
        return std::move(_model);
    }

    virtual void interrupt() {}
    virtual void join() {if (_fut_prepro.valid()) _fut_prepro.get();}
    virtual void reconstructSolution(std::vector<int>& sol) = 0;

    const std::string& getProofFormat() const {return _proof_format;}

    int nbInputVars() const {
        assert(_input_cnf.size() >= 2);
        return _input_cnf[_input_cnf.size() - 2];
    }
    int nbInputClauses() const {
        assert(_input_cnf.size() >= 2);
        return _input_cnf[_input_cnf.size() - 1];
    }
    const std::vector<int>& getInputCnf() const {
        return _input_cnf;
    }
    const char* getName() const {return _name.c_str();}

    // All actors write their in-progress proof output here instead of directly
    // into proofDirectory() -- a still-running (or not-yet-confirmed-stopped)
    // actor's files then never need to be deleted/waited-on as part of any
    // particular job's completion; only the winning chain's files ever get
    // moved out, via rename_proof() below. Cleanup of this directory happens
    // independently, elsewhere (it uses the "termrelev" tmp-file naming
    // convention, same as e.g. ExtSatsumaCaller's named pipes). Scoped by PID
    // (not e.g. a hash of proofDirectory()) so that a glob for
    // "...proofwork.*" -- ignoring the PID part -- reliably finds every such
    // directory ever created, e.g. for a one-off manual cleanup sweep.
    static std::string proofWorkDir(const Parameters& params) {
        return TmpDir::getGeneralTmpDir() + "/edu.kit.iti.mallobtermrelev.proofwork."
            + std::to_string(Proc::getPid()) + "/";
    }

    bool rename_proof(int i){
        // Copy+remove, not rename(): the work directory (TmpDir, typically local
        // disk/tmpfs) and proofDirectory() (user-chosen, e.g. on NFS) can be on
        // different filesystems, and rename()/std::filesystem::rename() cannot
        // cross a filesystem boundary (fails with EXDEV).
        std::string src = proofWorkDir(_params) + "tmp." + _name + "." + _proof_format;
        std::string dst = _params.proofDirectory() + "/step" + std::to_string(i) + "." + _proof_format;
        try {
            std::filesystem::copy_file(src, dst);
            std::filesystem::remove(src);
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            LOG(V0_CRIT, "[ERROR] Could not move proof file \"%s\" to \"%s\": %s\n",
                src.c_str(), dst.c_str(), e.what());
            return false;
        }
    }

protected:
    const Parameters& _params;
    std::string _name;
    std::string _proof_format;
    const std::vector<int> _input_cnf;
    std::vector<int> _output_cnf;
    std::vector<int> _model;
    volatile PreprocessActorResult _result {PENDING};
    std::future<void> _fut_prepro;
};
