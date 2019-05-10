// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_FILE_H
#define LLVM_CLANG_LEVITATION_FILE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include <memory>

namespace llvm {
  class raw_ostream;
  class raw_fd_ostream;
}

namespace clang {
namespace levitation {

using namespace llvm;

class File {
  public:
    enum StatusEnum {
      Good,
      HasStreamErrors,
      FiledToRename
    };

  private:
    StringRef TargetFileName;
    SmallString<128> TempPath;
    std::unique_ptr<llvm::raw_fd_ostream> OutputStream;
    StatusEnum Status;
  public:
    File(StringRef targetFileName)
      : TargetFileName(targetFileName), Status(Good) {}

    // TODO Levitation: we could introduce some generic template,
    //   something like scope_exit, but with ability to convert it to bool.
    class FileScope {
        File *F;
    public:
        FileScope(File *f) : F(f) {}
        FileScope(FileScope &&src) : F(src.F) { src.F = nullptr; }
        ~FileScope() { if (F) F->close(); }

        operator bool() const { return F; }
        llvm::raw_ostream& getOutputStream() { return *F->OutputStream; }
    };

    FileScope open() {
      // Write to a temporary file and later rename it to the actual file, to avoid
      // possible race conditions.
      TempPath = TargetFileName;
      TempPath += "-%%%%%%%%";
      int fd;

      if (llvm::sys::fs::createUniqueFile(TempPath, fd, TempPath))
        return FileScope(nullptr);

      OutputStream.reset(new llvm::raw_fd_ostream(fd, /*shouldClose=*/true));

      return FileScope(this);
    }

    void close() {
      OutputStream->close();
      if (OutputStream->has_error()) {
        OutputStream->clear_error();
        Status = HasStreamErrors;
      }

      if (llvm::sys::fs::rename(TempPath, TargetFileName)) {
        llvm::sys::fs::remove(TempPath);
        Status = FiledToRename;
      }
    }

    bool hasErrors() const {
      return Status != Good;
    }

    StatusEnum getStatus() const {
      return Status;
    }
  };
}
}

#endif