//
// Created by Anna Goerth on 20.01.26.
//

#include <assert.h>
#include <stdlib.h>
#include <string>
#include <initializer_list>

#include "util/random.hpp"
#include "app/sat/parse/sat_reader.hpp"
#include "util/logger.hpp"
#include "util/sys/timer.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include <filesystem>
#include "util/sys/thread_pool.hpp"
#include "scheduling/core_allocator.hpp"


int main(int argc, char *argv[]) {

    Timer::init();
    Random::init(rand(), rand());
    Logger::init(0, V5_DEBG);

    Parameters params;
    params.init(argc, argv);

    ProcessWideThreadPool::init(4); 
    ProcessWideCoreAllocator::init(4);

    const std::string unprocessed_path = "../instances/clique/clique_270.cnf";
    const std::string processed_path = "../instances/clique/clique_270.break.cnf";

    assert(std::filesystem::exists(unprocessed_path));
    assert(std::filesystem::exists(processed_path ));
    

     {
        auto f = unprocessed_path;
        LOG(V2_INFO, "Reading test CNF %s ...\n", f.c_str());
        float time = Timer::elapsedSeconds();
        SatReader r(params, f);
        JobDescription d;
        bool success = r.read(d);
        assert(success);
        time = Timer::elapsedSeconds() - time;
        LOG(V2_INFO, " - done, took %.3fs\n", time);

        auto f = processed_path;
        LOG(V2_INFO, "Reading test CNF %s ...\n", f.c_str());
        float time2 = Timer::elapsedSeconds();
        SatReader r2(params, f);
        JobDescription d2;
        bool success2 = r2.read(d);
        assert(success2);
        time2 = Timer::elapsedSeconds() - time2;
        LOG(V2_INFO, " - done, took %.3fs\n", time2);

        /*LOG(V2_INFO, "Only decompressing CNF %s for comparison ...\n", f.c_str());
        float time2 = Timer::elapsedSeconds();
        auto cmd = "xz -c -d " + f + " > /tmp/tmpfile";
        int retval = system(cmd.c_str());
        time2 = Timer::elapsedSeconds() - time2;
        LOG(V2_INFO, " - done, took %.3fs\n", time2);
        assert(retval == 0);

        LOG(V2_INFO, " -- difference: %.3fs\n", time - time2);*/
    }
}



