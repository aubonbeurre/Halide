#ifndef HALIDE_INTROSPECTION_H
#define HALIDE_INTROSPECTION_H

#include <string>
#include <iostream>
#include <stdint.h>

#include "Util.h"

/** \file
 *
 * Defines methods for introspecting in C++. Relies on DWARF debugging
 * metadata, so the compilation unit that uses this must be compiled
 * with -g.
 */

namespace Halide {
namespace Internal {

namespace Introspection {
/** Get the name of a stack variable from its address. The stack
 * variable must be in a compilation unit compiled with -g to
 * work. The expected type helps distinguish between variables at the
 * same address, e.g a class instance vs its first member. */
EXPORT std::string get_variable_name(const void *, const std::string &expected_type);

/** Register an untyped heap object. Derive type information from an
 * introspectable pointer to a pointer to a global object of the same
 * type. Not thread-safe. */
EXPORT void register_heap_object(const void *obj, size_t size, const void *helper);

/** Deregister a heap object. Not thread-safe. */
EXPORT void deregister_heap_object(const void *obj, size_t size);

/** Return the address of a global with type T *. Call this to
 * generate something to pass as the last argument to
 * register_heap_object.
 */
template<typename T>
const void *get_introspection_helper() {
    static T *introspection_helper = nullptr;
    return &introspection_helper;
}

/** Get the source location in the call stack, skipping over calls in
 * the Halide namespace. */
EXPORT std::string get_source_location();

// This gets called automatically by anyone who includes Halide.h by
// the code below. It tests if this functionality works for the given
// compilation unit, and disables it if not.
EXPORT void test_compilation_unit(bool (*test)(), void (*calib)());
}

}
}


// This code verifies that introspection is working before relying on
// it. The definitions must appear in Halide.h, but they should not
// appear in libHalide itself. They're defined as static so that clients
// can include Halide.h multiple times without link errors.
#ifndef COMPILING_HALIDE

namespace Halide {
namespace Internal {
static bool check_introspection(const void *var, const std::string &type,
                                const std::string &correct_name,
                                const std::string &correct_file, int line) {
    std::string correct_loc = correct_file + ":" + std::to_string(line);
    std::string loc = Introspection::get_source_location();
    std::string name = Introspection::get_variable_name(var, type);
    return name == correct_name && loc == correct_loc;
}
}
}

namespace HalideIntrospectionCanary {

// A function that acts as a signpost. By taking it's address and
// comparing it to the program counter listed in the debugging info,
// we can calibrate for any offset between the debugging info and the
// actual memory layout where the code was loaded.
static void offset_marker() {
    std::cerr << "You should not have called this function\n";
}

struct A {
    int an_int;

    class B {
        int private_member;
    public:
        float a_float;
        A *parent;
        B() : private_member(17) {
            a_float = private_member * 2.0f;
        }
    };

    B a_b;

    A() {
        a_b.parent = this;
    }

    bool test(const std::string &my_name);
};

static bool test_a(const A &a, const std::string &my_name) {
    bool success = true;
    success &= Halide::Internal::check_introspection(&a.an_int, "int", my_name + ".an_int", __FILE__ , __LINE__);
    success &= Halide::Internal::check_introspection(&a.a_b, "HalideIntrospectionCanary::A::B", my_name + ".a_b", __FILE__ , __LINE__);
    success &= Halide::Internal::check_introspection(&a.a_b.parent, "HalideIntrospectionCanary::A *", my_name + ".a_b.parent", __FILE__ , __LINE__);
    success &= Halide::Internal::check_introspection(&a.a_b.a_float, "float", my_name + ".a_b.a_float", __FILE__ , __LINE__);
    success &= Halide::Internal::check_introspection(a.a_b.parent, "HalideIntrospectionCanary::A", my_name, __FILE__ , __LINE__);
    return success;
}

static bool test() {
    A a1, a2;

    return test_a(a1, "a1") && test_a(a2, "a2");
}

// Run the tests, and calibrate for the PC offset at static initialization time.
namespace {
struct TestCompilationUnit {
    TestCompilationUnit() {
        Halide::Internal::Introspection::test_compilation_unit(&test, &offset_marker);
    }
};
}

static TestCompilationUnit test_object;

}

#endif

#endif
