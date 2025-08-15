#pragma once

#include <llvm/Support/raw_ostream.h>

#include "arguments_parse.hpp"

// TODO: Decide on this
#define LOG_INFO_FORMAT                                                \
    "\"" << __PRETTY_FUNCTION__ << "\"" << " " << __FILE_NAME__ << ":" \
         << __LINE__ << " | "

// Function file:line | message
#define log( _message )                           \
    do {                                          \
        if ( !g_isQuietRun ) {                    \
            llvm::outs() << ( _message ) << "\n"; \
        }                                         \
    } while ( 0 )

// Function file:line | message
#define logError( _message ) llvm::errs() << ( _message ) << "\n";
