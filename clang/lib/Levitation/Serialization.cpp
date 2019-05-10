// TODO Levitation: Licensing
//===--------------------------------------------------------------------===//
// Dependency handling
//
// Group of methods below implements
// package dependencies detection and handling.
//

#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/Serialization.h"
#include "clang/Levitation/WithOperator.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <utility>

using namespace llvm;

namespace clang {
namespace levitation {

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

    void writeAndFinalize(ValidatedDependencies &Dependencies) override {
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

        BLOCK(DEPS_STRINGS_BLOCK);
        RECORD(DEPS_STRING_RECORD);

        BLOCK(DEPS_DEPENDENCIES_MAIN_BLOCK);
        RECORD(DEPS_UNIT_FILE_PATH_RECORD);

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
    DependenciesData buildDependenciesData(ValidatedDependencies &Dependencies) {
      DependenciesData Data;

      addDeclarationsData(
          Data.Strings,
          Data.DeclarationDependencies,
          Dependencies.DeclarationDependencies
      );

      addDeclarationsData(
          Data.Strings,
          Data.DefinitionDependencies,
          Dependencies.DefinitionDependencies
      );

      Data.DependentUnitFilePathID =
          Data.Strings.addItem(Dependencies.DependentUnitFilePath);
      return Data;
    }

    void addDeclarationsData(
        DependenciesData::StringsTable &Strings,
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
        DependenciesData::StringsTable &Strings,
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

      writeStrings(Data.Strings);

      with (auto MainBlockScope = enterBlock(DEPS_DEPENDENCIES_MAIN_BLOCK_ID)) {

        writeDependentUnitFilePath(Data.DependentUnitFilePathID);

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

    void writeStrings(const DependenciesData::StringsTable &Data) {

      with (auto StringBlock = enterBlock(DEPS_STRINGS_BLOCK_ID)) {

        unsigned StringsTableAbbrev = AbbrevsBuilder(DEPS_STRING_RECORD_ID, Writer)
            .addBlobType()
        .done();

        for (const auto &StringItem : Data.items()) {

          RecordData::value_type Record[] = { DEPS_STRING_RECORD_ID };

          Writer.EmitRecordWithBlob(
              StringsTableAbbrev,
              Record,
              StringItem.second
          );
        }
      }
    }

    void writeDependentUnitFilePath(DependenciesData::StringIDType PathID) {
      RecordData::value_type Record[] { PathID };
      Writer.EmitRecord(DEPS_UNIT_FILE_PATH_RECORD_ID, Record);
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
}
}

