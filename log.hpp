#pragma once

#include <llvm/Support/raw_ostream.h>

#include "arguments_parse.hpp"
#include "common.hpp"

#define LOG_INFO_PREFIX "INFO: "
#define LOG_WARNING_PREFIX "WARNING: "
#define LOG_ERROR_PREFIX "ERROR: "

// TODO: Decide on this
// Function file:line | message
#define LOG_INFO_FORMAT                                                \
    "\"" << __PRETTY_FUNCTION__ << "\"" << " " << __FILE_NAME__ << ":" \
         << __LINE__ << " | "

#define log( _message )                                              \
    do {                                                             \
        if ( g_isVerboseRun ) {                                      \
            llvm::outs() << LOG_INFO_PREFIX << ( _message ) << "\n"; \
        }                                                            \
    } while ( 0 )

#define logWarning( _message )                                          \
    do {                                                                \
        if ( g_needWarningsAsErrors ) {                                 \
            logError( _message );                                       \
        } else if ( !g_isQuietRun ) {                                   \
            llvm::errs() << LOG_WARNING_PREFIX << ( _message ) << "\n"; \
        }                                                               \
    } while ( 0 )

#define logError( _message ) \
    llvm::errs() << LOG_ERROR_PREFIX << ( _message ) << "\n";

#define logVariable( _variable )                                            \
    do {                                                                    \
        if ( g_isVerboseRun ) {                                             \
            _logVariable( LOG_INFO_PREFIX __FILE_NAME__                     \
                          ":" MACRO_TO_STRING( __LINE__ ) " | " #_variable, \
                          _variable );                                      \
        }                                                                   \
    } while ( 0 )

template < typename T >
void _logVariable( const char* _variableName, const T& _value ) {
    llvm::outs() << _variableName << " = '" << _value << "'\n";
}

// NOTE: Overload for vector
template < typename T >
void _logVariable( const char* _variableName,
                   const std::vector< T >& _vector ) {
    llvm::outs() << _variableName << " = [";

    for ( size_t l_elementIndex = 0; l_elementIndex < _vector.size();
          ++l_elementIndex ) {
        llvm::outs() << _vector[ l_elementIndex ];

        if ( ( l_elementIndex + 1 ) < _vector.size() ) {
            llvm::outs() << ", ";
        }
    }

    llvm::outs() << "]\n";
}
