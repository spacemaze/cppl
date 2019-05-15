// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_FRONTENDACTIONEXTS_H
#define LLVM_CLANG_LEVITATION_FRONTENDACTIONEXTS_H

#include <string>
#include <vector>

namespace clang {
  class CompilerInstance;
  class FrontendAction;
  class FrontendInputFile;
namespace levitation {

/*static*/
class FrontendActionExts {
public:
  static bool createDepsSource(
      CompilerInstance &CI
  );
  static bool loadFromAST(
      CompilerInstance &CI,
      FrontendAction &FA,
      const FrontendInputFile &Input
  );
};

}
}

#endif //LLVM_CLANG_LEVITATION_FRONTENDACTIONEXTS_H
