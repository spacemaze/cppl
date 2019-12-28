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
#include "llvm/Bitstream/BitstreamReader.h"
#include "llvm/Bitstream/BitstreamWriter.h"
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

  class RecordWriter {
    BitstreamWriter &Writer;
    unsigned int RecordID;
    unsigned int AbbrevID;
    RecordData Record;
  public:
    RecordWriter(
        BitstreamWriter &writer,
        unsigned int recordID,
        unsigned int abbrevID
    ) : Writer(writer),
        RecordID(recordID),
        AbbrevID(abbrevID)
    {}

    template<typename T>
    RecordWriter& emitField(T v) {
      Record.push_back(v);
      return *this;
    }

    RecordWriter& emitField(size_t v) {
      Record.push_back(v & ((1L << 32) - 1L));
      Record.push_back(v >> 32);
      return *this;
    }

    void done() {
      Writer.EmitRecord(RecordID, Record, AbbrevID);
    }
  };

  template <typename RecordTy>
  class RecordReader {
    const RecordTy &Record;
    size_t CurIdx = 0;
  public:
    RecordReader(const RecordTy &Record) : Record(Record) {}

    template<typename T>
    RecordReader &read(T& dest) {
      dest = Record[CurIdx++];
      return *this;
    }

    RecordReader &read(size_t &dest) {
      uint32_t L = Record[CurIdx++];
      uint32_t H = Record[CurIdx++];
      dest = L | ((size_t)H << 32);
      return *this;
    }

    void done() const {}
  };

  template <>
  AbbrevsBuilder& AbbrevsBuilder::addFieldType<bool>() {
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 1));
    return *this;
  }

  template <>
  AbbrevsBuilder& AbbrevsBuilder::addFieldType<uint8_t>() {
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 8));
    return *this;
  }

  template <>
  AbbrevsBuilder& AbbrevsBuilder::addFieldType<uint32_t>() {
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 32));
    return *this;
  }

  template <>
  AbbrevsBuilder& AbbrevsBuilder::addFieldType<size_t>() {
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 32));
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

  template<>
  AbbrevsBuilder &
  AbbrevsBuilder::addRecordFieldTypes<DeclASTMeta::FragmentTy>() {

    using FragmentTy = DeclASTMeta::FragmentTy;

    addFieldType<decltype(std::declval<FragmentTy>().Start)>();
    addFieldType<decltype(std::declval<FragmentTy>().End)>();
    addFieldType<decltype(std::declval<FragmentTy>().ReplaceWithSemicolon)>();
    addFieldType<decltype(std::declval<FragmentTy>().PrefixWithExtern)>();

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

    // TODO Levitation: Perhaps we could also introduce template class,
    //   just like LevitationBitstreamReader
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
        RECORD(DEPS_PACKAGE_TOP_LEVEL_FIELDS_RECORD);

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
      Data.IsPublic = Dependencies.IsPublic;
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
                Declaration.second.getImportLoc()
            )
        );
      }
    }

    DependenciesData::Declaration buildDeclarationData(
        DependenciesStringsPool &Strings,
        StringRef FilePath,
        DependencyComponentsArrRef Components,
        SourceRange ImportLocation
    ) {
      auto FilePathID = Strings.addItem(FilePath);

      return {
          FilePathID,
          ImportLocation.getBegin().getRawEncoding(),
          ImportLocation.getEnd().getRawEncoding()
      };
    }

    void write(const DependenciesData &Data) {

      with (auto MainBlockScope = enterBlock(DEPS_DEPENDENCIES_MAIN_BLOCK_ID)) {

        writeStrings(*Data.Strings);

        writePackageTopLevelFields(
            Data.PackageFilePathID,
            Data.IsPublic
        );

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

    void writePackageTopLevelFields(
        StringID PathID, bool IsPublic
    ) {
      RecordData::value_type Record[] { PathID, (uint64_t)IsPublic };
      Writer.EmitRecord(DEPS_PACKAGE_TOP_LEVEL_FIELDS_RECORD_ID, Record);
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
    return std::make_unique<DependenciesBitstreamWriter>(OS);
  }

  // ==========================================================================
  // Levitation Bitstream Reader

  template<
      typename DataTy,
      char B0, char B1, char B2, char B3,
      unsigned MainBlockID
  >
  class LevitationBitstreamReader : public Failable {
  protected:
    BitstreamCursor Reader;
    Optional<llvm::BitstreamBlockInfo> BlockInfo;

    LevitationBitstreamReader(const llvm::MemoryBuffer &MemoryBuffer)
    : Reader(MemoryBuffer) {}

    bool readSignature() {
      return Reader.canSkipToPos(4) &&
          readAndCheckByte(B0) &&
          readAndCheckByte(B1) &&
          readAndCheckByte(B2) &&
          readAndCheckByte(B3);
    }

    bool readAndCheckByte(char B) {
        auto Res = Reader.Read(8);
        return Res && Res.get();
    }

    bool enterBlock(unsigned ID) {
      if (!isValid()) return false;

      while (true) {
        auto EntryRes = Reader.advance();
        if (!EntryRes) {
          setFailure("Failed to advance on record reading (enterBlock)");
          return false;
        }
        auto &Entry = EntryRes.get();

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

    bool readBlockInfo() {

      // Read the top level blocks.
      if (Reader.AtEndOfStream()) {
        setFailure("No blocks.");
        return false;
      }

      while (!Reader.AtEndOfStream()) {

        auto CodeRes = Reader.ReadCode();

        if (!CodeRes || CodeRes.get() != llvm::bitc::ENTER_SUBBLOCK) {
          setFailure("Expected BlockInfo Subblock, malformed file.");
          return false;
        }

        std::error_code EC;
        auto SubBlockIDRes = Reader.ReadSubBlockID();
        if (!SubBlockIDRes) {
          setFailure("Failed to read subblock ID");
          return false;
        }

        switch (SubBlockIDRes.get()) {
          case llvm::bitc::BLOCKINFO_BLOCK_ID: {
            auto BlockInfoRes = Reader.ReadBlockInfoBlock();
            if (!BlockInfoRes) {
              setFailure("Failed to read block info.");
              return false;
            }
            if ((BlockInfo = BlockInfoRes.get())) {
              Reader.setBlockInfo(BlockInfo.getPointer());
              return true;
            } else {
              setFailure("Malformed BlockInfo.");
              return false;
            }
          }
          case MainBlockID:
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
    using OnReadRecordFn = std::function<void(const RecordTy&, StringRef)>;

    bool readAllRecords(
        unsigned BlockID,
        unsigned RecordID,
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
        unsigned RecordID,
        RecordTy &RecordStorage,
        StringRef &BlobRef,
        bool WithBlob,
        const OnReadRecordFn &OnReadRecord
    ) {
      auto EntryRes = Reader.advanceSkippingSubblocks();
      if (!EntryRes) {
        setFailure("Failed to read record");
        return false;
      }
      auto &Entry = EntryRes.get();

      switch (Entry.Kind) {
        case BitstreamEntry::Error:
          setFailure("Failed to read strings");
          return false;
        case BitstreamEntry::Record: {
            RecordStorage.clear();

            StringRef *BlobPtr = WithBlob ? &BlobRef : nullptr;

            auto ObtainedRecordIDRes = Reader.readRecord(Entry.ID, RecordStorage, BlobPtr);
            if (!ObtainedRecordIDRes) {
                setFailure("Failed to read record ID");
                return false;
            }

            unsigned int ObtainedRecordID = ObtainedRecordIDRes.get();

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
        unsigned RecordID,
        unsigned int ObtainedID
    ) {
      if (ObtainedID != RecordID) {
        setFailure("Malformed file. Wrong record type read.");
        return false;
      }
      return true;
    }

  public:

  };

  // ==========================================================================
  // Dependencies Bitstream Reader

  class DependenciesBitstreamReader
      : public LevitationBitstreamReader<
          DependenciesData, 'L', 'D', 'E', 'P',
          DEPS_DEPENDENCIES_MAIN_BLOCK_ID
        >,
        public DependenciesReader
  {
    StringRef ErrorMessage;
    Optional<llvm::BitstreamBlockInfo> BlockInfo;
  public:
    DependenciesBitstreamReader(const llvm::MemoryBuffer &MemoryBuffer)
    : LevitationBitstreamReader(MemoryBuffer) {}

    bool read(DependenciesData &Data) override {
      if (!readSignature())
        return false;

      if (!readData(Data))
        return false;

      return true;
    }

    const Failable &getStatus() const override { return *this; }

  protected:

    bool readData(DependenciesData &Data) {
      if (!readBlockInfo())
        return false;

      if (enterBlock(DEPS_DEPENDENCIES_MAIN_BLOCK_ID)) {
        bool EndOfBlock = false;
        while (!EndOfBlock) {

          auto EntryRes = Reader.advance();
          if (!EntryRes) {
            setFailure("Failed to advance on reading bitstream.");
            return false;
          }

          auto &Entry = EntryRes.get();
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
              if (!readPackageTopLevelFields(Entry.ID, Data))
                return false;
              break;
          }
        }

        return true;
      }

      return false;
    }

    using RecordTy = LevitationBitstreamReader::RecordTy;

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

    bool readPackageTopLevelFields(unsigned int AbbrevID, DependenciesData &Data) {
      RecordTy Record;

      auto RecordIDRes = Reader.readRecord(AbbrevID, Record, nullptr);
      if (!RecordIDRes) {
        setFailure("Failed to read record ID");
        return false;
      }
      auto &RecordID = RecordIDRes.get();

      Data.PackageFilePathID = normalizeIfNeeded(
          *Data.Strings, (StringID)Record[0]
      );
      Data.IsPublic = (bool)Record[1];

      return checkRecordType(DEPS_PACKAGE_TOP_LEVEL_FIELDS_RECORD_ID, RecordID);
    }

  };

  std::unique_ptr<DependenciesReader> CreateBitstreamReader(
      const llvm::MemoryBuffer &MB
  ) {
    return std::make_unique<DependenciesBitstreamReader>(MB);
  }

  // ==========================================================================
  // Meta Dependencies Bitstream Writer

  class DeclASTMetaBitstreamWriter : public DeclASTMetaWriter {
  private:
    static const size_t BUFFER_DEFAULT_SIZE = 4096;

    llvm::raw_ostream &OutputStream;

    SmallVector<char, BUFFER_DEFAULT_SIZE> Buffer;
    BitstreamWriter Writer;

    bool HeaderWritten;
    bool Finalized;

  public:
    DeclASTMetaBitstreamWriter(llvm::raw_ostream &OS)
        : OutputStream(OS),
          Writer(Buffer),
          HeaderWritten(false),
          Finalized(false) {}

    ~DeclASTMetaBitstreamWriter() override = default;

    void writeAndFinalize(const DeclASTMeta &Meta) override {
      if (Finalized)
        llvm_unreachable("Can't write dependencies structure twice.");

      writeHeader();

      write(Meta);

      OutputStream << Buffer;

      Finalized = true;
    }

    void writeHeader() {
      if (HeaderWritten)
        llvm_unreachable("Can't write header twice.");

      writeSignature();

      HeaderWritten = true;
    }

    void writeSignature() {

      // Note: only 4 first bytes can be used for magic number.
      Writer.Emit((unsigned)'L', 8);
      Writer.Emit((unsigned)'M', 8);
      Writer.Emit((unsigned)'E', 8);
      Writer.Emit((unsigned)'T', 8);

      RecordData Record;

      Writer.EnterBlockInfoBlock();
      with (auto BlockInfoScope = make_scope_exit([&] { Writer.ExitBlock(); })) {

#define BLOCK(X) EmitBlockID(X ## _ID, #X, Writer, Record)
#define RECORD(X) EmitRecordID(X ## _ID, #X, Writer, Record)

        //  At current moment we should store only three arrays:
        //  0. Source hash
        //  1. Decl AST hash
        //  2. Skipped bytes ranges.
        //  So there is no reason in main block itself.
        //
        //  BLOCK(META_MAIN_BLOCK);
        //  RECORD(META_TOP_LEVEL_FIELDS_RECORD);

        // Note: order block-record matters.
        // Put record decls straight beneath the block it
        // belongs to.
        BLOCK(META_ARRAYS_BLOCK);
        RECORD(META_SOURCE_HASH_RECORD);
        RECORD(META_DECL_AST_HASH_RECORD);

        BLOCK(META_SKIPPED_FRAGMENT_BLOCK);
        RECORD(META_SKIPPED_FRAGMENT_RECORD);

#undef RECORD
#undef BLOCK
      }
    }

    void write(const DeclASTMeta &Meta) {

      with (auto MainBlockScope = enterBlock(META_ARRAYS_BLOCK_ID)) {
        writeArrays(
            Meta.getSourceHash(),
            Meta.getDeclASTHash()
        );

        writeSkippedFragments(Meta.getFragmentsToSkip());
      }
    }

    void writeArrays(
        ArrayRef<uint8_t> SourceHash,
        ArrayRef<uint8_t> DeclASTHash
    ) {
      unsigned SourceHashRecordAbbrev = AbbrevsBuilder(META_SOURCE_HASH_RECORD_ID, Writer)
          .addArrayType<uint8_t>()
      .done();

      unsigned DeclAstRecordAbbrev = AbbrevsBuilder(META_DECL_AST_HASH_RECORD_ID, Writer)
          .addArrayType<uint8_t>()
      .done();

      Writer.EmitRecord(
          META_SOURCE_HASH_RECORD_ID,
          SourceHash,
          SourceHashRecordAbbrev
      );
      Writer.EmitRecord(
          META_DECL_AST_HASH_RECORD_ID,
          DeclASTHash,
          DeclAstRecordAbbrev
      );
    }

    void writeSkippedFragments(const DeclASTMeta::FragmentsVectorTy &SkippedFragments) {

      with (auto FragmentsBlock = enterBlock(META_SKIPPED_FRAGMENT_BLOCK_ID)) {

        auto FragmentAbb = AbbrevsBuilder(META_SKIPPED_FRAGMENT_RECORD_ID, Writer)
            .addRecordFieldTypes<DeclASTMeta::FragmentTy>()
        .done();

        for (const auto &Fragment : SkippedFragments) {
          RecordWriter(Writer, META_SKIPPED_FRAGMENT_RECORD_ID, FragmentAbb)
            .emitField(Fragment.Start)
            .emitField(Fragment.End)
            .emitField(Fragment.ReplaceWithSemicolon)
            .emitField(Fragment.PrefixWithExtern)
          .done();
        }
      }
    }

  private:
    BlockScope enterBlock(unsigned BlockID, unsigned CodeLen = 3) {
      return BlockScope(Writer, BlockID, CodeLen);
    }
  };

  std::unique_ptr<DeclASTMetaWriter> CreateMetaBitstreamWriter(
      llvm::raw_ostream &OS
  ) {
    return std::make_unique<DeclASTMetaBitstreamWriter>(OS);
  }

  // ==========================================================================
  // Meta Dependencies Bitstream Reader

  class DeclASTMetaBitstreamReader
      : public LevitationBitstreamReader<
          DeclASTMeta,
          'L', 'M', 'E', 'T',
          META_ARRAYS_BLOCK_ID
        >,
        public DeclASTMetaReader
  {
    StringRef ErrorMessage;
    using RecordTy = SmallVector<uint64_t, 64>;
  public:
    DeclASTMetaBitstreamReader(const llvm::MemoryBuffer &MemoryBuffer)
        : LevitationBitstreamReader(MemoryBuffer) {}

    // TODO Levitation
    ~DeclASTMetaBitstreamReader() override = default;

    bool read(DeclASTMeta &Meta) override {
      if (!readSignature())
        return false;

      if (!readData(Meta))
        return false;

      return true;
    }

    bool readData(DeclASTMeta &Meta) {
      if (!readBlockInfo())
        return false;

      if (enterBlock(META_ARRAYS_BLOCK_ID)) {
        bool EndOfBlock = false;
        while (!EndOfBlock) {

          auto EntryRes = Reader.advance();
          if (!EntryRes) {
            setFailure("Failed to advance on reading bitstream.");
            return false;
          }

          auto &Entry = EntryRes.get();
          switch (Entry.Kind) {
            case BitstreamEntry::Error:
              setFailure("Failed to enter read bitstream.");
              return false;

            case BitstreamEntry::EndBlock:
              Reader.ReadBlockEnd();
              EndOfBlock = true;
              break;
            case BitstreamEntry::SubBlock: {

                auto BlockID = (MetaBlockIDs)Entry.ID;

                switch (BlockID) {
                  case META_SKIPPED_FRAGMENT_BLOCK_ID:
                    if (!readSkippedFragments(BlockID, Meta))
                      return false;
                    break;
                  case META_ARRAYS_BLOCK_ID:
                    llvm_unreachable("Recursive main block.");
                }
              }
              break;
            case BitstreamEntry::Record:
              if (!readArrays(Entry.ID, Meta))
                return false;
              break;
          }
        }

        return true;
      }

      return false;
    }

    bool readArrays(unsigned int AbbrevID, DeclASTMeta &Meta) {
      RecordTy Record;
      StringRef Blob;


      auto RecordIDRes = Reader.readRecord(AbbrevID, Record, &Blob);
      if (!RecordIDRes) {
        setFailure("Failed to read record ID");
        return false;
      }
      auto &RecordID = RecordIDRes.get();

      switch (RecordID) {
        case META_SOURCE_HASH_RECORD_ID:
          Meta.setSourceHash(Record);
          break;
        case META_DECL_AST_HASH_RECORD_ID:
          Meta.setDeclASTHash(Record);
          break;
        default:
          llvm_unreachable("Unknown record ID.");
      }
      return true;
    }

    bool readSkippedFragments(unsigned int BlockID, DeclASTMeta &Meta) {
      return readAllRecords(
          BlockID,
          META_SKIPPED_FRAGMENT_RECORD_ID,
          /*WithBlob*/false,

          [&](const RecordTy &Record, StringRef BlobStr) {

            DeclASTMeta::FragmentTy Fragment;

            RecordReader<RecordTy>(Record)
              .read(Fragment.Start)
              .read(Fragment.End)
              .read(Fragment.ReplaceWithSemicolon)
              .read(Fragment.PrefixWithExtern)
            .done();

            Meta.addSkippedFragment(Fragment);
          }
      );
    }

    const Failable &getStatus() const override {
      return *this;
    }
  };

  std::unique_ptr<DeclASTMetaReader> CreateMetaBitstreamReader(
      const llvm::MemoryBuffer &MB
  ) {
    return std::make_unique<DeclASTMetaBitstreamReader>(MB);
  }
}
}

