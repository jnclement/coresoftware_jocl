
#ifndef TRACKRECO_PHACTSTRACKS_H
#define TRACKRECO_PHACTSTRACKS_H


#include <fun4all/SubsysReco.h>
#include <trackbase/TrkrDefs.h>

/// Acts includes to create all necessary definitions
#include <Acts/Utilities/Definitions.hpp>
#include <Acts/Utilities/BinnedArray.hpp>                    
#include <Acts/Utilities/Logger.hpp>   
#include <ACTFW/EventData/TrkrClusterSourceLink.hpp>

#include <ACTFW/EventData/Track.hpp>
#include <Acts/EventData/TrackParameters.hpp>

#include <string>
#include <map>
#include <vector>

class PHCompositeNode;
class SvtxTrackMap;
class SvtxTrack;

using SourceLink = FW::Data::TrkrClusterSourceLink;

/**
 * A struct that contains an Acts track seed and the corresponding source links,
 * to be put on the node tree by this module.
 * Need to use a struct instead of a std::map because the Acts classes do not 
 * have defined operators for "<", which means they can't be used as keys
 * according to stl libraries
 */
struct ActsTrack
{
  /// Default constructor. There is not a no argument constructor
  /// as source links don't have a no argument constructor
ActsTrack(FW::TrackParameters seed, std::vector<SourceLink> links) :
  trackSeed(seed), sourceLinks(links) {}

  /// The acts track parameters for this track
  FW::TrackParameters trackSeed;
  
  /// The corresponding source links that are associated to the above track
  std::vector<SourceLink> sourceLinks;
};



/**
 * This class is responsible for taking SvtxTracks and converting them to track
 * seeds that Acts can take in to the track fitter. It collects SvtxTracks, 
 * converts them to Acts tracks, and then finds the corresponding
 * TrkrClusterSourceLinks to that SvtxTrack. The output is a node on the node
 * tree that is a map of Acts track seeds and corresponding source links
 */
class PHActsTracks : public SubsysReco
{

 public:
  /// Default constructor and destructor
  PHActsTracks(const std::string& name = "PHActsTracks");
  virtual ~PHActsTracks() {}

  /// Inherited SubsysReco functions
  int End(PHCompositeNode *topNode);
  int Init(PHCompositeNode *topNode);
  int InitRun(PHCompositeNode *topNode);
  int process_event(PHCompositeNode *topNode);
  
  
 private:
  
  /** 
   * Member functions
   */

  /// Create track seed node if it doesn't exist yet
  void createNodes(PHCompositeNode *topNode);
  
  /// Get nodes off node tree needed to execute module
  int getNodes(PHCompositeNode *topNode);

  Acts::BoundSymMatrix getActsCovMatrix(const SvtxTrack *track);

  /**
   * Member variables
   */

  /// A vector to hold the source links corresponding to a particular SvtxTrack
  std::vector<SourceLink> m_trackSourceLinks;

  /// A map of an Acts track seed and Acts-sPHENIX source links corresponding
  /// to that track seed
  std::vector<ActsTrack> *m_actsProtoTracks;

  /// Trackmap that contains SvtxTracks
  SvtxTrackMap *m_trackMap;

  /// Map between cluster key and arbitrary hit id created in PHActsSourceLinks
  std::map<TrkrDefs::cluskey, unsigned int> *m_hitIdClusKey;

  /// Map of hitid:SourceLinks created in PHActsSourceLinks
  std::map<unsigned int, SourceLink> *m_sourceLinks;

};



#endif
