#ifndef OPAL_EDM4HEP_NTUPLEREADER_H
#define OPAL_EDM4HEP_NTUPLEREADER_H

#include <TFile.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace opal {

/// Streaming reader for the `h10` TTree produced by `h2root` from an OPAL
/// `.histo` file (HBOOK column-wise ntuple `QQNT200`, blocks DAGNRL, DAXTRA,
/// DAEVSH, DAJETS, DACTRK, DACLUS, BTVAR and -- MC only -- the truth blocks).
///
/// The reader is deliberately event-at-a-time (`TTreeReader`): the dataset is
/// ~32 GB and the conversion hosts are small, so no branch is ever materialised
/// in full. Truth branches are optional and are only bound for MC inputs.
class NtupleReader {
public:
  /// Opens `path` and binds the tree. Throws std::runtime_error on failure.
  explicit NtupleReader(const std::string& path, const std::string& treeName = "h10");

  /// Advances to the next entry. Returns false at end of tree.
  bool next();

  /// Total number of entries in the tree.
  Long64_t numEntries() const { return m_entries; }

  /// True when the truth blocks (parton/hadron level) are present.
  bool isMonteCarlo() const { return m_isMC; }

  /// Underlying reader and tree, so a ScalarPassthrough can bind the branches
  /// this class does not model. Must be used before the first next() call.
  TTreeReader& reader() { return *m_reader; }
  TTree& tree() { return *m_tree; }

  // ---- DAGNRL: event identification and global quantities -----------------
  std::uint16_t irun() const { return **m_Irun; }
  std::int32_t ievnt() const { return **m_Ievnt; }
  float ebeam() const { return **m_Ebeam; }
  std::uint16_t ntkd02() const { return **m_Ntkd02; }
  const float* pgce() const { return &(*m_Pgce)[0]; }     ///< [4]
  const float* tvectc() const { return &(*m_Tvectc)[0]; } ///< [3] thrust axis

  // ---- DACTRK: charged tracks --------------------------------------------
  std::int32_t ntrk() const { return **m_Ntrk; }
  float ptrk(int i, int c) const { return (*m_Ptrk)[i * 3 + c]; } ///< px,py,pz [GeV]
  int ichg(int i) const { return (*m_Ichg)[i]; }
  float d0(int i) const { return (*m_D0)[i]; } ///< [cm]
  float z0(int i) const { return (*m_Z0)[i]; } ///< [cm]
  float dp(int i) const { return (*m_Dp)[i]; } ///< relative momentum error
  float dedx(int i) const { return (*m_Dedx)[i]; }
  float dded(int i) const { return (*m_Dded)[i]; }
  int nhde(int i) const { return (*m_Nhde)[i]; }
  int nhcj(int i) const { return (*m_Nhcj)[i]; }
  int id02(int i) const { return (*m_Id02)[i]; } ///< d02 good-track flag

  // ---- DACLUS: calorimeter clusters and the MT (matched) package ----------
  std::int32_t nclus() const { return **m_Nclus; }
  float pclus(int i, int c) const { return (*m_Pclus)[i * 3 + c]; }
  std::int32_t nmttrk() const { return **m_Nmttrk; }
  int imttrk(int i) const { return (*m_Imttrk)[i]; } ///< 1-based track index
  float mtscft(int i) const { return (*m_Mtscft)[i]; }
  std::int32_t nmtcls() const { return **m_Nmtcls; }
  int imtcls(int i) const { return (*m_Imtcls)[i]; } ///< 1-based cluster index
  float mtscfc(int i) const { return (*m_Mtscfc)[i]; }
  std::int32_t nmtkil() const { return **m_Nmtkil; }
  int imtkil(int i) const { return (*m_Imtkil)[i]; } ///< 1-based, cluster killed

  // ---- BTVAR: b-tagging vertices -----------------------------------------
  std::int32_t nvtxbt() const { return **m_Nvtxbt; }
  const float* prvtxbt() const { return &(*m_Prvtxbt)[0]; } ///< primary vtx [cm]
  float vtxbt(int i, int c) const { return (*m_Vtxbt)[i * 3 + c]; }
  float pvtxbt(int i, int c) const { return (*m_Pvtxbt)[i * 5 + c]; }
  float vchi2bt(int i) const { return (*m_Vchi2bt)[i]; }
  float vnnbt(int i) const { return (*m_Vnnbt)[i]; }
  int ivmulbt(int i) const { return (*m_Ivmulbt)[i]; }
  float vdlen3bt(int i) const { return (*m_Vdlen3bt)[i]; }
  float vderr3bt(int i) const { return (*m_Vderr3bt)[i]; }

