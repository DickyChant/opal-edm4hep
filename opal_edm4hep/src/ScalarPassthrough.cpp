#include "opal_edm4hep/ScalarPassthrough.h"

#include <TBranch.h>
#include <TLeaf.h>
#include <TObjArray.h>

namespace opal {
namespace {

/// True when the leaf carries more than one value per entry, either a fixed
/// dimension (`Njcedtc[7]`) or a counter-driven one (`Yddtc[Nxjdtc]`).
bool leafIsArray(TLeaf& leaf) {
  return leaf.GetLeafCount() != nullptr || leaf.GetLenStatic() > 1 || leaf.GetNdata() > 1;
}

} // namespace

ScalarPassthrough::ScalarPassthrough(TTreeReader& reader, TTree& tree,
                                     std::set<std::string> consumed) {
  TObjArray* branches = tree.GetListOfBranches();
  if (!branches) return;

  for (int i = 0; i < branches->GetEntries(); ++i) {
    auto* branch = static_cast<TBranch*>(branches->At(i));
    if (!branch) continue;
    const std::string name = branch->GetName();
    if (consumed.count(name)) continue;

    auto* leaf = branch->GetLeaf(name.c_str());
    if (!leaf) {
      auto* leaves = branch->GetListOfLeaves();
      if (!leaves || leaves->GetEntries() == 0) continue;
      leaf = static_cast<TLeaf*>(leaves->At(0));
    }
    const std::string type = leaf->GetTypeName();
    const bool isArray = leafIsArray(*leaf);

    auto bound = std::make_unique<Bound>();
    bound->name = name;
    bound->isArray = isArray;
    const char* bname = name.c_str();

    if (type == "Float_t") {
      if (isArray) bound->aFloat.emplace(reader, bname);
      else bound->vFloat.emplace(reader, bname);
    } else if (type == "Int_t") {
      if (isArray) bound->aInt.emplace(reader, bname);
      else bound->vInt.emplace(reader, bname);
    } else if (type == "UInt_t") {
      if (isArray) bound->aUInt.emplace(reader, bname);
      else bound->vUInt.emplace(reader, bname);
    } else if (type == "Short_t") {
      if (isArray) bound->aShort.emplace(reader, bname);
      else bound->vShort.emplace(reader, bname);
    } else if (type == "UShort_t") {
      if (isArray) bound->aUShort.emplace(reader, bname);
      else bound->vUShort.emplace(reader, bname);
    } else if (type == "Char_t") {
      if (isArray) bound->aChar.emplace(reader, bname);
      else bound->vChar.emplace(reader, bname);
    } else if (type == "UChar_t") {
      if (isArray) bound->aUChar.emplace(reader, bname);
      else bound->vUChar.emplace(reader, bname);
    } else {
      // Double_t and anything else the ntuple does not currently use.
      continue;
    }
    if (bound->isArray) {
      if (bound->aFloat) m_floatArrayNames.push_back(name);
      else m_intArrayNames.push_back(name);
    } else {
      if (bound->vFloat) m_floatNames.push_back(name);
      else m_intNames.push_back(name);
    }
    m_entries.push_back(std::move(bound));
  }
}

void ScalarPassthrough::refresh(Packed& out) const {
  out.floats.clear();
  out.ints.clear();
  out.floatArrayData.clear();
  out.floatArrayEnds.clear();
  out.intArrayData.clear();
  out.intArrayEnds.clear();

  for (auto& e : m_entries) {
    if (!e->isArray) {
      if (e->vFloat) out.floats.push_back(**e->vFloat);
      else if (e->vInt) out.ints.push_back(**e->vInt);
      else if (e->vUInt) out.ints.push_back(static_cast<int>(**e->vUInt));
      else if (e->vShort) out.ints.push_back(**e->vShort);
      else if (e->vUShort) out.ints.push_back(**e->vUShort);
      else if (e->vChar) out.ints.push_back(**e->vChar);
      else if (e->vUChar) out.ints.push_back(**e->vUChar);
      continue;
    }
    if (e->aFloat) {
      out.floatArrayData.insert(out.floatArrayData.end(), e->aFloat->begin(), e->aFloat->end());
      out.floatArrayEnds.push_back(static_cast<std::uint32_t>(out.floatArrayData.size()));
      continue;
    }
    const std::size_t before = out.intArrayData.size();
    if (e->aInt) out.intArrayData.insert(out.intArrayData.end(), e->aInt->begin(), e->aInt->end());
    else if (e->aUInt) for (auto x : *e->aUInt) out.intArrayData.push_back(static_cast<int>(x));
    else if (e->aShort) for (auto x : *e->aShort) out.intArrayData.push_back(x);
    else if (e->aUShort) for (auto x : *e->aUShort) out.intArrayData.push_back(x);
    else if (e->aChar) for (auto x : *e->aChar) out.intArrayData.push_back(x);
    else if (e->aUChar) for (auto x : *e->aUChar) out.intArrayData.push_back(x);
    else continue;
    (void)before;
    out.intArrayEnds.push_back(static_cast<std::uint32_t>(out.intArrayData.size()));
  }
}

} // namespace opal
