// RUN: %clang_cc1 -std=c++17 -flevitation-mode -fsyntax-only -verify %s

// Example of correct construction
package namespace A::B {}

package inline namespace A {} // expected-error {{package namespace cannot be inline}}
inline package namespace A {} // expected-error {{package namespace cannot be inline}}
package namespace A::inline B {} // expected-error {{package namespace cannot be inline}}
package namespace A::package B {} // expected-error {{'package' specifier must be prior to 'namespace' keyword}}

// We could put more verbal diagnostics, saying something about inappropriate use of
// 'package', but that requires to modify legacy ParseNamespace method.
// So far, we expect very common  expected '{' + "expected unqualified-id"
namespace A::package B {} // expected-error {{expected '{'}} expected-error {{expected unqualified-id}}

// Prohibit use of 'package' for aliases
package namespace A = B; // expected-error {{expected '{'}}