  // ---- Truth blocks (MC only; call sites must check isMonteCarlo()) -------
  int ievtyp() const { return **m_Ievtyp; }
  std::int32_t nprimf() const { return **m_Nprimf; }
  int iferid(int i) const { return (*m_Iferid)[i]; }
  float primf(int i, int c) const { return (*m_Primf)[i * 4 + c]; } ///< px,py,pz,E
  const float* pisr() const { return &(*m_Pisr)[0]; }               ///< [4]

  std::int32_t ntrkp() const { return **m_Ntrkp; } ///< parton level
  int ilucp(int i) const { return (*m_Ilucp)[i]; }
  float ptrkp(int i, int c) const { return (*m_Ptrkp)[i * 4 + c]; }

  std::int32_t ntrkh() const { return **m_Ntrkh; } ///< hadron level
  int iluch(int i) const { return (*m_Iluch)[i]; }
  int ichgh(int i) const { return (*m_Ichgh)[i]; }
  float ptrkh(int i, int c) const { return (*m_Ptrkh)[i * 4 + c]; }

  /// Truth match of the reconstructed charged tracks. `Ntrk2 == Ntrk` holds in
  /// every event inspected, so `iluc(i)`/`istrt(i)` describe reco track `i`.
  std::int32_t ntrk2() const { return **m_Ntrk2; }
  int iluc(int i) const { return (*m_Iluc)[i]; } ///< PDG, 0 = unmatched
  int istrt(int i) const { return (*m_Istrt)[i]; }

private:
  template <typename T> using Val = std::optional<TTreeReaderValue<T>>;
  template <typename T> using Arr = std::optional<TTreeReaderArray<T>>;

  std::unique_ptr<TFile> m_file;
  TTree* m_tree = nullptr;
  std::unique_ptr<TTreeReader> m_reader;
  Long64_t m_entries = 0;
  bool m_isMC = false;

  bool hasBranch(const char* name) const;
  template <typename T> void bindValue(Val<T>& v, const char* name);
  template <typename T> void bindArray(Arr<T>& a, const char* name);

  mutable Val<std::uint16_t> m_Irun, m_Ntkd02;
  mutable Val<std::int32_t> m_Ievnt;
  mutable Val<float> m_Ebeam;
  mutable Arr<float> m_Pgce, m_Tvectc;

  mutable Val<std::int32_t> m_Ntrk;
  mutable Arr<float> m_Ptrk, m_D0, m_Z0, m_Dp, m_Dedx, m_Dded;
  mutable Arr<std::int8_t> m_Ichg;
  mutable Arr<std::uint8_t> m_Nhde, m_Nhcj, m_Id02;

  mutable Val<std::int32_t> m_Nclus, m_Nmttrk, m_Nmtcls, m_Nmtkil;
  mutable Arr<float> m_Pclus, m_Mtscft, m_Mtscfc;
  mutable Arr<std::uint16_t> m_Imttrk, m_Imtcls, m_Imtkil;

  mutable Val<std::int32_t> m_Nvtxbt;
  mutable Arr<float> m_Prvtxbt, m_Vtxbt, m_Pvtxbt, m_Vchi2bt, m_Vnnbt, m_Vdlen3bt, m_Vderr3bt;
  mutable Arr<std::uint8_t> m_Ivmulbt;

  mutable Val<std::uint8_t> m_Ievtyp;
  mutable Val<std::int32_t> m_Nprimf, m_Ntrkp, m_Ntrkh, m_Ntrk2;
  mutable Arr<std::int8_t> m_Iferid, m_Ilucp, m_Ichgh;
  mutable Arr<float> m_Primf, m_Pisr, m_Ptrkp, m_Ptrkh;
  mutable Arr<std::int32_t> m_Iluch, m_Iluc;
  mutable Arr<std::uint8_t> m_Istrt;
};

} // namespace opal

#endif
