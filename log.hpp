#pragma once

#include <llvm/Support/raw_ostream.h>

#include "arguments_parse.hpp"

// Function file:line | message
#define log( _message )                                                 \
    do {                                                                \
        if ( !g_isQuietRun ) {                                          \
            llvm::outs() << __FUNCTION__ << " " << __FILE_NAME__ << ":" \
                         << __LINE__ << " | " << ( _message ) << "\n";  \
        }                                                               \
    } while ( 0 )

// Function file:line | message
#define logError( _message )                                                \
    llvm::errs() << __FUNCTION__ << " " << __FILE_NAME__ << ":" << __LINE__ \
                 << " | " << ( _message ) << "\n";
