<!-- :toc: macro -->
<!-- :toc-title: -->
<!-- :toclevels: 99 -->

# c_extra Roadmap <!-- omit from toc -->

## Overview <!-- omit from toc -->

This project adds meta-programming and more preprocessor functionality to C.
The goal is to output **100% valid C** compatible with the latest Clang supported C standard with **GNU extensions enabled**.

## Goals <!-- omit from toc -->

- Add more preprocessor functionality.
- Add meta-programming functionality.
- Add force evaluation of functions at compile-time.

## Table of Contents <!-- omit from toc -->

* [Milestones](#milestones)
    * [Version 0.1](#version-01-core-functionality-in-progress)
    * [Version 0.2](#version-02-extend-compile-time-functionality)
    * [Version 0.3](#version-03-extend-preprocessor-functionality)
    * [Version 0.4](#version-04-complete-limited-functionality)
    * [Version 0.5](#version-05-user-experience)
    * [Version 0.6](#version-06-stability)
    * [Version 0.7](#version-07-refactoring)
    * [Version 0.8](#version-08-extended-functionality)
    * [Version 0.9](#version-09-custom-functionality)
    * [Version 1.8](#version-10-release)
* [Contributing](#contributing)

## Milestones

### Version 0.1 - Core Functionality \[ _in progress_ ]

- [ ] Implement iteration over `struct`/ `enum`/ `union`/ function arguments
- [ ] Implement `constinit` and limited `consteval` (without `if` support) keywords

### Version 0.2 - Extend Compile-time Functionality

- [ ] Implement `constexpr` keywords
- [ ] Implement limited `iterate_annotation` (without scope support)

### Version 0.3 - Extend Preprocessor functionality

- [ ] Implement preprocessor `for` and `repeat` directives
- [ ] Implement `__VA_ARGS_COUNT__` macro

### Version 0.4 - Complete *limited* functionality

- [ ] Complete `consteval` keyword (with `if` support)
- [ ] Complete `iterate_annotation` (with scope support)
- [ ] Implement `iterate_scope`

### Version 0.5 - User Experience

- [ ] Complete flags accepted by tool
- [ ] Complete API documentation

### Version 0.6 - Stability

- [ ] Ensure 80% test coverage
- [ ] Implement benchmarking

### Version 0.7 - Refactoring

- [ ] Fix known bugs
- [ ] Optimize CPU, memory and disk usage

### Version 0.8 - Extended Functionality

- [ ] Implement passing `macro to function`

### Version 0.9 - Custom Functionality

- [ ] Integrate with My build system

### Version 1.0 - Release

- [ ] Fix known bugs
- [ ] Ensure 80% test coverage

## Contributing

<!-- TODO: Add contributing -->
If you want to help or suggest features, please see [contributing](CONTRIBUTING).
