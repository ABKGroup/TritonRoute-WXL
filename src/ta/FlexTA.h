/* Authors: Lutong Wang and Bangqi Xu */
/*
 * Copyright (c) 2019, The Regents of the University of California
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FR_FLEXTA_H_
#define _FR_FLEXTA_H_

#include <memory>
//#include "db/tech/frTechObject.h"
#include "frDesign.h"
//#include "db/obj/frGuide.h"
#include "db/obj/frVia.h"
//#include "db/taObj/taTrack.h"
#include "db/taObj/taPin.h"
#include <set>

//#include <boost/icl/interval_map.hpp>
//#include <boost/pool/pool_alloc.hpp>

namespace fr {
  //class FlexIroute {
  //public:
  //  FlexIroute(): guide(), track(), begin(0), end(0), wlen_helper(0) {};
  //  // getters
  //  frCoord getBegin() const {
  //    return begin;
  //  }
  //  frCoord getEnd() const {
  //    return end;
  //  }
  //  frGuide* getGuide() const {
  //    return guide;
  //  }
  //  FlexTrack* getTrack() const {
  //    return track;
  //  }
  //  int getWlenHelper() const {
  //    return wlen_helper;
  //  }
  //  // setters
  //  void setGuide(frGuide* in) {
  //    guide = in;
  //  }
  //  void setTrack(FlexTrack* in) {
  //    track = in;
  //  }
  //  void setBegin(frCoord in) {
  //    begin = in;
  //  }
  //  void setEnd(frCoord in) {
  //    end = in;
  //  }
  //  void setWlenHelper(int in) {
  //    wlen_helper = in;
  //  }
  //protected:
  //  frGuide*   guide;
  //  FlexTrack* track;
  //  frCoord    begin;
  //  frCoord    end;
  //  int        wlen_helper;
  //};


  class FlexTA {
  public:
    // constructors
    //FlexTA(): tech(std::make_shared<frTechObject>()), design(std::make_shared<frDesign>()) {};
    FlexTA(frDesign* in): tech(in->getTech()), design(in) {};
    // getters
    frTechObject* getTech() const {
      return tech;
    }
    frDesign* getDesign() const {
      return design;
    }
    // others
    int main();
  protected:
    frTechObject*   tech;
    frDesign*       design;
    // others
    void main_helper(frLayerNum lNum, int maxOffsetIter, int panelWidth);
    void initTA(int size);
    void searchRepair(int iter, int size, int offset);
    int  initTA_helper(int iter, int size, int offset, bool isH, int &numPanels);
  };


  class FlexTAWorker;
  class FlexTAWorkerRegionQuery {
  public:
    FlexTAWorkerRegionQuery(FlexTAWorker* in): taWorker(in) {}
    frDesign* getDesign() const;
    FlexTAWorker* getTAWorker() const {
      return taWorker;
    }
    
    void add(taPinFig* fig);
    void remove(taPinFig* fig);
    //void query(const frBox &box, frLayerNum layerNum, std::vector<rq_rptr_value_t<taConnFig> > &result);
    void query(const frBox &box, frLayerNum layerNum, std::set<taPin*, frBlockObjectComp> &result);

    void addCost(const frBox &box, frLayerNum layerNum, frBlockObject* obj, frConstraint* con);
    void removeCost(const frBox &box, frLayerNum layerNum, frBlockObject* obj, frConstraint* con);
    void queryCost(const frBox &box, frLayerNum layerNum, std::vector<rq_generic_value_t<std::pair<frBlockObject*, frConstraint*> > > &result);
    
    //void addAP(const frBox &box, frLayerNum layerNum, frBlockObject* obj, frConstraint* con);
    //void removeAP(const frBox &box, frLayerNum layerNum, frBlockObject* obj, frConstraint* con);
    //void queryAP(const frBox &box, frLayerNum layerNum, std::vector<rq_rptr_value_t<frBlockObject> > &result);
    
    void init();
  protected:
    FlexTAWorker* taWorker;
    std::vector<bgi::rtree<rq_rptr_value_t<taPinFig>, 
                           bgi::quadratic<16>/*, 
                           bgi::indexable<rq_rptr_value_t<taPinFig> >,
                           bgi::equal_to<rq_rptr_value_t<taPinFig> >,
                           boost::pool_allocator<rq_rptr_value_t<taPinFig> > */
                          >
               > shapes; // resource map
    // fixed objs, owner:: nullptr or net, con = short 
    std::vector<bgi::rtree<rq_generic_value_t<std::pair<frBlockObject*, frConstraint*> >, 
                           bgi::quadratic<16>/*,
                           bgi::indexable<rq_generic_value_t<std::pair<frBlockObject*, frConstraint*> > >,
                           bgi::equal_to<rq_generic_value_t<std::pair<frBlockObject*, frConstraint*> > >,
                           boost::pool_allocator<rq_generic_value_t<std::pair<frBlockObject*, frConstraint*> > > */
                          > 
               > costs;
    // ap, owner:net
    //std::vector<bgi::rtree<rq_rptr_value_t<frBlockObject>, bgi::quadratic<16> > > aps;
    
    void modCost(taPinFig* fig, bool isAddCost);
  };

  //class FlexTAWorkerSort {
  //public:
  //  FlexTAWorkerSort(std::vector<iroute> *in): ptr(in) {}
  //  bool operator()(const taIroute &lhs, const taIroute &rhs) const {
  //    return ptr
  //protected:
  //  std::vector<iroutes> *ptr;
  //}


  class FlexTAWorker {
  public:
    // constructors
    FlexTAWorker(frDesign* designIn): design(designIn), rq(this), numAssigned(0), totCost(0), maxRetry(1)/*, totDrcCost(0)*/ {};
    // setters
    void setRouteBox(const frBox &boxIn) {
      routeBox.set(boxIn);
    }
    void setExtBox(const frBox &boxIn) {
      extBox.set(boxIn);
    }
    void setDir(frPrefRoutingDirEnum in) {
      dir = in;
    }
    void setTAIter(int in) {
      taIter = in;
    }
    void addIroute(std::unique_ptr<taPin> &in, bool isExt = false) {
      in->setId(iroutes.size() + extIroutes.size());
      if (isExt) {
        extIroutes.push_back(std::move(in));
      } else {
        iroutes.push_back(std::move(in));
      }
    }
    void addToReassignIroutes(taPin* in) {
      reassignIroutes.insert(in);
    }
    void removeFromReassignIroutes(taPin* in) {
      auto it = reassignIroutes.find(in);
      if (it != reassignIroutes.end()) {
        reassignIroutes.erase(it);
      }
    }
    taPin* popFromReassignIroutes() {
      taPin *sol = nullptr;
      if (!reassignIroutes.empty()) {
        sol = *reassignIroutes.begin();
        reassignIroutes.erase(reassignIroutes.begin());
      }
      return sol;
    }
    // getters
    frTechObject* getTech() const {
      return design->getTech();
    }
    frDesign* getDesign() const {
      return design;
    }
    const frBox& getRouteBox() const {
      return routeBox;
    }
    const frBox& getExtBox() const {
      return extBox;
    }
    frPrefRoutingDirEnum getDir() const {
      return dir;
    }
    int getTAIter() const {
      return taIter;
    }
    bool isInitTA() const {
      return (taIter == 0);
    }
    frRegionQuery* getRegionQuery() const {
      return design->getRegionQuery();
    }
    void getTrackIdx(frCoord loc1, frCoord loc2, frLayerNum lNum, int &idx1, int &idx2) const {
      idx1 = int(std::lower_bound(trackLocs[lNum].begin(), trackLocs[lNum].end(), loc1) - trackLocs[lNum].begin());
      idx2 = int(std::upper_bound(trackLocs[lNum].begin(), trackLocs[lNum].end(), loc2) - trackLocs[lNum].begin()) - 1;
    }
    const std::vector<frCoord>& getTrackLocs(frLayerNum in) const {
      return trackLocs[in];
    }
    //const std::vector<taTrack>& getTracks(frLayerNum in) const {
    //  return tracks[in];
    //}
    //std::vector<taTrack>& getTracks(frLayerNum in) {
    //  return tracks[in];
    //}
    const FlexTAWorkerRegionQuery& getWorkerRegionQuery() const {
      return rq;
    }
    FlexTAWorkerRegionQuery& getWorkerRegionQuery() {
      return rq;
    }
    int getNumAssigned() const {
      return numAssigned;
    }
    // others
    int main();
    int main_mt();
    
  protected:
    frTechObject*                      tech; // not set
    frDesign*                          design;
    frBox                              routeBox;
    frBox                              extBox;
    frPrefRoutingDirEnum               dir;
    int                                taIter;
    FlexTAWorkerRegionQuery            rq;

    //std::vector<frGuide*>            guides;
    std::vector<std::unique_ptr<taPin> > iroutes; // unsorterd iroutes
    std::vector<std::unique_ptr<taPin> > extIroutes;
    std::vector<std::vector<frCoord> >   trackLocs;
    //std::vector<std::vector<taTrack> >   tracks;
    //std::priority_queue<taIroute*, std::vector<taIroute*>, taIrouteComp> pq;
    std::set<taPin*, taPinComp>  reassignIroutes; // iroutes to be assigned in sorted order

    int                                numAssigned;
    int                                totCost;
    int                                maxRetry;
    //frUInt4                            totDrcCost;
    
    //// others
    void init();
    void initFixedObjs();
    frCoord initFixedObjs_calcBloatDist(frBlockObject *obj, const frLayerNum lNum, const box_t &boostb);
    frCoord initFixedObjs_calcOBSBloatDistVia(frViaDef *viaDef, const frLayerNum lNum, const box_t &boostb, bool isOBS = true);
    void initFixedObjs_helper(const frBox &box, frCoord bloatDist, frLayerNum lNum, frNet* net);
    void initTracks();
    void initIroutes();
    void initIroute(frGuide *in);
    void initIroute_helper(frGuide* guide, frCoord &maxBegin, frCoord &minEnd, 
                           std::set<frCoord> &downViaCoordSet, std::set<frCoord> &upViaCoordSet, int &wlen, frCoord &wlen2);
    void initIroute_helper_generic(frGuide* guide, frCoord &maxBegin, frCoord &minEnd, 
                                   std::set<frCoord> &downViaCoordSet, std::set<frCoord> &upViaCoordSet, int &wlen, frCoord &wlen2);
    void initIroute_helper_generic_helper(frGuide* guide, frCoord &wlen2);
    bool initIroute_helper_pin(frGuide* guide, frCoord &maxBegin, frCoord &minEnd, 
                               std::set<frCoord> &downViaCoordSet, std::set<frCoord> &upViaCoordSet, int &wlen, frCoord &wlen2);
    void initCosts();
    void sortIroutes();

    // quick drc
    inline frCoord box2boxDistSquare(const frBox &box1, const frBox &box2, frCoord &dx, frCoord &dy);
    void addCost(taPinFig* fig, std::set<taPin*, frBlockObjectComp> *pinS = nullptr);
    void subCost(taPinFig* fig, std::set<taPin*, frBlockObjectComp> *pinS = nullptr);
    void modCost(taPinFig* fig, bool isAddCost, std::set<taPin*, frBlockObjectComp> *pinS = nullptr);
    void modMinSpacingCostPlanar(const frBox &box, frLayerNum lNum, taPinFig* fig, bool isAddCost, 
                                 std::set<taPin*, frBlockObjectComp> *pinS = nullptr);
    void modMinSpacingCostVia(const frBox &box, frLayerNum lNum, taPinFig* fig, bool isAddCost, bool isUpperVia, 
                              bool isCurrPs, std::set<taPin*, frBlockObjectComp> *pinS = nullptr);
    void modCutSpacingCost(const frBox &box, frLayerNum lNum, taPinFig* fig, bool isAddCost, std::set<taPin*, frBlockObjectComp> *pinS = nullptr);

    // initTA
    void assign();
    void assignIroute(taPin* iroute);
    void assignIroute_init(taPin* iroute, std::set<taPin*, frBlockObjectComp> *pinS);
    void assignIroute_availTracks(taPin* iroute, frLayerNum &lNum, int &idx1, int &idx2);
    int  assignIroute_bestTrack(taPin* iroute, frLayerNum lNum, int idx1, int idx2);
    void assignIroute_bestTrack_helper(taPin* iroute, frLayerNum lNum, int trackIdx, frUInt4 &bestCost, 
                                       frCoord &bestTrackLoc, int &bestTrackIdx, frUInt4 &drcCost);
    frUInt4 assignIroute_getCost(taPin *iroute, frCoord trackLoc, frUInt4 &drcCost);
    frUInt4 assignIroute_getWlenCost(taPin *iroute, frCoord trackLoc);
    frUInt4 assignIroute_getPinCost(taPin* iroute, frCoord trackLoc);
    frUInt4 assignIroute_getAlignCost(taPin* iroute, frCoord trackLoc);
    frUInt4 assignIroute_getDRCCost(taPin *iroute, frCoord trackLoc);
    frUInt4 assignIroute_getDRCCost_helper(taPin* iroute, const frBox &box, frLayerNum lNum);
    void assignIroute_updateIroute(taPin* iroute, frCoord bestTrackLoc, std::set<taPin*, frBlockObjectComp> *pinS);
    void assignIroute_updateOthers(std::set<taPin*, frBlockObjectComp> &pinS);

    // end
    void end();
    void saveToGuides();

    //frCost getCost();
    //void getAllGuides();
    //void getAllTracks();

    //void genIroutes();
    //void genIroutes_setBeginEnd(FlexIroute* iroute, bool isH);
    //void assignIroutes();
    ////int getWlenCost_helper(FlexIroute* iroute);
    //frUInt4 getWlenCost(FlexIroute* iroute, FlexTrack* track, 
    //                    bool isH, int wlen_helper);
    //void reassignIroutes();
    //void saveToGuides();
    //void reportCosts();



    //bool isTrackInGuide(const std::shared_ptr<frGuide> &guide, const std::shared_ptr<FlexTrack> &track);

    /* assumption
     * layer direction is always alternative
     * in track assignment, only do nets with guides only, no physical wires and vias
     * each track has one candidate
     * each global or local guide has 0<=x<=1 pathseg, pathsegonly
     * each via guide has >=0 physical vias, via only
     */
    //bool isViaOnPathSeg(const std::shared_ptr<frPathSeg> &pathSeg, const std::shared_ptr<frVia> &via);
    //bool isViaOnTrack(const std::shared_ptr<FlexTrack> &track, const std::shared_ptr<frVia> &via);
    //bool isPathSegOnTrack(const std::shared_ptr<FlexTrack> &track, const std::shared_ptr<frPathSeg> &pathSeg);
    // check whether nbr pathSeg length is bounded by non-current-layer segments
    //bool hasPathSegLimit(const std::shared_ptr<frGuide> &guide, const std::shared_ptr<frPathSeg> &pathSeg, 
    //                     bool checkLowerLayer, bool checkUpperLayer);
    //frCoord getPathSegLimit(const std::shared_ptr<frGuide> &guide, const std::shared_ptr<frPathSeg> &pathSeg, 
    //                        bool checkLowerLayer, bool checkUpperLayer, bool isBegin);
    //void getPathSegLimit_helper(const std::shared_ptr<frGuide> &guide, 
    //                            const std::shared_ptr<frPathSeg> &pathSeg, 
    //                            bool checkLowerLayer, bool checkUpperLayer, bool isBegin, 
    //                            const std::shared_ptr<frBlockObject> &v, 
    //                            bool &init, frCoord &limit); // init and limit must be reference
    friend class FlexTA;
  };

}


#endif
