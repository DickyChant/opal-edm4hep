#ifndef OPAL_EDM4HEP_SCALARPASSTHROUGH_H
#define OPAL_EDM4HEP_SCALARPASSTHROUGH_H

#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace opal {

/// Carries every ntuple variable that has no EDM4hep datatype.
///
/// The OPAL ntuple is dominated by event-level quantities -- the DAEVSH event
/// shapes (thrust, sphericity, jet masses, broadenings, C/D parameters) for
/// four different detector-object definitions, the DAJETS jet-resolution
/// scales, and the DAXTRA selection variables. EDM4hep models none of these,
/// and dropping them would make the converted files useless for the analyses
/// the ntuple exists to serve. They are therefore bound generically here and
/// written as podio Frame parameters.
///
/// Binding is driven by the tree's own leaf list, so a variable added to a
/// future ntuple version is carried across without a code change.
class ScalarPassthrough {
public:
  /// Binds every branch of `tree` whose name is not in `consumed`.
  /// Must be constructed before the first call to TTreeReader::Next().
  ScalarPassthrough(TTreeReader& reader, TTree& tree, std::set<std::string> consumed);

  /// Values for the current entry, packed in the fixed order given by the
  /// *Names() accessors.
  ///
  /// The names are deliberately NOT written per event. Storing ~160 variables
  /// as podio GenericParameters costs a full copy of every key string in every
  /// event, which measured at 75% of the output file. Here the names are
  /// emitted once into the metadata frame and the values travel columnar.
  struct Packed {
    std::vector<float> floats;                 ///< one per floatNames() entry
    std::vector<int> ints;                     ///< one per intNames() entry
    std::vector<float> floatArrayData;         ///< concatenated, in floatArrayNames() order
    std::vector<std::uint32_t> floatArrayEnds; ///< end offset of each array
    std::vector<int> intArrayData;
    std::vector<std::uint32_t> intArrayEnds;
  };

  /// Re-reads the bound branches for the current entry and packs them.
  void refresh(Packed& out) const;

  const std::vector<std::string>& floatNames() const { return m_floatNames; }
  const std::vector<std::string>& intNames() const { return m_intNames; }
  const std::vector<std::string>& floatArrayNames() const { return m_floatArrayNames; }
  const std::vector<std::string>& intArrayNames() const { return m_intArrayNames; }

  /// Number of branches bound; used by the tests to assert full coverage.
  std::size_t boundCount() const { return m_entries.size(); }

private:
  /// One bound branch. Only the member matching `kind` is engaged.
  struct Bound {
    std::string name;
    bool isArray = false;
    std::optional<TTreeReaderValue<float>> vFloat;
    std::optional<TTreeReaderValue<std::int32_t>> vInt;
    std::optional<TTreeReaderValue<std::uint32_t>> vUInt;
    std::optional<TTreeReaderValue<std::int16_t>> vShort;
    std::optional<TTreeReaderValue<std::uint16_t>> vUShort;
    std::optional<TTreeReaderValue<std::int8_t>> vChar;
    std::optional<TTreeReaderValue<std::uint8_t>> vUChar;
    std::optional<TTreeReaderArray<float>> aFloat;
    std::optional<TTreeReaderArray<std::int32_t>> aInt;
    std::optional<TTreeReaderArray<std::uint32_t>> aUInt;
    std::optional<TTreeReaderArray<std::int16_t>> aShort;
    std::optional<TTreeReaderArray<std::uint16_t>> aUShort;
    std::optional<TTreeReaderArray<std::int8_t>> aChar;
    std::optional<TTreeReaderArray<std::uint8_t>> aUChar;
  };

  mutable std::vector<std::unique_ptr<Bound>> m_entries;
  std::vector<std::string> m_floatNames, m_intNames;
  std::vector<std::string> m_floatArrayNames, m_intArrayNames;
};

} // namespace opal

#endif
