/// ===========================================================================
/*! \file   TrksInJetQAInclusiveFiller.cc
 *  \author Derek Anderson
 *  \date   04.03.2024
 *
 *  A submodule for the TrksInJetQA F4A module to produce
 *  QA histograms for tracks and more in jets
 */
/// ===========================================================================

#define TRKSINJETQAINCLUSIVEFILLER_CC

// submodule definition
#include "TrksInJetQAInclusiveFiller.h"

// inherited public methods ===================================================

// ----------------------------------------------------------------------------
//! Run fill routines for relevant histograms
// ----------------------------------------------------------------------------
void TrksInJetQAInclusiveFiller::Fill(PHCompositeNode* topNode)
{
  GetNodes(topNode);

  if (m_config.doHitQA)
  {
    FillHitQAHists();
  }
  if (m_config.doClustQA)
  {
    FillClustQAHists();
  }
  if (m_config.doTrackQA)
  {
    FillTrackQAHists();
  }
  if (m_config.doJetQA)
  {
    FillJetQAHists();
  }
}  // end 'Fill(PHCompositeNode* topNode)'

// private methods ============================================================

// ----------------------------------------------------------------------------
//! Fill histograms for inclusive tracker hits
// ----------------------------------------------------------------------------
void TrksInJetQAInclusiveFiller::FillHitQAHists()
{
  // loop over hit sets
  TrkrHitSetContainer::ConstRange hitSets = m_hitMap->getHitSets();
  for (
      TrkrHitSetContainer::ConstIterator itSet = hitSets.first;
      itSet != hitSets.second;
      ++itSet)
  {
    // grab hit set
    TrkrDefs::hitsetkey setKey = itSet->first;
    TrkrHitSet* set = itSet->second;

    // loop over all hits in hit set
    TrkrHitSet::ConstRange hits = set->getHits();
    for (
        TrkrHitSet::ConstIterator itHit = hits.first;
        itHit != hits.second;
        ++itHit)
    {
      // grab hit
      TrkrDefs::hitkey hitKey = itHit->first;
      TrkrHit* hit = itHit->second;

      // grab info and fill histograms
      m_hitManager->GetInfo(hit, setKey, hitKey);

    }  // end hit loop
  }  // end hit set loop
}  // end 'FillHitQAHists()'

// ----------------------------------------------------------------------------
//! Fill histograms for inclusive track clusters
// ----------------------------------------------------------------------------
void TrksInJetQAInclusiveFiller::FillClustQAHists()
{
  // loop over hit sets
  for (const auto& det : {TrkrDefs::TrkrId::mvtxId, TrkrDefs::TrkrId::inttId,
                  TrkrDefs::TrkrId::tpcId, TrkrDefs::TrkrId::micromegasId})
  {
    for (const auto& hitsetkey : m_clustMap->getHitSetKeys(det))
    {
      // loop over clusters associated w/ hit set
      TrkrClusterContainer::ConstRange clusters = m_clustMap->getClusters(hitsetkey);
      for (
          TrkrClusterContainer::ConstIterator itClust = clusters.first;
          itClust != clusters.second;
          ++itClust)
      {
        // grab cluster
        TrkrDefs::cluskey clustKey = itClust->first;
        TrkrCluster* cluster = m_clustMap->findCluster(clustKey);

        // grab cluster info
        m_clustManager->GetInfo(cluster, clustKey, m_actsGeom);
      }  // end cluster loop
    } // end hit set loop
  }
}  // end 'FillClustQAHists()'

// ----------------------------------------------------------------------------
//! Fill histograms for inclusive tracks
// ----------------------------------------------------------------------------
void TrksInJetQAInclusiveFiller::FillTrackQAHists()
{
  // loop over tracks
  for (auto& itTrk : *m_trkMap)
  {
    // grab track
    SvtxTrack* track = itTrk.second;

    // grab info and fill histograms
    m_trackManager->GetInfo(track);

  }  // end track loop
}  // end 'FillTrackQAHists()'

// ----------------------------------------------------------------------------
//! Fill histograms for inclusive jets
// ----------------------------------------------------------------------------
void TrksInJetQAInclusiveFiller::FillJetQAHists()
{
  // loop over jets
  for (
      uint64_t iJet = 0;
      iJet < m_jetMap->size();
      ++iJet)
  {
    // grab jet
    Jet* jet = m_jetMap->get_jet(iJet);

    // grab info and fill histograms
    m_jetManager->GetInfo(jet);

  }  // end jet loop
}  // end 'FillJetQAHists()'

// end ========================================================================
