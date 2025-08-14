#pragma once

#include "arguments_parse.hpp"
#include "log.hpp"

#define TRACE_ENTER_MESSAGE "Entering "
#define TRACE_EXIT_MESSAGE "Exiting "

// TODO: Implement arguments tracing
#define traceEnter()                                                        \
    do {                                                                    \
        if ( g_needTrace ) {                                                \
            std::string l_message = TRACE_ENTER_MESSAGE;                    \
            l_message.append( "\"" ).append( __FUNCTION__ ).append( "\"" ); \
            log( l_message );                                               \
        }                                                                   \
    } while ( 0 )

// TODO: Implement return value tracking
#define traceExit()                                                         \
    do {                                                                    \
        if ( g_needTrace ) {                                                \
            std::string l_message = TRACE_EXIT_MESSAGE;                     \
            l_message.append( "\"" ).append( __FUNCTION__ ).append( "\"" ); \
            log( l_message );                                               \
        }                                                                   \
    } while ( 0 )
