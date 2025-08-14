#pragma once

#include <llvm/Support/raw_ostream.h>

#include "arguments_parse.hpp"

#define LOG_INFO_FORMAT                                                \
    "\"" << __PRETTY_FUNCTION__ << "\"" << " " << __FILE_NAME__ << ":" \
         << __LINE__ << " | "

// Function file:line | message
#define log( _message )                                              \
    do {                                                             \
        if ( !g_isQuietRun ) {                                       \
            llvm::outs() << LOG_INFO_FORMAT << ( _message ) << "\n"; \
        }                                                            \
    } while ( 0 )

// Function file:line | message
#define logError( _message ) \
    llvm::errs() << LOG_INFO_FORMAT << ( _message ) << "\n";
