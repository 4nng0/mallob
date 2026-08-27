
#pragma once

#include "app/satwithpre/cadical_preprocessor.hpp"
#include "app/satwithpre/ext_satsuma_caller.hpp"
#include "app/satwithpre/kissat_preprocessor.hpp"
#include "app/satwithpre/lingeling_preprocessor.hpp"
#include "app/satwithpre/mallobsat_preprocess_actor.hpp"
#include "app/satwithpre/sat_preprocess_actor.hpp"
#include "app/satwithpre/satsuma_preprocessor.hpp"
#include "data/job_description.hpp"
#include "interface/api/api_connector.hpp"
#include "util/logger.hpp"
#include "util/params.hpp"
#include "util/sys/fileutils.hpp"
#include <list>

class PreprocessorOrchestrator {

private:
    const Parameters& _params;
    const JobDescription& _desc;
    APIConnector& _api;

    struct ActorContext {
        enum ActorType {SATSUMA_INT, SATSUMA_EXT, KISSAT, LINGELING, MALLOBSAT, CADICAL} type;
        ActorContext* prerequisite {nullptr};
        std::vector<ActorContext*> actorsBeingDisplaced;
        bool onlyStartIfPrerequisiteSimplified {false};

        bool onlyStartIfAnyAncestorSimplified {false};

        std::unique_ptr<SatPreprocessActor> actor;
        enum ActiveActorState {UNINITIALIZED, RUNNING, FINISHED} state {UNINITIALIZED};
        SatPreprocessActor::PreprocessActorResult result {SatPreprocessActor::PENDING};
        std::vector<int> formula;
        std::vector<int> model;
        float timeOfSignalledDisplacement {0};
    };
    std::list<ActorContext> _actors;
    const std::vector<int> _base_cnf;

    float _time_of_start;
    ActorContext* _winning_actor {nullptr};

public:
    PreprocessorOrchestrator(const Parameters& params, const JobDescription& desc, APIConnector& api) : _params(params), _desc(desc), _api(api),
            _base_cnf(getCnfFromJobDescription()) {

        _time_of_start = Timer::elapsedSeconds();

        if (_params.savePreprocessingProofs()) {
            auto existing = FileUtils::glob(_params.proofDirectory() + "/*");
            if (!existing.empty()) {
                LOG(V0_CRIT, "[ERROR] Proof directory \"%s\" is not empty - please clear it before running with -prepro-proofs=1\n",
                    _params.proofDirectory().c_str());
                abort();
            }
            FileUtils::mkdir(_params.proofDirectory() + "/tmp");
        }

        // ---------------------------------------------------------------------
        // PORTFOLIO MODE: baseline + one preprocessing chain that may displace it.
        //
        //   MallobSat on the untouched formula          (runs from t=0, displaceable)
        //   [Satsuma ->] CaDiCaL -> MallobSat           (displaces the baseline)
        //
        // The Satsuma stage only exists if built with -DMALLOB_USE_SATSUMA=2;
        // without it the chain simply starts at CaDiCaL, on the original formula.
        //
        // A stage finding nothing does NOT end the chain: CaDiCaL still runs on
        // whatever Satsuma passed on, and a stage that simplified nothing forwards
        // its input rather than falling back to the original formula. Only if *no*
        // stage achieved anything is the displacing solver suppressed -- solving
        // that output would just repeat the baseline while throwing away its
        // progress. That is what onlyStartIfAnyAncestorSimplified expresses.
        //
        // The competition "quick" topology (Lingeling + Satsuma->Kissat->MallobSat)
        // lives in scripts/experiments/orchestrator_QUICK.hpp and is swapped in only
        // when no portfolio pass is running -- rebuilding while one is in flight
        // changes the binary that its next instance picks up.
        // ---------------------------------------------------------------------

        // MallobSat on the original formula
        _actors.push_back({PreprocessorOrchestrator::ActorContext::MALLOBSAT, nullptr, {}});
        ActorContext* ctxMalOrig = &_actors.back();

        // Satsuma (preprocesses the formula) - only if built with -DMALLOB_USE_SATSUMA=2
        ActorContext* ctxSats = nullptr;
#if defined(MALLOB_USE_SATSUMA) && MALLOB_USE_SATSUMA == 2
        _actors.push_back({PreprocessorOrchestrator::ActorContext::SATSUMA_EXT, nullptr, {}});
        ctxSats = &_actors.back();
#else
        LOG(V1_WARN, "[WARN] Not built with external Satsuma (-DMALLOB_USE_SATSUMA=2):"
            " no Satsuma preprocessing, chain starts at CaDiCaL\n");
#endif

        // CaDiCaL on Satsuma's output, or on the original formula if there is no
        // Satsuma stage. Deliberately no condition: runs even if Satsuma found nothing.
        _actors.push_back({PreprocessorOrchestrator::ActorContext::CADICAL, ctxSats, {}});
        ActorContext* ctxCad = &_actors.back();

        // MallobSat on the preprocessed formula - displaces the baseline
        _actors.push_back({PreprocessorOrchestrator::ActorContext::MALLOBSAT, ctxCad, {ctxMalOrig}});
        ActorContext* ctxMalChain = &_actors.back();
        ctxMalChain->onlyStartIfAnyAncestorSimplified = true;
    }


