<!-- :toc: macro -->
<!-- :toc-title: -->
<!-- :toclevels: 99 -->

# c_extra Roadmap <!-- omit from toc -->

## Overview <!-- omit from toc -->

This project adds meta-programming and more preprocessor functionality to C.
The goal is to output **100% valid C** compatible with the latest Clang supported C standard with **GNU extensions enabled**.

## Goals <!-- omit from toc -->

- Produce code that compiles with supported Clang versions and GNU extensions enabled.
- Provide safe, auditable transformations that preserve semantics unless explicitly documented.
- Offer ergonomics (macros/annotations) that are easy to read and debug.
- Keep generated C readable enough for debugging and review.
- Add more preprocessor functionality.
- Add meta-programming functionality.
- Add force evaluation of functions at compile-time.

## How to read this roadmap

Each milestone contains:

- **Deliverables**: concrete outputs.
- **Acceptance criteria**: how to verify completion.

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

**Deliverables:**

- [ ] Implement iteration over `struct`/ `enum`/ `union`/ function arguments
- [ ] Implement `constinit` and limited `consteval` (no control flow)) keywords

**Acceptance criteria:**

- [ ] Each macro has unit tests (golden inputs to expected C output)
- [ ] Generated C compiles with Clang `-std=gnu23` on Linux
- [ ] Basic example project (small demo) builds successfully

### Version 0.2 - Extend Compile-time Functionality

**Deliverables:**

- [ ] Implement `constexpr` keyword
- [ ] Implement limited `iterate_annotation` (without scope support)

**Acceptance criteria:**

- [ ] Unit tests asserting correct compile-time results
- [ ] Error message style consistent and machine-parseable (exit code + stderr)
- [ ] Demo: small library using `constexpr` evaluated at build time

### Version 0.3 - Extend Preprocessor functionality

**Deliverables:**

- [ ] Implement preprocessor `for` and `repeat` directives
- [ ] Implement `__VA_ARGS_COUNT__` macro

**Acceptance criteria:**

- [ ] Feature tests with nested loops and variadic macros
- [ ] Generated C should be human-readable and maintainable

### Version 0.4 - Complete *limited* functionality

**Deliverables:**

- [ ] Complete `consteval` keyword (with `if` support)
- [ ] Complete `iterate_annotation` (with scope support)
- [ ] Implement `iterate_scope`

**Acceptance criteria:**

- [ ] Tests for branching/ loops in `consteval`
- [ ] Bench: compile-time evaluation overhead measured and documented

### Version 0.5 - User Experience

**Deliverables:**

- [ ] Complete flags accepted by tool and config file format
- [ ] Complete API documentation and a "Quick Start" guide

### Version 0.6 - Stability

**Deliverables:**

- [ ] Ensure 80% test coverage
- [ ] Implement benchmarking
- [ ] Static analysis tools (clang-tidy, scan-build), fuzzing harness for preprocessor inputs

**Acceptance criteria:**

- [ ] Coverage badge and performance baseline in repo

### Version 0.7 - Refactoring

**Deliverables:**

- [ ] Fix known bugs
- [ ] Optimize CPU, memory and disk usage
- [ ] Add code-size and runtime overhead tests for generated C

### Version 0.8 - Extended Functionality

**Deliverables:**

- [ ] Implement passing `macro to function`

**Acceptance criteria:**

- [ ] Tests and demos that exercise macro-to-function behavior

### Version 0.9 - Custom Functionality

**Deliverables:**

- [ ] Integrate with My build system

**Acceptance criteria:**

- [ ] Users can add tool to existing build pipelines with minimal changes

### Version 1.0 - Release

**Deliverables:**

- [ ] Fix known bugs
- [ ] Ensure 80% test coverage

## Contributing

If you want to help or suggest features, please see *contributing*.
