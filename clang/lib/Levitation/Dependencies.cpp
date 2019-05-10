// TODO Levitation: Licensing
//===--------------------------------------------------------------------===//
// Dependency handling
//
// Group of methods below implements
// package dependencies detection and handling.
//

#include "clang/Levitation/Dependencies.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace levitation {

void DepenciesMap::mergeDependency(PackageDependency &&Dep) {
  auto Res = try_emplace(Dep.getComponents(), std::move(Dep));
  if (!Res.second)
    // Note: we use Dep after std::move(Dep) only in case it wasn't really moved
    // by try_emplace
    Res.first->second.addUse(Dep.getSingleUse());
}

void PackageDependency::print(llvm::raw_ostream &out) const {
  size_t numComponents = Components.size();

  if (!numComponents)
    return;

  if (numComponents == 1) {
    out << Components[0];
    return;
  }

  StringRef Separator = "::";

  for (size_t i = 0, e = numComponents-1; i != e; ++i) {
    out << Components[i] << Separator;
  }
  out << Components[numComponents-1];
}

const DiagnosticBuilder &operator<<(
    const DiagnosticBuilder &DB,
    const PackageDependency &V
) {
  std::string str;
  llvm::raw_string_ostream OS(str);
  V.print(OS);
  OS.flush();
  DB.AddString(str);
  return DB;
}

}
}