    // True if this actor or any stage above it in the chain simplified the
    // formula, i.e. whether there is anything to gain from solving its output.
    static bool anyAncestorSimplified(const ActorContext& actor) {
        for (const ActorContext* a = actor.prerequisite; a; a = a->prerequisite)
            if (a->result == SatPreprocessActor::SIMPLIFIED) return true;
        return false;
    }

    int loop() {

        int actorIdx = -1;
        for (auto& actor : _actors) {
            actorIdx++;

            if (actor.state == ActorContext::UNINITIALIZED) {

                // check prerequisite
                if (actor.prerequisite && actor.prerequisite->state != ActorContext::FINISHED)
                    continue; // prerequisite not done yet - skip for now
                if (actor.prerequisite && actor.onlyStartIfPrerequisiteSimplified && actor.prerequisite->result != SatPreprocessActor::SIMPLIFIED)
                    continue; // never initialize this actor since its prerequisite didn't lead to a simplification
                if (actor.onlyStartIfAnyAncestorSimplified && !anyAncestorSimplified(actor))
                    continue; // the entire chain achieved nothing - solving its output would just redo the baseline

                // prerequisite done: initialize actor. Its formula is whatever the
                // prerequisite passes on, which is its simplified output if it
                // simplified and otherwise its own unchanged input -- so a stage
                // that finds nothing forwards its predecessor's work instead of
                // discarding it and falling back to the original formula.
                auto formula = actor.prerequisite ? actor.prerequisite->formula : _base_cnf;
                switch (actor.type) {
                //case ActorContext::SATSUMA_INT:
                //    actor.actor.reset(new SatsumaPreprocessor(_params, _desc, std::to_string(actorIdx) + ":SatsumaInt", std::move(formula)));
                    break;
                case ActorContext::SATSUMA_EXT:
                    actor.actor.reset(new ExtSatsumaCaller(_params, _desc, std::to_string(actorIdx) + ":SatsumaExt", std::move(formula)));
                    break;
                case ActorContext::KISSAT:
                    actor.actor.reset(new KissatPreprocessor(_params, _desc, std::to_string(actorIdx) + ":Kissat", std::move(formula)));
                    break;
                case ActorContext::LINGELING:
                    actor.actor.reset(new LingelingPreprocessor(_params, _desc, std::to_string(actorIdx) + ":Lingeling", std::move(formula)));
                    break;
                case ActorContext::MALLOBSAT:
                    actor.actor.reset(new MallobSatPreprocessActor(_params, _desc, std::to_string(actorIdx) + ":MallobSat", _api, std::move(formula), _time_of_start));
                    break;
                case ActorContext::CADICAL:
                    actor.actor.reset(new CadicalPreprocessor(_params, _desc, std::to_string(actorIdx) + ":Cadical", std::move(formula)));
                    break;
                }
                LOG(V2_INFO, "SATWP launch %s\n", actor.actor->getName());
                actor.actor->preprocessAsync();
                actor.state = ActorContext::RUNNING;

                // signal displacement to actors being displaced
                for (auto& other : actor.actorsBeingDisplaced) {
                    if (other->timeOfSignalledDisplacement <= 0) {
                        if (other->actor)
                            LOG(V2_INFO, "SATWP %s --displace--> %s\n", actor.actor->getName(), other->actor->getName());
                        else
                            LOG(V2_INFO, "SATWP %s --displace--\n", actor.actor->getName());
                        other->timeOfSignalledDisplacement = Timer::elapsedSeconds();
                    }
                }
            }

            if (actor.state == ActorContext::RUNNING && actor.actor->isDonePreprocessing()) {
                // this actor is done
                auto res = actor.actor->getPreprocessingResult();
                LOG(V2_INFO, "SATWP %s done, result %s\n", actor.actor->getName(),
                    actor.actor->getPreprocessingResultAsString().c_str());
                if (res == SatPreprocessActor::SAT) {
                    actor.model = std::move(actor.actor->getModel());
                }
                if (res == SatPreprocessActor::SIMPLIFIED) {
                    actor.formula = std::move(actor.actor->getPreprocessedFormula());
                    if (_params.savePreprocessingProofs()) actor.actor->writeCnf(actor.formula);
                } else {
                    actor.formula = std::move(actor.actor->getInputCnf());
                }
                assert(actor.formula.size() > 2);
                assert(actor.formula[actor.formula.size() - 2] >= 0); // # vars
                assert(actor.formula[actor.formula.size() - 1] >= 0); // # clauses
                actor.result = res;
                actor.state = ActorContext::FINISHED;
                if (res == SatPreprocessActor::SAT) {
                    LOG(V2_INFO, "SATWP %s found SAT\n", actor.actor->getName());
                    _winning_actor = &actor;
                    return 10;
                }
                if (res == SatPreprocessActor::UNSAT) {
                    LOG(V2_INFO, "SATWP %s found UNSAT\n", actor.actor->getName());
                    _winning_actor = &actor;
                    return 20;
                }
                assert(actor.formula[0] != 0); // no empty clause without reporting UNSAT!
            }

            if (actor.state == ActorContext::RUNNING && actor.timeOfSignalledDisplacement > 0) {
                // this actor is being displaced (after some time)
                if (_params.preprocessBalancing() == 0) {
                    LOG(V2_INFO, "SATWP %s interrupt\n", actor.actor->getName());
                    actor.actor->interrupt();
                    actor.timeOfSignalledDisplacement = 0;
                }
                if (_params.preprocessBalancing() == 1 && Timer::elapsedSeconds() - _time_of_start >=
                    _params.preprocessExpansionFactor() * (actor.timeOfSignalledDisplacement - _time_of_start)) {
                    LOG(V2_INFO, "SATWP %s interrupt\n", actor.actor->getName());
                    actor.actor->interrupt();
                    actor.timeOfSignalledDisplacement = 0;
                }
            }
        }

        return 0;
    }

