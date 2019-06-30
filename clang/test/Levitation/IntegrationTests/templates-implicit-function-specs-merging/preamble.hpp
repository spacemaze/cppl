  struct Dummy {
     template <typename T>
     inline void f() {};
  };

#ifdef COMPILE_PCH
  extern Dummy d;
#else
  Dummy d;
#endif
  
