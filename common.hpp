#pragma once

// This macro turns a value into a string literal
#define STRINGIFY( _value ) #_value
// This is a helper macro that handles the stringify of macros
#define MACRO_TO_STRING( _macro ) STRINGIFY( _macro )
