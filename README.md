[toc]

# C++ Levitation Packages
This is an extension to C++17, and it introduces original
[modularity](https://en.wikipedia.org/wiki/Modular_programming)
support for C++.

## Basic concepts
Levitation Packages is a replacement for _C/C++_ `#include` directives.
In _C++ Levitation_ mode the latter is still supported, but only as landing pad for legacy C++ code.

## Simplest things

### Example 1

Let's consider simple example program with one package
and two classes.

_**MyPackage/A.cppl**_

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

_**MyPackage/B.cppl**_

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

_**main.cpp**_

```cpp
int main() {
  MyPackage::B::useA();
  return 0;
}
```

In this example we have introduced two classes `A` and `B`, both belong to
same package `MyPackage`, and `B` calls static method of `A`, namely
`MyPackage::A::sayHello()`. 
 
In order to tell compiler that `B` depends on `A` declaration we added
`#import` directive in top of _A.cppl_.

Compiler automatically informs _main.cpp_ about all collected packages,
so there is no need to use `#import` directive there. In our example we
just call `MyPackage::B::useA()` and then return `0`.

**Note:** `#include` directives are also supported. But whenever programmer
uses them, current package and all dependent packages will
include whatever `#include` directive refers to.

### Example 2

Below is another example which demonstrates
_automatic dependencies lookup mode_ feature (ADLM).
Namely it is not neccessary to add `#import` directives in the beginning of
source file. There is a tradeoff though.

In this case `package namespace` is considered as a very specific
template with implicitly passed parameter _'global'_.

`global` allows user to access symbols declared in another source
yet unknown to compiler.

_`automatic dependencies lookup` is enabled if and only if there are
no `#import` directives in source file._

_**MyPackage/A.cppl**_

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

_**MyPackage/B.cppl**_

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

_**main.cpp**_

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
_Related tasks: L-28_

Consider two classes `A` and `B`.

* Class `A`  somehow refers to `B`.
* While class `B` also refers to class `A`.

In general this is a circular dependency and it is not allowed. And yet
it is possible to support some special cases.

* If `A` refers to `B` only through its non-inline method bodies, then
class `B` can refer to `A` without any limitations.

* If class `B` depends on `A` but some method bodies of
`A` uses declaration of `B` then we say that `B` has _round-trip_
dependency with `A`.

User can inform about such dependency by `#import` directive with
`bodydep` attribute in dependency class.

**Note:** 'bodydep' is short from 'body dependency'

The latter tells compiler that it should import symbol only for method
bodies. It basically means that such symbol will be imported on
object file creation stage only. 

```cpp
// Related tasks: L-5
#import bodydep MyPackage::B;
package namespace MyPackage {
  class A {
  public:
    void f() {
      B b; // use of B
    }    
  };
}
``` 
**Note:** Inline methods are parts of class declaration. So it is not
allowed to refer dependent class in _inline_ method bodies. 

**Note:** There is still no need in `#import` directive if
_automatic dependencies lookup_ is used.  

### Example

_**MyPackage/A.cppl**_

```cpp
#include <iostream>
#import bodydep MyPackage::B;
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

_**MyPackage/B.cppl**_

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

_**main.cpp**_

```cpp
int main() {
  MyPackage::B::useA();
  return 0;
}
```

In this example the body of `MyPackage::A::useB` refers to class `B`.
User informs compiler that class `A` uses class `B` only in non-inline
function definitions by means of `#import` directive with `bodydep`
attribute.

Class `B` uses `A` in two places:

 1. It calls its method
`MyPackage::A::sayHello` in body of `useA`,
 2. It declares member field `MyPackage::A a`.

The former means that declaration of `A` affects declaration of `B`.
And thus the use of `B` in declaration of `A` or in inline methods of
`A` is prohibited.

Omitting `bodydep` attribute will cause compiler to exit with error,
for compiler can't use `B` in declaration of `A`. 

## Project structure, limitations
> _Good laws are limitations of our worst to release our best._

In C++ Levitation mode use of File System is restricted.

1. Directories correspond to packages. For example, all declarations of
package `com::MyOuterScope::MyPackage` should be located at path
`<project-root>/com/MyOuterScope/MyPackage`.
2. Source files should be named after contained declaration names. E.g.
if source file contains package namespace with declaration of class `A`
it should be named 'A.cppl'.
3. In each source file user can declare or define one of the next
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
	  // Related tasks: L-24
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

**Note:** inline definitions are allowed for regular structs, classes and for templates.
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
4. Macros. If programmer needs macros, then he should define them
in separate header. Limited macros support is considered in future
C++ Levitation Package versions though.

Symbols with different names can't be defined in same file.

_Related tasks: L-18, L-25, L26_

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
implicitly passed `global` template parameter.

The latter is used to access global namespace once all dependencies will
be solved and compiler will add all required dependencies to the package.

And yet user should treat `global` as template parameter and should
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
 
So far, the only way to obtain C++ Levitation Compiler is to get sources and build them.

1. `git clone <todo:url> llvm-cppl`
2. Create directory for binaries, for example 'llvm-cppl.build'
3. `cd llvm-cppl.build`
4. Run _cmake_ (assuming you want use 'llvm-cppl.instal' as directory
with installed binaries, and 'llvm-cppl' is accessable as '../llvm-cppl').

	```sh
	cmake -DLLVM_ENABLE_PROJECTS=clang \
	      -DCMAKE_INSTALL_PREFIX=llvm-cppl.install \
	      -G "Unix Makefiles" ../llvm-cppl
	```

5. `make`
6. `make check-clang`
7. `make install`
8. `alias cppl=<path-to-llvm-cppl.install>\bin\cppl`

### How to build executable
_Related tasks: L-4_

In order to build C++ Levitation code user should provide compiler
with following information:

* Project root directory (by default it is current directory). 
* Path to .cpp file with `main` function (by default it is 'main.cpp').
* Number of parallel jobs. Usually it is double of number of
available CPU cores.
* Name of output file, by default is 'a.out'.

Consider we want to compile project located at directory 'my-project'
with `main` located at 'my-project/my-project.cpp'.

Assuming we have a quad-core CPU we should run command:

`cppl -root="my-project" -main="my-project/my-project.cpp" -j8 -o app.out`

If user is not fine with long and complicated command-lines, then she could rename
'my-project.cpp' to 'main.cpp' and change directory to 'my-project'.

Then build command could be reduced to

`cppl -j8 -o app.out`

Or even to

`cppl`

In latter case compiler will use single thread compilation and saves
executable as _a.out_.

### Building library
_Related tasks: L-4, L-27_

Just like a traditional C++ compilers, `cppl` produces set of object
files.

Library creation is a bit out of compilers competence.

But, it is possible to inform compiler that we need object files
to be saved somewhere for future use.

As long as we working with non-standard C++ source code,
we also need to generate .h file with all exported declarations.

Finally, we obtain set of object files and regular C++ .h file with
exported symbols. Having this at hands it is possible to create library
with standard tools.

For example, building static library with _gcc_ tools and _Bash_ consists of
2 steps (assuming current directory is project root, and compiler uses single
thread):

1. `cppl -h=my-project.h -c=lib-objects`
2. `ar rcs my-project.a $(ls lib-objects/*.o)`

The only difference to regular C++ approach is step 1. On this step
we ask `cppl` to produce legacy object files and .h file.

* `-h=<filename>` asks compiler to generate C++ header file, and save
it with _'\<filename\>'_ name.
* `-c=<directory>` asks compiler to produce object files and store them
in directory with _'\<directory\>'_ name. It also tells compiler,
that there is no main file. Theoretically
it is still possible to declare `int main()` somewhere though.

On step 2 `ar` tool is instructed to create a static library `my-project.a`
and include into it all objects from `lib-objects` directory. 

## Theory of operation. Manual build
If only manual dependencies mode is used, then build process consists
of several steps:

0. Build preamble (optional)
1. Parsing `#import` directives.
2. Dependencies solving.
3. Parse sources and emit binaries (_.o_ and _.decl-ast files_).
4. Linkage.

With _automatic dependencies lookup_ in game though
build process consists of 5 general steps:

0. Build preamble (optional)
1. Initial parsing.
2. Dependencies solving.
3. Instantiation.
4. Code generation.
5. Linkage.

Note, that former "Parse and emit" step is expanded onto two steps, namely
"3. Instantiation" and "4. Code generation".

### Preamble
Preamble step is optional. Preamble supposed to contain code used by every
file in project. On this step precompiled header (PCH) might be produced.
For more details why it might be good to use PCH files read
[on clang site](https://clang.llvm.org/docs/PCHInternals.html#using-precompiled-headers-with-clang)
Preamble can be created with next command:

```
clang -cc1 -std=c++17 -xc++ -levitation-build-preamble \
    <path-to-preamble-source> \
    -o <path-to-precompiled-header>
```

* `-levitation-build-preamble` instructs compiler
to build preamble.
* `<path-to-preamble-source>` is path to source
file with common declarations. Usually it is .hpp file with set of
`#include` directives.
* `-o <path-to-precompiled-header>` instructs compiler to save precompiled
header in file with path '_\<path-to-precompiled-header\>_'

### Initial parsing
On this step compiler parses C++ Levitation source files and saves result
as set _.ast_ files. The latter represent binary form of abstract syntax
tree (AST).

On this stage compiler also gathers dependencies information and stores
it in binary format as set of _.ldeps_ files. _.ldeps_ files also store
ADLM flag, which tells whether _automatic dependencies lookup_ mode was
enabled for corresponding file.

Each call of `cppl` will parse single source file and may produce
one _.ldeps_ file.  

If parser meets `#import` directives then it exits straight after last
`#import` directive. The rest of code will be
parsed on later stages. In this case _.ldeps_ file will be created with
`ADLM=0`.

Files can be parsed with next command:

```
clang -cc1 -std=c++17 -xc++ -levitation-preamble=<path to precompiled preamble> \
    -levitation-build-ast \
    -levitation-sources-root-dir=<path to project root> \
    -levitation-deps-output-file=<path to output .ldeps file> \
    <path to input source file> \
    -o <path to output .ast file>
```

* `-levitation-preamble` - instructs compiler to use preamble
* `-levitation-build-ast` - informs compiler that we're going to build
initial AST files.
* `-levitation-deps-output-file` - Optional. Is used to specify output dependencies file.
* `-levitation-sources-root-dir` - Required for `-levitation-deps-output-file`.
Is used to specify project root directory.

There is a another special note about `-levitation-deps-output-file`.
For regular parse stage it is required to produce dependencies. But it is
not necessary in two cases though:

1. If user already knows dependencies. There is no need in dependencies solving.
2. In test mode. Stage are tested spearately from
each other and thus there is also no need in dependencies information. 
 
### Dependencies solving
On this stage `levitation-deps` tool is called. It collects all .ldeps files in project directory and tries to build dependencies graph.

If all dependencies are correct, then graph is acyclic (DAG), and tool
produces set of _.d_ and _.fulld_ text files.
Each _.d_ or _.fulld_ file corresponds to particular output file.

For example for _A.cppl_ compiler first produces _A.ast_ files and then
produces two output files, namely
_A.o_ and _A.decl-ast_.

`levitation-deps` is launched straight after initial parser stage and
creates _four_ files, namely

* _A.decl-ast.d_,
* _A.decl-ast.fulld_,
* _A.o.d_,
* and _A.o.fulld_.

#### _.d_ files
Are required for build system and allows to determine
instantiation and code generation order.

Files have text format.

Each file is named after corresponding
output file. Filename has format _\<output-file\>.d_.
Each string in _.d_ file contains path to one of its direct dependencies.
For example if `A.decl-ast` depends on `B.decl-ast` and `B.decl-ast` in
turn depends on `C.decl-ast`, then `A.decl-ast.d` will contain path
to `B.cppl` only. Note that in this case dependency is described by source file.

#### _.fulld_ files
Are required for _instantiation_ and _code generation_
stage itself and used to provide current source with all required
declarations it depends on.

_.fulld_ files are text files as well.

And each file also named after corresponding
output file with similar naming format: _\<output-file\>.fulld_. Difference
with _.d_ files is that _.fulld_ files contain all direct and indirect
dependencies.
For former example if `A.decl-ast` depends on `B.decl-ast`, and `B.decl-ast` in
turn depends on `C.decl-ast`, then `A.decl-ast.fulld` will contain _four_ strings:
* path to `B.ast`,
* path to `B.decl-ast`,
* path to `C.ast`
* and path to `C.decl-ast`.

Dependencies can be solved by next command:

```
levitation-deps -src-root=<path project sources root> \
    -build-root=<path to root build directory> \
    -main-file=<path to .cpp file with main function>   
```

### Instantiation

On this stage parsed Declaration AST files (with _.decl-ast_ extension)
are produced.
Compiler should be informed about all other AST files current file
depends on.

Binary AST files created on _initial parsing_ stage (_.ast_ files) are
used as input files.

For each _.ast_ file compiler goes through next steps.

1. Reads _.decl-ast_ files with declarations current AST file depends on.
2. Reads _.ast_ file itself.
3. Strips all non-inline method bodies.
4. Istantiates package namespace. Namely it replaces all _'global::'_ specifiers with
global namespace specifier ('::').
5. Saves result into corresponding _.decl-ast_ file.

Example of instantiation command:

```
clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch \
    -flevitation-build-decl \
    -levitation-dependency=<path to .ast or .decl-ast file> \
    -levitation-dependency=<another dependency> \
    ...
    -emit-pch \    
    <path to AST file to be instantiated> \
    -o <path to output .decl-ast file>
```

* `-levitation-preamble` is optional and used to specify precompiled
header.
* `-flevitation-build-decl` informs compiler that it should run
in instantiation mode.
* Set of `-levitation-dependency` parameters specify dependencies
represented by set of _.ast_ and _.decl-ast_ files.
* `-emit-pch` instructs compiler to produce binary AST file.

### Code generation
This stage is similar to instantiation with only difference, that it
doesn't strip non-inline method bodies and produces object (.o) file
rather then binary AST file.

Just like for previous stage compiler should be informed about all
other AST files current file depends on.

Binary AST files created on _initial parsing_ stage (_.ast_ files) are
used as input files.

For each _.ast_ file compiler goes through next steps.

1. Reads _.decl-ast_ files with declarations current AST file depends on.
2. Reads _.ast_ file itself.
3. If source file uses _automatic dependencies lookup_ compiler instantiates
package namespace. Namely it replaces all _'global::'_ specifiers with
global namespace specifier ('::').
4. Generates binary _.o_ file.

Example of instantiation command:

```
clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch \
    -flevitation-build-object \
    -emit-obj \
    -levitation-dependency=<path to .ast or .decl-ast file> \
    ...
    <path to input AST file> \
    -o <path to output .o file>
```

* `-levitation-preamble` is optional and used to specify precompiled
header.
* `-flevitation-build-object` informs compiler that it should run
in code generation mode. Basically it means that compiler won't strip
non-inline method bodies.
* Set of `-levitation-dependency` parameters specify dependencies
represented by set of _.decl-ast_ files.
* `-emit-obj` instructs compiler to produce binary object file.

### Parse sources and emit binaries
_(Replacement of stages "3. Instantiation" and "4. Code generation"
for files with manual dependencies declaration.)_

_Related tasks: L-28_

If it is known that source file uses manual dependencies declaration, then
initial parsing is basically skipped. For it is possible to combine parsing
and binary emission stages, and thus improve compiler performance.

Example of parse and emit command:
```
clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch \
    -flevitation-parse-and-emit \
    -levitation-dependency=<path to .ast or .decl-ast file>
    ...
    -levitation-body-dependency=<path to .ast or .decl-ast file>
    ...
    -levitation-decl-ast-output=<path to output .decl-ast file>
    -emit-pch \
    <path to source file to be compiled> \
    -o <path to output object file>
```

Set of `-levitation-body-dependency` parameters specify set of dependencies
required by non-inline method definitions only.

### Linkage

Linkage is done in traditional (legacy) way.

Having set of _.o_ files linker just combines them into output executable.

When building libraries, there is no Linkage step. One may say it is
replaced by library creation step.

### CMake sample
CMake sample files with implemented manual build process are present
in `<llvm-root>/levitation/cmake-sample` directory.

In order to use it in your C++ Levitation project, copy end edit
_CMakeLists-sample.txt_ and _build-cmake.sh_.

As a long as cmake doesn't support dynamic dependencies build requires
to run make in 3 steps:

1. Parse and solve dependencies.
2. Regenerate make files.
3. Run _instantiation_, _code generation_ and _linkage_ stages.

## Appendix 1. Syntax
### \#import directive
```
import-directive:
  '#import' ['bodydep'] path-to-symbol ';'
path-to-symbol:
  namespace-specifier '::' identifier
namespace-specifier:
  identifier
  namespace-specifier '::' namespace-specifier
```
**Note:**: `#import` directives can be present only at the top of file.

### package namespace
```
package-namespace-definition:
  'package' 'namespace' [attributes] ns_name '{'
  namespace-body '}'
ns_name:
  identifier
  ns_name '::' identifier  
```
**Note:** Only one package namespace is allowed per source file.

## Appendix 2. Implementation status
C++ Levitation Packages is still in development. Current implementation
status is shown in table below:

| Feature       | Status        | Open tasks | Description |
| ------------- |:-------------:|:----------:|:------------|
| Initial parsing stage | implemented for ADLM | L-5 | \#import directive is not supported yet. |
| Dependencies solving  | implemented | | |
| Instantiation stage | implemented | | |
| Code generation stage | implemented | | |
| Parse and build stage | not implemented | L-5, L-28 | It is possible to combine instantiation with code generation for one-way dependent sources |
| Automatic dependencies lookup mode | implemented | | |
| Manual dependencies mode | not implemented |      L-5 | \#import directive is not supported yet. |
| Build controlled by driver | implemented |      L-4 | |
| Support of libraries creation | partially implemented |      L-27 | So far, user should run through -cc1 the compilation of object files, then add them to library. User also should manually create .h file. |
