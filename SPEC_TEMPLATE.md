<!-- The full declaration, including return type, name, and parameter list -->
```cpp
FORCE_INLINE void iterate_struct( struct T* _iterable, auto _callback )
```

##### Calls `_callback` with every field from `_iterable`

### **Parameters**

```cpp
- _iterable (struct T*): Pointer to variable of type struct T to iterate over.
- _callback (auto): Callback to call with each field.
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
Depends on callback.

### **Error Handling**

<!-- How the function handles errors. -->
<!-- Any `errno` values set. -->
<!-- Return value conventions (e.g., negative on error). -->
Error-exempt.

### **Examples/ Usage**

```c
typedef struct {
    uint8_t* data;
    size_t size;
} asset_t;

asset_t l_asset;

#define ASSET_FORMAT \
    "Field name: '%s'\n" \
    "Field type: '%s'\n" \
    "Field offset: '%zu'\n" \
    "Field size: '%zu'\n\n"

#define printAsset( _fieldName, _fieldTypeAsString, _fieldReference, _fieldOffset, _fieldSize ) do {
    printf( ASSET_FORMAT, (_fieldName), (_fieldTypeAsString), (_fieldOffset), (_fieldSize) );
} while ( 0 )

iterate_struct( &l_asset, printAsset );

#undef ASSET_FORMAT
#undef printAsset
```

#### Possible Output

```c
Field name: 'data'
Field type: 'uint8_t*'
Field offset: '123'
Field size: '321'

Field name: 'size'
Field type: 'size_t'
Field offset: '456'
Field size: '654'
```

### **Dependencies/ Requirements**

<!-- Any required headers, macros, or preconditions. -->
<!-- Is a certain feature or configuration needed? -->
```c
#include <c_extra.h>
```

### **Version/ Availability**

<!-- If you have multiple versions or evolving APIs, note when the function was added or changed. -->
Since 1.0

### See Also

<!-- References to related functions. -->
<!-- TODO: Write -->

### **Notes/ Caveats**

<!-- Tricky behavior or known limitations. -->
<!-- TODO: Write -->

### **Memory Management**

<!-- Who allocates/frees if pointers are involved? -->
Does not allocate on heap/ stack.
