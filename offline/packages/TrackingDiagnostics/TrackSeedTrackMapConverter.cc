
#include "TrackSeedTrackMapConverter.h"

#include <trackbase/ActsGeometry.h>
#include <trackbase/TrackFitUtils.h>
#include <trackbase/TrkrCluster.h>
#include <trackbase/TrkrClusterContainer.h>

#include <trackbase_historic/SvtxTrackMap_v1.h>
#include <trackbase_historic/SvtxTrack_v4.h>
#include <trackbase_historic/SvtxTrackState_v2.h>
#include <trackbase_historic/TrackSeed.h>
#include <trackbase_historic/TrackSeedContainer.h>
#include <trackbase_historic/TrackSeedHelper.h>

#include <fun4all/Fun4AllReturnCodes.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNode.h>  // for PHNode
#include <phool/PHNodeIterator.h>
#include <phool/PHObject.h>  // for PHObject
#include <phool/getClass.h>
#include <phool/phool.h>

#include <cmath>
namespace
{
  template <class T>
  inline T square(const T& x)
  {
    return x * x;
  }
}  // namespace
//____________________________________________________________________________..
TrackSeedTrackMapConverter::TrackSeedTrackMapConverter(const std::string& name)
  : SubsysReco(name)
{
}

//____________________________________________________________________________..
int TrackSeedTrackMapConverter::InitRun(PHCompositeNode* topNode)
{
  int ret = getNodes(topNode);
  std::istringstream stringline(m_fieldMap);
  stringline >> fieldstrength;
  if (!stringline.fail())  // it is a float
  {
    m_ConstField = true;
    if(std::stof(m_fieldMap) < 0.02)
    {
      m_zeroField = true;
    }
  }
  return ret;
}

