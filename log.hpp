#pragma once

#include <llvm/Support/raw_ostream.h>

// Function file:line | message
#define logError( _message )                                                \
    llvm::errs() << __FUNCTION__ << " " << __FILE_NAME__ << ":" << __LINE__ \
                 << " | " << ( _message ) << "\n";\
