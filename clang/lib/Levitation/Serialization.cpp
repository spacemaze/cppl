//===--- C++ Levitation Serialization.cpp ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines utils and methods for C++ Levitation serialization
//  tools. It also contains several additional private classes required by
//  implementation only.
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/Serialization.h"
#include "clang/Levitation/Common/Failable.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/WithOperator.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <utility>
#include <clang/Levitation/Common/SimpleLogger.h>

using namespace llvm;

namespace clang {
namespace levitation {

  // ==========================================================================
  // Dependencies Bitstream Writer

  // TODO Levitation: may be in future introduce LevitationMetadataWriter.

  using RecordData = SmallVector<uint64_t, 64>;
  using RecordDataImpl = SmallVectorImpl<uint64_t>;

  static void EmitBlockID(unsigned ID, const char *Name,
                          llvm::BitstreamWriter &Stream,
                          RecordDataImpl &Record) {
    Record.clear();
    Record.push_back(ID);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETBID, Record);

    // Emit the block name if present.
    if (!Name || Name[0] == 0)
      return;
    Record.clear();
    while (*Name)
      Record.push_back(*Name++);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_BLOCKNAME, Record);
  }

  static void EmitRecordID(unsigned ID, const char *Name,
                           llvm::BitstreamWriter &Stream,
                           RecordDataImpl &Record) {
    Record.clear();
    Record.push_back(ID);
    while (*Name)
      Record.push_back(*Name++);
    Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETRECORDNAME, Record);
  }

  /*static*/
  class AbbrevsBuilder {
    BitstreamWriter &Writer;
    std::shared_ptr<BitCodeAbbrev> Abbrev;
  public:
    AbbrevsBuilder(unsigned RecordID, BitstreamWriter &writer) : Writer(writer) {
      Abbrev = makeAbbrev(RecordID);
    }

    template <typename FieldTy>
    AbbrevsBuilder& addFieldType() {
      llvm_unreachable("This field type is not supported.");
    }

    template <typename RecordTy>
    AbbrevsBuilder& addRecordFieldTypes() {
      llvm_unreachable("This record type is not supported.");
    }

    unsigned done() {
      return Writer.EmitAbbrev(std::move(Abbrev));
    }

    AbbrevsBuilder& addBlobType() {
      Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Blob));
      return *this;
    }

    template <typename ArrayItemTy>
    AbbrevsBuilder& addArrayType() {
      Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Array));
      addFieldType<ArrayItemTy>();
      return *this;
    }

  protected:
    static std::shared_ptr<BitCodeAbbrev> makeAbbrev(
        unsigned RecordID
    ) {
      auto Abbrev = std::make_shared<BitCodeAbbrev>();
      Abbrev->Add(BitCodeAbbrevOp(RecordID));
      return Abbrev;
    }
  };

  template <>
  AbbrevsBuilder& AbbrevsBuilder::addFieldType<uint32_t>() {
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 32));
    return *this;
  }

  template<>
  AbbrevsBuilder &
  AbbrevsBuilder::addRecordFieldTypes<DependenciesData::Declaration>() {
    using Declaration = DependenciesData::Declaration;

    addFieldType<decltype(std::declval<Declaration>().FilePathID)>();
    addFieldType<decltype(std::declval<Declaration>().LocationIDBegin)>();
    addFieldType<decltype(std::declval<Declaration>().LocationIDEnd)>();

    return *this;
  }

    class BlockScope : public WithOperand {
    BitstreamWriter &Writer;
    bool Moved;
  public:
    BlockScope(BlockScope &&src)
    : Writer(src.Writer), Moved(false) { src.Moved = true; }

    BlockScope(BitstreamWriter &W, unsigned BlockID, unsigned CodeLen)
    : Writer(W), Moved(false) {
      Writer.EnterSubblock(BlockID, CodeLen);
    }
    ~BlockScope() {
      if (!Moved)
        Writer.ExitBlock();
    }
  };

  class DependenciesBitstreamWriter : public DependenciesWriter {
  private:
    static const size_t BUFFER_DEFAULT_SIZE = 4096;

    llvm::raw_ostream &OutputStream;

    SmallVector<char, BUFFER_DEFAULT_SIZE> Buffer;
    BitstreamWriter Writer;

    bool HeaderWritten;
    bool Finalized;

  public:
    DependenciesBitstreamWriter(llvm::raw_ostream &OS)
    : OutputStream(OS),
      Writer(Buffer),
      HeaderWritten(false),
      Finalized(false)
    {}

    void writeHeader() {
      if (HeaderWritten)
        llvm_unreachable("Can't write header twice.");

      writeSignature();

      HeaderWritten = true;
    }

    void writeAndFinalize(PackageDependencies &Dependencies) override {
      if (Finalized)
        llvm_unreachable("Can't write dependencies structure twice.");

      writeHeader();

      DependenciesData Data = buildDependenciesData(Dependencies);
      write(Data);

      OutputStream << Buffer;

      Finalized = true;
    }

  private:

    BlockScope enterBlock(unsigned BlockID, unsigned CodeLen = 3) {
      return BlockScope(Writer, BlockID, CodeLen);
    }

    void writeSignature() {

      // Note: only 4 first bytes can be used for magic number.
      Writer.Emit((unsigned)'L', 8);
      Writer.Emit((unsigned)'D', 8);
      Writer.Emit((unsigned)'E', 8);
      Writer.Emit((unsigned)'P', 8);

      RecordData Record;

      Writer.EnterBlockInfoBlock();
      with (auto BlockInfoScope = make_scope_exit([&] { Writer.ExitBlock(); })) {

#define BLOCK(X) EmitBlockID(X ## _ID, #X, Writer, Record)
#define RECORD(X) EmitRecordID(X ## _ID, #X, Writer, Record)

        BLOCK(DEPS_DEPENDENCIES_MAIN_BLOCK);
        RECORD(DEPS_PACKAGE_FILE_PATH_RECORD);

        BLOCK(DEPS_STRINGS_BLOCK);
        RECORD(DEPS_STRING_RECORD);

        BLOCK(DEPS_DEFINITION_DEPENDENCIES_BLOCK);
        RECORD(DEPS_DECLARATION_RECORD);

        BLOCK(DEPS_DECLARATION_DEPENDENCIES_BLOCK);
        RECORD(DEPS_DECLARATION_RECORD);

#undef RECORD
#undef BLOCK
      }
    }

    // TODO Levitation: It should be probably moved into separate source
    //  and combined with DependenciesData class.
    //  I mean all buildXXXX methods and their helpers.
    DependenciesData buildDependenciesData(PackageDependencies &Dependencies) {
      DependenciesData Data;

      addDeclarationsData(
          *Data.Strings,
          Data.DeclarationDependencies,
          Dependencies.DeclarationDependencies
      );

      addDeclarationsData(
          *Data.Strings,
          Data.DefinitionDependencies,
          Dependencies.DefinitionDependencies
      );

      Data.PackageFilePathID =
          Data.Strings->addItem(Dependencies.PackageFilePath);
      return Data;
    }

    void addDeclarationsData(
        DependenciesStringsPool &Strings,
        DependenciesData::DeclarationsBlock &StoredDeclarations,
        const ValidatedDependenciesMap &Declarations
    ) {
      for (const auto& Declaration : Declarations) {
        StoredDeclarations.emplace_back(
            buildDeclarationData(
                Strings,
                Declaration.second.getPath(),
                Declaration.second.getComponents(),
                Declaration.second.getFirstUse().Location
            )
        );
      }
    }

    DependenciesData::Declaration buildDeclarationData(
        DependenciesStringsPool &Strings,
        StringRef FilePath,
        DependencyComponentsArrRef Components,
        SourceRange FirstUse
    ) {
      auto FilePathID = Strings.addItem(FilePath);

      return {
          FilePathID,
          FirstUse.getBegin().getRawEncoding(),
          FirstUse.getEnd().getRawEncoding()
      };
    }

    void write(const DependenciesData &Data) {

      with (auto MainBlockScope = enterBlock(DEPS_DEPENDENCIES_MAIN_BLOCK_ID)) {

        writeStrings(*Data.Strings);

        writeDependentPackageFilePath(Data.PackageFilePathID);

        writeDeclarations(
                DEPS_DECLARATION_DEPENDENCIES_BLOCK_ID,
                Data.DeclarationDependencies
        );

        writeDeclarations(
                DEPS_DEFINITION_DEPENDENCIES_BLOCK_ID,
                Data.DefinitionDependencies
        );
      }
    }

    void writeStrings(const DependenciesStringsPool &Data) {

      with (auto StringBlock = enterBlock(DEPS_STRINGS_BLOCK_ID)) {

        unsigned StringsTableAbbrev = AbbrevsBuilder(DEPS_STRING_RECORD_ID, Writer)
            .addFieldType<uint32_t>()
            .addBlobType()
        .done();

        for (const auto &StringItem : Data.items()) {

          RecordData::value_type Record[] = { DEPS_STRING_RECORD_ID, StringItem.first };

          Writer.EmitRecordWithBlob(
              StringsTableAbbrev,
              Record,
              StringItem.second
          );
        }
      }
    }

    void writeDependentPackageFilePath(StringID PathID) {
      RecordData::value_type Record[] { PathID };
      Writer.EmitRecord(DEPS_PACKAGE_FILE_PATH_RECORD_ID, Record);
    }

    void writeDeclaration(
        const DependenciesData::Declaration &Data,
        unsigned DeclAbbrev
    ) {
      RecordData Record;

      Record.push_back(Data.FilePathID);
      Record.push_back(Data.LocationIDBegin);
      Record.push_back(Data.LocationIDEnd);

      Writer.EmitRecord(DEPS_DECLARATION_RECORD_ID, Record, DeclAbbrev);
    }

    void writeDeclarations(
        DependenciesBlockIDs BlockID,
        const DependenciesData::DeclarationsBlock &Declarations
    ) {
      with (auto DeclarationsBlock = enterBlock(BlockID)) {

        unsigned DeclAbbrev =
          AbbrevsBuilder(DEPS_DECLARATION_RECORD_ID, Writer)
              .addRecordFieldTypes<DependenciesData::Declaration>()
          .done();

        for (const auto &Decl : Declarations)
          writeDeclaration(Decl, DeclAbbrev);
      }
    }
  };

  std::unique_ptr<DependenciesWriter> CreateBitstreamWriter(
      llvm::raw_ostream &OS
  ) {
    return llvm::make_unique<DependenciesBitstreamWriter>(OS);
  }

  // ==========================================================================
  // Dependencies Bitstream Reader

  class DependenciesBitstreamReader
      : public DependenciesReader,
        public Failable {
    BitstreamCursor Reader;
    StringRef ErrorMessage;
    Optional<llvm::BitstreamBlockInfo> BlockInfo;
  public:
    DependenciesBitstreamReader(const llvm::MemoryBuffer &MemoryBuffer)
    : Reader(MemoryBuffer) {}

    bool read(DependenciesData &Data) override {
      if (!readSignature())
        return false;

      if (!readData(Data))
        return false;

      return true;
    }

    const Failable &getStatus() const override { return *this; }

  protected:

    bool readSignature() {
      return Reader.canSkipToPos(4) &&
             Reader.Read(8) == 'L' &&
             Reader.Read(8) == 'D' &&
             Reader.Read(8) == 'E' &&
             Reader.Read(8) == 'P';
    }

    bool readData(DependenciesData &Data) {
      if (!readBlockInfo())
        return false;

      if (enterBlock(DEPS_DEPENDENCIES_MAIN_BLOCK_ID)) {
        bool EndOfBlock = false;
        while (!EndOfBlock) {
          auto Entry = Reader.advance();
          switch (Entry.Kind) {
            case BitstreamEntry::Error:
              setFailure("Failed to enter read bitstream.");
              return false;

            case BitstreamEntry::EndBlock:
              Reader.ReadBlockEnd();
              EndOfBlock = true;
              break;
            case BitstreamEntry::SubBlock: {

                auto BlockID = (DependenciesBlockIDs)Entry.ID;

                switch (BlockID) {
                  case DEPS_STRINGS_BLOCK_ID:
                    if (!readStringsTable(Data))
                      return false;
                    break;

                  case DEPS_DEFINITION_DEPENDENCIES_BLOCK_ID:
                    if (!readDependencies(
                         BlockID,
                         *Data.Strings,
                         Data.DefinitionDependencies
                    ))
                      return false;
                    break;

                  case DEPS_DECLARATION_DEPENDENCIES_BLOCK_ID:
                    if (!readDependencies(
                        BlockID,
                        *Data.Strings,
                        Data.DeclarationDependencies
                    ))
                      return false;
                    break;

                  case DEPS_DEPENDENCIES_MAIN_BLOCK_ID:
                    llvm_unreachable("Recursive main block.");
                }
              }
              break;
            case BitstreamEntry::Record:
              if (!readPackageFilePath(Entry.ID, Data))
                return false;
              break;
          }
        }

        return true;
      }

      return false;
    }

    bool readBlockInfo() {

      // Read the top level blocks.
      if (Reader.AtEndOfStream()) {
        setFailure("No blocks.");
        return false;
      }

      while (!Reader.AtEndOfStream()) {

        if (Reader.ReadCode() != llvm::bitc::ENTER_SUBBLOCK) {
          setFailure("Expected BlockInfo Subblock, malformed file.");
          return false;
        }


        std::error_code EC;
        switch (Reader.ReadSubBlockID()) {
          case llvm::bitc::BLOCKINFO_BLOCK_ID:
            if ((BlockInfo = Reader.ReadBlockInfoBlock())) {
              Reader.setBlockInfo(BlockInfo.getPointer());
              return true;
            } else {
              setFailure("Malformed BlockInfo.");
              return false;
            }
          case DEPS_DEPENDENCIES_MAIN_BLOCK_ID:
            setFailure("Blockinfo missed");
            return false;
          default:
            break;
            // Skip all unknown blocks.
        }
      }

      setFailure("BlockInfo missed");
      return false;
    }

    using RecordTy = SmallVector<uint64_t, 64>;

    bool readStringsTable(DependenciesData &Data) {

      auto recordHandler = [&](const RecordTy &Record, StringRef BlobStr) {
        auto SID = (StringID) Record[0];
        Data.Strings->addItem(SID, BlobStr);
      };

      return readAllRecords(
          DEPS_STRINGS_BLOCK_ID,
          DEPS_STRING_RECORD_ID,
          /*WithBlob=*/true,
          std::move(recordHandler)
      );
    }

    StringID normalizeIfNeeded(
        DependenciesStringsPool &Strings, StringID PathID
    ) {
      const auto &PathStr = *Strings.getItem(PathID);
      auto Res = levitation::Path::normalize<SmallString<256>>(PathStr);

      if (Res.compare(PathStr) != 0) {
        setWarning()
        << "Path '" << PathStr << "' was not normalized.\n"
        << "'" << Res << "' will be used instead\n";

        return Strings.addItem(Res);
      }

      return PathID;
    }

    bool readDependencies(
        DependenciesBlockIDs BlockID,
        DependenciesStringsPool &Strings,
        DependenciesData::DeclarationsBlock &Deps
    ) {
      auto recordHandler = [&](const RecordTy &Record, StringRef BlobStr) {
        StringID PathID = normalizeIfNeeded(
            Strings, (StringID)Record[0]
        );
        Deps.emplace_back(DependenciesData::Declaration {
            /*FilePathID=*/ PathID,
            /*LocationIDBegin=*/ (DependenciesData::LocationIDType) Record[1],
            /*LocationIDEnd=*/ (DependenciesData::LocationIDType) Record[2]
        });
      };

      return readAllRecords(
          BlockID,
          DEPS_DECLARATION_RECORD_ID,
          /*WithBlob*/false,
          recordHandler
      );
    }

    bool readPackageFilePath(unsigned int AbbrevID, DependenciesData &Data) {
      RecordTy Record;
      auto RecordID = Reader.readRecord(AbbrevID, Record, nullptr);
      Data.PackageFilePathID = normalizeIfNeeded(
          *Data.Strings, (StringID)Record[0]
      );

      return checkRecordType(DEPS_PACKAGE_FILE_PATH_RECORD_ID, RecordID);
    }

    using OnReadRecordFn = std::function<void(const RecordTy&, StringRef)>;

    bool readAllRecords(
        DependenciesBlockIDs BlockID,
        DependenciesRecordTypes RecordID,
        bool WithBlob,
        OnReadRecordFn &&OnReadRecord) {
      RecordTy RecordStorage;
      StringRef BlobRef;

      if (!Reader.EnterSubBlock(BlockID)) {

        while (readRecord(
            RecordID, RecordStorage, BlobRef, WithBlob, OnReadRecord
        )) {
          // do nothing
        }
        return isValid();
      }
      setFailure("Failed to read records, can't enter subblock");
      return false;
    }

    /// Reads record
    /// \param RecordStorage heap for reacord values
    /// \param BlobRef blob variable
    /// \param RecordID record ID, is used to pick up prober abbrev.
    /// \param WithBlob true if record has blob data.
    /// \param OnReadRecord record handler.
    /// \return true, if we can read more,
    /// false if there was an error, or end of block.
    bool readRecord(
        DependenciesRecordTypes RecordID,
        RecordTy &RecordStorage,
        StringRef &BlobRef,
        bool WithBlob,
        const OnReadRecordFn &OnReadRecord
    ) {
      auto Entry = Reader.advanceSkippingSubblocks();
      switch (Entry.Kind) {
        case BitstreamEntry::Error:
          setFailure("Failed to read strings");
          return false;
        case BitstreamEntry::Record: {
            RecordStorage.clear();

            StringRef *BlobPtr = WithBlob ? &BlobRef : nullptr;

            unsigned int ObtainedRecordID =
              Reader.readRecord(Entry.ID, RecordStorage, BlobPtr);

            if (!checkRecordType(RecordID, ObtainedRecordID))
              return false;

            OnReadRecord(RecordStorage, BlobRef);
          }
          return true;
        case BitstreamEntry::EndBlock:
          return false;
        default:
          return true;
      }
    }

    bool checkRecordType(
        DependenciesRecordTypes RecordID,
        unsigned int ObtainedID
    ) {
      if (ObtainedID != RecordID) {
        setFailure("Malformed file. Wrong record type read.");
        return false;
      }
      return true;
    }

    bool enterBlock(DependenciesBlockIDs ID) {
      if (!isValid()) return false;

      while (true) {
        auto Entry = Reader.advance();

        switch (Entry.Kind) {
          case BitstreamEntry::Error:
            setFailure("Failed to enter read bitstream.");
            return false;

          case BitstreamEntry::EndBlock:
            break;
          case BitstreamEntry::SubBlock:
            if (Entry.ID != ID) {
              if (Reader.SkipBlock()) {
                setFailure("Failed to skip block.");
                return false;
              }
            } else {
              if (!Reader.EnterSubBlock(Entry.ID))
                return true;
              setFailure("Failed to enter subblock.");
              return false;
            }
            break;
          case BitstreamEntry::Record:
            break;
        }
      }
    }
  };

  std::unique_ptr<DependenciesReader> CreateBitstreamReader(
      const llvm::MemoryBuffer &MB
  ) {
    return llvm::make_unique<DependenciesBitstreamReader>(MB);
  }
}
}

