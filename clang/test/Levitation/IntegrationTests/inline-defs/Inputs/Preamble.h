
// CHECK-NOT: int a = 123123
#include "IncludedPart.h"

// CHECK-NOT: int a = 1
// CHECK: int b = 55;
// CHECK: int keepThisInit = 777;
// CHECK: int b = 555;
// CHECK: staticField = 9191;
// CHECK-NOT: int c = 11;
// CHECK-NOT: int Inputs::NonTemplate::staticField = 8888;
// CHECK: static int keepThisInitToo = 7777;
// CHECK-NOT: andThisInitYouSkip = 9090;
// CHECK: 12323

namespace Inputs {

void thisSkip() { int a = 1; }

class NonTemplate {
  void implicitInline() { int b = 55; }

  void outOfLine();

  int keepThisInit = 777;
  static int staticField;
};

template <typename T>
class TemplateClass {
  static int staticField;
  void implicitInline() { int b = 555; }
};

template <typename T>
int TemplateClass<T>::staticField = 9191;

void NonTemplate::outOfLine() { int c = 11; }
int NonTemplate::staticField = 8888;

static int keepThisInitToo = 7777;
int andThisInitYouSkip = 9090;

template <typename T>
T keepThisTemplateVarInit = T(12323);

}