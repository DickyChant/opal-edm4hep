#include "opal_edm4hep/NtupleReader.h"

#include <TBranch.h>
#include <TLeaf.h>

#include <stdexcept>

namespace opal {

bool NtupleReader::hasBranch(const char* name) const {
  return m_tree->GetBranch(name) != nullptr;
}

template <typename T> void NtupleReader::bindValue(Val<T>& v, const char* name) {
  if (hasBranch(name)) v.emplace(*m_reader, name);
}

template <typename T> void NtupleReader::bindArray(Arr<T>& a, const char* name) {
  if (hasBranch(name)) a.emplace(*m_reader, name);
}

NtupleReader::NtupleReader(const std::string& path, const std::string& treeName) {
  m_file.reset(TFile::Open(path.c_str(), "READ"));
  if (!m_file || m_file->IsZombie())
    throw std::runtime_error("cannot open ROOT file: " + path);

  m_tree = m_file->Get<TTree>(treeName.c_str());
  if (!m_tree)
    throw std::runtime_error("no tree '" + treeName + "' in " + path +
                             " (was the file produced by h2root?)");

  m_reader = std::make_unique<TTreeReader>(m_tree);
  m_entries = m_tree->GetEntries();

  // The truth blocks are written only for MC. Ntrkh is the hadron-level
  // counter and is the cheapest unambiguous discriminator.
  m_isMC = hasBranch("Ntrkh");

  bindValue(m_Irun, "Irun");
  bindValue(m_Ievnt, "Ievnt");
  bindValue(m_Ebeam, "Ebeam");
  bindValue(m_Ntkd02, "Ntkd02");
  bindArray(m_Pgce, "Pgce");
  bindArray(m_Tvectc, "Tvectc");

  bindValue(m_Ntrk, "Ntrk");
  bindArray(m_Ptrk, "Ptrk");
  bindArray(m_Ichg, "Ichg");
  bindArray(m_D0, "D0");
  bindArray(m_Z0, "Z0");
  bindArray(m_Dp, "Dp");
  bindArray(m_Dedx, "Dedx");
  bindArray(m_Dded, "Dded");
  bindArray(m_Nhde, "Nhde");
  bindArray(m_Nhcj, "Nhcj");
  bindArray(m_Id02, "Id02");

  bindValue(m_Nclus, "Nclus");
  bindArray(m_Pclus, "Pclus");
  bindValue(m_Nmttrk, "Nmttrk");
  bindArray(m_Imttrk, "Imttrk");
  bindArray(m_Mtscft, "Mtscft");
  bindValue(m_Nmtcls, "Nmtcls");
  bindArray(m_Imtcls, "Imtcls");
  bindArray(m_Mtscfc, "Mtscfc");
  bindValue(m_Nmtkil, "Nmtkil");
  bindArray(m_Imtkil, "Imtkil");

  bindValue(m_Nvtxbt, "Nvtxbt");
  bindArray(m_Prvtxbt, "Prvtxbt");
  bindArray(m_Vtxbt, "Vtxbt");
  bindArray(m_Pvtxbt, "Pvtxbt");
  bindArray(m_Vchi2bt, "Vchi2bt");
  bindArray(m_Vnnbt, "Vnnbt");
  bindArray(m_Ivmulbt, "Ivmulbt");
  bindArray(m_Vdlen3bt, "Vdlen3bt");
  bindArray(m_Vderr3bt, "Vderr3bt");

  if (m_isMC) {
    bindValue(m_Ievtyp, "Ievtyp");
    bindValue(m_Nprimf, "Nprimf");
    bindArray(m_Iferid, "Iferid");
    bindArray(m_Primf, "Primf");
    bindArray(m_Pisr, "Pisr");
    bindValue(m_Ntrkp, "Ntrkp");
    bindArray(m_Ilucp, "Ilucp");
    bindArray(m_Ptrkp, "Ptrkp");
    bindValue(m_Ntrkh, "Ntrkh");
    bindArray(m_Iluch, "Iluch");
    bindArray(m_Ichgh, "Ichgh");
    bindArray(m_Ptrkh, "Ptrkh");
    bindValue(m_Ntrk2, "Ntrk2");
    bindArray(m_Iluc, "Iluc");
    bindArray(m_Istrt, "Istrt");
  }
}

bool NtupleReader::next() { return m_reader->Next(); }

} // namespace opal