/* #include "data/job_description.hpp"
#include <iostream>
#include "app/sat/parse/sat_reader.hpp" // Mallobs CNF-Reader
#include "util/params.hpp"
#include "util/logger.hpp"
#include "app/satwithpre/sat_preprocessor.hpp"
#include <filesystem>

#include "util/json.hpp"
#include <string>
#include <vector>

#include "util/params.hpp"
#include "util/logger.hpp"
#include "util/sys/timer.hpp"
#include "util/sys/proc.hpp"
#include "util/sys/thread_pool.hpp" // EXTREM WICHTIG für Preprocessor

// Daten & SAT
#include "data/job_description.hpp"
#include "app/sat/parse/sat_reader.hpp"
#include "app/satwithpre/sat_preprocessor.hpp"
#include "app/sat/parse/serialized_formula_parser.hpp"

#include "data/job_description.hpp"
#include "app/sat/parse/sat_reader.hpp"
#include "app/satwithpre/sat_preprocessor.hpp"
#include "app/sat/parse/serialized_formula_parser.hpp"

// Diese Funktion baut dir das JSON-Objekt, das Mallob als Job versteht
nlohmann::json buildSimpleJobDescription(const std::string& filePath, const std::string& appName = "SAT") {
    nlohmann::json job;
    
    job["user"] = "admin";
    job["name"] = "my-local-job";
    job["files"] = {filePath}; // Hier kommt dein Pfad rein
    job["priority"] = 1.0;
    job["application"] = appName; // z.B. "SAT" oder "LOGIC"
    
    // Optional: Limits hinzufügen, falls gewünscht
    // job["wallclock-limit"] = "60s"; 
    
    return job;
}

#include <iostream>
#include <iomanip>

void test(std::string unprocessed_path, std::string target_path) {
    std::cout << "  Input:  " << unprocessed_path << "\n  Target: " << target_path << std::endl;

    std::cout << "[START] Test über JobReader" << std::endl;

    Parameters params;
    params.init(0, nullptr);
    
    // Logger initialisieren, falls noch nicht geschehen (verhindert Abstürze bei LOG-Aufrufen)
    Timer::init(); 
    Proc::nameThisThread("MainThread");

    // Logger initialisieren, damit wir sehen was passiert
    Logger::LoggerConfig logConfig;
    logConfig.verbosity = V2_INFO;
    Logger::init(logConfig);

    // ThreadPool für den Preprozessor
    ProcessWideThreadPool::init(4); 
    ProcessWideCoreAllocator::init(4);


    // WICHTIG: JobReader erwartet eine JobDescription, die er befüllen kann
    // 2. Objekte für die Daten
    JobDescription unprocessed_desc;
    JobDescription target_desc;

    // 3. Datei 1 laden mit SatReader
    std::cout << "[STEP 1] Lade: " << unprocessed_path << std::flush;
    SatReader reader1(params, unprocessed_path);
    if (!reader1.read(unprocessed_desc)) {
        std::cerr << "\n[FEHLER] Konnte Datei 1 nicht lesen!" << std::endl;
        return;
    }
    std::cout << " -> OK (" << reader1.getNbVars() << " Vars)" << std::endl;

    // 4. Datei 2 laden mit SatReader
    std::cout << "[STEP 2] Lade: " << target_path << std::flush;
    SatReader reader2(params, target_path);
    if (!reader2.read(target_desc)) {
        std::cerr << "\n[FEHLER] Konnte Datei 2 nicht lesen!" << std::endl;
        return;
    }
    std::cout << " -> OK (" << reader2.getNbVars() << " Vars)" << std::endl;

    // 5. Metadaten manuell in die JobDescription übertragen
    // Das ist der Schritt, den der SatReader NICHT automatisch macht
    std::cout << "[STEP 3/7] Metadaten... " << std::flush;
    unprocessed_desc.getAppConfiguration().updateFixedSizeEntry("__NV", reader1.getNbVars());
    unprocessed_desc.getAppConfiguration().updateFixedSizeEntry("__NC", reader1.getNbClauses());
    target_desc.getAppConfiguration().updateFixedSizeEntry("__NV", reader2.getNbVars());
    target_desc.getAppConfiguration().updateFixedSizeEntry("__NC", reader2.getNbClauses());

    // 5. Preprozessor-Lauf
    std::cout << "[STEP 4/7] Initialisiere SatPreprocessor... " << std::flush;
    SatPreprocessor preprocessor(params, unprocessed_desc, false);
    preprocessor.init();
    std::cout << "OK" << std::endl;

    std::cout << "[STEP 5/7] Warte auf Preprozessor-Fertigstellung... " << std::flush;
    int timeout_seconds = 0;
    while (!preprocessor.hasPreprocessedFormula()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "." << std::flush;
        timeout_seconds++;
        if (timeout_seconds > 60) { // 30 Sekunden Sicherheits-Timeout
            std::cerr << "\n[TIMEOUT] Preprozessor braucht zu lange (>30s)!" << std::endl;
            return;
        }
    }
    std::cout << " FERTIG" << std::endl;

    // 6. Daten extrahieren und bereinigen
    std::cout << "[STEP 6/7] Extrahiere Payload... " << std::flush;
    std::vector<int> formula = preprocessor.extractPreprocessedFormula();
    preprocessor.join(false);

    if (formula.size() < 2) {
        std::cerr << "\n[ERROR] Preprocessed Formula ist leer oder zu klein!" << std::endl;
        return;
    }

    int nbClauses = formula.back(); formula.pop_back();
    int nbVars = formula.back();    formula.pop_back();
    std::cout << "OK (Extrahiert: " << nbVars << " Vars, " << nbClauses << " Clauses)" << std::endl;

    // 7. Vergleichsschleife
    std::cout << "[STEP 7/7] Vergleiche Literale... " << std::flush;
    
    SerializedFormulaParser preParser(Logger::getMainInstance(), formula.data(), formula.size());
    SerializedFormulaParser targetParser(Logger::getMainInstance(), target_desc.getFormulaPayload(0), target_desc.getFormulaPayloadSize(0));

    int preLit, targetLit;
    size_t count = 0;
    while (preParser.getNextLiteral(preLit)) {
        if (!targetParser.getNextLiteral(targetLit)) {
            std::cerr << "\n[FAIL] Target-Datei endete vorzeitig bei Literal #" << count << std::endl;
            return;
        }
        if (preLit != targetLit) {
            std::cerr << "\n[FAIL] Abweichung bei Literal #" << count 
                      << ": Pre=" << preLit << " vs Target=" << targetLit << std::endl;
            return;
        }
        count++;
        if (count % 100000 == 0) std::cout << "h" << std::flush; // Fortschritt bei großen Formeln
    }

    if (targetParser.getNextLiteral(targetLit)) {
        std::cerr << "\n[FAIL] Target-Datei hat noch zusätzliche Literale!" << std::endl;
        return;
    }

    std::cout << "\n[SUCCESS] Test bestanden! " << count << " Literale identisch." << std::endl;
}



int main() {

    const std::string unprocessed_path = "../instances/clique/clique_270.cnf";
    const std::string processed_path = "../instances/clique/clique_270.break.cnf";

    assert(std::filesystem::exists(unprocessed_path));
    assert(std::filesystem::exists(processed_path ));


    test(unprocessed_path, processed_path);
}
*/