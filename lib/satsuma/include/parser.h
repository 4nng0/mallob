//
// Created by Anna Goerth on 07.12.25.
//

#ifndef SATSUMA_PARSER_H
#define SATSUMA_PARSER_H

#include "cnf2wl.h"
#include <string>

/// Parser-Funktion für DIMACS-Dateien
void parse_dimacs_to_cnf2wl(std::string& filename, cnf2wl& formula, bool entered_file);

#endif // SATSUMA_PARSER_H