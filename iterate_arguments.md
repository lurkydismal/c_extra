<!-- The full declaration, including return type, name, and argument list -->
```cpp
FORCE_INLINE void iterate_arguments( const char* _callback )
```

##### Calls `_callback` with every argument from ancestor function

### **arguments**

```cpp
- _callback (const char*): Callback to call with each argument.
```

### **Return Value**

<!-- Type and meaning of the return value. -->
<!-- Include possible error codes or special cases (e.g., `NULL` on failure). -->
```cpp
void
```

### **Attributes/ Qualifiers**

<!-- Any special C attributes (e.g., `inline`, `FORCE_INLINE`, `static`, `CONST`, `PURE`, `NO_RETURN`, `NO_OPTIMIZE`, `__attribute__`, `DEPRECATED`, `HOT`, `COLD`, `SENTINEL`). -->
```cpp
FORCE_INLINE
```

### **Side Effects**

<!-- Describe any side effects like modifying global variables, allocating memory, writing to files, etc. -->
Depends on callback.

### **Thread Safety/ Reentrancy**

<!-- Mention whether the function is thread-safe or reentrant. -->
Depends on arguments and callback.

### **Error Handling**

<!-- How the function handles errors. -->
<!-- Any `errno` values set. -->
<!-- Return value conventions (e.g., negative on error). -->
On precondition violation processing aborts.

### **Examples/ Usage**

```c
#define ARGUMENT_FORMAT \
    "Argument name: '%s'\n" \
    "Argument type: '%s'\n" \
    "Argument size: '%zu'\n\n"

#define printArgument( _argumentName, _argumentTypeAsString, _argumentSize ) do { \
    printf( ARGUMENT_FORMAT, (_argumentName), (_argumentTypeAsString), (_argumentSize) );        \
} while ( 0 )

int main( int _argumentCount, char* _argumentVector[] ) {
    iterate_arguments( printArgument );
}

#undef ARGUMENT_FORMAT
#undef printArgument
```

#### Possible Output

```c
Argument name: '_argumentCount'
Argument type: 'int'
Argument size: '8'

Argument name: '_argumentVector'
Argument type: 'char**'
Argument size: '8'
```

### **Dependencies/ Requirements**

<!-- Any required headers, macros, or preconditions. -->
<!-- Is a certain feature or configuration needed? -->
```c
#include <c_extra.h>
```

### **Version/ Availability**

<!-- If you have multiple versions or evolving APIs, note when the function was added or changed. -->
Since 0.1

### See Also

<!-- References to related functions. -->
[_iterate_struct_union_](/SPEC_TEMPLATE.md)
[_iterate_enum_](/SPEC_TEMPLATE.md)

### **Notes/ Caveats**

<!-- Tricky behavior or known limitations. -->
Does not call callback with unnamed arguments (e.g. `void`)

### **Memory Management**

<!-- Who allocates/frees if pointers are involved? -->
Does not allocate on heap/ stack.
