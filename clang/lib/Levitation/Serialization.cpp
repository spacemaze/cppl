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
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/WithOperator.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Bitstream/BitstreamReader.h"
#include "llvm/Bitstream/BitstreamWriter.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <utility>

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

    template<typename CallableTy>
    RecordReader &readcb(CallableTy&& cb) {
      cb(Record[CurIdx++]);
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
  AbbrevsBuilder::addRecordFieldTypes<Declaration>() {
    using Declaration = Declaration;

    addFieldType<decltype(std::declval<Declaration>().UnitIdentifier)>();

    return *this;
  }

  template <>
  AbbrevsBuilder& AbbrevsBuilder::addFieldType<SourceFragmentAction>() {
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 8));
    return *this;
  }

  template<>
  AbbrevsBuilder &
  AbbrevsBuilder::addRecordFieldTypes<DeclASTMeta::FragmentTy>() {

    using FragmentTy = DeclASTMeta::FragmentTy;

    addFieldType<decltype(std::declval<FragmentTy>().Start)>();
    addFieldType<decltype(std::declval<FragmentTy>().End)>();
    addFieldType<decltype(std::declval<FragmentTy>().Action)>();

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
      DependenciesData Data(&Dependencies.PathsPool);

      addDeclarationsData(
          Data.DeclarationDependencies,
          Dependencies.DeclarationDependencies
      );

      addDeclarationsData(
          Data.DefinitionDependencies,
          Dependencies.DefinitionDependencies
      );

      Data.IsPublic = Dependencies.IsPublic;
      Data.IsBodyOnly = Dependencies.IsBodyOnly;
      return Data;
    }

    void addDeclarationsData(
        DependenciesData::DeclarationsBlock &StoredDeclarations,
        const PathIDsSet &Declarations
    ) {
      for (auto PathId : Declarations) {
        StoredDeclarations.insert(Declaration(PathId));
      }
    }

    void write(const DependenciesData &Data) {

      with (auto MainBlockScope = enterBlock(DEPS_DEPENDENCIES_MAIN_BLOCK_ID)) {

        writeStrings(*Data.Strings);

        writePackageTopLevelFields(Data.IsPublic, Data.IsBodyOnly);

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

    void writePackageTopLevelFields(bool IsPublic, bool IsBodyOnly) {
      RecordData::value_type Record[] { (uint64_t)IsPublic, (uint64_t) IsBodyOnly };
      Writer.EmitRecord(DEPS_PACKAGE_TOP_LEVEL_FIELDS_RECORD_ID, Record);
    }

    void writeDeclaration(
        const Declaration &Data,
        unsigned DeclAbbrev,
        unsigned RecordId = DEPS_DECLARATION_RECORD_ID
    ) {
      RecordData Record;

      Record.push_back(Data.UnitIdentifier);

      // TODO Levitation: add location info, see task #67
      //      Record.push_back(Data.LocationIDBegin);
      //      Record.push_back(Data.LocationIDEnd);

      Writer.EmitRecord(RecordId, Record, DeclAbbrev);
    }

    void writeDeclarations(
        DependenciesBlockIDs BlockID,
        const DependenciesData::DeclarationsBlock &Declarations
    ) {
      with (auto block = enterBlock(BlockID)) {

        unsigned DeclAbbrev =
          AbbrevsBuilder(DEPS_DECLARATION_RECORD_ID, Writer)
              .addRecordFieldTypes<Declaration>()
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

    enum EnterBlockResult {
      EnterBlockFailure,
      EnterBlockEmpty,
      EnterBlockSuccess
    };

    BitstreamCursor Reader;
    Optional<llvm::BitstreamBlockInfo> BlockInfo;
    levitation::log::Logger &Log = levitation::log::Logger::get();

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

    template <typename Callable>
    EnterBlockResult enterBlock(unsigned ID, Callable &&OnNonEmptyBlock) {
      if (!isValid()) return EnterBlockFailure;

      while (true) {
        auto EntryRes = Reader.advance();
        if (!EntryRes) {
          setFailure("Failed to advance on record reading (enterBlock)");
          return EnterBlockFailure;
        }
        auto &Entry = EntryRes.get();

        switch (Entry.Kind) {
          case BitstreamEntry::Error:
            setFailure("Failed to enter read bitstream.");
            return EnterBlockFailure;

          case BitstreamEntry::EndBlock:
            return EnterBlockEmpty;
          case BitstreamEntry::SubBlock:
            if (Entry.ID != ID) {
              if (Reader.SkipBlock()) {
                setFailure("Failed to skip block.");
                return EnterBlockFailure;
              }
            } else {
              if (!Reader.EnterSubBlock(Entry.ID))
                return OnNonEmptyBlock() ? EnterBlockSuccess : EnterBlockFailure;

              setFailure("Failed to enter subblock.");
              return EnterBlockFailure;
            }
            break;
          case BitstreamEntry::Record:
            break;
        }
      }
    }

    using BlockActionFn = std::function<bool()>;
    using BlockActionsTable = llvm::DenseMap<unsigned, BlockActionFn>;
    using BlockActionsList = std::initializer_list<BlockActionsTable::value_type>;

    using RecordTy = SmallVector<uint64_t, 64>;
    using RecordActionFn = std::function<bool(const RecordTy &, StringRef)>;
    using RecordActionsTable = llvm::DenseMap<unsigned, RecordActionFn>;
    using RecordActionsList = std::initializer_list<RecordActionsTable::value_type>;


    bool parse(
      BlockActionsList BlockActionsList,
      RecordActionsList RecordActionsList
    ) {

      BlockActionsTable BlockActions = BlockActionsList.size() ?
        BlockActionsTable(BlockActionsList) :
        BlockActionsTable();

      RecordActionsTable RecordActions = RecordActionsList.size() ?
        RecordActionsTable(RecordActionsList) :
        RecordActionsTable();

      if (!isValid()) return false;

      while (true) {
        if (Reader.AtEndOfStream()) {
          Log.log_trace("End of stream");
          return true;
        }

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
            Log.log_trace("End current block");
            // Block is popped by BitstreamReader automatically
            // Reader.ReadBlockEnd();
            return true;

          case BitstreamEntry::SubBlock: {
            Log.log_trace("Enter subblock ", Entry.ID);
            auto found = BlockActions.find(Entry.ID);
            if (found == BlockActions.end()) {
              if (Reader.SkipBlock()) {
                setFailure("Failed to skip block.");
                return false;
              }
            } else {
              auto &OnNonEmptyBlock = found->second;

              if (Reader.EnterSubBlock(Entry.ID)) {
                setFailure("Failed to process block.");
                return false;
              }

              if (!OnNonEmptyBlock()) {
                setFailure("Failed to process block.");
                return false;
              }
            }
            break;
          }

          case BitstreamEntry::Record: {

            RecordTy Record;
            StringRef Blob;

            auto RecordIDRes = Reader.readRecord(Entry.ID, Record, &Blob);
            if (!RecordIDRes) {
              setFailure("Failed to read record ID");
              return false;
            }

            unsigned int RecordID = RecordIDRes.get();

            auto found = RecordActions.find(RecordID);
            if (found != RecordActions.end()) {
              auto &OnRecordAction = found->second;
              if (!OnRecordAction(Record, Blob)) {
                setFailure("Failed to parse record.");
                return false;
              }
            }
            break;
          }
        }
      }

      llvm_unreachable("It seem's that you have left while(true) loop.");
    }

    bool parse(BlockActionsList BlockActions) {
      return parse(BlockActions, {});
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

    using OnReadRecordFn = std::function<void(
        const RecordTy& /*Record items*/,
        StringRef /*Blob ref*/
    )>;

    using OnReadAnyRecordFn = std::function<void(
        unsigned /*RecordID*/,
        const RecordTy& /*Record items*/,
        StringRef /*Blob ref*/
    )>;

    // FIXME Levitation: readAllRecords should not enter block,
    //  it should be entered by caller.
    bool readAllRecords(
        unsigned BlockID,
        unsigned RecordID,
        bool WithBlob,
        OnReadRecordFn &&OnReadRecord) {
      RecordTy RecordStorage;
      StringRef BlobRef;

      auto reader = [&] {
        while (readRecord(
            RecordID, RecordStorage, BlobRef, WithBlob, OnReadRecord
        )) {
          // do nothing
        }
        return isValid();
      };

      bool Successful;
      if (BlockID != INVALID_BLOCK_ID)
        Successful = enterBlock(BlockID, reader);
      else
        Successful = reader();

      if (!Successful) {
        setFailure("Failed to read records, can't enter subblock");
        return false;
      }

      return true;
    }

    bool readAllRecords(
        unsigned BlockID,
        bool WithBlob,
        OnReadAnyRecordFn &&OnReadRecord
    ) {
      RecordTy RecordStorage;
      StringRef BlobRef;

      auto reader = [&] {
        while (readRecords(RecordStorage, BlobRef, WithBlob, OnReadRecord)) {
          // do nothing
        }
        return isValid();
      };

      bool Successfull;
      if (BlockID != INVALID_BLOCK_ID)
        Successfull = enterBlock(BlockID, reader);
      else
        Successfull = reader();


      if (!Successfull) {
        setFailure("Failed to read records, can't enter subblock");
        return false;
      }

      return true;
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

    /// Reads record
    /// \param RecordStorage heap for reacord values
    /// \param BlobRef blob variable
    /// \param WithBlob true if record has blob data.
    /// \param OnReadRecord record handler.
    /// \return true, if we can read more,
    /// false if there was an error, or end of block.
    bool readRecords(
        RecordTy &RecordStorage,
        StringRef &BlobRef,
        bool WithBlob,
        const OnReadAnyRecordFn &OnReadRecord
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

            OnReadRecord(ObtainedRecordID, RecordStorage, BlobRef);
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

      return parse(
        {
          {
            DEPS_DEPENDENCIES_MAIN_BLOCK_ID,
            [&] { return parse(
              {
                {
                  DEPS_STRINGS_BLOCK_ID,
                  [&] { return readStringsTable(Data); }
                },
                {
                  DEPS_DEFINITION_DEPENDENCIES_BLOCK_ID,
                  [&] {
                    Log.log_trace("Reading definition deps...");
                    return readDependencies(
                      *Data.Strings,
                      Data.DefinitionDependencies
                    );
                  }
                },
                {
                  DEPS_DECLARATION_DEPENDENCIES_BLOCK_ID,
                  [&] {
                    Log.log_trace("Reading declaration deps...");
                    return readDependencies(
                      *Data.Strings,
                      Data.DeclarationDependencies
                    );
                  }
                }
              },
              {
                {
                  DEPS_PACKAGE_TOP_LEVEL_FIELDS_RECORD_ID,
                  [&] (const RecordTy & Record, StringRef _) {
                    return readPackageTopLevelFields(Data, Record);
                  }
                }
              }
            );}
          }
        }
      );
    }

    using RecordTy = LevitationBitstreamReader::RecordTy;

    bool readStringsTable(DependenciesData &Data) {

      Log.log_trace("Reading strings table...");

      return parse(
        {},
        {
          {
            DEPS_STRING_RECORD_ID,
            [&](const RecordTy &Record, StringRef BlobStr) {
              auto SID = (StringID) Record[0];
              Data.Strings->addItem(SID, BlobStr);
              Log.log_trace("Loaded string \"", BlobStr, "\"...");
              return true;
            }
          }
        }
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
        DependenciesStringsPool &Strings,
        DependenciesData::DeclarationsBlock &Deps
    ) {
      auto recordHandler = [&](const RecordTy &Record, StringRef BlobStr) {
        StringID PathID = normalizeIfNeeded(Strings, (StringID)Record[0]);
        Deps.insert(Declaration(PathID));
        return true;
      };

      return parse(
        {},
        {
          {
            DEPS_DECLARATION_RECORD_ID,
            recordHandler
          }
        }
      );
    }

    bool readPathIds(
        DependenciesBlockIDs BlockID,
        DependenciesStringsPool &Strings,
        DenseSet<PathsPoolTy::key_type> &Deps
    ) {
      auto recordHandler = [&](const RecordTy &Record, StringRef BlobStr) {
        for (auto Id : Record)
          Deps.insert((PathsPoolTy::key_type)Id);
      };

      return readAllRecords(
          BlockID,
          DEPS_IDS_SET_RECORD_ID,
          /*WithBlob*/false,
          recordHandler
      );
    }

    bool readPackageTopLevelFields(
      DependenciesData &Data, const RecordTy &Record
    ) {
      Data.IsPublic = (bool)Record[0];
      Data.IsBodyOnly = (bool)Record[1];
      return true;
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
            .emitField((unsigned)Fragment.Action)
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

    bool readSkippedFragmentBlock(DeclASTMeta &Meta) {
      Log.log_trace("Reading skipped fragment block...");
      return parse(
        {},
        {
          {
            META_SKIPPED_FRAGMENT_RECORD_ID,
            [&](
              const RecordTy &Record, StringRef BlobStr
            ) {

              Log.log_trace("Reading skipped fragment record...");

              DeclASTMeta::FragmentTy Fragment;

              RecordReader<RecordTy>(Record)
                .read(Fragment.Start)
                .read(Fragment.End)
                .readcb([&](unsigned v) {Fragment.Action = (SourceFragmentAction)v; })
                .done();

              Meta.addSkippedFragment(Fragment);

              return true;
            }
          }
        }
      );
    }

    bool readData(DeclASTMeta &Meta) {
      if (!readBlockInfo())
        return false;

      return parse(
        {
          {
            META_ARRAYS_BLOCK_ID,
            [&] { return parse(
              {
                {
                  META_SKIPPED_FRAGMENT_BLOCK_ID,
                  [&] { return readSkippedFragmentBlock(Meta); }
                }
              },
              {
                {
                  META_SOURCE_HASH_RECORD_ID,
                  [&](const RecordTy &Record, StringRef _) {
                    Log.log_trace("Source hash record...");
                    Meta.setSourceHash(Record);
                    return true;
                  }
                },
                {
                  META_DECL_AST_HASH_RECORD_ID,
                  [&](const RecordTy &Record, StringRef _) {
                    Log.log_trace("Decl AST hash record...");
                    Meta.setDeclASTHash(Record);
                    return true;
                  }
                }
              }
            );}
          }
        });
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