//____________________________________________________________________________..
int TrackSeedTrackMapConverter::process_event(PHCompositeNode* /*unused*/)
{
  if (Verbosity() > 1)
  {
    std::cout << "silicon seed map size " << m_siContainer->size() << std::endl;
    for (auto iter = m_siContainer->begin(); iter != m_siContainer->end();
         ++iter)
    {
      auto seed = *iter;
      if (!seed)
      {
        std::cout << "no seed at index " << m_siContainer->index(iter)
                  << std::endl;
        continue;
      }
      seed->identify();
    }
    if (m_tpcContainer)
    {
      std::cout << "tpc seed map size " << m_tpcContainer->size() << std::endl;
      for (auto iter = m_tpcContainer->begin(); iter != m_tpcContainer->end();
           ++iter)
      {
        auto seed = *iter;
        if (!seed)
        {
          std::cout << "no tpc seed at entry " << m_tpcContainer->index(iter)
                    << std::endl;
          continue;
        }
        seed->identify();
      }
    }
  }

  /// Start with a fresh track map in case running iterative tracking
  m_trackMap->Reset();

  unsigned int trackid = 0;
  for (const auto& trackSeed : *m_seedContainer)
  {
    /// If the seed was removed, skip it
    if (!trackSeed)
    {
      continue;
    }

    if (m_trackSeedName.find("SvtxTrackSeed") != std::string::npos)
    {
      /// Catches entries in the vector removed by ghost finder
      unsigned int tpcindex = trackSeed->get_tpc_seed_index();
      TrackSeed* seed = m_tpcContainer->get(tpcindex);
      if (!seed)
      {
        continue;
      }
    }

    auto svtxtrack = std::make_unique<SvtxTrack_v4>();

    if (Verbosity() > 0)
    {
      std::cout << "iterating track seed " << trackid << std::endl;
    }

    svtxtrack->set_id(trackid);
    trackid++;
    /// If we've run the track matching
    if (m_trackSeedName.find("SvtxTrackSeed") != std::string::npos)
    {
      if (Verbosity() > 0)
      {
        std::cout << "tpc seed id " << trackSeed->get_tpc_seed_index() << std::endl;
        std::cout << "si seed id " << trackSeed->get_silicon_seed_index() << std::endl;
        std::cout << "crossing_estimate " << trackSeed->get_crossing_estimate() << std::endl;
      }

      unsigned int seedindex = trackSeed->get_tpc_seed_index();
      TrackSeed* tpcseed = m_tpcContainer->get(seedindex);
      if (!m_cosmics)
      {
        if (trackSeed->get_silicon_seed_index() == std::numeric_limits<unsigned int>::max())
        {
          /// Didn't find a match, so just use the tpc seed
          const auto position = TrackSeedHelper::get_xyz(tpcseed);
          svtxtrack->set_x(position.x());
          svtxtrack->set_y(position.y());
          svtxtrack->set_z(position.z());
          svtxtrack->set_crossing(0);
        }
        else
        {
          TrackSeed* siseed = m_siContainer->get(trackSeed->get_silicon_seed_index());
          const auto position = TrackSeedHelper::get_xyz(siseed);
          svtxtrack->set_x(position.x());
          svtxtrack->set_y(position.y());
          svtxtrack->set_z(position.z());
          svtxtrack->set_crossing(siseed->get_crossing());

          svtxtrack->set_silicon_seed(siseed);
        }
        svtxtrack->set_charge(tpcseed->get_qOverR() > 0 ? 1 : -1);
        if (!m_ConstField)
        {
          svtxtrack->set_px(tpcseed->get_pt() * std::cos(tpcseed->get_phi()));
          svtxtrack->set_py(tpcseed->get_pt() * std::sin(tpcseed->get_phi()));
          svtxtrack->set_pz(tpcseed->get_pz());
        }
        else
        {
          float pt = fabs(1. / tpcseed->get_qOverR()) * (0.3 / 100) * fieldstrength;
          float phi = tpcseed->get_phi();
          svtxtrack->set_px(pt * std::cos(phi));
          svtxtrack->set_py(pt * std::sin(phi));
          svtxtrack->set_pz(pt * std::cosh(tpcseed->get_eta()) * std::cos(tpcseed->get_theta()));
        }
      }
      else
      {
        unsigned int silseedindex = trackSeed->get_silicon_seed_index();
        TrackSeed* silseed = m_siContainer->get(silseedindex);

        // get positions from cluster keys
        // TODO: should implement distortions
        TrackSeedHelper::position_map_t positions;
        for( auto key_iter = tpcseed->begin_cluster_keys(); key_iter != tpcseed->end_cluster_keys(); ++key_iter )
        {
          const auto& key(*key_iter);
          positions.emplace(key, m_tGeometry->getGlobalPosition( key, m_clusters->findCluster(key)));
        }

        // perform circle fit
        TrackSeedHelper::circleFitByTaubin(tpcseed, positions, 0, 58);

        float tpcR = fabs(1. / tpcseed->get_qOverR());
        float tpcx = tpcseed->get_X0();
        float tpcy = tpcseed->get_Y0();
        float vertexradius = 80;
        const auto intersect = TrackFitUtils::circle_circle_intersection(vertexradius, tpcR, tpcx, tpcy);
        float intx, inty;

        if (std::get<1>(intersect) < std::get<3>(intersect))
        {
          intx = std::get<0>(intersect);
          inty = std::get<1>(intersect);
        }
        else
        {
          intx = std::get<2>(intersect);
          inty = std::get<3>(intersect);
        }
        float slope = tpcseed->get_slope();

        float intz = vertexradius * slope + tpcseed->get_Z0();

        Acts::Vector3 inter(intx, inty, intz);

        std::vector<float> tpcparams{tpcR, tpcx, tpcy, tpcseed->get_slope(),
                                     tpcseed->get_Z0()};
        auto tangent = TrackFitUtils::get_helix_tangent(tpcparams,
                                                        inter);

        auto tan = tangent.second;
        auto pca = tangent.first;

        svtxtrack->set_x(pca.x());
        svtxtrack->set_y(pca.y());

        float p;
        if (m_ConstField)
        {
          p = std::cosh(tpcseed->get_eta()) * fabs(1. / tpcseed->get_qOverR()) * (0.3 / 100) * fieldstrength;
        }
        else
        {
          p = tpcseed->get_p();
        }

        tan *= p;
        auto [charge, cosmicslope] = getCosmicCharge(tpcseed, vertexradius);

        svtxtrack->set_px(charge < 0 ? tan.x() : tan.x() * -1);
        svtxtrack->set_py(charge < 0 ? tan.y() : tan.y() * -1);
        svtxtrack->set_pz(cosmicslope > 0 ? fabs(tan.z()) : -1 * fabs(tan.z()));
        svtxtrack->set_z(svtxtrack->get_pz() > 0 ? (slope < 0 ? intz : vertexradius * slope * -1 + tpcseed->get_Z0()) : (slope > 0 ? intz : vertexradius * slope * -1 + tpcseed->get_Z0()));
        svtxtrack->set_charge(charge);
        addKeys(svtxtrack, tpcseed);
        if (silseed)
        {
          addKeys(svtxtrack, silseed);
        }

        svtxtrack->set_tpc_seed(tpcseed);
        svtxtrack->set_silicon_seed(silseed);
        if (Verbosity() > 5)
        {
          svtxtrack->identify();
        }
      }
      svtxtrack->set_tpc_seed(tpcseed);
    }

    else
    {
      /// Otherwise we are using an individual subdetectors container
      svtxtrack->set_id(m_seedContainer->find(trackSeed));

      const auto position = TrackSeedHelper::get_xyz(trackSeed);
      svtxtrack->set_x(position.x());
      svtxtrack->set_y(position.y());
      if(m_zeroField)
      {
        // replace x and y with a line fit instead of helix fit
        lineFit(svtxtrack.get(), trackSeed);
      }
      svtxtrack->set_z(position.z());
      svtxtrack->set_charge(trackSeed->get_qOverR() > 0 ? 1 : -1);
      if (m_ConstField)
      {
        float pt = fabs(1. / trackSeed->get_qOverR()) * (0.3 / 100) * fieldstrength;

        float phi = trackSeed->get_phi();
        svtxtrack->set_px(pt * std::cos(phi));
        svtxtrack->set_py(pt * std::sin(phi));
        svtxtrack->set_pz(pt * std::cosh(trackSeed->get_eta()) * std::cos(trackSeed->get_theta()));
      }
      else
      {

        svtxtrack->set_px(trackSeed->get_pt() * std::cos(trackSeed->get_phi()));
        svtxtrack->set_py(trackSeed->get_pt() * std::sin(trackSeed->get_phi()));
        svtxtrack->set_pz(trackSeed->get_pz());
      }

      // calculate chisq and ndf
      float R = 1. / std::fabs(trackSeed->get_qOverR());
      float X0 = trackSeed->get_X0();
      float Y0 = trackSeed->get_Y0();
      float Z0 = trackSeed->get_Z0();
      float slope = trackSeed->get_slope();

      std::vector<float> fitpars = {R, X0, Y0, slope, Z0};

      svtxtrack->set_crossing(trackSeed->get_crossing());

      SvtxTrackState_v2 initial_state(0.);
      initial_state.set_x(svtxtrack->get_x());
      initial_state.set_y(svtxtrack->get_y());
      initial_state.set_z(svtxtrack->get_z());
      initial_state.set_px(svtxtrack->get_px());
      initial_state.set_py(svtxtrack->get_py());
      initial_state.set_pz(svtxtrack->get_pz());
      svtxtrack->insert_state(&initial_state);

      // TPC-only tracks don't have crossing info, so we set crossing to 0
      // Silicon-only tracks may have crossing info, so a crossing of SHRT_MAX indicates a genuine ambiguity and we leave it as is
      if (m_trackSeedName.find("TpcTrackSeed") != std::string::npos)
      {
        if(svtxtrack->get_crossing() == SHRT_MAX)
        {
          svtxtrack->set_crossing(0);
        }
      }

      std::vector<double> xy_error2;
      std::vector<double> rz_error2;
      std::vector<double> xy_residuals;
      std::vector<double> rz_residuals;
      std::vector<double> x_circle;
      std::vector<double> y_circle;
      std::vector<double> z_line;

      // TrackFitUtils::get_helix_tangent sometimes returns a tangent vector in the opposite direction that we would want
      // To guard against this, we keep track of the previous momentum vector and un-flip the current momentum vector if it flips
      float last_px = initial_state.get_px();
      float last_py = initial_state.get_py();
      float last_pz = initial_state.get_pz();

      for (auto c_iter = trackSeed->begin_cluster_keys();
           c_iter != trackSeed->end_cluster_keys();
           ++c_iter)
      {
        TrkrDefs::cluskey key = *c_iter;
        TrkrCluster* c = m_clusters->findCluster(key);
        Acts::Vector3 pos = m_tGeometry->getGlobalPosition(key, c);

        auto surf = m_tGeometry->maps().getSurface(key,c);
        Acts::Vector3 surface_center = surf->center(m_tGeometry->geometry().getGeoContext()) * 0.1; // to cm
        Acts::Vector3 intersection = TrackFitUtils::get_helix_surface_intersection(surf,fitpars,pos,m_tGeometry);
        float pathlength = TrackFitUtils::get_helix_pathlength(fitpars,position,intersection);
        std::pair<Acts::Vector3,Acts::Vector3> tangent = TrackFitUtils::get_helix_tangent(fitpars,surface_center);

        SvtxTrackState_v2 state(pathlength);
        state.set_x(intersection.x());
        state.set_y(intersection.y());
        state.set_z(intersection.z());
        float p = trackSeed->get_p();
        float new_px = p*tangent.second.x();
        float new_py = p*tangent.second.y();
        float new_pz = p*tangent.second.z();
        if((last_px*new_px+last_py*new_py+last_pz*new_pz)<0.)
        {
          new_px = -new_px;
          new_py = -new_py;
          new_pz = -new_pz;
        }
        state.set_px(new_px);
        state.set_py(new_py);
        state.set_pz(new_pz);
        state.set_cluskey(key);
        svtxtrack->insert_state(&state);

        last_px = new_px;
        last_py = new_py;
        last_pz = new_pz;

        double x = pos(0);
        double y = pos(1);
        double z = pos(2);
        double r = sqrt(x*x+y*y);
        double dx = x - intersection.x();
        double dy = y - intersection.y();
        double dz = z - intersection.z();
        double dr = r - sqrt(intersection.x()*intersection.x()+intersection.y()*intersection.y());
        xy_residuals.push_back(sqrt(dx*dx+dy*dy));
        rz_residuals.push_back(sqrt(dr*dr+dz*dz));

        // ignoring covariance for simplicity
        xy_error2.push_back(c->getRPhiError()*c->getRPhiError());
        rz_error2.push_back(c->getZError()*c->getZError());
        double phi = atan2(dy, dx);
        x_circle.push_back(R * cos(phi) + X0);
        y_circle.push_back(R * sin(phi) + Y0);
        z_line.push_back(R * slope + Z0);
      }
      double chi2 = 0.;
      for (unsigned int i = 0; i < xy_residuals.size(); i++)
      {
        // method lifted from GPUTPCTrackParam::Filter
        chi2 += xy_residuals[i] * xy_residuals[i] / xy_error2[i] + rz_residuals[i] * rz_residuals[i] / rz_error2[i];
      }
      svtxtrack->set_chisq(chi2);
      // GPUTPCTrackParam initially sets NDF to -3 on first cluster and increments by 2 with every application of filter
      svtxtrack->set_ndf(2 * xy_residuals.size() - 5);

      addKeys(svtxtrack, trackSeed);
      if (m_trackSeedName.find("SiliconTrackSeed") != std::string::npos)
      {
        svtxtrack->set_silicon_seed(trackSeed);
        svtxtrack->set_tpc_seed(nullptr);
      }
      else if (m_trackSeedName.find("TpcTrackSeed") != std::string::npos)
      {
        svtxtrack->set_tpc_seed(trackSeed);
        svtxtrack->set_silicon_seed(nullptr);
      }
    }

    if (Verbosity() > 0)
    {
      std::cout << "Inserting svtxtrack into map " << std::endl;
      svtxtrack->identify();
    }

    m_trackMap->insert(svtxtrack.get());
  }

  return Fun4AllReturnCodes::EVENT_OK;
}
void TrackSeedTrackMapConverter::lineFit(SvtxTrack_v4* track, TrackSeed* seed)
{
  auto firstclusterckey = *(seed->begin_cluster_keys());
  auto cluster = m_clusters->findCluster(firstclusterckey);
  auto glob = m_tGeometry->getGlobalPosition(firstclusterckey, cluster);
  float phi = seed->get_phi();
  float theta = seed->get_theta();
  Acts::Vector3 mom(std::cos(phi) * std::sin(theta),
                    std::sin(phi) * std::sin(theta), std::cos(theta));

  auto pca = TrackFitUtils::getPCALinePoint(Acts::Vector3::Zero(), mom, glob);
  track->set_x(pca.x());
  track->set_y(pca.y());
}
//____________________________________________________________________________..
int TrackSeedTrackMapConverter::End(PHCompositeNode* /*unused*/)
{
  return Fun4AllReturnCodes::EVENT_OK;
}
void TrackSeedTrackMapConverter::addKeys(TrackSeed* seedToAddTo,
                                         TrackSeed* seedToAdd)
{
  for (auto iter = seedToAdd->begin_cluster_keys();
       iter != seedToAdd->end_cluster_keys();
       ++iter)
  {
    seedToAddTo->insert_cluster_key(*iter);
  }
}
void TrackSeedTrackMapConverter::addKeys(std::unique_ptr<SvtxTrack_v4>& track, TrackSeed* seed)
{
  for (TrackSeed::ConstClusterKeyIter iter = seed->begin_cluster_keys();
       iter != seed->end_cluster_keys();
       ++iter)
  {
    track->insert_cluster_key(*iter);
  }
}