    std::vector<int> getModel() {
        auto actor = _winning_actor;
        assert(actor);
        auto model = std::move(actor->model);
        while (true) {
            actor = actor->prerequisite;
            if (!actor) break;
            actor->actor->reconstructSolution(model);
        }
        return model;
    }

    void stopAll() {
        for (auto& actor : _actors) if (actor.state == ActorContext::RUNNING) {
            LOG(V2_INFO, "SATWP %s interrupt\n", actor.actor->getName());
            actor.actor->interrupt();
        }
    }

    void finalizeProofs(){
        if (!_params.savePreprocessingProofs() ) return;
        ActorContext* last = _winning_actor ;
        if (!last || last->result != SatPreprocessActor::UNSAT) return;
        std::vector<ActorContext*> line;
        // only include actors that acually simplify the formula 

        bool isWinner = true;
        while (last != nullptr){
            if (isWinner || last->result == SatPreprocessActor::SIMPLIFIED) line.push_back(last);
            isWinner = false;
            last = last->prerequisite;
        }
        int total = line.size();
        for (int i = 1; i <= total; i++){
            ActorContext* step = line[total - i];
            step->actor->rename_proof(i);
        }
    }

private:
    std::vector<int> getCnfFromJobDescription() {

        SerializedFormulaParser parser(Logger::getMainInstance(), _desc.getFormulaPayload(0),
            _desc.getFormulaPayloadSize(0));
        if (_params.compressFormula()) parser.setCompressed();
        int nbVars = _desc.getAppConfiguration().fixedSizeEntryToInt("__NV");
        int nbCls = _desc.getAppConfiguration().fixedSizeEntryToInt("__NC");

        std::vector<int> cnf;
        int lit;
        while (parser.getNextLiteral(lit)) cnf.push_back(lit);
        cnf.push_back(nbVars);
        cnf.push_back(nbCls);
        return cnf;
    }
};
