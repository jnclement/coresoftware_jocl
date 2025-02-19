#include "PHSiliconHelicalPropagator.h"

#include <trackbase/TrkrClusterContainer.h>
#include <trackbase_historic/SvtxTrackSeed_v1.h>
#include <trackbase_historic/TrackSeedContainer_v1.h>
#include <trackbase_historic/TrackSeed_v2.h>
#include <trackbase_historic/TrackSeedHelper.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <phool/PHCompositeNode.h>
#include <phool/getClass.h>

namespace
{
  template <typename T>
  int sgn(const T& x)
  {
    if (x > 0)
    {
      return 1;
    }
    else
    {
      return -1;
    }
  }
}  // namespace

PHSiliconHelicalPropagator::PHSiliconHelicalPropagator(const std::string& name)
  : SubsysReco(name)
{
}

PHSiliconHelicalPropagator::~PHSiliconHelicalPropagator() = default;

int PHSiliconHelicalPropagator::InitRun(PHCompositeNode* topNode)
{
  _cluster_map = findNode::getClass<TrkrClusterContainer>(topNode, "TRKR_CLUSTER");
  if (!_cluster_map)
  {
    std::cerr << PHWHERE << " ERROR: Can't find node TRKR_CLUSTER" << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }
  _cluster_crossing_map = findNode::getClass<TrkrClusterCrossingAssoc>(topNode, "TRKR_CLUSTERCROSSINGASSOC");
  if (!_cluster_crossing_map)
  {
    std::cerr << PHWHERE << " ERROR: Can't find TRKR_CLUSTERCROSSINGASSOC " << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }
  _tgeometry = findNode::getClass<ActsGeometry>(topNode, "ActsGeometry");
  if (!_tgeometry)
  {
    std::cout << "No Acts tracking geometry, exiting." << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }
  _tpc_seeds = findNode::getClass<TrackSeedContainer>(topNode, "TpcTrackSeedContainer");
  if (!_tpc_seeds)
  {
    std::cout << "No TpcTrackSeedContainer, exiting." << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }
  _si_seeds = findNode::getClass<TrackSeedContainer>(topNode, "SiliconTrackSeedContainer");
  if (!_si_seeds)
  {
    std::cout << "No SiliconTrackSeedContainer, creating..." << std::endl;
    if (createSeedContainer(_si_seeds, "SiliconTrackSeedContainer", topNode) != Fun4AllReturnCodes::EVENT_OK)
    {
      std::cout << "Cannot create, exiting." << std::endl;
      return Fun4AllReturnCodes::ABORTEVENT;
    }
  }
  _svtx_seeds = findNode::getClass<TrackSeedContainer>(topNode, _track_map_name);
  if (!_svtx_seeds)
  {
    std::cout << "No " << _track_map_name << " found, creating..." << std::endl;
    if (createSeedContainer(_svtx_seeds, _track_map_name, topNode) != Fun4AllReturnCodes::EVENT_OK)
    {
      std::cout << "Cannot create, exiting." << std::endl;
      return Fun4AllReturnCodes::ABORTEVENT;
    }
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

int PHSiliconHelicalPropagator::createSeedContainer(TrackSeedContainer*& container, const std::string& container_name, PHCompositeNode* topNode)
{
  PHNodeIterator iter(topNode);

  PHCompositeNode* dstNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST"));

  if (!dstNode)
  {
    std::cerr << "DST node is missing, quitting" << std::endl;
    throw std::runtime_error("Failed to find DST node in PHSiliconHelicalPropagator::createNodes");
  }

  PHNodeIterator dstIter(dstNode);
  PHCompositeNode* svtxNode = dynamic_cast<PHCompositeNode*>(dstIter.findFirst("PHCompositeNode", "SVTX"));

  if (!svtxNode)
  {
    svtxNode = new PHCompositeNode("SVTX");
    dstNode->addNode(svtxNode);
  }

  container = findNode::getClass<TrackSeedContainer>(topNode, container_name);
  if (!container)
  {
    container = new TrackSeedContainer_v1;
    PHIODataNode<PHObject>* trackNode = new PHIODataNode<PHObject>(container, container_name, "PHObject");
    svtxNode->addNode(trackNode);
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int PHSiliconHelicalPropagator::process_event(PHCompositeNode* /*topNode*/)
{
  for (unsigned int seedID = 0; seedID < _tpc_seeds->size(); ++seedID)
  {
    TrackSeed* tpcseed = _tpc_seeds->get(seedID);
    if (!tpcseed)
    {
      continue;
    }
  if(Verbosity() > 2)
  {
    tpcseed->identify();
  }
    std::vector<Acts::Vector3> clusterPositions;
    std::vector<TrkrDefs::cluskey> clusterKeys;
    for (auto iter = tpcseed->begin_cluster_keys();
         iter != tpcseed->end_cluster_keys(); ++iter)
    {
      clusterKeys.push_back(*iter);
    }
    TrackFitUtils::getTrackletClusters(_tgeometry, _cluster_map, clusterPositions, clusterKeys);
    std::vector<float> fitparams = TrackFitUtils::fitClusters(clusterPositions, clusterKeys);
    //! There weren't enough clusters to fit
    if (fitparams.size() == 0)
    {
      continue;
    }
    if(Verbosity() > 3)
    {
      for(auto& param : fitparams)
      {
        std::cout << "fit param " << param << std::endl;
      }
    }
    std::vector<TrkrDefs::cluskey> si_clusterKeys, si_clusterKeysrz;
    std::vector<Acts::Vector3> si_clusterPositions, si_clusterPositionsrz;
    std::map<TrkrDefs::cluskey, Acts::Vector3> positionMap;

    unsigned int nSiClusters = std::numeric_limits<unsigned int>::quiet_NaN();
    TrackFitUtils::position_vector_t xypoints, rzpoints;
    for (auto& pos : clusterPositions)
    {
      xypoints.push_back({pos.x(), pos.y()});
      float clusr = std::sqrt(pos.x() * pos.x() + pos.y() * pos.y());
      if (pos.y() < 0)
      {
        clusr = -clusr;
      }
      rzpoints.push_back({pos.z(), clusr});

      if (Verbosity() > 5)
      {
        std::cout << "Cluster pos " << pos.transpose() << " and r " << std::sqrt(pos.x() * pos.x() + pos.y() * pos.y()) << std::endl;
      }
    }
    auto rzparams = TrackFitUtils::line_fit(rzpoints);

    if (m_zeroField)
    {

     auto xyparams = TrackFitUtils::line_fit(xypoints);
     nSiClusters = TrackFitUtils::addClustersOnLine(xyparams,
                                                    true,
                                                    _dca_cut,
                                                    _tgeometry, _cluster_map,
                                                    si_clusterPositions,
                                                    si_clusterKeys,
                                                    0, 6);
   }
   else{
    nSiClusters = TrackFitUtils::addClusters(fitparams, _dca_cut, _tgeometry, _cluster_map, si_clusterPositions, si_clusterKeys, 0, 6);
   }
   int nrzClusters = TrackFitUtils::addClustersOnLine(rzparams,
                                                       false,
                                                       _dca_z_cut,
                                                       _tgeometry,
                                                       _cluster_map,
                                                       si_clusterPositionsrz,
                                                       si_clusterKeysrz,
                                                       0, 6);
    std::vector<TrkrDefs::cluskey> newkeys;
    std::set_intersection(si_clusterKeys.begin(), si_clusterKeys.end(),
                          si_clusterKeysrz.begin(), si_clusterKeysrz.end(),
                          std::back_inserter(newkeys));

    if (newkeys.size() > 0)
    {
      if(Verbosity() > 0)
      {
        std::cout << "Adding " << newkeys.size() << " Keys " << std::endl;
        for(auto& key : newkeys)
        {
          std::cout << "key " << (unsigned int) key << std::endl;
        }
      }
      std::unique_ptr<TrackSeed_v2> si_seed = std::make_unique<TrackSeed_v2>();
      std::map<short, int> crossing_frequency;
      Acts::Vector3 tpcExGlobal = clusterPositions.front();
      for (auto& clusterkey : newkeys)
      {
        //! Check that the clusters are in the same quadrant
        auto cluster = _cluster_map->findCluster(clusterkey);
        auto global = _tgeometry->getGlobalPosition(clusterkey, cluster);
        positionMap.insert({clusterkey, global});
        if (sgn(global.x()) == sgn(tpcExGlobal.x()) && sgn(global.y()) == sgn(tpcExGlobal.y()))
        {
          si_seed->insert_cluster_key(clusterkey);
        }
        /*
        else if (TrkrDefs::getTrkrId(clusterkey) == TrkrDefs::inttId)
        {
          auto hit_crossings = _cluster_crossing_map->getCrossings(clusterkey);
          for (auto iter = hit_crossings.first; iter != hit_crossings.second; ++iter)
          {
            short crossing = iter->second;
            if (crossing_frequency.count(crossing) == 0)
            {
              crossing_frequency.insert({crossing, 1});
            }
            else
            {
              crossing_frequency[crossing]++;
            }
          }


          if (sgn(global.x()) == sgn(tpcExGlobal.x()) && sgn(global.y()) == sgn(tpcExGlobal.y()))
          {
            si_seed->insert_cluster_key(clusterkey);
          }
        }
        */
      }
/*
      if (crossing_frequency.size() > 0)
      {
        short most_common_crossing = (std::max_element(crossing_frequency.begin(), crossing_frequency.end(),
                                                       [](auto entry1, auto entry2)
                                                       { return entry1.second > entry2.second; }))
                                         ->first;
        si_seed->set_crossing(most_common_crossing);
      }
      */
      TrackSeedHelper::circleFitByTaubin(si_seed.get(), positionMap, 0, 8);
      TrackSeedHelper::lineFit(si_seed.get(), positionMap, 0, 8);
      si_seed->set_crossing(0);
      TrackSeed* mapped_seed = _si_seeds->insert(si_seed.get());

      std::unique_ptr<SvtxTrackSeed_v1> full_seed = std::make_unique<SvtxTrackSeed_v1>();
      int tpc_seed_index = _tpc_seeds->find(tpcseed);
      int si_seed_index = _si_seeds->find(mapped_seed);
      if (Verbosity() > 0)
      {
        std::cout << "found  " << nSiClusters << " silicon clusters in xy for tpc seed " << tpc_seed_index << std::endl;
        std::cout << "found " << nrzClusters << " silicon clusters in rz for tpc seed " << tpc_seed_index << std::endl;
        std::cout << "intersection is " << newkeys.size() << std::endl;
        std::cout << "new silicon seed index: " << si_seed_index << std::endl;
      }
      full_seed->set_tpc_seed_index(tpc_seed_index);
      full_seed->set_silicon_seed_index(si_seed_index);
      _svtx_seeds->insert(full_seed.get());
    }
    else
    {
      // no Si clusters found, put TPC-only seed in SvtxTrackSeedContainer
      std::unique_ptr<SvtxTrackSeed_v1> partial_seed = std::make_unique<SvtxTrackSeed_v1>();
      int tpc_seed_index = _tpc_seeds->find(tpcseed);
      partial_seed->set_tpc_seed_index(tpc_seed_index);
      _svtx_seeds->insert(partial_seed.get());
    }
  }
  if (Verbosity() > 2)
  {
    std::cout << "svtx seed map size is " << _svtx_seeds->size() << std::endl;
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

int PHSiliconHelicalPropagator::End(PHCompositeNode* /*topNode*/)
{
  return Fun4AllReturnCodes::EVENT_OK;
}