int TrackSeedTrackMapConverter::getNodes(PHCompositeNode* topNode)
{
  m_trackMap = findNode::getClass<SvtxTrackMap>(topNode, "SvtxTrackMap");
  if (!m_trackMap)
  {
    // create it
    PHNodeIterator iter(topNode);

    PHCompositeNode* dstNode = static_cast<PHCompositeNode*>(iter.findFirst(
        "PHCompositeNode", "DST"));
    if (!dstNode)
    {
      std::cerr << PHWHERE << "DST Node missing, doing nothing." << std::endl;
      return Fun4AllReturnCodes::ABORTEVENT;
    }
    PHNodeIterator iter_dst(dstNode);

    // Create the SVTX node
    PHCompositeNode* tb_node =
        dynamic_cast<PHCompositeNode*>(iter_dst.findFirst("PHCompositeNode",
                                                          "SVTX"));
    if (!tb_node)
    {
      tb_node = new PHCompositeNode("SVTX");
      dstNode->addNode(tb_node);
      if (Verbosity() > 0)
      {
        std::cout << PHWHERE << "SVTX node added" << std::endl;
      }
    }
    m_trackMap = new SvtxTrackMap_v1;
    PHIODataNode<PHObject>* tracks_node =
        new PHIODataNode<PHObject>(m_trackMap, "SvtxTrackMap", "PHObject");
    tb_node->addNode(tracks_node);
  }

  if(m_trackMapName != "SvtxTrackMap")
  {
  m_trackMap = findNode::getClass<SvtxTrackMap>(topNode, m_trackMapName);
  if (!m_trackMap)
  {
    PHNodeIterator iter(topNode);

    PHCompositeNode* dstNode = static_cast<PHCompositeNode*>(iter.findFirst(
        "PHCompositeNode", "DST"));
    if (!dstNode)
    {
      std::cerr << PHWHERE << "DST Node missing, doing nothing." << std::endl;
      return Fun4AllReturnCodes::ABORTEVENT;
    }
    PHNodeIterator iter_dst(dstNode);

    // Create the SVTX node
    PHCompositeNode* tb_node =
        dynamic_cast<PHCompositeNode*>(iter_dst.findFirst("PHCompositeNode",
                                                          "SVTX"));
    if (!tb_node)
    {
      tb_node = new PHCompositeNode("SVTX");
      dstNode->addNode(tb_node);
      if (Verbosity() > 0)
      {
        std::cout << PHWHERE << "SVTX node added" << std::endl;
      }
    }

    m_trackMap = new SvtxTrackMap_v1;
    PHIODataNode<PHObject>* tracks_node =
        new PHIODataNode<PHObject>(m_trackMap, m_trackMapName, "PHObject");
    tb_node->addNode(tracks_node);
    if (Verbosity() > 0)
    {
      std::cout << PHWHERE << "Svtx/" << m_trackMapName << " node added" << std::endl;
    }
  }
  }
  m_seedContainer = findNode::getClass<TrackSeedContainer>(topNode, m_trackSeedName);
  if (!m_seedContainer)
  {
    std::cout << PHWHERE << " Can't find track seed container " << m_trackSeedName << ", can't continue."
              << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  m_tpcContainer = findNode::getClass<TrackSeedContainer>(topNode, "TpcTrackSeedContainer");
  if (!m_tpcContainer)
  {
    std::cout << PHWHERE << "WARNING, TrackSeedTrackMapConverter may seg fault depending on what seeding algorithm this is run after" << std::endl;
  }

  m_siContainer = findNode::getClass<TrackSeedContainer>(topNode, "SiliconTrackSeedContainer");
  if (!m_siContainer)
  {
    std::cout << PHWHERE << "WARNING, TrackSeedTrackMapConverter may seg fault depending on what seeding algorithm this is run after" << std::endl;
  }

  m_clusters = findNode::getClass<TrkrClusterContainer>(topNode, "TRKR_CLUSTER");
  if (!m_clusters)
  {
    std::cout << PHWHERE << " Can't find cluster container, can't continue."
              << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  m_tGeometry = findNode::getClass<ActsGeometry>(topNode, "ActsGeometry");
  if (!m_tGeometry)
  {
    std::cout << PHWHERE << " Can't find ActsGeometry, can't continue."
              << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

std::pair<int, float> TrackSeedTrackMapConverter::getCosmicCharge(TrackSeed* seed,
                                                                  float vertexradius) const
{
  Acts::Vector3 globalMostOuter = Acts::Vector3::Zero();
  Acts::Vector3 globalSecondMostOuter(0, 999999, 0);
  std::vector<Acts::Vector3> globpos;
  float largestR = 0;
  for (auto it = seed->begin_cluster_keys();
       it != seed->end_cluster_keys();
       ++it)
  {
    auto ckey = *it;
    auto clus = m_clusters->findCluster(ckey);
    auto glob = m_tGeometry->getGlobalPosition(ckey, clus);
    globpos.push_back(glob);
    float r = std::sqrt(square(glob.x()) + square(glob.y()));
    if (r > largestR && glob.y() < 0)
    {
      globalMostOuter = glob;
      largestR = r;
    }
  }

  float maxdr = std::numeric_limits<float>::max();
  for (auto& glob : globpos)
  {
    if (glob.y() > 0)
    {
      continue;
    }
    float dr = std::sqrt(square(globalMostOuter.x()) + square(globalMostOuter.y())) - std::sqrt(square(glob.x()) + square(glob.y()));
    if (dr < maxdr && dr > 10)
    {
      maxdr = dr;
      globalSecondMostOuter = glob;
    }
  }

  Acts::Vector3 vertex(0, -1 * vertexradius, 0);
  globalMostOuter -= vertex;
  globalSecondMostOuter -= vertex;

  const auto firstphi = atan2(globalMostOuter.y(), globalMostOuter.x());
  const auto secondphi = atan2(globalSecondMostOuter.y(),
                               globalSecondMostOuter.x());
  auto dphi = secondphi - firstphi;
  if (dphi > M_PI)
  {
    dphi = 2. * M_PI - dphi;
  }
  if (dphi < -M_PI)
  {
    dphi = 2. * M_PI + dphi;
  }

  float r1 = std::sqrt(square(globalMostOuter.x()) + square(globalMostOuter.y()));
  float r2 = std::sqrt(square(globalSecondMostOuter.x()) + square(globalSecondMostOuter.y()));
  float z1 = globalMostOuter.z();
  float z2 = globalSecondMostOuter.z();

  float slope = (r2 - r1) / (z2 - z1);
  int charge = 1;

  if (dphi > 0)
  {
    charge = -1;
  }

  return std::make_pair(charge, slope);
  ;
}
