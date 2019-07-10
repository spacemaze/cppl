# C++ Levitation Packages
This is an extension to C++17, and it introduces original
[modularity](https://en.wikipedia.org/wiki/Modular_programming)
support for C++.

## Basic concepts
Levitation Packages is a replacement for _C/C++_ `#include` directives.
In _C++ Levitation_ mode the latter is still supported, and user can
include some files.

Whenever user does it, current package and all dependent packages will
include whatever `#include` directive refers to.

## Simplest things

### Example 1

Let's consider simple example program with one package
and two classes.

_File `MyPackage/A.cppl`_
```cpp
#include <iostream>
package namespace MyPackage {
  class A {
  public:
    static void sayHello() {
      std::cout << "Hello!\n";
    }
  };
}
```
_File `MyPackage/B.cppl`_
```cpp
#import MyPackage::A
package namespace MyPackage {
  class B {
  public:
    static void useA() {
      A::sayHello();
    }
  };
}
```
_File `main.cpp`_
```cpp
int main() {
  MyPackage::B::useA();
  return 0;
}
```
In example we have introduced two classes `A` and `B`, both belong to
same package `MyPackage`, and `B` calls static method of `A`, namely
`MyPackage::A::sayHello()`. 
 
In order to tell compiler that `B` depends on `A` declaration we added
\#import directive in top of `FileA.cppl`.

Compiler automatically informs `main.cpp` about all collected packages,
so there is no need to use \#import directive there. In our example we
just call `MyPackage::B::useA()` and then return `0`.

### Example 2

Below is another example which demonstrates C++ Levitation Packages
_automatic dependencies lookup_ feature.
Namely there is no need to add `#import` directives in the beginning of
source file. There is a tradeoff though.

In this case `package namespace` is considered as a very specific
template with implicitly passed parameter `global`.
`global` allows user to access symbols declared in another sources.

_`automatic dependencies lookup` is enabled if and only if there are
no `#import` directives in source file._

_File `MyPackage/A.cppl`_
```cpp
\#include <iostream>
package namespace MyPackage {
  class A {
  public:
    static void sayHello() {
      std::cout << "Hello!\n";
    }
  };
}
```
_File `MyPackage/B.cppl`_
```cpp
package namespace MyPackage {
  class B {
  public:
    void useA() {
      global::MyPackage::A::sayHello();
    }
  };
}
```
_File `main.cpp`_
```cpp
int main() {
  MyPackage::B::useA();
  return 0;
}
```
Note that there is no need in `global` in `main.cpp`.

## _Inline_ methods
In C++ Levitation all methods are considered as non-inline and
externally visible, unless `inline` is not specified.
```cpp
package namespace MyPackage {
  class A {
  public:
    // Method below is static and can be refered by its declaration
    // only in other object files.
    static void availableExternally() {
      // ...
    }

    // Method below is non-static and can be refered by its declaration
    // only in other object files.
    void availableExternallyToo() {
      // ...
    }
    
    // Method below inline and static.
    static inline void inlineStaticMethod() {
      // ...
    }
    
    // Method below is inline.
    void inlineMethod() {
      // ...
    }    
  };
}
```

## _Round-trip_ dependencies
Consider two classes `A` and `B`.
* Class `A`  somehow refers to `B`.
* While class `B` also refers to class `A`.

In general this is a circular dependency and it is not allowed. And yet
it is possible to support some special cases.

_If `A` refers to `B` only through its non-inline method bodies, then
class `B` can refer to `A` without any limitations._

If class `B` depends on `A` but some method bodies of
`A` uses declaration of `B` then we say that `B` has _round-trip_
dependency with `A`.

User can inform about such dependency by `#import` directive with
`for_bodies` attribute in dependency class.

The latter tells compiler that it should import symbol only for method
bodies. It basically means that such symbol will be imported on
object file creation stage only. 

```cpp
// Related tasks: L-5
#import for_bodies MyPackage::B;
package namespace MyPackage {
  class A {
  public:
    void f() {
      B b; // use of B
    }    
  };
}
``` 
*Note:* Inline methods are parts of class declaration. So it is not
allowed to refer dependent class in _inline_ method bodies. 

*Note:* There is still no need in `#import` directive if
_automatic dependencies lookup_ is used.  

### Example

_File `MyPackage/A.cppl`_
```cpp
#include <iostream>
#import for_bodies MyPackage::B;
package namespace MyPackage {
  class A {
  public:
    static void useB() {
      MyPackage::B::useA();
    }
    static void sayHello() {
      std::cout << "Hello!\n";
    }
  }
}
```
_File `MyPackage/B.cppl`_
```cpp
#import MyPackage::A;
package namespace MyPackage {
  class B {
  public:
    static void useA() {
      A::sayHello();
    }
  protected:
    A a; // another use of `A`
  }
}
```
_File `main.cpp`_
```cpp
int main() {
  MyPackage::B::useA();
  return 0;
}
```

In this example the body of `MyPackage::A::useB` refers to class `B`.
User informs compiler that class `A` uses class `B` only in non-inline
function definitions by means of `#import` directive with `for_bodies`
attribute.

Class `B` uses `A` in two places
 1. It calls its method
`MyPackage::A::sayHello` in body of `useA`,
 2. It declares member field `MyPackage::A a`.

The former means that declaration of `A` affects declaration of `B`.
And thus the use of `B` in declaration of `A` or in inline methods of
`A` is prohibited.

Omitting `for_bodies` attribute will cause compiler to exit with error,
for compiler can't use `B` in declaration of `A`. 

## Project structure, limitations
In C++ Levitation mode use of File System is restricted.
1. Directories correspond to packages. For example, all declarations of
package `com::MyOuterScope::MyPackage` should be located at path
`<project-root>/MyOuterScope/MyPackage`.
2. In each package file user can declare or define one of the next
set of symbols:
* Class or structure and its out-of-scope member definitions.
```cpp
package namespace P {
  struct A {
    void f();
  };
  void A::f() {
  // do something
  }
}
```
* Template with its specializations and out-of-scope member definitions.
```cpp
package namespace P {
  template<typename T, typename N>
  struct A {
    void f();
  };

  // Partial template specialization  
  template<typename T>
  struct A<T, int> {
    void g();
  };

  // Full template specialization
  // Related bugs: L-24
  template<>
  struct A<int, int> {
    void h();
  };
  
  // Definition of method 'f' of generic template A<T, N> 
  template <typename T, typename N>
  void A<T, N>::f() {
    // do something
  }
  
  // Definition of method 'g' of partially specialized template A<T, int>
  template <typename T>
  void A<T, int>::f() {
    // do something
  }
    
  // Definition of method 'h' of fully specialized template A<int, int>
  void A<int, int>::h() {
    // do something
  }
}
```

* Templates with parameter pack arguments
```cpp
package namespace P {
  template<typename ...T>
  struct A {
    static void f();
  };

  template <typename ...T>
  void A<T...>::f() {
    levitation::Test::context() << "P1::A::f()";
  }
}
```

* Simple enums.

```cpp
package namespace P {
  enum A {
    Value0, Value1
    //, ...
  };
}
```

* Scoped enums
```cpp
package namespace P {
  enum class A {
    Value0, Value1
    //, ...
  };
}
```

_*Note:*_ inline definitions are allowed for both regular
structs and classes and for templates.
But only those declared with `inline` specifier will be considered
as inline methods.

```cpp
package namespace P {
  template<typename ...T>
  struct A {
    static void outOfLineF();
    
    void anotherF() {
      // This method has inline definition, but its symbol
      // will be exported by compiler.
      
      // do something
    }
    void inline inlineF() {
      // The only way to define inline method in C++ Levitation.
      
      // do something 
    }
  };

  template <typename ...T>
  void A<T...>::outOfLineF() {
    // do something.
  }
}
```

Following types of symbols are _*not*_ supported:
1. Top-level functions.
2. Top-level variables.
3. Top-level constants.
4. Macros. If programmer really need macros, then he should define them
in separate header. Limited macros support is considered in future
C++ Levitation Package versions though.

Symbols with different names can't be defined in same file.
Related tasks: L-18, L-25, L26

```cpp
namespace P {
  class B; // OK. Predeclaration can be added to non-package namespace.
}

package namespace P {
  class A {
  };
  class B; // Error. Only 'A' declarations are allowed in package namespace.
  class B { // Error. Only 'A' definitions are allowed in package namespace.
  };
}
```

## 'global' keyword
As it was mentioned before, in _automatic dependencies lookup_
mode package namespaces are considered as very specific templates with
implicitly passed 'global' template parameter.

The latter is used to access global namespace once all dependencies will
be solved and compiler will add all required dependencies to the package.

And yet user should treat 'global' as template parameter and should
provide compiler with all required
[typename](https://en.cppreference.com/w/cpp/language/dependent_name#The_typename_disambiguator_for_dependent_names)
and
[template](https://en.cppreference.com/w/cpp/language/dependent_name#The_template_disambiguator_for_dependent_names)
disambiguators.

For example:

```cpp
package namespace MyPackage {
  class A {
    typename global::MyPackage::B b;
  public:
    // ...
  };
}
```
This is a sad thing of course, and we hope to get rid of some of
disambiguators in future C++ Levitation versions.

## Getting started
### Getting C++ Levitation Compiler
C++ Levitation Compiler implementation is based on LLVM Clang frontend.
 
So far, the only way to obtain it is to get sources and build them.
1. git clone <url> `llvm-cppl`
2. Create directory for binaries, for example `llvm-cppl.build`
3. `cd llvm-cppl`
4. Run cmake, assuming you want use `llvm-cppl.install` as directory
with installed binaries.
```sh
cmake -DLLVM_ENABLE_PROJECTS=clang \
      -DCMAKE_INSTALL_PREFIX=llvm-cppl.install \
      -G "Unix Makefiles" ../llvm-cppl`
```
5. `make`
6. `make check-clang`
7. `make install`
8. `alias c++l=<path-to-llvm-cppl.install>\bin\c++l`

### How to build code
_Related tasks: L-4, L-27_

In order to build C++ Levitation code user should provide compiler
with following information:
* Project root directory (by default it is current directory). 
* Path to .cpp file with `main` function (by default it is `main.cpp`).
* Number of parallel jobs. Usually it is double of number of
available CPU cores.
* Name of output file, by default it is `a.out`.

Consider we want to compile project located at directory `my-project`
with `main` located at `my-project/my-project.cpp`.
Assuming we have quad-core CPU we can compile it with following command:

`c++l -cppl-root="my-project" -cppl-main="my-project/my-project.cpp" -cppl-j=8 -o app.out`

If user is not fine with long complicated command-lines he could rename
`my-project.cpp` to `main.cpp` and change directory to `my-project`.

Then build command could be reduced to

`c++l -cppl-j=8 -o app.out`

Or even to

`c++l`

In this case project will compiled in single thread. And output will be
saved as `a.out`.

## Appendix 1. Syntax
### \#import directive
```
import-directive:
  '#import' ['for_bodies'] path-to-symbol ';'
path-to-symbol:
  namespace-specifier '::' identifier
namespace-specifier:
  identifier
  namespace-specifier '::' namespace-specifier
```
_*Note:*_: `#import` directives can be present only at the top of file.

### package namespace
```
package-namespace-definition:
  'package' 'namespace' [attributes] ns_name '{'
  namespace-body '}'
ns_name:
  identifier
  ns_name '::' identifier  
```
_*Note:*_: Only one package namespace is allowed per source file.

## Appendix 2. Implementation status
C++ Levitation Packages is still in development. Current implementation
status is shown in table below:

| Feature       | Status        | Open tasks | Description |
| ------------- |:-------------:|:----------:|------------:|
| Automatic dependencies lookup mode | implemented |      - | This mode is fully supported |
| Manual dependencies mode | not implemented |      L-5 | \#import directive is not implemented |
| Build controlled by driver | not implemented |      L-4 | Highest priority task. Is to be implemented in first place. |
| Support of libraries creation | partially implemented |      L-27 | So far, user should run through -cc1 the compilation of object files, then add them to library. User also should manually create .h file. |

## Appendix 3. Roadmap
