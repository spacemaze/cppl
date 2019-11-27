#ifndef INPUTS_HEADER_H
#define INPUTS_HEADER_H

class V {
public:
  static int get() { return 123; }
};

#define DEF_CLASS_C(A) class C { \
public: \
  static int get() { return A; } \
};

DEF_CLASS_C(777);

#endif // INPUTS_HEADER_H