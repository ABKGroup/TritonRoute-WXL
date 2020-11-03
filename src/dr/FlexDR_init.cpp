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

#include "dr/FlexDR.h"
#include <algorithm>

using namespace std;
using namespace fr;

void FlexDRWorker::initNetObjs_pathSeg(frPathSeg* pathSeg,
                                       set<frNet*, frBlockObjectComp> &nets, 
                                       map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netRouteObjs,
                                       map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netExtObjs) {
  //bool enableOutput = true;
  bool enableOutput = false;
  auto gridBBox = getRouteBox();
  auto net = pathSeg->getNet();
  nets.insert(net);
  // split seg
  frPoint begin, end;
  pathSeg->getPoints(begin, end);
  //cout <<"here" <<endl;
  // vertical seg
  if (begin.x() == end.x()) {
    //cout <<"vert seg" <<endl;
    // may cross routeBBox
    bool condition1 = isInitDR() ? (begin.x() < gridBBox.right()) : (begin.x() <= gridBBox.right());
    if (gridBBox.left() <= begin.x() && condition1) {
      //cout << " pathSeg (" << begin.x() << ", " << begin.y() << ") - (" << end.x() << ", " << end.y() << ")\n";
      // bottom seg to ext
      if (begin.y() < gridBBox.bottom()) {
        auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
        auto ps = uPathSeg.get();
        uPathSeg->setPoints(begin, frPoint(end.x(), min(end.y(), gridBBox.bottom())));
        unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
        if (end.y() < gridBBox.bottom()) {
          netExtObjs[net].push_back(std::move(uDRObj)); // pure ext
        } else {
          // change boundary style to ext if pathSeg does not end exactly at boundary
          frSegStyle style;
          pathSeg->getStyle(style);
          if (end.y() != gridBBox.bottom()) {
            style.setEndStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
          }
          ps->setStyle(style);

          netRouteObjs[net].push_back(std::move(uDRObj));
        }
        if (enableOutput) {
          cout <<"find pathseg to ext bottom (" << begin.x() / 2000.0 << ", " << begin.y() / 2000.0 << ") to ("
               << end.x() / 2000.0 << ", " << min(end.y(), gridBBox.bottom()) / 2000.0 << ") on Layer " << ps->getLayerNum() <<endl;
        }
      }
      // middle seg to route
      if (!(begin.y() >= gridBBox.top() || end.y() <= gridBBox.bottom())) {
        auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
        auto ps = uPathSeg.get();
        uPathSeg->setPoints(frPoint(begin.x(), max(begin.y(), gridBBox.bottom())), 
                            frPoint(end.x(),   min(end.y(),   gridBBox.top())));
        unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
        // change boundary style to ext if it does not end within at boundary
        frSegStyle style;
        pathSeg->getStyle(style);
        if (begin.y() < gridBBox.bottom()) {
          style.setBeginStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
        }
        if (end.y() > gridBBox.top()) {
          style.setEndStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
        }
        ps->setStyle(style);

        netRouteObjs[net].push_back(std::move(uDRObj));
        if (enableOutput) {
          cout <<"find pathseg to route vert (" << begin.x() / 2000.0 << ", " << max(begin.y(), gridBBox.bottom()) / 2000.0
               << ") to (" << end.x() / 2000.0 << ", " << min(end.y(),   gridBBox.top()) / 2000.0 << ")" <<endl;
        }
      }
      // top seg to ext
      if (end.y() > gridBBox.top()) {
        auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
        auto ps = uPathSeg.get();
        uPathSeg->setPoints(frPoint(begin.x(), max(begin.y(), gridBBox.top())), end);
        unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
        if (begin.y() > gridBBox.top()) {
          netExtObjs[net].push_back(std::move(uDRObj)); // pure ext
        } else {
          // change boundary style to ext if pathSeg does not end exactly at boundary
          frSegStyle style;
          pathSeg->getStyle(style);
          if (begin.y() != gridBBox.top()) {
            style.setBeginStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
          }
          ps->setStyle(style);

          netRouteObjs[net].push_back(std::move(uDRObj));
        }
        if (enableOutput) {
          cout <<"find pathseg to ext top (at " << begin.x() / 2000.0 << ", " 
               << max(begin.y(), gridBBox.top()) / 2000.0 << ", " << ps->getLayerNum() << ") )" <<endl;
        }
      }
    // cannot cross routeBBox
    } else {
      auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
      unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
      netExtObjs[net].push_back(std::move(uDRObj));
      if (enableOutput) {
        cout <<"find pathseg to pure ext vert" <<endl;
      }
    }
  // horizontal seg
  } else if (begin.y() == end.y()) {
    //cout <<"horz seg" <<endl;
    // may cross routeBBox
    bool condition1 = isInitDR() ? (begin.y() < gridBBox.top()) : (begin.y() <= gridBBox.top());
    if (gridBBox.bottom() <= begin.y() && condition1) {
      //cout << " pathSeg (" << begin.x() / 2000.0 << ", " << begin.y() / 2000.0 << ") - (" 
      //                     << end.x()   / 2000.0 << ", " << end.y() / 2000.0   << ")\n";
      // left seg to ext
      if (begin.x() < gridBBox.left()) {
        auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
        auto ps = uPathSeg.get();
        uPathSeg->setPoints(begin, frPoint(min(end.x(), gridBBox.left()), end.y()));
        unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
        if (end.x() < gridBBox.left()) {
          netExtObjs[net].push_back(std::move(uDRObj)); // pure ext
        } else {
          // change boundary style to ext if pathSeg does not end exactly at boundary
          frSegStyle style;
          pathSeg->getStyle(style);
          if (end.x() != gridBBox.left()) {
            style.setEndStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
          }
          ps->setStyle(style);

          netRouteObjs[net].push_back(std::move(uDRObj)); //touching bounds
        }
        if (enableOutput) {
          cout <<"find pathseg to ext left (at " << min(end.x(), gridBBox.left()) / 2000.0 << ", " 
               << end.y() / 2000.0 << ", " << ps->getLayerNum() << ") )" <<endl;
        }
      }
      // middle seg to route
      if (!(begin.x() >= gridBBox.right() || end.x() <= gridBBox.left())) {
        auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
        auto ps = uPathSeg.get();
        uPathSeg->setPoints(frPoint(max(begin.x(), gridBBox.left()),  begin.y()), 
                            frPoint(min(end.x(),   gridBBox.right()), end.y()));
        unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
        // change boundary style to ext if it does not end within at boundary
        frSegStyle style;
        pathSeg->getStyle(style);
        if (begin.x() < gridBBox.left()) {
          style.setBeginStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
        }
        if (end.x() > gridBBox.right()) {
          style.setEndStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
        }
        ps->setStyle(style);

        netRouteObjs[net].push_back(std::move(uDRObj));
        if (enableOutput) {
          cout <<"find pathseg to route horz" <<endl;
        }
      }
      // right seg to ext
      if (end.x() > gridBBox.right()) {
        auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
        auto ps = uPathSeg.get();
        uPathSeg->setPoints(frPoint(max(begin.x(), gridBBox.right()), begin.y()), end);
        unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
        if (begin.x() > gridBBox.right()) {
          netExtObjs[net].push_back(std::move(uDRObj)); // pure ext
        } else {
          // change boundary style to ext if pathSeg does not end exactly at boundary
          frSegStyle style;
          pathSeg->getStyle(style);
          if (begin.x() != gridBBox.right()) {
            style.setBeginStyle(frEndStyle(frcExtendEndStyle), getTech()->getLayer(pathSeg->getLayerNum())->getWidth() / 2);
          }
          ps->setStyle(style);

          netRouteObjs[net].push_back(std::move(uDRObj)); // touching bounds
        }
        if (enableOutput) {
          cout <<"find pathseg to ext right (at " << max(begin.x(), gridBBox.right()) / 2000.0 << ", " 
               << begin.y() / 2000.0 << ", " << ps->getLayerNum() << ") )" <<endl;
        }
      }
    // cannot cross routeBBox
    } else {
      auto uPathSeg = make_unique<drPathSeg>(*pathSeg);
      unique_ptr<drConnFig> uDRObj(std::move(uPathSeg));
      netExtObjs[net].push_back(std::move(uDRObj));
      if (enableOutput) {
        cout <<"find pathseg to pure ext horz" <<endl;
      }
    }
  } else {
    cout <<"wtf" <<endl;
  }
}

void FlexDRWorker::initNetObjs_via(frVia* via,
                                          set<frNet*, frBlockObjectComp> &nets, 
                                          map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netRouteObjs,
                                          map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netExtObjs) {
  //bool enableOutput = true;
  bool enableOutput = false;
  auto gridBBox = getRouteBox();
  auto net = via->getNet();
  nets.insert(net);
  frPoint viaPoint;
  via->getOrigin(viaPoint);
  bool condition1 = isInitDR() ? 
                    (viaPoint.x() <  gridBBox.right() && viaPoint.y() <  gridBBox.top()) :
                    (viaPoint.x() <= gridBBox.right() && viaPoint.y() <= gridBBox.top());
  if (viaPoint.x() >= gridBBox.left() && viaPoint.y() >= gridBBox.bottom() && condition1) {
    auto uVia = make_unique<drVia>(*via);
    unique_ptr<drConnFig> uDRObj(std::move(uVia));
    netRouteObjs[net].push_back(std::move(uDRObj));
    if (enableOutput) {
      cout <<"find via route" <<endl;
    }
  } else {
    auto uVia = make_unique<drVia>(*via);
    unique_ptr<drConnFig> uDRObj(std::move(uVia));
    netExtObjs[net].push_back(std::move(uDRObj));
    if (enableOutput) {
      cout <<"find via ext" <<endl;
    }
  }
}

void FlexDRWorker::initNetObjs_patchWire(frPatchWire* pwire,
                                         set<frNet*, frBlockObjectComp> &nets, 
                                         map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netRouteObjs,
                                         map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netExtObjs) {
  //bool enableOutput = true;
  bool enableOutput = false;
  auto gridBBox = getRouteBox();
  auto net = pwire->getNet();
  nets.insert(net);
  frPoint origin;
  pwire->getOrigin(origin);
  bool condition1 = isInitDR() ? 
                    (origin.x() <  gridBBox.right() && origin.y() <  gridBBox.top()) :
                    (origin.x() <= gridBBox.right() && origin.y() <= gridBBox.top());
  if (origin.x() >= gridBBox.left() && origin.y() >= gridBBox.bottom() && condition1) {
    auto uPWire = make_unique<drPatchWire>(*pwire);
    unique_ptr<drConnFig> uDRObj(std::move(uPWire));
    netRouteObjs[net].push_back(std::move(uDRObj));
    if (enableOutput) {
      cout <<"find patch wire route" <<endl;
    }
  } else {
    auto uPWire = make_unique<drPatchWire>(*pwire);
    unique_ptr<drConnFig> uDRObj(std::move(uPWire));
    netExtObjs[net].push_back(std::move(uDRObj));
    if (enableOutput) {
      cout <<"find patch wire ext" <<endl;
    }
  }
}

void FlexDRWorker::initNetObjs(set<frNet*, frBlockObjectComp> &nets, 
                               map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netRouteObjs,
                               map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netExtObjs,
                               map<frNet*, vector<frRect>, frBlockObjectComp> &netOrigGuides) {
  bool enableOutput = false;
  //bool enableOutput = true;
  vector<frBlockObject*> result;
  getRegionQuery()->queryDRObj(getExtBox(), result);
  int cnt1 = 0;
  int cnt2 = 0;
  for (auto rptr: result) {
    if (rptr->typeId() == frcPathSeg) {
      auto cptr = static_cast<frPathSeg*>(rptr);
      if (cptr->hasNet()) {
        initNetObjs_pathSeg(cptr, nets, netRouteObjs, netExtObjs);
        cnt1++;
      } else {
        cout <<"Error: initNetObjs hasNet() empty" <<endl;
      }
    } else if (rptr->typeId() == frcVia) {
      auto cptr = static_cast<frVia*>(rptr);
      if (cptr->hasNet()) {
        initNetObjs_via(cptr, nets, netRouteObjs, netExtObjs);
        cnt2++;
      } else {
        cout <<"Error: initNetObjs hasNet() empty" <<endl;
      }
    } else if (rptr->typeId() == frcPatchWire) {
      auto cptr = static_cast<frPatchWire*>(rptr);
      if (cptr->hasNet()) {
        initNetObjs_patchWire(cptr, nets, netRouteObjs, netExtObjs);
        cnt1++;
      } else {
        cout <<"Error: initNetObjs hasNet() empty" <<endl;
      }
    } else {
      cout << rptr->typeId() << "\n";
      cout <<"Error: initCopyDRObjs unsupported type" <<endl;
    }
  }
  if (isInitDR()) {
    vector<frGuide*> guides;
    getRegionQuery()->queryGuide(getRouteBox(), guides);
    for (auto &guide: guides) {
      if (guide->hasNet()) {
        auto net = guide->getNet();
        if (nets.find(net) == nets.end()) {
          nets.insert(net);
          netRouteObjs[net].clear();
          netExtObjs[net].clear();
        }
      }
    }
  }
  
  if (isFollowGuide()) {
    vector<rq_rptr_value_t<frNet> > origGuides;
    frRect rect;
    frBox  box;
    for (auto lNum = getDesign()->getTech()->getBottomLayerNum(); 
         lNum <= getDesign()->getTech()->getTopLayerNum(); lNum++) {
      origGuides.clear();
      getRegionQuery()->queryOrigGuide(getRouteBox(), lNum, origGuides);
      for (auto &[boostb, net]: origGuides) {
        if (nets.find(net) == nets.end()) {
          continue;
        }
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), 
                boostb.max_corner().x(), boostb.max_corner().y());
        //if (getExtBox().overlaps(box, false)) {
        //  continue;
        //}
        rect.setBBox(box);
        rect.setLayerNum(lNum);
        netOrigGuides[net].push_back(rect);
        if (enableOutput) {
          double dbu = getDesign()->getTopBlock()->getDBUPerUU();
          cout <<"found net/guide=" <<net->getName() <<" ("
               <<box.left()  / dbu <<", " <<box.bottom() / dbu <<") ("
               <<box.right() / dbu <<", " <<box.top()    / dbu <<") "
               <<getDesign()->getTech()->getLayer(lNum)->getName() <<endl;
        }
      }
    }
  }

  if (enableOutput) {
    cout <<"cnt1/2 = " <<cnt1 <<"/" <<cnt2 <<endl;
    cout <<"nets size = " <<nets.size() <<endl;
  }
}

void FlexDRWorker::initNets_initDR(set<frNet*, frBlockObjectComp> &nets,
                                   map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netRouteObjs,
                                   map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netExtObjs,
                                   map<frNet*, vector<frRect>, frBlockObjectComp> &netOrigGuides) {
  //map<frNet*, vector<frBlockObject*>, frBlockObjectComp > netTerms;
  map<frNet*, set<frBlockObject*, frBlockObjectComp>, frBlockObjectComp > netTerms;
  vector<frBlockObject*> result;
  getRegionQuery()->queryGRPin(getRouteBox(), result);
  for (auto obj: result) {
    if (obj->typeId() == frcInstTerm) {
      auto net = static_cast<frInstTerm*>(obj)->getNet();
      nets.insert(net);
      //netTerms[net].push_back(obj);
      netTerms[net].insert(obj);
    } else if (obj->typeId() == frcTerm) {
      auto net = static_cast<frTerm*>(obj)->getNet();
      nets.insert(net);
      //netTerms[net].push_back(obj);
      netTerms[net].insert(obj);
    } else {
      cout <<"Error: initNetTerms unsupported obj" <<endl;
    }
  }
  vector<unique_ptr<drConnFig> > vRouteObjs;
  vector<unique_ptr<drConnFig> > vExtObjs;
  for (auto net: nets) {
    vRouteObjs.clear();
    vExtObjs.clear();
    vExtObjs = std::move(netExtObjs[net]);
    for (int i = 0; i < (int)netRouteObjs[net].size(); i++) {
      auto &obj = netRouteObjs[net][i];
      if (obj->typeId() == drcPathSeg) {
        auto ps = static_cast<drPathSeg*>(obj.get());
        frPoint bp, ep;
        ps->getPoints(bp, ep);
        auto &box = getRouteBox();
        if (box.contains(bp) && box.contains(ep)) {
          vRouteObjs.push_back(std::move(netRouteObjs[net][i]));
        } else {
          vExtObjs.push_back(std::move(netRouteObjs[net][i]));
        }
      } else if (obj->typeId() == drcVia) {
        vRouteObjs.push_back(std::move(netRouteObjs[net][i]));
      } else if (obj->typeId() == drcPatchWire) {
        vRouteObjs.push_back(std::move(netRouteObjs[net][i]));
      }
    }
    //initNet(net, netRouteObjs[net], netExtObjs[net], netTerms[net]);
    vector<frBlockObject*> tmpTerms;
    tmpTerms.assign(netTerms[net].begin(), netTerms[net].end());
    //initNet(net, vRouteObjs, vExtObjs, netTerms[net]);
    initNet(net, vRouteObjs, vExtObjs, netOrigGuides[net], tmpTerms);
  }
}

// copied to FlexDR::checkConnectivity_pin2epMap_helper
void FlexDRWorker::initNets_searchRepair_pin2epMap_helper(frNet *net, const frPoint &bp, frLayerNum lNum, 
                                                          map<frBlockObject*, 
                                                              set<pair<frPoint, frLayerNum> >, 
                                                              frBlockObjectComp > &pin2epMap) {
  bool enableOutput = false;
  //bool enableOutput = true;
  auto regionQuery = getRegionQuery();
  vector<rq_rptr_value_t<frBlockObject> > result;
  //result.clear();
  regionQuery->query(frBox(bp, bp), lNum, result);
  for (auto &[bx, rqObj]: result) {
    if (rqObj->typeId() == frcInstTerm) {
      auto instTerm = static_cast<frInstTerm*>(rqObj);
      if (instTerm->getNet() == net) {
        if (enableOutput) {
          cout <<"found instTerm" <<endl;
        }
        pin2epMap[rqObj].insert(make_pair(bp, lNum));
        // if (instTerm->getInst()->getName() == string("_435747_") && instTerm->getTerm()->getName() == string("B")) {
        //   cout << "  @@@ debug @@@ (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << "(" << bp.x() << ")" << ", "
        //               <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << "(" << bp.y() << ") " <<") lNum = " << lNum << "\n";
        // }
      } else {
        if (enableOutput) {
          cout <<"found other instTerm" <<endl;
        }
      }
    } else if (rqObj->typeId() == frcTerm) {
      auto term = static_cast<frTerm*>(rqObj);
      if (term->getNet() == net) {
        if (enableOutput) {
          cout <<"found term" <<endl;
        }
        pin2epMap[rqObj].insert(make_pair(bp, lNum));
      } else {
        if (enableOutput) {
          cout <<"found other term" <<endl;
        }
      }
    }
  }
}

void FlexDRWorker::initNets_searchRepair_pin2epMap(frNet* net, 
                                                   vector<unique_ptr<drConnFig> > &netRouteObjs,
                                                   /*vector<unique_ptr<drConnFig> > &netExtObjs,*/
                                                   /*vector<frBlockObject> &netPins,*/
                                                   map<frBlockObject*, 
                                                       set<pair<frPoint, frLayerNum> >,
                                                       frBlockObjectComp> &pin2epMap/*,
                                                   map<pair<frPoint, frLayerNum>, set<int> > &nodeMap*/) {
  bool enableOutput = false;
  frPoint bp, ep;
  //auto regionQuery = getRegionQuery();
  //vector<rq_rptr_value_t<frBlockObject> > result;
  // should not count extObjs in union find
  for (auto &uPtr: netRouteObjs) {
    auto connFig = uPtr.get();
    if (connFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(connFig);
      obj->getPoints(bp, ep);
      auto lNum = obj->getLayerNum();
      if (enableOutput) {
       cout <<"(bp, ep) (" <<bp.x() / 2000.0 <<" ," <<bp.y() / 2000.0 <<") ("
                           <<ep.x() / 2000.0 <<" ," <<ep.y() / 2000.0 <<") " 
            <<getTech()->getLayer(lNum)->getName() <<endl;
      }
      frSegStyle style;
      obj->getStyle(style);
      if (style.getBeginStyle() == frEndStyle(frcTruncateEndStyle) && getRouteBox().contains(bp)) {
        if (enableOutput) {
          cout <<"query bp" <<endl;
        }
        // if (bp.x() == 1658879 && bp.y() == 2370048) {
        //   cout << "bp @ (1658879, 2370048)\n";
        //   cout << "  ep @ (" << ep.x() << ", " << ep.y() << ")\n";
        //   cout << "lNum = " << lNum << endl;
        // }
        initNets_searchRepair_pin2epMap_helper(net, bp, lNum, pin2epMap);
      }
      if (style.getEndStyle() == frEndStyle(frcTruncateEndStyle) && getRouteBox().contains(ep)) {
        if (enableOutput) {
          cout <<"query ep" <<endl;
        }
        // if (ep.x() == 1658879 && ep.y() == 2370048) {
        //   cout << "ep @ (1658879, 2370048)\n";
        //   cout << "  bp @ (" << bp.x() << ", " << bp.y() << ")\n";
        //   cout << "lNum = " << lNum << endl;
        // }
        initNets_searchRepair_pin2epMap_helper(net, ep, lNum, pin2epMap);
      }
    } else if (connFig->typeId() == drcVia) {
      auto obj = static_cast<drVia*>(connFig);
      obj->getOrigin(bp);
      auto l1Num = obj->getViaDef()->getLayer1Num();
      auto l2Num = obj->getViaDef()->getLayer2Num();
      if (enableOutput) {
        cout <<"bp (" <<bp.x() / 2000.0 <<" ," <<bp.y() / 2000.0 <<") "
             <<getTech()->getLayer(l1Num)->getName() <<endl;
      }
      if (getRouteBox().contains(bp)) {
        // if (bp.x() == 1511039 && bp.y() == 3519744) {
        //   cout << "bp @ (1511039, 3519740)\n";
        //   cout << "  via\n";
        // }
        if (enableOutput) {
          cout <<"query bp l1" <<endl;
        }
        initNets_searchRepair_pin2epMap_helper(net, bp, l1Num, pin2epMap);
        if (enableOutput) {
          cout <<"query bp l2" <<endl;
        }
        initNets_searchRepair_pin2epMap_helper(net, bp, l2Num, pin2epMap);
      }
    } else if (connFig->typeId() == drcPatchWire) {
    } else {
      cout <<"Error: initNets_searchRepair_pin2epMap unsupported type" <<endl;
    }
  }
  //cout <<net->getName() <<" " <<pin2epMap.size() <<endl;
}

void FlexDRWorker::initNets_searchRepair_nodeMap_routeObjEnd(frNet* net, vector<unique_ptr<drConnFig> > &netRouteObjs,
                                                   map<pair<frPoint, frLayerNum>, set<int> > &nodeMap) {
  bool enableOutput = false;
  frPoint bp, ep;
  for (int i = 0; i < (int)netRouteObjs.size(); i++) {
    auto connFig = netRouteObjs[i].get();
    if (connFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(connFig);
      obj->getPoints(bp, ep);
      auto lNum = obj->getLayerNum();
      nodeMap[make_pair(bp, lNum)].insert(i);
      nodeMap[make_pair(ep, lNum)].insert(i);
      if (enableOutput) {
        cout <<"node idx = " <<i <<", (" <<bp.x() / 2000.0 <<", " <<bp.y() / 2000.0 <<") (" 
                                         <<ep.x() / 2000.0 <<", " <<ep.y() / 2000.0 <<") " 
             <<getTech()->getLayer(lNum)->getName() <<endl;
      }
    } else if (connFig->typeId() == drcVia) {
      auto obj = static_cast<drVia*>(connFig);
      obj->getOrigin(bp);
      auto l1Num = obj->getViaDef()->getLayer1Num();
      auto l2Num = obj->getViaDef()->getLayer2Num();
      nodeMap[make_pair(bp, l1Num)].insert(i);
      nodeMap[make_pair(bp, l2Num)].insert(i);
      if (enableOutput) {
        cout <<"node idx = " <<i <<", (" <<bp.x() / 2000.0 <<", " <<bp.y() / 2000.0 <<") "
             <<getTech()->getLayer(l1Num)->getName() <<" --> " <<getTech()->getLayer(l2Num)->getName() <<endl;
      }
    } else if (connFig->typeId() == drcPatchWire) {
      auto obj = static_cast<drPatchWire*>(connFig);
      obj->getOrigin(bp);
      auto lNum = obj->getLayerNum();
      nodeMap[make_pair(bp, lNum)].insert(i);
      if (enableOutput) {
        cout <<"node idx = " <<i <<", (" <<bp.x() / 2000.0 <<", " <<bp.y() / 2000.0 <<") "
             <<getTech()->getLayer(lNum)->getName() << endl;
      }
    } else {
      cout <<"Error: initNets_searchRepair_nodeMap unsupported type" <<endl;
    }
  }
}

void FlexDRWorker::initNets_searchRepair_nodeMap_routeObjSplit_helper(const frPoint &crossPt, 
                   frCoord trackCoord, frCoord splitCoord, frLayerNum lNum, 
                   vector<map<frCoord, map<frCoord, pair<frCoord, int> > > > &mergeHelper,
                   map<pair<frPoint, frLayerNum>, set<int> > &nodeMap) {
  auto it1 = mergeHelper[lNum].find(trackCoord);
  if (it1 != mergeHelper[lNum].end()) {
    auto &mp = it1->second; // map<ep, pair<bp, objIdx> >
    auto it2 = mp.lower_bound(splitCoord);
    if (it2 != mp.end()) {
      auto &endP = it2->first;
      auto &[beginP, objIdx] = it2->second;
      if (endP > splitCoord && beginP < splitCoord) {
        nodeMap[make_pair(crossPt, lNum)].insert(objIdx);
      }
    }
  }
}

void FlexDRWorker::initNets_searchRepair_nodeMap_routeObjSplit(frNet* net, vector<unique_ptr<drConnFig> > &netRouteObjs,
                                                               map<pair<frPoint, frLayerNum>, set<int> > &nodeMap) {
  frPoint bp, ep;
  // vector<map<track, map<ep, pair<bp, objIdx> > > > interval_map
  vector<map<frCoord, map<frCoord, pair<frCoord, int> > > > horzMergeHelper(getTech()->getLayers().size());
  vector<map<frCoord, map<frCoord, pair<frCoord, int> > > > vertMergeHelper(getTech()->getLayers().size());
  for (int i = 0; i < (int)netRouteObjs.size(); i++) {
    auto connFig = netRouteObjs[i].get();
    if (connFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(connFig);
      obj->getPoints(bp, ep);
      auto lNum = obj->getLayerNum();
      // vert seg
      if (bp.x() == ep.x()) {
        vertMergeHelper[lNum][bp.x()][ep.y()] = make_pair(bp.y(), i);
      // horz seg
      } else {
        horzMergeHelper[lNum][bp.y()][ep.x()] = make_pair(bp.x(), i);
      }
    }
  }
  for (int i = 0; i < (int)netRouteObjs.size(); i++) {
    auto connFig = netRouteObjs[i].get();
    // ep on pathseg
    if (connFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(connFig);
      obj->getPoints(bp, ep);
      auto lNum = obj->getLayerNum();
      // vert seg, find horz crossing seg
      if (bp.x() == ep.x()) {
        //find whether there is horz track at bp
        auto crossPt    = bp;
        auto trackCoord = bp.y();
        auto splitCoord = bp.x();
        initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, horzMergeHelper, nodeMap);
        //find whether there is horz track at ep
        crossPt    = ep;
        trackCoord = ep.y();
        splitCoord = ep.x();
        initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, horzMergeHelper, nodeMap);
      // horz seg
      } else {
        //find whether there is vert track at bp
        auto crossPt    = bp;
        auto trackCoord = bp.x();
        auto splitCoord = bp.y();
        initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, vertMergeHelper, nodeMap);
        //find whether there is vert track at ep
        crossPt    = ep;
        trackCoord = ep.x();
        splitCoord = ep.y();
        initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, vertMergeHelper, nodeMap);
      }
    } else if (connFig->typeId() == drcVia) {
      auto obj = static_cast<drVia*>(connFig);
      obj->getOrigin(bp);
      auto lNum = obj->getViaDef()->getLayer1Num();
      //find whether there is horz track at bp on layer1
      auto crossPt    = bp;
      auto trackCoord = bp.y();
      auto splitCoord = bp.x();
      initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, horzMergeHelper, nodeMap);
      //find whether there is vert track at bp on layer1
      crossPt    = bp;
      trackCoord = bp.x();
      splitCoord = bp.y();
      initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, vertMergeHelper, nodeMap);
      
      lNum = obj->getViaDef()->getLayer2Num();
      //find whether there is horz track at bp on layer2
      crossPt    = bp;
      trackCoord = bp.y();
      splitCoord = bp.x();
      initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, horzMergeHelper, nodeMap);
      //find whether there is vert track at bp on layer2
      crossPt    = bp;
      trackCoord = bp.x();
      splitCoord = bp.y();
      initNets_searchRepair_nodeMap_routeObjSplit_helper(crossPt, trackCoord, splitCoord, lNum, vertMergeHelper, nodeMap);
    }
  }
}
  
void FlexDRWorker::initNets_searchRepair_nodeMap_pin(frNet* net, 
                                                     vector<unique_ptr<drConnFig> > &netRouteObjs,
                                                     vector<frBlockObject*> &netPins,
                                                     map<frBlockObject*, 
                                                         set<pair<frPoint, frLayerNum> >, 
                                                         frBlockObjectComp > &pin2epMap,
                                                     map<pair<frPoint, frLayerNum>, set<int> > &nodeMap) {
  bool enableOutput = false;
  int currCnt = (int)netRouteObjs.size();
  for (auto &[obj, locS]: pin2epMap) {
    netPins.push_back(obj);
    for (auto &pr: locS) {
      nodeMap[pr].insert(currCnt);
      if (enableOutput) {
        cout <<"pin idx = " <<currCnt <<", (" <<pr.first.x() <<", " <<pr.first.y() <<") " 
             <<getTech()->getLayer(pr.second)->getName() <<endl;
      }
    }
    ++currCnt;
  }
}

void FlexDRWorker::initNets_searchRepair_nodeMap(frNet* net, 
                                                 vector<unique_ptr<drConnFig> > &netRouteObjs,
                                                 vector<frBlockObject*> &netPins,
                                                 map<frBlockObject*, 
                                                     set<pair<frPoint, frLayerNum> >,
                                                     frBlockObjectComp> &pin2epMap,
                                                 map<pair<frPoint, frLayerNum>, set<int> > &nodeMap) {
  initNets_searchRepair_nodeMap_routeObjEnd(net, netRouteObjs, nodeMap);
  initNets_searchRepair_nodeMap_routeObjSplit(net, netRouteObjs, nodeMap);
  initNets_searchRepair_nodeMap_pin(net, netRouteObjs, netPins, pin2epMap, nodeMap);
}

void FlexDRWorker::initNets_searchRepair_connComp(frNet* net, 
                                                  map<pair<frPoint, frLayerNum>, set<int> > &nodeMap,
                                                  vector<int> &compIdx) {
  bool enableOutput = false;
  int nCnt = (int)compIdx.size(); // total node cnt

  vector<vector<int> > adjVec(nCnt, vector<int>());
  vector<bool> adjVisited(nCnt, false);
  for (auto &[pr, idxS]: nodeMap) {
    //auto &[pt, lNum] = pr;
    for (auto it1 = idxS.begin(); it1 != idxS.end(); it1++) {
      auto it2 = it1;
      it2++;
      auto idx1 = *it1;
      for (; it2 != idxS.end(); it2++) {
        auto idx2 = *it2;
        if (enableOutput) {
          cout <<"edge = " <<idx1 <<"/" <<idx2 <<endl <<flush;
        }
        adjVec[idx1].push_back(idx2);
        adjVec[idx2].push_back(idx1);
      }
    }
  }

  struct wf {
    int nodeIdx;
    int cost;
    bool operator<(const wf &b) const {
      if (cost == b.cost) {
        return nodeIdx > b.nodeIdx;
      } else {
        return cost > b.cost;
      }
    }
  };

  int currNetIdx = 0;
  auto it = find(adjVisited.begin(), adjVisited.end(), false);
  while (it != adjVisited.end()) {
    if (enableOutput) {
      cout <<"union";
    }
    priority_queue<wf> pq;
    int srcIdx = distance(adjVisited.begin(), it);
    pq.push({srcIdx, 0});
    while (!pq.empty()) {
      auto wfront = pq.top();
      auto currIdx = wfront.nodeIdx;
      pq.pop();
      if (adjVisited[currIdx]) {
        continue;
      }
      adjVisited[currIdx] = true;
      if (enableOutput) {
        cout <<" " <<currIdx;
      }
      compIdx[currIdx] = currNetIdx;
      for (auto nbrIdx: adjVec[currIdx]) {
        if (!adjVisited[nbrIdx]) {
          pq.push({nbrIdx, wfront.cost + 1});
        }
      }
    }
    if (enableOutput) {
      cout <<endl;
    }
    it = find(adjVisited.begin(), adjVisited.end(), false);
    ++currNetIdx;
  }
  //if (currNetIdx > 1) {
  //  cout <<"@@debug " <<net->getName() <<" union find get " <<currNetIdx <<" subnets" <<endl;
  //}
}

void FlexDRWorker::initNets_searchRepair(set<frNet*, frBlockObjectComp> &nets, 
                                         map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netRouteObjs,
                                         map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp> &netExtObjs,
                                         map<frNet*, vector<frRect>, frBlockObjectComp> &netOrigGuides) {
  for (auto net: nets) {
    // build big graph;
    // node number : routeObj, pins
    map<frBlockObject*, set<pair<frPoint, frLayerNum> >, frBlockObjectComp> pin2epMap;
    initNets_searchRepair_pin2epMap(net, netRouteObjs[net]/*, netExtObjs[net], netPins*/, pin2epMap/*, nodeMap*/);

    vector<frBlockObject*> netPins;
    map<pair<frPoint, frLayerNum>, set<int> > nodeMap;
    initNets_searchRepair_nodeMap(net, netRouteObjs[net], netPins, pin2epMap, nodeMap);

    //cout <<"size1(pin/robj) = " <<netPins.size() <<"/" <<netRouteObjs.size() <<endl;
    vector<int> compIdx((int)netPins.size() + (int)netRouteObjs[net].size(), 0);
    //cout <<"size2(pin/robj/comp) = " <<netPins.size() <<"/" <<netRouteObjs[net].size() <<"/" <<compIdx.size() <<endl;
    initNets_searchRepair_connComp(net, nodeMap, compIdx);

    vector<vector<unique_ptr<drConnFig> > > vRouteObjs;
    vector<vector<unique_ptr<drConnFig> > > vExtObjs;
    vector<vector<frBlockObject*> >         vPins;

    auto it = max_element(compIdx.begin(), compIdx.end());
    int numSubNets = (it == compIdx.end()) ? 1 : ((*it) + 1);
    // put all pure ext objs to the first subnet
    vExtObjs.resize(numSubNets);
    vExtObjs[0] = std::move(netExtObjs[net]);

    vRouteObjs.resize(numSubNets);
    vPins.resize(numSubNets);

    for (int i = 0; i < (int)compIdx.size(); i++) {
      int subNetIdx = compIdx[i];
      if (i < (int)netRouteObjs[net].size()) {
        auto &obj = netRouteObjs[net][i];
        if (obj->typeId() == drcPathSeg) {
          auto ps = static_cast<drPathSeg*>(obj.get());
          frPoint bp, ep;
          ps->getPoints(bp, ep);
          auto &box = getRouteBox();
          if (box.contains(bp) && box.contains(ep)) {
            vRouteObjs[subNetIdx].push_back(std::move(netRouteObjs[net][i]));
          } else {
            vExtObjs[subNetIdx].push_back(std::move(netRouteObjs[net][i]));
          }
        } else if (obj->typeId() == drcVia) {
          vRouteObjs[subNetIdx].push_back(std::move(netRouteObjs[net][i]));
        } else if (obj->typeId() == drcPatchWire) {
          vRouteObjs[subNetIdx].push_back(std::move(netRouteObjs[net][i]));
        }
      } else {
        vPins[subNetIdx].push_back(netPins[i - (int)netRouteObjs[net].size()]);
      }
    }

    for (int i = 0; i < numSubNets; i++) {
      initNet(net, vRouteObjs[i], vExtObjs[i], netOrigGuides[net], vPins[i]);
    }

  }
}

/*
void FlexDRWorker::initNet_termGenAp(drPin* dPin) {
  using namespace boost::polygon::operators;

  //bool enableOutput = true;
  bool enableOutput = false;
  bool hasOnTrack = false;
  auto routeBox = getRouteBox();
  Rectangle routeRect(routeBox.left(), routeBox.bottom(), routeBox.right(), routeBox.top());
  auto dPinTerm = dPin->getFrTerm();
  if (dPinTerm->typeId() == frcInstTerm) {
    frInstTerm *instTerm = static_cast<frInstTerm*>(dPinTerm);
    //frTransform xform;
    frInst *inst = instTerm->getInst();
    //frBox mbox;
    //
    //inst->getTransform(xform);
    //inst->getRefBlock()->getBoundaryBBox(mbox);
    //frPoint size(mbox.right(), mbox.top());
    //xform.updateXform(size);
    frTransform xform;
    inst->getUpdatedXform(xform);
    
    for (auto &pin: instTerm->getTerm()->getPins()) {
      auto pinPtr = pin.get();
      auto pinLayer2PolySet = pinPtr->getLayer2PolySet();
      std::map<frLayerNum, PolygonSet> layer2PolySet;
      // populate layer2PolySet
      for (auto layerIt = pinLayer2PolySet.begin(); layerIt != pinLayer2PolySet.end(); ++layerIt) {
        auto currLayerNum = layerIt->first;
        auto currPolySet = layerIt->second;
        std::vector<Rectangle> pinRects;
        boost::polygon::get_rectangles(pinRects, currPolySet);
        for (auto &pinRect: pinRects) {
          frBox tmpBox(xl(pinRect), yl(pinRect), xh(pinRect), yh(pinRect));
          tmpBox.transform(xform);
          Rectangle transformedRect(tmpBox.left(), tmpBox.bottom(), tmpBox.right(), tmpBox.top());
          layer2PolySet[currLayerNum] += transformedRect;
        }
      }
      // now let the work begin with transformed pin shapes
      for (auto layerIt = layer2PolySet.begin(); layerIt != layer2PolySet.end(); ++layerIt) {
        auto currLayerNum = layerIt->first;
        auto currPolyset = layerIt->second;
        std::vector<Rectangle> pinRects;
        boost::polygon::get_rectangles(pinRects, currPolyset);
        for (auto &pinRect: pinRects) {
          if (!intersect(pinRect, routeRect)) {
            continue;
          }
          Rectangle overlapRect;
          overlapRect = generalized_intersect(pinRect, routeRect);
          if (xl(overlapRect) == xl(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              hasOnTrack = true;
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(xl(overlapRect), trackLoc);
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                dPin->addAccessPattern(uap);
              }
            }
          }
          if (xh(overlapRect) == xh(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(xh(overlapRect), trackLoc);
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                if (!isInitDR() || yh(overlapRect) != yh(routeRect)) {
                  hasOnTrack = true;
                  dPin->addAccessPattern(uap);
                }
              }
            }
          }
          if (yl(overlapRect) == yl(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              hasOnTrack = true;
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(trackLoc, yl(overlapRect));
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                dPin->addAccessPattern(uap);
              }
            }
          }
          if (yh(overlapRect) == yh(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(trackLoc, yh(overlapRect));
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                if (!isInitDR() || xh(overlapRect) != xh(routeRect)) {
                  hasOnTrack = true;
                  dPin->addAccessPattern(uap);
                }
              }
            }
          }

        }

        // if there is no on track...
        if (!hasOnTrack) {
          for (auto &pinRect: pinRects) {
            if (!intersect(pinRect, routeRect)) {
              continue;
            }
            Rectangle overlapRect;
            overlapRect = generalized_intersect(pinRect, routeRect);
            if (xl(overlapRect) == xl(routeRect)) {
              // std::set<frCoord> trackLocs;
              // getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint(xl(overlapRect), (yl(overlapRect) + yh(overlapRect)) / 2);
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  dPin->addAccessPattern(uap);
                  break;
                // }
              // }
            }
            if (xh(overlapRect) == xh(routeRect)) {
              // std::set<frCoord> trackLocs;
              // getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint(xh(overlapRect), (yl(overlapRect) + yh(overlapRect)) / 2);
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  if (!isInitDR() || ((yl(overlapRect) + yh(overlapRect)) / 2) != yh(routeRect)) {
                    dPin->addAccessPattern(uap);
                    break;
                  }
                // }
              // }
            }
            if (yl(overlapRect) == yl(routeRect)) {
              // std::set<frCoord> trackLocs;
              // getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint((xl(overlapRect) + xh(overlapRect)) / 2, yl(overlapRect));
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  dPin->addAccessPattern(uap);
                  break;
                // }
              // }
            }
            if (yh(overlapRect) == yh(routeRect)) {
              std::set<frCoord> trackLocs;
              // getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint((xl(overlapRect) + xh(overlapRect)) / 2, yh(overlapRect));
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  if (!isInitDR() || ((xl(overlapRect) + xh(overlapRect)) / 2) != xh(routeRect)) {
                    dPin->addAccessPattern(uap);
                    break;
                  }
                // }
              // }
            }

          }
        }


      }
      
    }
  } else if (dPinTerm->typeId() == frcTerm) {
    frTerm *term = static_cast<frTerm*>(dPinTerm);
    for (auto &pin: term->getPins()) {
      auto pinPtr = pin.get();
      // auto pinLayer2PolySet = pinPtr->getLayer2PolySet();
      std::map<frLayerNum, PolygonSet> layer2PolySet = pinPtr->getLayer2PolySet();
      // populate layer2PolySet
      // for (auto layerIt = pinLayer2PolySet.begin(); layerIt != pinLayer2PolySet.end(); ++layerIt) {
      //   auto currLayerNum = layerIt->first;
      //   auto currPolySet = layerIt->second;
      //   std::vector<Rectangle> pinRects;
      //   boost::polygon::get_rectangles(pinRects, currPolySet);
      //   for (auto &pinRect: pinRects) {
      //     frBox tmpBox(xl(pinRect), yl(pinRect), xh(pinRect), yh(pinRect));
      //     tmpBox.transform(xform);
      //     Rectangle transformedRect(tmpBox.left(), tmpBox.bottom(), tmpBox.right(), tmpBox.top());
      //     layer2PolySet[currLayerNum] += transformedRect;
      //   }
      // }

      // now let the work begin with transformed pin shapes
      for (auto layerIt = layer2PolySet.begin(); layerIt != layer2PolySet.end(); ++layerIt) {
        auto currLayerNum = layerIt->first;
        auto currPolyset = layerIt->second;
        std::vector<Rectangle> pinRects;
        boost::polygon::get_rectangles(pinRects, currPolyset);
        for (auto &pinRect: pinRects) {
          if (!intersect(pinRect, routeRect)) {
            continue;
          }
          Rectangle overlapRect;
          overlapRect = generalized_intersect(pinRect, routeRect);
          if (xl(overlapRect) == xl(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              hasOnTrack = true;
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(xl(overlapRect), trackLoc);
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                dPin->addAccessPattern(uap);
              }
            }
          }
          if (xh(overlapRect) == xh(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(xh(overlapRect), trackLoc);
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                if (!isInitDR() || yh(overlapRect) != yh(routeRect)) {
                  hasOnTrack = true;
                  dPin->addAccessPattern(uap);
                }
              }
            }
          }
          if (yl(overlapRect) == yl(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              hasOnTrack = true;
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(trackLoc, yl(overlapRect));
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                dPin->addAccessPattern(uap);
              }
            }
          }
          if (yh(overlapRect) == yh(routeRect)) {
            std::set<frCoord> trackLocs;
            getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
            if (!trackLocs.empty()) {
              for (auto &trackLoc: trackLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                frPoint startPoint(trackLoc, yh(overlapRect));
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(startPoint);
                if (!isInitDR() || xh(overlapRect) != xh(routeRect)) {
                  hasOnTrack = true;
                  dPin->addAccessPattern(uap);
                }
              }
            }
          }

        }

        // if there is no on track...
        if (!hasOnTrack) {
          for (auto &pinRect: pinRects) {
            if (!intersect(pinRect, routeRect)) {
              continue;
            }
            Rectangle overlapRect;
            overlapRect = generalized_intersect(pinRect, routeRect);
            if (xl(overlapRect) == xl(routeRect)) {
              std::set<frCoord> trackLocs;
              // getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint(xl(overlapRect), (yl(overlapRect) + yh(overlapRect)) / 2);
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  dPin->addAccessPattern(uap);
                  break;
                // }
              // }
            }
            if (xh(overlapRect) == xh(routeRect)) {
              std::set<frCoord> trackLocs;
              // getTrackLocs(true, currLayerNum, yl(overlapRect), yh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint(xh(overlapRect), (yl(overlapRect) + yh(overlapRect)) / 2);
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  if (!isInitDR() || ((yl(overlapRect) + yh(overlapRect)) / 2) != yh(routeRect)) {
                    dPin->addAccessPattern(uap);
                    break;
                  }
                // }
              // }
            }
            if (yl(overlapRect) == yl(routeRect)) {
              std::set<frCoord> trackLocs;
              // getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint((xl(overlapRect) + xh(overlapRect)) / 2, yl(overlapRect));
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  dPin->addAccessPattern(uap);
                  break;
                // }
              // }
            }
            if (yh(overlapRect) == yh(routeRect)) {
              std::set<frCoord> trackLocs;
              // getTrackLocs(false, currLayerNum, xl(overlapRect), xh(overlapRect), trackLocs);
              // if (!trackLocs.empty()) {
                // for (auto &trackLoc: trackLocs) {
                  auto uap = std::make_unique<drAccessPattern>();
                  frPoint startPoint((xl(overlapRect) + xh(overlapRect)) / 2, yh(overlapRect));
                  uap->setBeginLayerNum(currLayerNum);
                  uap->setPoint(startPoint);
                  if (!isInitDR() || ((xl(overlapRect) + xh(overlapRect)) / 2) != xh(routeRect)) {
                    dPin->addAccessPattern(uap);
                    break;
                  }
                // }
              // }
            }

          }
        }


      }
      
    }
  } else {
    if (enableOutput) {
      std::cout << "Error: unexpected type in initNet_termGenAp";
    }
  }
}
*/

void FlexDRWorker::initNet_termGenAp_new(drPin* dPin) {
  using namespace boost::polygon::operators;

  //bool enableOutput = true;
  auto routeBox = getRouteBox();
  Rectangle routeRect(routeBox.left(), routeBox.bottom(), routeBox.right(), routeBox.top());
  Point routeRectCenter;
  center(routeRectCenter, routeRect);

  auto dPinTerm = dPin->getFrTerm();
  if (dPinTerm->typeId() == frcInstTerm) {
    auto instTerm = static_cast<frInstTerm*>(dPinTerm);
    auto inst = instTerm->getInst();
    frTransform xform;
    inst->getUpdatedXform(xform);

    for (auto &uPin:instTerm->getTerm()->getPins()) {
      auto pin = uPin.get();
      for (auto &uPinFig: pin->getFigs()) {
        auto pinFig = uPinFig.get();
        // horizontal tracks == yLocs
        std::set<frCoord> xLocs, yLocs;
        if (pinFig->typeId() == frcRect) {
          auto rpinRect = static_cast<frRect*>(pinFig);
          frLayerNum currLayerNum = rpinRect->getLayerNum();
          if (getTech()->getLayer(currLayerNum)->getType() != frLayerTypeEnum::ROUTING) {
            continue;
          }
          frRect instPinRect(*rpinRect);
          instPinRect.move(xform);
          frBox instPinRectBBox;
          instPinRect.getBBox(instPinRectBBox);
          Rectangle pinRect(instPinRectBBox.left(), instPinRectBBox.bottom(), instPinRectBBox.right(), instPinRectBBox.top());
          if (!boost::polygon::intersect(pinRect, routeRect)) {
            continue;
          }
          // pinRect now equals intersection of pinRect and routeRect
          auto currPrefRouteDir = getTech()->getLayer(currLayerNum)->getDir();
          // get intersecting tracks if any
          if (currPrefRouteDir == frcHorzPrefRoutingDir) {
            getTrackLocs(true, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
            if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
              getTrackLocs(false, currLayerNum + 2, xl(pinRect), xh(pinRect), xLocs);
            } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
              getTrackLocs(false, currLayerNum - 2, xl(pinRect), xh(pinRect), xLocs);
            } else {
              getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
            }
          } else {
            getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
            if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
              getTrackLocs(false, currLayerNum + 2, yl(pinRect), yh(pinRect), yLocs);
            } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
              getTrackLocs(false, currLayerNum - 2, yl(pinRect), yh(pinRect), yLocs);
            } else {
              getTrackLocs(false, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
            }
          }
          // gen new temp on-track access point if any
          if (!xLocs.empty() && !yLocs.empty()) {
            // TODO: update access pattern information as needed
            auto uap = std::make_unique<drAccessPattern>();
            auto xLoc =*(xLocs.begin());
            auto yLoc =*(yLocs.begin());
            frPoint pt(xLoc, yLoc);
            uap->setBeginLayerNum(currLayerNum);
            uap->setPoint(pt);
            // prevent unchecked planar access
            vector<bool> validAccess(6, true);
            validAccess[5] = false;
            if (currLayerNum == VIA_ACCESS_LAYERNUM) {
              validAccess[0] = false;
              validAccess[1] = false;
              validAccess[2] = false;
              validAccess[3] = false;
            }
            uap->setValidAccess(validAccess);
            uap->setOnTrack(false, true);
            uap->setOnTrack(false, false);
            uap->setPin(dPin);
            // any non-zero value works
            uap->setPinCost(7);
            // to resolve temp AP end seg minArea patch problem
            // frCoord reqArea = 0;
            auto minAreaConstraint = getDesign()->getTech()->getLayer(currLayerNum)->getAreaConstraint();
            if (minAreaConstraint) {
              auto reqArea = minAreaConstraint->getMinArea();
              uap->setBeginArea(reqArea);
            }

            if (!isInitDR() || (xLoc != xh(routeRect) && yLoc != yh(routeRect))) {
              dPin->addAccessPattern(uap);
              break;
            }
          }
        } else {
          cout << "Error: initNet_termGenAp_new unsupported pinFig\n";
        }
      }
      
      // no on-track temp ap found
      for (auto &uPinFig: pin->getFigs()) {
        auto pinFig = uPinFig.get();
        // horizontal tracks == yLocs
        std::set<frCoord> xLocs, yLocs;
        if (pinFig->typeId() == frcRect) {
          auto rpinRect = static_cast<frRect*>(pinFig);
          frLayerNum currLayerNum = rpinRect->getLayerNum();
          if (getTech()->getLayer(currLayerNum)->getType() != frLayerTypeEnum::ROUTING) {
            continue;
          }
          frRect instPinRect(*rpinRect);
          instPinRect.move(xform);
          frBox instPinRectBBox;
          instPinRect.getBBox(instPinRectBBox);
          Rectangle pinRect(instPinRectBBox.left(), instPinRectBBox.bottom(), instPinRectBBox.right(), instPinRectBBox.top());
          if (!boost::polygon::intersect(pinRect, routeRect)) {
            continue;
          }
          // pinRect now equals intersection of pinRect and routeRect
          auto currPrefRouteDir = getTech()->getLayer(currLayerNum)->getDir();
          // get intersecting tracks if any
          if (currPrefRouteDir == frcHorzPrefRoutingDir) {
            getTrackLocs(true, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
            if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
              getTrackLocs(false, currLayerNum + 2, xl(pinRect), xh(pinRect), xLocs);
            } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
              getTrackLocs(false, currLayerNum - 2, xl(pinRect), xh(pinRect), xLocs);
            } else {
              getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
            }
          } else {
            getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
            if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
              getTrackLocs(false, currLayerNum + 2, yl(pinRect), yh(pinRect), yLocs);
            } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
              getTrackLocs(false, currLayerNum - 2, yl(pinRect), yh(pinRect), yLocs);
            } else {
              getTrackLocs(false, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
            }
          }
          // gen new temp on-track access point if any
          frCoord xLoc, yLoc;
          // xLoc
          if (!xLocs.empty() && !isInitDR()) {
            xLoc = *(xLocs.begin());
          } else {
            xLoc = (xl(pinRect) + xh(pinRect)) / 2;
          }
          // xLoc
          if (!yLocs.empty() && !isInitDR()) {
            yLoc = *(yLocs.begin());
          } else {
            yLoc = (yl(pinRect) + yh(pinRect)) / 2;
          }
          if (!isInitDR() || (xLoc != xh(routeRect) || yLoc != yh(routeRect))) {
            // TODO: update as drAccessPattern updated
            auto uap = std::make_unique<drAccessPattern>();
            frPoint pt(xLoc, yLoc);
            uap->setBeginLayerNum(currLayerNum);
            uap->setPoint(pt);
            // prevent unchecked planar access
            vector<bool> validAccess(6, true);
            validAccess[5] = false;
            if (currLayerNum == VIA_ACCESS_LAYERNUM) {
              validAccess[0] = false;
              validAccess[1] = false;
              validAccess[2] = false;
              validAccess[3] = false;
            }
            uap->setValidAccess(validAccess);
            uap->setOnTrack(false, true);
            uap->setOnTrack(false, false);
            uap->setPin(dPin);
            // any non-zero value works
            uap->setPinCost(7);
            // to resolve temp AP end seg minArea patch problem
            // frCoord reqArea = 0;
            auto minAreaConstraint = getDesign()->getTech()->getLayer(currLayerNum)->getAreaConstraint();
            if (minAreaConstraint) {
              auto reqArea = minAreaConstraint->getMinArea();
              uap->setBeginArea(reqArea);
            }

            dPin->addAccessPattern(uap);
            break;
          }
        } else {
          cout << "Error: initNet_termGenAp_new unsupported pinFig\n";
        }
      }
    }
  } else if (dPinTerm->typeId() == frcTerm) {
    auto term = static_cast<frTerm*>(dPinTerm);
    for (auto &uPin: term->getPins()) {
      auto pin = uPin.get();
      bool hasTempAp = false;
      for (auto &uPinFig: pin->getFigs()) {
        auto pinFig = uPinFig.get();
        // horizontal tracks == yLocs
        std::set<frCoord> xLocs, yLocs;
        if (pinFig->typeId() == frcRect) {
          auto rpinRect = static_cast<frRect*>(pinFig);
          frLayerNum currLayerNum = rpinRect->getLayerNum();
          if (getTech()->getLayer(currLayerNum)->getType() != frLayerTypeEnum::ROUTING) {
            continue;
          }
          frRect instPinRect(*rpinRect);
          // instPinRect.move(xform);
          frBox instPinRectBBox;
          instPinRect.getBBox(instPinRectBBox);
          Rectangle pinRect(instPinRectBBox.left(), instPinRectBBox.bottom(), instPinRectBBox.right(), instPinRectBBox.top());
          if (!boost::polygon::intersect(pinRect, routeRect)) {
            continue;
          }
          // pinRect now equals intersection of pinRect and routeRect
          auto currPrefRouteDir = getTech()->getLayer(currLayerNum)->getDir();
          bool useCenterLine = true;
          auto xSpan = instPinRectBBox.right() - instPinRectBBox.left();
          auto ySpan = instPinRectBBox.top() - instPinRectBBox.bottom();
          bool isPinRectHorz = (xSpan > ySpan);
          // if ((isPinRectHorz && currPrefRouteDir == frcHorzPrefRoutingDir) ||
          //     (!isPinRectHorz && currPrefRouteDir == frcVertPrefRoutingDir)) {
          //   auto layerWidth = getTech()->getLayer(currLayerNum)->getWidth();
          //   if ((isPinRectHorz && instPinRectBBox.width() < 2 * layerWidth) ||
          //       (!isPinRectHorz && instPinRectBBox.width() < 2 * layerWidth)) {
          //     useCenterLine = true;
          //   }

          // }

          if (!useCenterLine) {
            // get intersecting tracks if any
            if (currPrefRouteDir == frcHorzPrefRoutingDir) {
              getTrackLocs(true, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
              if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
                getTrackLocs(false, currLayerNum + 2, xl(pinRect), xh(pinRect), xLocs);
              } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
                getTrackLocs(false, currLayerNum - 2, xl(pinRect), xh(pinRect), xLocs);
              } else {
                getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
              }
            } else {
              getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
              if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
                getTrackLocs(false, currLayerNum + 2, yl(pinRect), yh(pinRect), yLocs);
              } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
                getTrackLocs(false, currLayerNum - 2, yl(pinRect), yh(pinRect), yLocs);
              } else {
                getTrackLocs(false, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
              }
            }
          } else {
            int layerWidth = getTech()->getLayer(currLayerNum)->getWidth(); // for ISPD off track pins
            if (isPinRectHorz) {
              bool didUseCenterline = false;
              frCoord manuGrid = getDesign()->getTech()->getManufacturingGrid();
              auto centerY = (instPinRectBBox.top() + instPinRectBBox.bottom()) / 2 / manuGrid * manuGrid;
              if (centerY >= yl(routeRect) && centerY < yh(routeRect)) {
                yLocs.insert(centerY);
                didUseCenterline = true;
              }
              if (!didUseCenterline) {
                getTrackLocs(true, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
              }
              if (didUseCenterline) {
                if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
                  getTrackLocs(false, currLayerNum + 2, xl(pinRect), xh(pinRect), xLocs);
                } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
                  getTrackLocs(false, currLayerNum - 2, xl(pinRect), xh(pinRect), xLocs);
                } else {
                  getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
                }
              } else {
                frCoord lowerBoundX = xl(pinRect);
                frCoord upperBoundX = xh(pinRect);
                if ((upperBoundX - lowerBoundX) >= 2 * layerWidth) {
                  lowerBoundX += layerWidth;
                  upperBoundX -= layerWidth;
                }
                if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
                  getTrackLocs(false, currLayerNum + 2, lowerBoundX, upperBoundX, xLocs);
                } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
                  getTrackLocs(false, currLayerNum - 2, lowerBoundX, upperBoundX, xLocs);
                } else {
                  getTrackLocs(false, currLayerNum, lowerBoundX, upperBoundX, xLocs);
                }
              }
              
            } else {
              bool didUseCenterline = false;
              frCoord manuGrid = getDesign()->getTech()->getManufacturingGrid();
              auto centerX = (instPinRectBBox.left() + instPinRectBBox.right()) / 2 / manuGrid * manuGrid;
              if (centerX >= xl(routeRect) && centerX < xh(routeRect)) {
                xLocs.insert(centerX);
                didUseCenterline = true;
              }
              if (!didUseCenterline) {
                getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
              }
              if (didUseCenterline) {
                if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
                  getTrackLocs(false, currLayerNum + 2, yl(pinRect), yh(pinRect), yLocs);
                } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
                  getTrackLocs(false, currLayerNum - 2, yl(pinRect), yh(pinRect), yLocs);
                } else {
                  getTrackLocs(false, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
                }
              } else {
                frCoord lowerBoundY = yl(pinRect);
                frCoord upperBoundY = yh(pinRect);
                if ((upperBoundY - lowerBoundY) >= 2 * layerWidth) {
                  lowerBoundY += layerWidth;
                  upperBoundY -= layerWidth;
                  if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
                    getTrackLocs(false, currLayerNum + 2, lowerBoundY, upperBoundY, yLocs);
                  } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
                    getTrackLocs(false, currLayerNum - 2, lowerBoundY, upperBoundY, yLocs);
                  } else {
                    getTrackLocs(false, currLayerNum, lowerBoundY, upperBoundY, yLocs);
                  }
                }
              }
              
            }
          }


          // gen new temp on-track access point if any
          if (!xLocs.empty() && !yLocs.empty()) {
            // TODO: update access pattern information as needed
            for (auto xLoc: xLocs) {
              for (auto yLoc: yLocs) {
                auto uap = std::make_unique<drAccessPattern>();
                // auto xLoc =*(xLocs.begin());
                // auto yLoc =*(yLocs.begin());
                frPoint pt(xLoc, yLoc);
                uap->setBeginLayerNum(currLayerNum);
                uap->setPoint(pt);
                // to resolve temp AP end seg minArea patch problem
                // frCoord reqArea = 0;
                auto minAreaConstraint = getDesign()->getTech()->getLayer(currLayerNum)->getAreaConstraint();
                if (minAreaConstraint) {
                  auto reqArea = minAreaConstraint->getMinArea();
                  uap->setBeginArea(reqArea);
                }
                // io pin pref direction setting
                // Point pinCenter;
                // center(pinCenter, pinRect);
                vector<bool> validAccess(6, false);
                if (isPinRectHorz) {
                    validAccess[0] = true;
                    validAccess[2] = true;
                } else {
                    validAccess[3] = true;
                    validAccess[1] = true;
                }
                uap->setValidAccess(validAccess);
                uap->setPin(dPin);
                if (!isInitDR() || xLoc != xh(routeRect) || yLoc != yh(routeRect)) {
                  if (xLoc >= xl(routeRect) && xLoc < xh(routeRect) && yLoc >= yl(routeRect) && yLoc < yh(routeRect)) {
                    hasTempAp = true;
                    dPin->addAccessPattern(uap);
                    // if (term->getName() == string("pin128")) {
                    //   cout << "@@@ debug @@@: pin128 temp AP loc (" << xLoc / 2000.0 << ", " << yLoc / 2000.0 << ")\n";
                    // }
                    // break;
                  }
                }
              }
            }

            if (hasTempAp) {
              break;
            }

          }
        } else {
          cout << "Error: initNet_termGenAp_new unsupported pinFig\n";
        }
      }
      
      // no on-track temp ap found
      if (!hasTempAp) {
        for (auto &uPinFig: pin->getFigs()) {
          auto pinFig = uPinFig.get();
          // horizontal tracks == yLocs
          std::set<frCoord> xLocs, yLocs;
          if (pinFig->typeId() == frcRect) {
            auto rpinRect = static_cast<frRect*>(pinFig);
            frLayerNum currLayerNum = rpinRect->getLayerNum();
            if (getTech()->getLayer(currLayerNum)->getType() != frLayerTypeEnum::ROUTING) {
              continue;
            }
            frRect instPinRect(*rpinRect);
            // instPinRect.move(xform);
            frBox instPinRectBBox;
            instPinRect.getBBox(instPinRectBBox);
            Rectangle pinRect(instPinRectBBox.left(), instPinRectBBox.bottom(), instPinRectBBox.right(), instPinRectBBox.top());
            if (!boost::polygon::intersect(pinRect, routeRect)) {
              continue;
            }
            // pinRect now equals intersection of pinRect and routeRect

            // auto currPrefRouteDir = getTech()->getLayer(currLayerNum)->getDir();
            // // get intersecting tracks if any
            // if (currPrefRouteDir == frcHorzPrefRoutingDir) {
            //   getTrackLocs(true, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
            //   if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
            //     getTrackLocs(false, currLayerNum + 2, xl(pinRect), xh(pinRect), xLocs);
            //   } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
            //     getTrackLocs(false, currLayerNum - 2, xl(pinRect), xh(pinRect), xLocs);
            //   } else {
            //     getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
            //   }
            // } else {
            //   getTrackLocs(false, currLayerNum, xl(pinRect), xh(pinRect), xLocs);
            //   if (currLayerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
            //     getTrackLocs(false, currLayerNum + 2, yl(pinRect), yh(pinRect), yLocs);
            //   } else if (currLayerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
            //     getTrackLocs(false, currLayerNum - 2, yl(pinRect), yh(pinRect), yLocs);
            //   } else {
            //     getTrackLocs(false, currLayerNum, yl(pinRect), yh(pinRect), yLocs);
            //   }
            // }
            // // gen new temp on-track access point if any
            // frCoord xLoc, yLoc;
            // // xLoc
            // if (!xLocs.empty()) {
            //   xLoc = *(xLocs.begin());
            // } else {
            //   xLoc = (xl(pinRect) + xh(pinRect)) / 2;
            // }


            frCoord xLoc, yLoc;
            auto instPinCenterX = (instPinRectBBox.left() + instPinRectBBox.right()) / 2;
            auto instPinCenterY = (instPinRectBBox.bottom() + instPinRectBBox.top()) / 2;
            auto pinCenterX = (xl(pinRect) + xh(pinRect)) / 2;
            auto pinCenterY = (yl(pinRect) + yh(pinRect)) / 2;
            if (instPinCenterX >= xl(routeRect) && instPinCenterX < xh(routeRect)) {
              xLoc = instPinCenterX;
            } else {
              xLoc = pinCenterX;
            }
            if (instPinCenterY >= yl(routeRect) && instPinCenterY < yh(routeRect)) {
              yLoc = instPinCenterY;
            } else {
              yLoc = pinCenterY;
            }

            if (!isInitDR() || xLoc != xh(routeRect) || yLoc != yh(routeRect)) {
              // TODO: update as drAccessPattern updated
              auto uap = std::make_unique<drAccessPattern>();
              frPoint pt(xLoc, yLoc);
              uap->setBeginLayerNum(currLayerNum);
              uap->setPoint(pt);
              uap->setPin(dPin);
              // to resolve temp AP end seg minArea patch problem
              // frCoord reqArea = 0;
              auto minAreaConstraint = getDesign()->getTech()->getLayer(currLayerNum)->getAreaConstraint();
              if (minAreaConstraint) {
                auto reqArea = minAreaConstraint->getMinArea();
                uap->setBeginArea(reqArea);
              }
              // if (dPinTerm->typeId() == frcTerm) {
              //   // io pin pref direction setting
              //   Point pinCenter;
              //   center(pinCenter, pinRect);
              //   vector<bool> validAccess(6, true);
              //   validAccess[4] = false;
              //   validAccess[5] = false;
              //   if (getDesign()->getTech()->getLayer(currLayerNum)->getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir) {
              //     validAccess[1] = false;
              //     validAccess[3] = false;
              //   } else {
              //     validAccess[0] = false;
              //     validAccess[2] = false;
              //   }
              //   uap->setValidAccess(validAccess);
              // }

              dPin->addAccessPattern(uap);
              break;
            }
          } else {
            cout << "Error: initNet_termGenAp_new unsupported pinFig\n";
          }
        }
      }
    }
  } else {
    cout << "Error: initNet_termGenAp_new unexpected type\n";
  }
}

// when isHorzTracks == true, it means track loc == y loc
void FlexDRWorker::getTrackLocs(bool isHorzTracks, frLayerNum currLayerNum, frCoord low, frCoord high, std::set<frCoord> &trackLocs) {
  frPrefRoutingDirEnum currPrefRouteDir = getTech()->getLayer(currLayerNum)->getDir(); 
  for (auto &tp: design->getTopBlock()->getTrackPatterns(currLayerNum)) {
    if (tp->isHorizontal() && currPrefRouteDir == frcVertPrefRoutingDir ||
       !tp->isHorizontal() && currPrefRouteDir == frcHorzPrefRoutingDir) {
      int trackNum = (low - tp->getStartCoord()) / (int)tp->getTrackSpacing();
      if (trackNum < 0) {
        trackNum = 0;
      }
      if (trackNum * (int)tp->getTrackSpacing() + tp->getStartCoord() < low) {
        ++trackNum;
      }
      for (; 
           trackNum < (int)tp->getNumTracks() && 
           trackNum * (int)tp->getTrackSpacing() + tp->getStartCoord() <= high; 
           ++trackNum) {
        frCoord trackLoc = trackNum * tp->getTrackSpacing() + tp->getStartCoord();
        trackLocs.insert(trackLoc);
        if (tp->isHorizontal() && !isHorzTracks) {
          trackLocs.insert(trackLoc);
        } else if (!tp->isHorizontal() && isHorzTracks) {
          trackLocs.insert(trackLoc);
        } else {
          continue;
        }
      }
    }
  }
}

/*
void FlexDRWorker::initNet_term(drNet* dNet, vector<frBlockObject*> &terms) {
  bool enableOutput = false;
  //bool enableOutput = true;
  for (auto term: terms) {
    auto dPin = make_unique<drPin>();
    dPin->setFrTerm(term);
    // ap
    frTransform instXform; // (0,0), frcR0
    frTransform shiftXform;
    frTerm* trueTerm = nullptr;
    string  name;
    bool hasInst = false;
    frInst* inst = nullptr;
    if (term->typeId() == frcInstTerm) {
      hasInst = true;
      inst = static_cast<frInstTerm*>(term)->getInst();
      //inst->getTransform(instXform);
      inst->getTransform(shiftXform);
      shiftXform.set(frOrient(frcR0));
      inst->getUpdatedXform(instXform);
      //inst->getUpdatedXform(shiftXform, true); // get no orient version
      trueTerm = static_cast<frInstTerm*>(term)->getTerm();
      name = inst->getName() + string("/") + trueTerm->getName();
    } else if (term->typeId() == frcTerm) {
      trueTerm = static_cast<frTerm*>(term);
      name = string("PIN/") + trueTerm->getName();
    }
    if (enableOutput) {
      cout <<"pin " <<name;
    }
    for (auto &pin: trueTerm->getPins()) {
      for (auto &ap: pin->getAccessPatterns(instXform.orient())) {
        if (hasInst && !(ap->hasInst(inst))) {
          continue;
        }
        if (ap->isConflict()) {
          continue;
        }
        frPoint bp, ep;
        ap->getPoints(bp, ep);
        if (enableOutput) {
          cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                      <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") origin";
        }
        auto bNum = ap->getBeginLayerNum();
        bp.transform(shiftXform);

        auto dAp  = make_unique<drAccessPattern>();
        dAp->setPoint(bp);
        dAp->setBeginLayerNum(bNum);
        // set min area
        if (ENABLE_BOUNDARY_MAR_FIX) {
          auto minAreaConstraint = getDesign()->getTech()->getLayer(bNum)->getAreaConstraint();
          if (minAreaConstraint) {
            auto reqArea = minAreaConstraint->getMinArea();
            dAp->setBeginArea(reqArea);
          }
        }
        dAp->setValidAccess(ap->getValidAccess());
        if (!(ap->getAccessViaDef(frDirEnum::U).empty())) {
          dAp->setAccessViaDef(frDirEnum::U, &(ap->getAccessViaDef(frDirEnum::U)));
        }
        if (!(ap->getAccessViaDef(frDirEnum::D).empty())) {
          dAp->setAccessViaDef(frDirEnum::D, &(ap->getAccessViaDef(frDirEnum::D)));
        }
        if (getRouteBox().contains(bp)) {
          dPin->addAccessPattern(dAp);
          if (enableOutput) {
            cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                        <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") added";
          }
        } else {
          if (enableOutput) {
            cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                        <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") skipped";
          }
        }
      }
    }


    if (enableOutput) {
       cout <<endl;
       cout <<"ap size = " <<dPin->getAccessPatterns().size() <<endl;
    }

    if (dPin->getAccessPatterns().empty()) {
      if (enableOutput) {
        cout <<"Warning: pin " <<name <<" does not have pre-calculated ap, gen temp ap ";
        //for (auto &[bp, bNum]: pts) {
        //  cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
        //              <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") ";
        //}
        //cout <<endl;
      }
      initNet_termGenAp(dPin.get());
      if (dPin->getAccessPatterns().empty()) {
        cout <<endl <<"Error: pin " <<name <<" still does not have temp ap" <<endl;
        exit(1);
      } else {
        if (enableOutput) {
          for (auto &ap: dPin->getAccessPatterns()) {
            frPoint bp;
            ap->getPoint(bp);
            cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                        <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") ";
          }
          cout <<endl;
        }
      }
    }
    dPin->setId(pinCnt);
    pinCnt++;
    dNet->addPin(dPin);
  }
}
*/

void FlexDRWorker::initNet_term_new(drNet* dNet, vector<frBlockObject*> &terms) {
  bool enableOutput = false;
  //bool enableOutput = true;
  for (auto term: terms) {
    auto dPin = make_unique<drPin>();
    dPin->setFrTerm(term);
    // ap
    frTransform instXform; // (0,0), frcR0
    frTransform shiftXform;
    frTerm* trueTerm = nullptr;
    string  name;
    //bool hasInst = false;
    frInst* inst = nullptr;
    //vector<frAccessPoint*> *instTermPrefAps = nullptr;
    if (term->typeId() == frcInstTerm) {
      //hasInst = true;
      inst = static_cast<frInstTerm*>(term)->getInst();
      //inst->getTransform(instXform);
      inst->getTransform(shiftXform);
      shiftXform.set(frOrient(frcR0));
      inst->getUpdatedXform(instXform);
      //inst->getUpdatedXform(shiftXform, true); // get no orient version
      trueTerm = static_cast<frInstTerm*>(term)->getTerm();
      name = inst->getName() + string("/") + trueTerm->getName();
      //instTermPrefAps = static_cast<frInstTerm*>(term)->getAccessPoints();
    } else if (term->typeId() == frcTerm) {
      trueTerm = static_cast<frTerm*>(term);
      name = string("PIN/") + trueTerm->getName();
    }
    if (enableOutput) {
      cout <<"pin " <<name;
    }
    int pinIdx = 0;
    int pinAccessIdx = (inst) ? inst->getPinAccessIdx() : -1;
    for (auto &pin: trueTerm->getPins()) {
      frAccessPoint* prefAp = nullptr;
      if (inst) {
        prefAp = (static_cast<frInstTerm*>(term)->getAccessPoints())[pinIdx];
      }
      if (!pin->hasPinAccess()) {
        continue;
      }
      if (pinAccessIdx == -1) {
        continue;
      }
      for (auto &ap: pin->getPinAccess(pinAccessIdx)->getAccessPoints()) {
        frPoint bp;
        ap->getPoint(bp);
        if (enableOutput) {
          cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                      <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") origin";
        }
        auto bNum = ap->getLayerNum();
        auto bLayer = getDesign()->getTech()->getLayer(bNum);
        bp.transform(shiftXform);

        auto dAp  = make_unique<drAccessPattern>();
        dAp->setPoint(bp);
        // if (term->typeId() == frcInstTerm) {
        //   inst = static_cast<frInstTerm*>(term)->getInst();
        //   trueTerm = static_cast<frInstTerm*>(term)->getTerm();
        //   if (inst->getRefBlock()->getName() == string("DFFSQ_X1N_A10P5PP84TR_C14_mod") && trueTerm->getName() == string("Q")) {
        //     cout << "@@@191028: (" << bp.x() / 2000.0 << ", " << bp.y() / 2000.0 << "), routeBox = (" 
        //          << getRouteBox().left() / 2000.0 << ", " << getRouteBox().bottom() / 2000.0 << ") - ("
        //          << getRouteBox().right() / 2000.0 << ", " << getRouteBox().top() / 2000.0 << ")\n";
        //   }
        // }
        dAp->setBeginLayerNum(bNum);
        if (ap.get() == prefAp) {
          dAp->setPinCost(0);
        } else {
          dAp->setPinCost(1);
        }
        // set min area
        if (ENABLE_BOUNDARY_MAR_FIX) {
          auto minAreaConstraint = getDesign()->getTech()->getLayer(bNum)->getAreaConstraint();
          if (minAreaConstraint) {
            auto reqArea = minAreaConstraint->getMinArea();
            dAp->setBeginArea(reqArea);
          }
        }
        dAp->setValidAccess(ap->getAccess());
        if (ap->hasAccess(frDirEnum::U)) {
          if (!(ap->getViaDefs().empty())) {
            dAp->setAccessViaDef(frDirEnum::U, &(ap->getViaDefs()));
          }
        }
        //if (!(ap->getAccessViaDef(frDirEnum::D).empty())) {
        //  dAp->setAccessViaDef(frDirEnum::D, &(ap->getAccessViaDef(frDirEnum::D)));
        //}
        if (getRouteBox().contains(bp)) {
          if (isInitDR() && getRouteBox().right() == bp.x() && getRouteBox().top() == bp.y()) {
            if (enableOutput) {
              cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                          <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") skipped";
            }
          } else if ((getRouteBox().right() == bp.x() && bLayer->getDir() == frcVertPrefRoutingDir && bLayer->getLef58RectOnlyConstraint()) || 
                     (getRouteBox().top() == bp.y() && bLayer->getDir() == frcHorzPrefRoutingDir && bLayer->getLef58RectOnlyConstraint())) {
            if (enableOutput) {
              cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                          <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") skipped";
            }
          } else {
            dPin->addAccessPattern(dAp);
            if (enableOutput) {
              cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                          <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") added";
            }
          }
        } else {
          if (enableOutput) {
            cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                        <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") skipped";
          }
        }
      }
      pinIdx++;
    }


    if (enableOutput) {
       cout <<endl;
       cout <<"ap size = " <<dPin->getAccessPatterns().size() <<endl;
    }

    if (dPin->getAccessPatterns().empty()) {
      if (enableOutput) {
        cout <<"Warning: pin " <<name <<" does not have pre-calculated ap, gen temp ap ";
        //for (auto &[bp, bNum]: pts) {
        //  cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
        //              <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") ";
        //}
        //cout <<endl;
      }
      initNet_termGenAp_new(dPin.get());
      if (dPin->getAccessPatterns().empty()) {
        cout <<endl <<"Error: pin " <<name <<" still does not have temp ap" <<endl;
        exit(1);
      } else {
        if (enableOutput) {
          for (auto &ap: dPin->getAccessPatterns()) {
            frPoint bp;
            ap->getPoint(bp);
            cout <<" temp ap (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
                        <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") ";
          }
          cout <<endl;
        }
        // if (dNet->getFrNet()->getName() == string("iopin242")) {
        //   cout <<"@@@@@debug ";
        //   if (term->typeId() == frcInstTerm) {
        //     auto inst = static_cast<frInstTerm*>(term)->getInst();
        //     auto trueTerm = static_cast<frInstTerm*>(term)->getTerm();
        //     name = inst->getName() + string("/") + trueTerm->getName();
        //   } else if (term->typeId() == frcTerm) {
        //     trueTerm = static_cast<frTerm*>(term);
        //     name = string("PIN/") + trueTerm->getName();
        //   }
        //   for (auto &ap: dPin->getAccessPatterns()) {
        //     frPoint bp;
        //     ap->getPoint(bp);
        //       cout <<" (" <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
        //                   <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") ";
        //   }
        //   cout <<endl;
        // }
      }
    }
    dPin->setId(pinCnt);
    pinCnt++;
    dNet->addPin(dPin);
  }
}

void FlexDRWorker::initNet_boundary(drNet* dNet, 
                                    vector<unique_ptr<drConnFig> > &extObjs) {
  //bool enableOutput = true;
  bool enableOutput = false;
  //if (!isInitDR() && dNet->getFrNet()->getName() == string("net14488")) {
  //if (!isInitDR() && dNet->getFrNet()->getName() == string("net100629")) {
  //  enableOutput = true;
  //  cout <<"here" <<endl;
  //}
  auto gridBBox = getRouteBox();
  // location to area
  map<pair<frPoint, frLayerNum>, frCoord> extBounds;
  frSegStyle segStyle;
  frCoord currArea = 0;
  if (!isInitDR()) {
    for (auto &obj: extObjs) {
      if (obj->typeId() == drcPathSeg) {
        auto ps = static_cast<drPathSeg*>(obj.get());
        frPoint begin, end;
        ps->getPoints(begin, end);
        // set begin area 
        // moved to initNets_boundaryArea
        //if (ENABLE_BOUNDARY_MAR_FIX) {
        //  ps->getStyle(segStyle);
        //  currArea = segStyle.getWidth() * (end.x() - begin.x() + end.y() - begin.y());
        //}
        frLayerNum lNum = ps->getLayerNum();
        // vert pathseg
        if (begin.x() == end.x() && begin.x() >= gridBBox.left() && end.x() <= gridBBox.right()) {
          if (begin.y() == gridBBox.top()) {
            if (enableOutput) {
              cout << "top bound (" 
                   << begin.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ", " 
                   << begin.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ") - (" 
                   << end.x()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ", " 
                   << end.y()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ") " 
                   << getTech()->getLayer(lNum)->getName() <<"\n";
            }
            //extBounds.insert(make_pair(begin, lNum));
            extBounds[make_pair(begin, lNum)] = currArea;
          }
          if (end.y() == gridBBox.bottom()) {
            if (enableOutput) {
              cout << "bottom bound ("
                   << begin.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ", " 
                   << begin.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ") - (" 
                   << end.x()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ", " 
                   << end.y()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ") " 
                   << getTech()->getLayer(lNum)->getName() <<"\n";
            }
            //extBounds.insert(make_pair(end, lNum));
            extBounds[make_pair(end, lNum)] = currArea;
          }
        // horz pathseg
        } else if (begin.y() == end.y() && begin.y() >= gridBBox.bottom() && end.y() <= gridBBox.top()) {
          if (begin.x() == gridBBox.right()) {
            if (enableOutput) {
              cout << "right bound ("
                   << begin.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ", " 
                   << begin.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ") - (" 
                   << end.x()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ", " 
                   << end.y()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ") " 
                   << getTech()->getLayer(lNum)->getName() <<"\n";
            }
            //extBounds.insert(make_pair(begin, lNum));
            extBounds[make_pair(begin, lNum)] = currArea;
          }
          if (end.x() == gridBBox.left()) {
            if (enableOutput) {
              cout << "left bound (" 
                   << begin.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ", " 
                   << begin.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ") - (" 
                   << end.x()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ", " 
                   << end.y()   * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()<< ") " 
                   << getTech()->getLayer(lNum)->getName() <<"\n";
            }
            //extBounds.insert(make_pair(end, lNum));
            extBounds[make_pair(end, lNum)] = currArea;
          }
        }
      }
    }
  // initDR
  } else {
    auto it = boundaryPin.find(dNet->getFrNet());
    //cout <<string(dNet->getFrNet() == nullptr ? "null" : dNet->getFrNet()->getName()) <<endl;
    if (it != boundaryPin.end()) {
      //cout <<"here" <<endl;
      //extBounds = it->second;
      transform(it->second.begin(), it->second.end(), inserter(extBounds, extBounds.end()), 
                [](const pair<frPoint, frLayerNum> &pr) {
                  return make_pair(pr, 0);
                });
      if (enableOutput) {
        for (auto &[pt, lNum]: it->second) {
          cout << "init bound (" 
               << pt.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ", " 
               << pt.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() << ") " 
               << getTech()->getLayer(lNum)->getName() <<"\n";
        }
      }
    }
  }
  //for (auto &[pt, lNum]: extBounds) {
  for (auto &[pr, area]: extBounds) {
    auto &[pt, lNum] = pr;
    auto dPin = make_unique<drPin>();
    auto dAp  = make_unique<drAccessPattern>();
    dAp->setPoint(pt);
    dAp->setBeginLayerNum(lNum);
    // set begin area
    //if (ENABLE_BOUNDARY_MAR_FIX) {
    //  dAp->setBeginArea(area);
    //}
    dPin->addAccessPattern(dAp);
    // ap
    dPin->setId(pinCnt);
    pinCnt++;
    dNet->addPin(dPin);
  }
}

void FlexDRWorker::initNet_addNet(unique_ptr<drNet> &in) {
  owner2nets[in->getFrNet()].push_back(in.get());
  nets.push_back(std::move(in));
}

void FlexDRWorker::initNet(frNet* net, 
                           vector<unique_ptr<drConnFig> > &routeObjs,
                           vector<unique_ptr<drConnFig> > &extObjs,
                           vector<frRect> &origGuides,
                           vector<frBlockObject*> &terms) {
  //bool enableOutput = false;
  //bool enableOutput = true;
  auto dNet = make_unique<drNet>();
  dNet->setFrNet(net);
  // true pin
  initNet_term_new(dNet.get(), terms);
  // boundary pin, could overlap with any of true pins
  initNet_boundary(dNet.get(), extObjs);
  // no ext routes in initDR to avoid weird TA shapes
  for (auto &obj: extObjs) {
    dNet->addRoute(obj, true);
  }
  if (getRipupMode() == 0) {
    for (auto &obj: routeObjs) {
      dNet->addRoute(obj, false);
    }
  }
  dNet->setOrigGuides(origGuides);
  dNet->setId(nets.size());
  initNet_addNet(dNet);
  //nets.push_back(std::move(dNet));
}

void FlexDRWorker::initNets_regionQuery() {
  auto &workerRegionQuery = getWorkerRegionQuery();
  workerRegionQuery.init();
}

void FlexDRWorker::initNets_numPinsIn() {
  vector<rq_rptr_value_t<drPin> > allPins;
  frPoint pt;
  for (auto &net: nets) {
    for (auto &pin: net->getPins()) {
      bool hasPrefAP = false;
      drAccessPattern *firstAP = nullptr;
      for (auto &ap: pin->getAccessPatterns()) {
        if (firstAP == nullptr) {
          firstAP = ap.get();
        }
        if (ap->getPinCost() == 0) {
          ap->getPoint(pt);
          allPins.push_back(make_pair(box_t(point_t(pt.x(), pt.y()), point_t(pt.x(), pt.y())), pin.get()));
          hasPrefAP = true;
          break;
        }
      }
      if (!hasPrefAP) {
        firstAP->getPoint(pt);
        allPins.push_back(make_pair(box_t(point_t(pt.x(), pt.y()), point_t(pt.x(), pt.y())), pin.get()));
      }
    }
  }
  bgi::rtree<rq_rptr_value_t<drPin>, bgi::quadratic<16> > pinRegionQuery(allPins);
  for (auto &net: nets) {
    frCoord x1 = getExtBox().right();
    frCoord x2 = getExtBox().left();
    frCoord y1 = getExtBox().top();
    frCoord y2 = getExtBox().bottom();
    for (auto &pin: net->getPins()) {
      bool hasPrefAP = false;
      drAccessPattern *firstAP = nullptr;
      for (auto &ap: pin->getAccessPatterns()) {
        if (firstAP == nullptr) {
          firstAP = ap.get();
        }
        if (ap->getPinCost() == 0) {
          ap->getPoint(pt);
          hasPrefAP = true;
          break;
        }
      }
      if (!hasPrefAP) {
        firstAP->getPoint(pt);
      }

      if (pt.x() < x1) {
        x1 = pt.x();
      }
      if (pt.x() > x2) {
        x2 = pt.x();
      }
      if (pt.y() < y1) {
        y1 = pt.y();
      }
      if (pt.y() > y2) {
        y2 = pt.y();
      }
    }
    //x1++;
    //x2--;
    //y1++;
    //y2--;
    if (x1 <= x2 && y1 <= y2) {
      box_t boostb = box_t(point_t(x1, y1), point_t(x2, y2));
      allPins.clear();
      pinRegionQuery.query(bgi::intersects(boostb), back_inserter(allPins));
      net->setNumPinsIn(allPins.size());
      frBox tmpBox(x1, y1, x2, y2);
      net->setPinBox(tmpBox);
    } else {
      net->setNumPinsIn(99999);
      net->setPinBox(getExtBox());
    }
  }
}

void FlexDRWorker::initNets_boundaryArea() {
  frPoint bp, psBp, psEp, pt2/*, psBp2, psEp2*/;
  frLayerNum lNum;
  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_rptr_value_t<drConnFig> > results;
  vector<rq_rptr_value_t<drConnFig> > results2;
  frBox queryBox;
  frBox queryBox2;
  frCoord currArea = 0;
  frSegStyle segStyle;
  frBox viaBox2;
  //frSegStyle segStyle2;
  for (auto &uNet: nets) {
    auto net = uNet.get();
    for (auto &pin: net->getPins()) {
      if (pin->hasFrTerm()) {
        continue;
      }
      for (auto &ap: pin->getAccessPatterns()) {
        // initialization
        results.clear();
        currArea = 0;

        ap->getPoint(bp);
        lNum = ap->getBeginLayerNum();
        queryBox.set(bp, bp);
        workerRegionQuery.query(queryBox, lNum, results);
        for (auto &[boostB, connFig]: results) {
          if (connFig->getNet() != net) {
            continue;
          }
          if (connFig->typeId() == drcPathSeg) {
            auto obj = static_cast<drPathSeg*>(connFig);
            obj->getPoints(psBp, psEp);
            // psEp is outside
            if (bp == psBp && (!getRouteBox().contains(psEp))) {
              // calc area
              obj->getStyle(segStyle);
              currArea += (abs(psEp.x() - psBp.x()) + abs(psEp.y() - psBp.y())) * segStyle.getWidth();
              results2.clear();
              queryBox2.set(psEp, psEp);
              workerRegionQuery.query(queryBox2, lNum, results2);
              for (auto &[boostB2, connFig2]: results) {
                if (connFig2->getNet() != net) {
                  continue;
                }
                //if (connFig2->typeId() == drcPathSeg) {
                //  auto obj2 = static_cast<drPathSeg*>(connFig2);
                //  obj2->getPoints(psBp2, psEp2);
                //  if (psBp2 == psBp && psEp2 == psEp) {
                //    continue;
                //  }
                //  obj2->getStyle(segStyle2);
                //  currArea += (abs(psEp2.x() - psBp2.x()) + abs(psEp2.y() - psBp2.y())) * segStyle2.getWidth();
                //}
                if (connFig2->typeId() == drcVia) {
                  auto obj2 = static_cast<drVia*>(connFig2);
                  obj2->getOrigin(pt2);
                  if (pt2 == psEp) {
                    viaBox2.set(boostB2.min_corner().x(), boostB2.min_corner().y(), 
                                boostB2.max_corner().x(), boostB2.max_corner().y());
                    currArea += viaBox2.width() * viaBox2.length() / 2;
                    break;
                  }
                } else if (connFig2->typeId() == drcPatchWire) {
                  auto obj2 = static_cast<drPatchWire*>(connFig2);
                  obj2->getOrigin(pt2);
                  if (pt2 == psEp) {
                    viaBox2.set(boostB2.min_corner().x(), boostB2.min_corner().y(), 
                                boostB2.max_corner().x(), boostB2.max_corner().y());
                    //currArea += viaBox2.width() * viaBox2.length() / 2;
                    currArea += viaBox2.width() * viaBox2.length(); // patch wire no need / 2
                    break;
                  }
                }
              }
            }
            // psBp is outside
            if ((!getRouteBox().contains(psBp)) && bp == psEp) {
              // calc area
              obj->getStyle(segStyle);
              currArea += (abs(psEp.x() - psBp.x()) + abs(psEp.y() - psBp.y())) * segStyle.getWidth();
              results2.clear();
              queryBox2.set(psEp, psEp);
              workerRegionQuery.query(queryBox2, lNum, results2);
              for (auto &[boostB2, connFig2]: results) {
                if (connFig2->getNet() != net) {
                  continue;
                }
                //if (connFig2->typeId() == drcPathSeg) {
                //  auto obj2 = static_cast<drPathSeg*>(connFig2);
                //  obj2->getPoints(psBp2, psEp2);
                //  if (psBp2 == psBp && psEp2 == psEp) {
                //    continue;
                //  }
                //  obj2->getStyle(segStyle2);
                //  currArea += (abs(psEp2.x() - psBp2.x()) + abs(psEp2.y() - psBp2.y())) * segStyle2.getWidth();
                //}
                if (connFig2->typeId() == drcVia) {
                  auto obj2 = static_cast<drVia*>(connFig2);
                  obj2->getOrigin(pt2);
                  if (pt2 == psBp) {
                    viaBox2.set(boostB2.min_corner().x(), boostB2.min_corner().y(), 
                                boostB2.max_corner().x(), boostB2.max_corner().y());
                    currArea += viaBox2.width() * viaBox2.length() / 2;
                    break;
                  }
                } else if (connFig2->typeId() == drcPatchWire) {
                  auto obj2 = static_cast<drPatchWire*>(connFig2);
                  obj2->getOrigin(pt2);
                  if (pt2 == psBp) {
                    viaBox2.set(boostB2.min_corner().x(), boostB2.min_corner().y(), 
                                boostB2.max_corner().x(), boostB2.max_corner().y());
                    //currArea += viaBox2.width() * viaBox2.length() / 2;
                    currArea += viaBox2.width() * viaBox2.length();
                    break;
                  }
                }
              }
            }
          }
        }
        ap->setBeginArea(currArea);
      }
    }
  }
}

void FlexDRWorker::initNets() {
  set<frNet*, frBlockObjectComp>                                      nets;
  map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp>      netRouteObjs;
  map<frNet*, vector<unique_ptr<drConnFig> >, frBlockObjectComp>      netExtObjs;
  map<frNet*, vector<frRect>, frBlockObjectComp>                      netOrigGuides;
  // get lock
  initNetObjs(nets, netRouteObjs, netExtObjs, netOrigGuides);
  // release lock
  if (isInitDR()) {
    initNets_initDR(nets, netRouteObjs, netExtObjs, netOrigGuides);
  } else {
    // find inteTerm/terms using netRouteObjs;
    initNets_searchRepair(nets, netRouteObjs, netExtObjs, netOrigGuides);
  }
  initNets_regionQuery();
  initNets_numPinsIn();
  // here because region query is needed
  if (ENABLE_BOUNDARY_MAR_FIX) {
    initNets_boundaryArea();
  }
}

void FlexDRWorker::initTrackCoords_route(drNet* net, 
                                         map<frCoord, map<frLayerNum, frTrackPattern*> > &xMap,
                                         map<frCoord, map<frLayerNum, frTrackPattern*> > &yMap) {
  //auto rbox = getRouteBox();
  //auto ebox = getExtBox();
  // add for routes
  vector<drConnFig*> allObjs;
  for (auto &uConnFig: net->getExtConnFigs()) {
    allObjs.push_back(uConnFig.get());
  }
  for (auto &uConnFig: net->getRouteConnFigs()) {
    allObjs.push_back(uConnFig.get());
  }
  //for (auto &uConnFig: net->getExtConnFigs()) {
  for (auto &uConnFig: allObjs) {
    if (uConnFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(uConnFig);
      frPoint bp, ep;
      obj->getPoints(bp, ep);
      auto lNum = obj->getLayerNum();
      // vertical
      if (bp.x() == ep.x()) {
        // non pref dir
        if (getTech()->getLayer(lNum)->getDir() == frcHorzPrefRoutingDir) {
          if (lNum + 2 <= getTech()->getTopLayerNum()) {
            xMap[bp.x()][lNum + 2] = nullptr; // default add track to upper layer
          } else if (lNum - 2 >= getTech()->getBottomLayerNum()) {
            xMap[bp.x()][lNum - 2] = nullptr;
          } else {
            cout <<"Error: initTrackCoords cannot add non-pref track" <<endl;
          }
          // add bp, ep
          //if (!isInitDR()) {
            yMap[bp.y()][lNum] = nullptr;
            yMap[ep.y()][lNum] = nullptr;
          //}
        // pref dir 
        } else {
          xMap[bp.x()][lNum] = nullptr;
          // add bp, ep
          //if (!isInitDR()) {
            if (lNum + 2 <= getTech()->getTopLayerNum()) {
              yMap[bp.y()][lNum + 2] = nullptr; // default add track to upper layer
              yMap[ep.y()][lNum + 2] = nullptr; // default add track to upper layer
            } else if (lNum - 2 >= getTech()->getBottomLayerNum()) {
              yMap[bp.y()][lNum - 2] = nullptr;
              yMap[ep.y()][lNum - 2] = nullptr;
            } else {
              cout <<"Error: initTrackCoords cannot add non-pref track" <<endl;
            }
          //}
        }
      // horizontal
      } else {
        // non pref dir
        if (getTech()->getLayer(lNum)->getDir() == frcVertPrefRoutingDir) {
          if (lNum + 2 <= getTech()->getTopLayerNum()) {
            yMap[bp.y()][lNum + 2] = nullptr;
          } else if (lNum - 2 >= getTech()->getBottomLayerNum()) {
            yMap[bp.y()][lNum - 2] = nullptr;
          } else {
            cout <<"Error: initTrackCoords cannot add non-pref track" <<endl;
          }
          // add bp, ep
          //if (!isInitDR()) {
            xMap[bp.x()][lNum] = nullptr;
            xMap[ep.x()][lNum] = nullptr;
          //}
        } else {
          yMap[bp.y()][lNum] = nullptr;
          // add bp, ep
          //if (!isInitDR()) {
            if (lNum + 2 <= getTech()->getTopLayerNum()) {
              xMap[bp.x()][lNum + 2] = nullptr; // default add track to upper layer
              xMap[ep.x()][lNum + 2] = nullptr; // default add track to upper layer
            } else if (lNum - 2 >= getTech()->getBottomLayerNum()) {
              xMap[bp.x()][lNum - 2] = nullptr;
              xMap[ep.x()][lNum - 2] = nullptr;
            } else {
              cout <<"Error: initTrackCoords cannot add non-pref track" <<endl;
            }
          //}
        }
      }
    } else if (uConnFig->typeId() == drcVia) {
      auto obj = static_cast<drVia*>(uConnFig);
      frPoint pt;
      obj->getOrigin(pt);
      // add pref dir track to layer1
      auto layer1Num = obj->getViaDef()->getLayer1Num();
      if (getTech()->getLayer(layer1Num)->getDir() == frcHorzPrefRoutingDir) {
        yMap[pt.y()][layer1Num] = nullptr;
      } else {
        xMap[pt.x()][layer1Num] = nullptr;
      }
      // add pref dir track to layer2
      auto layer2Num = obj->getViaDef()->getLayer2Num();
      if (getTech()->getLayer(layer2Num)->getDir() == frcHorzPrefRoutingDir) {
        yMap[pt.y()][layer2Num] = nullptr;
      } else {
        xMap[pt.x()][layer2Num] = nullptr;
      }
    } else if (uConnFig->typeId() == drcPatchWire) {

    } else {
      cout <<"Error: initTrackCoords unsupported type" <<endl;
    }
  }
}
    
void FlexDRWorker::initTrackCoords_pin(drNet* net, 
                                       map<frCoord, map<frLayerNum, frTrackPattern*> > &xMap,
                                       map<frCoord, map<frLayerNum, frTrackPattern*> > &yMap) {
  //auto rbox = getRouteBox();
  //auto ebox = getExtBox();
  // add for aps
  for (auto &pin: net->getPins()) {
    for (auto &ap: pin->getAccessPatterns()) {
      frPoint pt;
      ap->getPoint(pt);
      // ap must be within rbox
      //if (!ebox.contains(pt)) {
      //  continue;
      //}
      auto lNum = ap->getBeginLayerNum();
      frLayerNum lNum2 = 0;
      if (lNum + 2 <= getTech()->getTopLayerNum()) {
        lNum2 = lNum + 2;
      } else if (lNum - 2 >= getTech()->getBottomLayerNum()) {
        lNum2 = lNum - 2;
      } else {
        cout <<"Error: initTrackCoords cannot add non-pref track" <<endl;
      }
      //if (net->getFrNet() && net->getFrNet()->getName() == string("sa13_1_")) {
      //  if (pin->getFrTerm() && pin->getFrTerm()->typeId() == frcInstTerm) {
      //    frInstTerm* instTerm = (frInstTerm*)pin->getFrTerm();
      //    // if (instTerm->getInst()->getRefBlock()->getName() == "DFFSQ_X1N_A10P5PP84TR_C14_mod" && instTerm->getTerm()->getName() == "Q") {
      //    //   cout << "  initTrackCoords ap (" << pt.x() / 2000.0 << ", " << pt.y() / 2000.0 << ")\n";
      //    //   cout << "    lNum = " << lNum << ", lNum2 = " << lNum2 << endl;
      //    // }
      //  }
      //}
      if (getTech()->getLayer(lNum)->getDir() == frcHorzPrefRoutingDir) {
        yMap[pt.y()][lNum] = nullptr;
      } else {
        xMap[pt.x()][lNum] = nullptr;
      }
      if (getTech()->getLayer(lNum2)->getDir() == frcHorzPrefRoutingDir) {
        yMap[pt.y()][lNum2] = nullptr;
      } else {
        xMap[pt.x()][lNum2] = nullptr;
      }
    }
  }
}

void FlexDRWorker::initTrackCoords(map<frCoord, map<frLayerNum, frTrackPattern*> > &xMap,
                                   map<frCoord, map<frLayerNum, frTrackPattern*> > &yMap) {
  // add boundary points
  // lNum = -10 to indicate routeBox and extBox frCoord
  auto rbox = getRouteBox();
  auto ebox = getExtBox();
  yMap[rbox.bottom()][-10] = nullptr;
  yMap[rbox.top()][-10] = nullptr;
  yMap[ebox.bottom()][-10] = nullptr;
  yMap[ebox.top()][-10] = nullptr;
  xMap[rbox.left()][-10] = nullptr;
  xMap[rbox.right()][-10] = nullptr;
  xMap[ebox.left()][-10] = nullptr;
  xMap[ebox.right()][-10] = nullptr;
  // add all track coords
  for (auto &net: nets) {
    initTrackCoords_route(net.get(), xMap, yMap);
    initTrackCoords_pin(net.get(), xMap, yMap);
  }
}

void FlexDRWorker::initGridGraph() {
  // get all track coords based on existing objs and aps
  map<frCoord, map<frLayerNum, frTrackPattern*> > xMap;
  map<frCoord, map<frLayerNum, frTrackPattern*> > yMap;
  initTrackCoords(xMap, yMap);
  gridGraph.setCost(workerDRCCost, workerMarkerCost);
  gridGraph.init(getRouteBox(), getExtBox(), xMap, yMap, isInitDR(), isFollowGuide());
  //gridGraph.print();
}

void FlexDRWorker::initMazeIdx_connFig(drConnFig *connFig) {
  if (connFig->typeId() == drcPathSeg) {
    auto obj = static_cast<drPathSeg*>(connFig);
    frPoint bp, ep;
    obj->getPoints(bp, ep);
    bp.set(max(bp.x(), getExtBox().left()),  max(bp.y(), getExtBox().bottom()));
    ep.set(min(ep.x(), getExtBox().right()), min(ep.y(), getExtBox().top()));
    auto lNum = obj->getLayerNum();
    if (gridGraph.hasMazeIdx(bp, lNum) && gridGraph.hasMazeIdx(ep, lNum)) {
      FlexMazeIdx bi, ei;
      gridGraph.getMazeIdx(bi, bp, lNum);
      gridGraph.getMazeIdx(ei, ep, lNum);
      obj->setMazeIdx(bi, ei);
      //cout <<"has idx pathseg" <<endl;
    } else {
      cout <<"Error: initMazeIdx_connFig pathseg no idx (" 
           << bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<", "
           << bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<") ("
           << ep.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<", "
           << ep.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<") "
           << getTech()->getLayer(lNum)->getName()
           <<endl;
    }
  } else if (connFig->typeId() == drcVia) {
    auto obj = static_cast<drVia*>(connFig);
    frPoint bp;
    obj->getOrigin(bp);
    auto layer1Num = obj->getViaDef()->getLayer1Num();
    auto layer2Num = obj->getViaDef()->getLayer2Num();
    if (gridGraph.hasMazeIdx(bp, layer1Num) && gridGraph.hasMazeIdx(bp, layer2Num)) {
      FlexMazeIdx bi, ei;
      gridGraph.getMazeIdx(bi, bp, layer1Num);
      gridGraph.getMazeIdx(ei, bp, layer2Num);
      obj->setMazeIdx(bi, ei);
      //cout <<"has idx via" <<endl;
    } else {
      cout <<"Error: initMazeIdx_connFig via no idx (" 
           << bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<", "
           << bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<") "
           << getTech()->getLayer(layer1Num + 1)->getName()
           <<endl;
    }
  } else if (connFig->typeId() == drcPatchWire) {

  } else {
    cout <<"Error: initMazeIdx_connFig unsupported type" <<endl;
  }
}

void FlexDRWorker::initMazeIdx_ap(drAccessPattern *ap) {
  frPoint bp;
  ap->getPoint(bp);
  auto lNum = ap->getBeginLayerNum();
  if (gridGraph.hasMazeIdx(bp, lNum)) {
    FlexMazeIdx bi;
    gridGraph.getMazeIdx(bi, bp, lNum);
    ap->setMazeIdx(bi);
    // set curr layer on track status
    if (getDesign()->getTech()->getLayer(lNum)->getDir() == frcHorzPrefRoutingDir) {
      if (gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::W) ||
          gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::E)) {
        ap->setOnTrack(false, true);
      }
    } else {
      if (gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::S) ||
          gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::N)) {
        ap->setOnTrack(false, false);
      }
    }
    //cout <<"has idx via" <<endl;
  } else {
    cout <<"Error: initMazeIdx_ap no idx (" 
         << bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<", "
         << bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU()  <<") "
         << getTech()->getLayer(lNum)->getName()
         <<endl;
  }

  if (gridGraph.hasMazeIdx(bp, lNum + 2)) {
    FlexMazeIdx bi;
    gridGraph.getMazeIdx(bi, bp, lNum + 2);
    // set curr layer on track status
    if (getDesign()->getTech()->getLayer(lNum + 2)->getDir() == frcHorzPrefRoutingDir) {
      if (gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::W) ||
          gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::E)) {
        ap->setOnTrack(false, true);
      }
    } else {
      if (gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::S) ||
          gridGraph.hasGridCost(bi.x(), bi.y(), bi.z(), frDirEnum::N)) {
        ap->setOnTrack(false, false);
      }
    }
    //cout <<"has idx via" <<endl;
  } else {
    ;
  }
}

void FlexDRWorker::initMazeIdx() {
  for (auto &net: nets) {
    for (auto &connFig: net->getExtConnFigs()) {
      initMazeIdx_connFig(connFig.get());
    }
    for (auto &connFig: net->getRouteConnFigs()) {
      initMazeIdx_connFig(connFig.get());
    }
    for (auto &pin: net->getPins()) {
      for (auto &ap: pin->getAccessPatterns()) {
        initMazeIdx_ap(ap.get());
      }
    }
  }
}

void FlexDRWorker::initMazeCost_ap_planarGrid_helper(const FlexMazeIdx &mi, const frDirEnum &dir, frCoord bloatLen, bool isAddPathCost) {
  frCoord prevLen = 0;
  frCoord currLen = 0;
  frMIdx x = mi.x();
  frMIdx y = mi.y();
  frMIdx z = mi.z();
  while(1) {
    if (currLen > bloatLen) {
      break;
    }
    if (isAddPathCost) {
      gridGraph.setGridCost(x, y, z, dir);
      gridGraph.setGridCost(x, y, z, frDirEnum::D);
      gridGraph.setGridCost(x, y, z, frDirEnum::U);
    } else {
      gridGraph.resetGridCost(x, y, z, dir);
      gridGraph.resetGridCost(x, y, z, frDirEnum::D);
      gridGraph.resetGridCost(x, y, z, frDirEnum::U);
    }
    switch(dir) {
      case frDirEnum::W:
        --x;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::W);
        break;
      case frDirEnum::E:
        ++x;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::E);
        break;
      case frDirEnum::S:
        --y;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::S);
        break;
      case frDirEnum::N:
        ++y;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::N);
        break;
      default:
        ;
    }
    if (prevLen == currLen) {
      break;
    } else {
      prevLen = currLen;
    }
  }
}

void FlexDRWorker::initMazeCost_ap_planar_helper(const FlexMazeIdx &mi, const frDirEnum &dir, frCoord bloatLen, bool isAddPathCost) {
  frCoord prevLen = 0;
  frCoord currLen = 0;
  frMIdx x = mi.x();
  frMIdx y = mi.y();
  frMIdx z = mi.z();
  while(1) {
    if (currLen > bloatLen) {
      break;
    }
    if (isAddPathCost) {
      // gridGraph.setShapePlanar(x, y, z);
      gridGraph.addShapeCostPlanar(x, y, z);
    } else {
      // gridGraph.resetShapePlanar(x, y, z);
      gridGraph.subShapeCostPlanar(x, y, z);
    }
    switch(dir) {
      case frDirEnum::W:
        --x;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::W);
        break;
      case frDirEnum::E:
        ++x;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::E);
        break;
      case frDirEnum::S:
        --y;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::S);
        break;
      case frDirEnum::N:
        ++y;
        currLen += gridGraph.getEdgeLength(x, y, z, frDirEnum::N);
        break;
      default:
        ;
    }
    if (prevLen == currLen) {
      break;
    } else {
      prevLen = currLen;
    }
  }
}

void FlexDRWorker::initMazeCost_ap_helper(drNet* net, bool isAddPathCost) {
  FlexMazeIdx mi;
  frLayerNum lNum;
  frCoord defaultWidth;
  //int planarBloatNumWidth = 3;
  // int planarGridBloatNumWidth = 5;
  int planarGridBloatNumWidth = 10;
  for (auto &pin: net->getPins()) {
    bool isStdCellPin = true;
    auto term = pin->getFrTerm();
    if (term) {
      // macro cell or stdcell
      if (term->typeId() == frcInstTerm) {
        if (static_cast<frInstTerm*>(term)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
            static_cast<frInstTerm*>(term)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
            static_cast<frInstTerm*>(term)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
            static_cast<frInstTerm*>(term)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::RING) {
          isStdCellPin = false;
          //cout <<"curr dPin is macro pin" <<endl;
        } else {
          //cout <<"curr dPin is stdcell pin" <<endl;
        }
      // IO
      } else if (term->typeId() == frcTerm) {
        isStdCellPin = false;
        //cout <<"curr dPin is io pin" <<endl;
      }
    } else {
      continue;
    }

    bool hasUpperOnTrackAP = false;
    if (isStdCellPin) {
      for (auto &ap: pin->getAccessPatterns()) {
        lNum = ap->getBeginLayerNum();
        if (ap->hasValidAccess(frDirEnum::U)) {
          if (getDesign()->getTech()->getLayer(lNum+2)->getDir() == frcHorzPrefRoutingDir && ap->isOnTrack(true)) {
            hasUpperOnTrackAP = true;
            break;
          }
          if (getDesign()->getTech()->getLayer(lNum+2)->getDir() == frcVertPrefRoutingDir && ap->isOnTrack(false)) {
            hasUpperOnTrackAP = true;
            break;
          }
        }
      }
    }

    for (auto &ap: pin->getAccessPatterns()) {
      ap->getMazeIdx(mi);
      lNum = ap->getBeginLayerNum();
      defaultWidth = getDesign()->getTech()->getLayer(lNum)->getWidth();
      if (ap->hasValidAccess(frDirEnum::U)) {
        lNum = ap->getBeginLayerNum();
        if (lNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
          auto upperDefaultWidth = getDesign()->getTech()->getLayer(lNum+2)->getWidth();
          if (getDesign()->getTech()->getLayer(lNum+2)->getDir() == frcHorzPrefRoutingDir && !ap->isOnTrack(true)) {
            if (!hasUpperOnTrackAP) {
              auto upperMi = FlexMazeIdx(mi.x(), mi.y(), mi.z()+1);    
              initMazeCost_ap_planarGrid_helper(upperMi, frDirEnum::W, planarGridBloatNumWidth * upperDefaultWidth, isAddPathCost);
              initMazeCost_ap_planarGrid_helper(upperMi, frDirEnum::E, planarGridBloatNumWidth * upperDefaultWidth, isAddPathCost);
            }
          }
          if (getDesign()->getTech()->getLayer(lNum+2)->getDir() == frcVertPrefRoutingDir && !ap->isOnTrack(false)) {
            if (!hasUpperOnTrackAP) {
              auto upperMi = FlexMazeIdx(mi.x(), mi.y(), mi.z()+1);    
              initMazeCost_ap_planarGrid_helper(upperMi, frDirEnum::N, planarGridBloatNumWidth * upperDefaultWidth, isAddPathCost);
              initMazeCost_ap_planarGrid_helper(upperMi, frDirEnum::S, planarGridBloatNumWidth * upperDefaultWidth, isAddPathCost);
            }
          }
        }


        if (isAddPathCost) {
          // gridGraph.setShapeVia(mi.x(), mi.y(), mi.z());
          // gridGraph.addShapeCostVia(mi.x(), mi.y(), mi.z());
          gridGraph.resetOverrideShapeCostVia(mi.x(), mi.y(), mi.z());
        } else {
          // gridGraph.resetShapeVia(mi.x(), mi.y(), mi.z());
          // gridGraph.subShapeCostVia(mi.x(), mi.y(), mi.z());
          gridGraph.setOverrideShapeCostVia(mi.x(), mi.y(), mi.z());
        }
      }

      if (ap->hasValidAccess(frDirEnum::W)) {
        //initMazeCost_ap_planar_helper(mi, frDirEnum::W, planarBloatNumWidth * defaultWidth, isAddPathCost);
        // if (!ap->isOnTrack(true) && !isStdCellPin) {
        if (!isStdCellPin) {
          initMazeCost_ap_planarGrid_helper(mi, frDirEnum::W, planarGridBloatNumWidth * defaultWidth, isAddPathCost);
        }
      }
      if (ap->hasValidAccess(frDirEnum::E)) {
        //initMazeCost_ap_planar_helper(mi, frDirEnum::E, planarBloatNumWidth * defaultWidth, isAddPathCost);
        // if (!ap->isOnTrack(true) && !isStdCellPin) {
        if (!isStdCellPin) {
          initMazeCost_ap_planarGrid_helper(mi, frDirEnum::E, planarGridBloatNumWidth * defaultWidth, isAddPathCost);
        }
      }
      if (ap->hasValidAccess(frDirEnum::S)) {
        //initMazeCost_ap_planar_helper(mi, frDirEnum::S, planarBloatNumWidth * defaultWidth, isAddPathCost);
        // if (!ap->isOnTrack(false) && !isStdCellPin) {
        if (!isStdCellPin) {
          initMazeCost_ap_planarGrid_helper(mi, frDirEnum::S, planarGridBloatNumWidth * defaultWidth, isAddPathCost);
        }
      }
      if (ap->hasValidAccess(frDirEnum::N)) {
        //initMazeCost_ap_planar_helper(mi, frDirEnum::N, planarBloatNumWidth * defaultWidth, isAddPathCost);
        // if (!ap->isOnTrack(false) && !isStdCellPin) {
        if (!isStdCellPin) {
          initMazeCost_ap_planarGrid_helper(mi, frDirEnum::N, planarGridBloatNumWidth * defaultWidth, isAddPathCost);
        }
      }
    }
  }
}

void FlexDRWorker::initMazeCost_ap() {
  bool enableOutput = false;
  //bool enableOutput = true;
  int cnt = 0;
  FlexMazeIdx mi;
  for (auto &net: nets) {
    for (auto &pin: net->getPins()) {
      for (auto &ap: pin->getAccessPatterns()) {
        ap->getMazeIdx(mi);
        if (!ap->hasValidAccess(frDirEnum::E)) {
          gridGraph.setBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::E);
        } else {
          gridGraph.resetBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::E);
        }

        if (!ap->hasValidAccess(frDirEnum::W)) {
          gridGraph.setBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::W);
        } else {
          gridGraph.resetBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::W);
        }

        if (!ap->hasValidAccess(frDirEnum::N)) {
          gridGraph.setBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::N);
        } else {
          gridGraph.resetBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::N);
        }

        if (!ap->hasValidAccess(frDirEnum::S)) {
          gridGraph.setBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::S);
        } else {
          gridGraph.resetBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::S);
        }

        if (!ap->hasValidAccess(frDirEnum::U)) {
          gridGraph.setBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::U);
        } else {
          gridGraph.resetBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::U);
        }

        if (!ap->hasValidAccess(frDirEnum::D)) {
          gridGraph.setBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::D);
        } else {
          gridGraph.resetBlocked(mi.x(), mi.y(), mi.z(), frDirEnum::D);
        }
        
        //if (ap->hasValidAccess(frDirEnum::E) ||
        //    ap->hasValidAccess(frDirEnum::S) ||
        //    ap->hasValidAccess(frDirEnum::W) ||
        //    ap->hasValidAccess(frDirEnum::N)) {
        //  gridGraph.resetShapePlanar(mi.x(), mi.y(), mi.z());
        //}
        // currently hasValidAccess down is somehow true......
        //if (ap->hasValidAccess(frDirEnum::D)) {
        //  gridGraph.resetShapeVia(mi.x(), mi.y(), mi.z() - 1);
        //}

        // set when routing this net
        //if (ap->hasValidAccess(frDirEnum::U)) {
        //  gridGraph.resetShapeVia(mi.x(), mi.y(), mi.z());
        //  //gridGraph.resetShapePlanar(mi.x(), mi.y(), mi.z());
        //}

        if (ap->hasAccessViaDef(frDirEnum::U)) {
          gridGraph.setSVia(mi.x(), mi.y(), mi.z());
          apSVia[mi] = ap.get();
          if (ap->getAccessViaDef() != 
              getDesign()->getTech()->getLayer(ap->getBeginLayerNum() + 1)->getDefaultViaDef()) {
            cnt++;
          }
        }
      }
    }
  }
  if (enableOutput) {
    cout <<"@@Test: " <<cnt <<" svia detected" <<endl;
  }
}

void FlexDRWorker::initMazeCost_marker_fixMode_0(const frMarker &marker) {
  bool enableOutput = false;
  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_rptr_value_t<drConnFig> > results;
  frBox mBox, bloatBox;
  FlexMazeIdx mIdx1, mIdx2;
  int xDim, yDim, zDim;
  gridGraph.getDim(xDim, yDim, zDim);

  results.clear();
  marker.getBBox(mBox);
  if (!getDrcBox().overlaps(mBox)) {
    return;
  }
  auto lNum   = marker.getLayerNum();
  frCoord bloatDist;
  if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::CUT) {
    bloatDist = getDesign()->getTech()->getLayer(lNum - 1)->getWidth() * workerMarkerBloatWidth;
  } else {
    bloatDist = getDesign()->getTech()->getLayer(lNum)->getWidth() * workerMarkerBloatWidth;
  }
  mBox.bloat(bloatDist, bloatBox);
  if (enableOutput) {
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    cout <<"marker @(" <<mBox.left()  / dbu <<", " <<mBox.bottom() / dbu <<") ("
                       <<mBox.right() / dbu <<", " <<mBox.top()    / dbu <<") "
         <<getDesign()->getTech()->getLayer(lNum)->getName() <<" " <<bloatDist
         <<endl;
  }
  //bool markerHasDir = marker.hasDir();
  //bool isHSpc = marker.isH();
  workerRegionQuery.query(bloatBox, lNum, results);
  frBox objBox;
  for (auto &[boostB, connFig]: results) {
    objBox.set(boostB.min_corner().x(), boostB.min_corner().y(), boostB.max_corner().x(), boostB.max_corner().y());
    // for pathseg-related marker, bloat marker by half width and add marker cost planar
    if (connFig->typeId() == drcPathSeg) {
      //cout <<"@@pathseg" <<endl;
      //if (markerHasDir && isHSpc && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //}
      //if (markerHasDir && !isHSpc && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      // update marker dist
      auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
      auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
      connFig->getNet()->updateMarkerDist(dx * dx + dy * dy);

      connFig->getNet()->setRipup();
      if (enableOutput) {
        cout <<"ripup pathseg from " <<connFig->getNet()->getFrNet()->getName() <<endl;
      }
      // new
      gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);
      auto obj = static_cast<drPathSeg*>(connFig);
      FlexMazeIdx psMIdx1, psMIdx2;
      obj->getMazeIdx(psMIdx1, psMIdx2);
      bool isH = (psMIdx1.y() == psMIdx2.y());
      if (isH) {
        for (int i = max(0, max(psMIdx1.x(), mIdx1.x() - 1)); 
             i <= min(xDim - 1, (psMIdx2.x(), mIdx2.x() + 1)); i++) {
          gridGraph.addMarkerCostPlanar(i, psMIdx1.y(), psMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost planar @(" <<i <<", " <<psMIdx1.y() <<", " <<psMIdx1.z() <<")" <<endl;
          }
          planarHistoryMarkers.insert(FlexMazeIdx(i, psMIdx1.y(), psMIdx1.z()));
        }
      } else {
        for (int i = max(0, max(psMIdx1.y(), mIdx1.y() - 1)); 
             i <= min(yDim - 1, min(psMIdx2.y(), mIdx2.y() + 1)); i++) {
          gridGraph.addMarkerCostPlanar(psMIdx1.x(), i, psMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost planar @(" <<psMIdx1.x() <<", " <<i <<", " <<psMIdx1.z() <<")" <<endl;
          }
          planarHistoryMarkers.insert(FlexMazeIdx(psMIdx1.x(), i, psMIdx1.z()));
        }
      }

    // for via-related marker, add marker cost via
    } else if (connFig->typeId() == drcVia) {
      //cout <<"@@via" <<endl;
      //if (markerHasDir && isHSpc && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //}
      //if (markerHasDir && !isHSpc && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::ROUTING &&
          marker.getConstraint()->typeId() == frConstraintTypeEnum::frcShortConstraint) {
        continue;
      }
      // update marker dist
      auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
      auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
      connFig->getNet()->updateMarkerDist(dx * dx + dy * dy);

      auto obj = static_cast<drVia*>(connFig);
      obj->getMazeIdx(mIdx1, mIdx2);
      connFig->getNet()->setRipup();
      gridGraph.addMarkerCostVia(mIdx1.x(), mIdx1.y(), mIdx1.z());
      if (enableOutput) {
        cout <<"ripup via from " <<connFig->getNet()->getFrNet()->getName() <<endl;
      }
      if (enableOutput) {
        cout <<"add marker cost via @(" <<mIdx1.x() <<", " <<mIdx1.y() <<", " <<mIdx1.z() <<")" <<endl;
      }
      viaHistoryMarkers.insert(mIdx1);
    } else if (connFig->typeId() == drcPatchWire) {
      // TODO: could add marker // for now we think the other part in the violation would not be patchWire
      auto obj = static_cast<drPatchWire*>(connFig);
      // update marker dist
      auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
      auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
      connFig->getNet()->updateMarkerDist(dx * dx + dy * dy);
      
      frPoint bp;
      obj->getOrigin(bp);
      if (getExtBox().contains(bp)) {
        gridGraph.getMazeIdx(mIdx1, bp, lNum);
        connFig->getNet()->setRipup();
        gridGraph.addMarkerCostPlanar(mIdx1.x(), mIdx1.y(), mIdx1.z());
        planarHistoryMarkers.insert(FlexMazeIdx(mIdx1.x(), mIdx1.y(), mIdx1.z()));
        if (enableOutput) {
          cout <<"ripup pwire from " <<connFig->getNet()->getFrNet()->getName() <<endl;
        }
        if (enableOutput) {
          cout <<"add marker cost pwire @(" <<mIdx1.x() <<", " <<mIdx1.y() <<", " <<mIdx1.z() <<")" <<endl;
        }
      }
    } else {
      cout <<"Error: unsupported dr type" <<endl;
    }
  }
}

void FlexDRWorker::initMazeCost_marker_fixMode_1(const frMarker &marker, bool keepViaNet) {
  //bool enableOutput = false;
  bool enableOutput = true;

  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_rptr_value_t<drConnFig> > results;
  frBox mBox, bloatBox;
  FlexMazeIdx mIdx1, mIdx2;
  int xDim, yDim, zDim;
  gridGraph.getDim(xDim, yDim, zDim);

  marker.getBBox(mBox);
  auto lNum   = marker.getLayerNum();
  //if (enableOutput) {
  //  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  //  cout <<"marker @(" <<mBox.left()  / dbu <<", " <<mBox.bottom() / dbu <<") ("
  //                     <<mBox.right() / dbu <<", " <<mBox.top()    / dbu <<") "
  //       <<getDesign()->getTech()->getLayer(lNum)->getName()
  //       <<endl;
  //}
  // skip if marker outside of routebox, ignore drcbox here because we would like to push wire
  if (!getRouteBox().overlaps(mBox)) {
    //if (enableOutput) {
    //  cout <<"  out of route box" <<endl;
    //}
    return;
  }
  // skip if marker not metal layer
  if (!(getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::ROUTING)) {
    //if (enableOutput) {
    //  cout <<"  not routing layer" <<endl;
    //}
    return;
  }
  // skip if marker is unknown type
  if (!(marker.getConstraint()->typeId() == frConstraintTypeEnum::frcShortConstraint   ||
        marker.getConstraint()->typeId() == frConstraintTypeEnum::frcSpacingConstraint ||
        marker.getConstraint()->typeId() == frConstraintTypeEnum::frcSpacingEndOfLineConstraint ||
        marker.getConstraint()->typeId() == frConstraintTypeEnum::frcSpacingTablePrlConstraint ||
        marker.getConstraint()->typeId() == frConstraintTypeEnum::frcSpacingTableTwConstraint)) {
    //if (enableOutput) {
    //  cout <<"  unknown type" <<endl;
    //}
    return;
  }
  // skip if not fat via
  bool hasFatVia = false;
  results.clear();
  workerRegionQuery.query(mBox, lNum, results);
  frBox objBox;
  bool bloatXp = false; // bloat to x+ dir
  bool bloatXn = false; // bloat to x- dir
  bool bloatYp = false; // bloat to y+ dir
  bool bloatYn = false; // bloat to y- dir
  int  numVias = 0;
  frCoord defaultWidth = getDesign()->getTech()->getLayer(lNum)->getWidth();
  frCoord viaWidth = 0;
  bool isLayerH = (getDesign()->getTech()->getLayer(lNum)->getDir() == frcHorzPrefRoutingDir);
  drNet* viaNet = nullptr;
  for (auto &[boostB, connFig]: results) {
    if (connFig->typeId() == drcVia) {
      //cout <<"@@via" <<endl;
      objBox.set(boostB.min_corner().x(), boostB.min_corner().y(), boostB.max_corner().x(), boostB.max_corner().y());
      viaNet = connFig->getNet();
      if (marker.hasDir()) { // spacing violation
        if (marker.isH() && isLayerH) {
          if (objBox.top()    == mBox.bottom()) {
            bloatYp = true;
          }
          if (objBox.bottom() == mBox.top()) {
            bloatYn = true;
          }
          if (objBox.top() - objBox.bottom() > defaultWidth) {
            hasFatVia = true;
            viaWidth = objBox.top() - objBox.bottom();
          }
        } else if ((!marker.isH()) && (!isLayerH)) {
          if (objBox.right()  == mBox.left()) {
            bloatXp = true;
          }
          if (objBox.left()   == mBox.right()) {
            bloatXn = true;
          }
          if (objBox.right() - objBox.left() > defaultWidth) {
            hasFatVia = true;
            viaWidth = objBox.right() - objBox.left();
          }
        }
      } else { // short violation
        bloatXp = true;
        bloatXn = true;
        bloatYp = true;
        bloatYn = true;
        if (isLayerH) {
          if (objBox.top() - objBox.bottom() > defaultWidth) {
            hasFatVia = true;
            viaWidth = objBox.top() - objBox.bottom();
          }
        } else {
          if (objBox.right() - objBox.left() > defaultWidth) {
            hasFatVia = true;
            viaWidth = objBox.right() - objBox.left();
          }
        }
      }
      numVias++;
    }
  }
  // skip if no via or more than one via
  if (numVias != 1) {
    //if (enableOutput) {
    //  cout <<"  #vias=" <<numVias <<endl;
    //}
    return;
  }
  // skip if no fat via
  if (!hasFatVia) {
    //if (enableOutput) {
    //  cout <<"  not fat via or spacing dir not perp to pref dir" <<endl;
    //}
    return;
  }

  // calc bloat box
  frCoord bloatDist;
  //bloatDist = getDesign()->getTech()->getLayer(lNum)->getWidth() * workerMarkerBloatWidth;
  bloatDist = getDesign()->getTech()->getLayer(lNum)->getWidth() * viaWidth;
  mBox.bloat(bloatDist, bloatBox);
  bloatBox.set(bloatXn ? bloatBox.left()   : mBox.left(),
               bloatYn ? bloatBox.bottom() : mBox.bottom(),
               bloatXp ? bloatBox.right()  : mBox.right(),
               bloatYp ? bloatBox.top()    : mBox.top());
  
  if (enableOutput) {
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    cout <<"marker @(" <<mBox.left()  / dbu <<", " <<mBox.bottom() / dbu <<") ("
                       <<mBox.right() / dbu <<", " <<mBox.top()    / dbu <<") "
         <<getDesign()->getTech()->getLayer(lNum)->getName()
         <<endl;
  }

  // get all objs
  results.clear();
  workerRegionQuery.query(bloatBox, lNum, results);
  for (auto &[boostB, connFig]: results) {
    objBox.set(boostB.min_corner().x(), boostB.min_corner().y(), boostB.max_corner().x(), boostB.max_corner().y());
    // do not add marker cost for ps, rely on DRCCOST
    if (connFig->typeId() == drcPathSeg) {
      //cout <<"@@pathseg" <<endl;
      auto currNet = connFig->getNet();
      // update marker dist
      auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
      auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
      connFig->getNet()->updateMarkerDist(dx * dx + dy * dy);
      if (viaNet != currNet) {
        currNet->setRipup();
        if (enableOutput) {
          double dbu = getDesign()->getTopBlock()->getDBUPerUU();
          cout <<"ripup pathseg from " <<connFig->getNet()->getFrNet()->getName() 
               <<", dist=" <<sqrt(dx * dx + dy * dy) / dbu <<endl;
        }
      }
    // add marker cost via
    } else if (connFig->typeId() == drcVia) {
      //cout <<"@@via" <<endl;
      // update marker dist
      //auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
      //auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
      //connFig->getNet()->updateMarkerDist(dx * dx + dy * dy);

      if (!keepViaNet) {
        //if (viaNet == currNet) {
          connFig->getNet()->updateMarkerDist(-1);
        //}
        auto obj = static_cast<drVia*>(connFig);
        obj->getMazeIdx(mIdx1, mIdx2);
        connFig->getNet()->setRipup();
        //gridGraph.addMarkerCostVia(mIdx1.x(), mIdx1.y(), mIdx1.z());
        if (enableOutput) {
          cout <<"ripup via from " <<connFig->getNet()->getFrNet()->getName() <<endl;
        }
        //if (enableOutput) {
        //  cout <<"add marker cost via @(" <<mIdx1.x() <<", " <<mIdx1.y() <<", " <<mIdx1.z() <<")" <<endl;
        //}
        //viaHistoryMarkers.insert(mIdx1);
      }
    } else if (connFig->typeId() == drcPatchWire) {
      // TODO: could add marker // for now we think the other part in the violation would not be patchWire
    } else {
      cout <<"Error: unsupported dr type" <<endl;
    }
  }
}

void FlexDRWorker::initMazeCost_marker_route_queue_addHistoryCost(const frMarker &marker) {
  bool enableOutput = false;
  //bool enableOutput = true;
  // bool isSameNetCut = false;
  // int sameNetViaCnt = 0;

  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_rptr_value_t<drConnFig> > results;
  frBox mBox, bloatBox;
  FlexMazeIdx mIdx1, mIdx2;
  set<drNet*> vioNets; // for self-violation, only add cost for one side (experiment with self cut spacing)

  marker.getBBox(mBox);
  auto lNum   = marker.getLayerNum();

  workerRegionQuery.query(mBox, lNum, results);
  frPoint bp, ep;
  frBox objBox;
  frCoord width;
  frSegStyle segStyle;
  FlexMazeIdx objMIdx1, objMIdx2;

  // if (marker.getSrcs().size() == 1) {
  //   auto fNet = *(marker.getSrcs().begin());
  //   for (auto &[boostB, connFig]: results) {
  //     if (connFig->typeId() == drcVia) {
  //       auto obj = static_cast<drVia*>(connFig);
  //       if (obj->getNet()->getFrNet() == fNet) {
  //         sameNetViaCnt++;
  //       }
  //     }
  //   }
  //   if (sameNetViaCnt >= 2) {
  //     isSameNetCut = true;
  //   }
  // }

  // if (isSameNetCut) {
  //   cout << "here\n"
  // }

  for (auto &[boostB, connFig]: results) {
    objBox.set(boostB.min_corner().x(), boostB.min_corner().y(), boostB.max_corner().x(), boostB.max_corner().y());
    if (connFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(connFig);
      // skip if unfixable obj
      obj->getPoints(bp, ep);
      if (!(getRouteBox().contains(bp) && getRouteBox().contains(ep))) {
        continue;
      }
      // if (isSameNetCut) {
      //   continue;
      // }
      // skip if curr is irrelavant spacing violation
      //if (marker.hasDir() && marker.isH() && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //} else if (marker.hasDir() && !marker.isH() && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      // add history cost
      // get points to mark up, markup up to "width" grid points to the left and right of pathseg
      obj->getStyle(segStyle);
      width = segStyle.getWidth();
      mBox.bloat(width, bloatBox);
      gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);
      
      obj->getMazeIdx(objMIdx1, objMIdx2);
      bool isH = (objMIdx1.y() == objMIdx2.y());
      // temporarily bloat marker at most 4X width to find neighboring grid point
      // block at least two width grid points to ensure no matter where the startpoint is
      for (int i = 0; i < 5; i++) {
        if (i == 4) {
          cout <<"Warning: marker bloat 4x width but could not find two grids to add marker cost" <<endl;
          cout << "  marker -- src: ";
          for (auto src: marker.getSrcs()) {
            if (src) {
              switch (src->typeId()) {
                case frcNet:
                  cout << (static_cast<frNet*>(src))->getName() << " ";
                  break;
                case frcInstTerm: {
                  frInstTerm* instTerm = (static_cast<frInstTerm*>(src));
                  cout <<instTerm->getInst()->getName() <<"/" <<instTerm->getTerm()->getName() << " ";
                  break;
                }
                case frcTerm: {
                  frTerm* term = (static_cast<frTerm*>(src));
                  cout <<"PIN/" << term->getName() << " ";
                  break;
                }
                case frcInstBlockage: {
                  frInstBlockage* instBlockage = (static_cast<frInstBlockage*>(src));
                  cout <<instBlockage->getInst()->getName() <<"/OBS" << " ";
                  break;
                }
                case frcBlockage: {
                  cout << "PIN/OBS" << " ";
                  break;
                }
                default:
                  ;
              }
            }
            cout << "\n";
            // get violation bbox
            frBox bbox;
            marker.getBBox(bbox);
            double dbu = design->getTech()->getDBUPerUU();
            cout << "    bbox = ( " << bbox.left() / dbu << ", " << bbox.bottom() / dbu << " ) - ( "
                   << bbox.right() / dbu << ", " << bbox.top() / dbu << " ) on Layer ";
            if (getTech()->getLayer(marker.getLayerNum())->getType() == frLayerTypeEnum::CUT && 
                marker.getLayerNum() - 1 >= getTech()->getBottomLayerNum()) {
              cout << getTech()->getLayer(marker.getLayerNum() - 1)->getName() << "\n";
            } else {
              cout << getTech()->getLayer(marker.getLayerNum())->getName() << "\n";
            }
          }
        }
        if (isH && max(objMIdx1.x(), mIdx1.x()) >= min(objMIdx2.x(), mIdx2.x())) {
          bloatBox.bloat(width, bloatBox);
          //cout <<"i=" <<i <<" " <<width <<", " <<bloatBox <<endl;
        } else if ((!isH) && max(objMIdx1.y(), mIdx1.y()) >= min(objMIdx2.y(), mIdx2.y())) {
          bloatBox.bloat(width, bloatBox);
          //cout <<bloatBox <<endl;
        } else {
          break;
        }
        gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);
      }
      if (isH) {
        for (int i = max(objMIdx1.x(), mIdx1.x()); i <= min(objMIdx2.x(), mIdx2.x()); i++) {
          gridGraph.addMarkerCostPlanar(i, objMIdx1.y(), objMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost planar @(" <<i <<", " <<objMIdx1.y() <<", " <<objMIdx1.z() <<")" <<endl;
          }
          planarHistoryMarkers.insert(FlexMazeIdx(i, objMIdx1.y(),objMIdx1.z()));
        }
      } else {
        for (int i = max(objMIdx1.y(), mIdx1.y()); i <= min(objMIdx2.y(), mIdx2.y()); i++) {
          gridGraph.addMarkerCostPlanar(objMIdx1.x(), i, objMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost planar @(" <<objMIdx1.x() <<", " <<i <<", " <<objMIdx1.z() <<")" <<endl;
          }
          planarHistoryMarkers.insert(FlexMazeIdx(objMIdx1.x(), i, objMIdx1.z()));
        }
      }
    } else if (connFig->typeId() == drcVia) {
      auto obj = static_cast<drVia*>(connFig);
      obj->getOrigin(bp);
      // skip if unfixable obj
      if (!getRouteBox().contains(bp)) {
        continue;
      }
      // skip if curr is irrelavant spacing violation
      //if (marker.hasDir() && marker.isH() && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //} else if (marker.hasDir() && !marker.isH() && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      if (vioNets.find(obj->getNet()) == vioNets.end()) {
        // add history cost
        obj->getMazeIdx(objMIdx1, objMIdx2);
        if (viaHistoryMarkers.find(objMIdx1) == viaHistoryMarkers.end()) {
          gridGraph.addMarkerCostVia(objMIdx1.x(), objMIdx1.y(), objMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost via @(" <<objMIdx1.x() <<", " <<objMIdx1.y() <<", " <<objMIdx1.z() <<")" <<endl;
          }
          viaHistoryMarkers.insert(objMIdx1);

          vioNets.insert(obj->getNet());
        }
      }
    } else if (connFig->typeId() == drcPatchWire) {
      auto obj = static_cast<drPatchWire*>(connFig);
      obj->getOrigin(bp);
      // skip if unfixable obj
      if (!getRouteBox().contains(bp)) {
        continue;
      }
      // add history cost
      // gridGraph.getMazeIdx(objMIdx1, bp, lNum);
      frBox patchBBox;
      obj->getBBox(patchBBox);
      frPoint patchLL = patchBBox.lowerLeft();
      frPoint patchUR = patchBBox.upperRight();
      FlexMazeIdx startIdx, endIdx;
      gridGraph.getMazeIdx(startIdx, patchLL, lNum);
      gridGraph.getMazeIdx(endIdx, patchUR, lNum);
      for (auto xIdx = startIdx.x(); xIdx <= endIdx.x(); xIdx++) {
        for (auto yIdx = startIdx.y(); yIdx <= endIdx.y(); yIdx++) {
          gridGraph.addMarkerCostPlanar(xIdx, yIdx, startIdx.z());
          gridGraph.addMarkerCost(xIdx, yIdx, startIdx.z(), frDirEnum::U); // always block upper via in case stack via
          gridGraph.addMarkerCost(xIdx, yIdx, startIdx.z(), frDirEnum::D); // always block upper via in case stack via
          FlexMazeIdx objMIdx(xIdx, yIdx, startIdx.z());
          planarHistoryMarkers.insert(objMIdx);
        }
      }
    }
  }
}

bool FlexDRWorker::initMazeCost_marker_fixMode_3_addHistoryCost1(const frMarker &marker) {
  // bool enableOutput = false;
  // bool enableOutput = true;

  frBox mBox, bloatBox;
  FlexMazeIdx mIdx1, mIdx2;

  marker.getBBox(mBox);
  auto lNum   = marker.getLayerNum();
  auto topLNum = getDesign()->getTech()->getTopLayerNum();
  frCoord width;
  frMIdx zIdx;
  if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::CUT) {
    width = getDesign()->getTech()->getLayer(lNum - 1)->getWidth();
    zIdx = gridGraph.getMazeZIdx(lNum - 1);
  } else {
    width = getDesign()->getTech()->getLayer(lNum)->getWidth();
    zIdx = gridGraph.getMazeZIdx(lNum);
  }
  mBox.bloat(width * workerMarkerBloatWidth, bloatBox);
  gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);

  frMIdx minZIdx = max(0, int(zIdx - workerMarkerBloatDepth / 2));
  frMIdx maxZIdx;
  if (getDesign()->getTech()->getLayer(topLNum)->getType() == frLayerTypeEnum::CUT) {
    maxZIdx = gridGraph.getMazeZIdx(topLNum- 1);
  } else {
    maxZIdx = gridGraph.getMazeZIdx(topLNum);
  }
  maxZIdx = min(int(maxZIdx), int(zIdx + workerMarkerBloatDepth / 2));

  for (int k = minZIdx; k <= maxZIdx; k++) {
    for (int i = mIdx1.x(); i <= mIdx2.x(); i++) {
      for (int j = mIdx1.y(); j <= mIdx2.y(); j++) {
        gridGraph.addMarkerCostPlanar(i, j, k);
        gridGraph.addMarkerCost(i, j, k, frDirEnum::U); // always block upper via in case stack via
      }
    }
  }
  return true;
}

// return fixable
bool FlexDRWorker::initMazeCost_marker_fixMode_3_addHistoryCost(const frMarker &marker) {
  bool enableOutput = false;
  //bool enableOutput = true;

  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_rptr_value_t<drConnFig> > results;
  frBox mBox, bloatBox;
  FlexMazeIdx mIdx1, mIdx2;

  marker.getBBox(mBox);
  auto lNum   = marker.getLayerNum();

  workerRegionQuery.query(mBox, lNum, results);
  frPoint bp, ep;
  frBox objBox;
  frCoord width;
  frSegStyle segStyle;
  FlexMazeIdx objMIdx1, objMIdx2;
  bool fixable = false;
  for (auto &[boostB, connFig]: results) {
    objBox.set(boostB.min_corner().x(), boostB.min_corner().y(), boostB.max_corner().x(), boostB.max_corner().y());
    if (connFig->typeId() == drcPathSeg) {
      auto obj = static_cast<drPathSeg*>(connFig);
      // skip if unfixable obj
      obj->getPoints(bp, ep);
      if (!(getRouteBox().contains(bp) && getRouteBox().contains(ep))) {
        continue;
      }
      fixable = true;
      // skip if curr is irrelavant spacing violation
      //if (marker.hasDir() && marker.isH() && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //} else if (marker.hasDir() && !marker.isH() && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      // add history cost
      // get points to mark up, markup up to "width" grid points to the left and right of pathseg
      obj->getStyle(segStyle);
      width = segStyle.getWidth();
      mBox.bloat(width, bloatBox);
      gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);
      
      obj->getMazeIdx(objMIdx1, objMIdx2);
      bool isH = (objMIdx1.y() == objMIdx2.y());
      // temporarily bloat marker at most 4X width to find neighboring grid point
      // block at least two width grid points to ensure no matter where the startpoint is
      for (int i = 0; i < 5; i++) {
        if (i == 4) {
          cout <<"Warning: marker bloat 4x width but could not find two grids to add marker cost" <<endl;
          cout << "  marker -- src: ";
          for (auto src: marker.getSrcs()) {
            if (src) {
              switch (src->typeId()) {
                case frcNet:
                  cout << (static_cast<frNet*>(src))->getName() << " ";
                  break;
                case frcInstTerm: {
                  frInstTerm* instTerm = (static_cast<frInstTerm*>(src));
                  cout <<instTerm->getInst()->getName() <<"/" <<instTerm->getTerm()->getName() << " ";
                  break;
                }
                case frcTerm: {
                  frTerm* term = (static_cast<frTerm*>(src));
                  cout <<"PIN/" << term->getName() << " ";
                  break;
                }
                case frcInstBlockage: {
                  frInstBlockage* instBlockage = (static_cast<frInstBlockage*>(src));
                  cout <<instBlockage->getInst()->getName() <<"/OBS" << " ";
                  break;
                }
                case frcBlockage: {
                  cout << "PIN/OBS" << " ";
                  break;
                }
                default:
                  ;
              }
            }
            cout << "\n";
            // get violation bbox
            frBox bbox;
            marker.getBBox(bbox);
            double dbu = design->getTech()->getDBUPerUU();
            cout << "    bbox = ( " << bbox.left() / dbu << ", " << bbox.bottom() / dbu << " ) - ( "
                   << bbox.right() / dbu << ", " << bbox.top() / dbu << " ) on Layer ";
            if (getTech()->getLayer(marker.getLayerNum())->getType() == frLayerTypeEnum::CUT && 
                marker.getLayerNum() - 1 >= getTech()->getBottomLayerNum()) {
              cout << getTech()->getLayer(marker.getLayerNum() - 1)->getName() << "\n";
            } else {
              cout << getTech()->getLayer(marker.getLayerNum())->getName() << "\n";
            }
          }
        }
        if (isH && max(objMIdx1.x(), mIdx1.x()) >= min(objMIdx2.x(), mIdx2.x())) {
          bloatBox.bloat(width, bloatBox);
          //cout <<"i=" <<i <<" " <<width <<", " <<bloatBox <<endl;
        } else if ((!isH) && max(objMIdx1.y(), mIdx1.y()) >= min(objMIdx2.y(), mIdx2.y())) {
          bloatBox.bloat(width, bloatBox);
          //cout <<bloatBox <<endl;
        } else {
          break;
        }
        gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);
      }
      if (isH) {
        for (int i = max(objMIdx1.x(), mIdx1.x()); i <= min(objMIdx2.x(), mIdx2.x()); i++) {
          gridGraph.addMarkerCostPlanar(i, objMIdx1.y(), objMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost planar @(" <<i <<", " <<objMIdx1.y() <<", " <<objMIdx1.z() <<")" <<endl;
          }
          planarHistoryMarkers.insert(FlexMazeIdx(i, objMIdx1.y(),objMIdx1.z()));
        }
      } else {
        for (int i = max(objMIdx1.y(), mIdx1.y()); i <= min(objMIdx2.y(), mIdx2.y()); i++) {
          gridGraph.addMarkerCostPlanar(objMIdx1.x(), i, objMIdx1.z());
          if (enableOutput) {
            cout <<"add marker cost planar @(" <<objMIdx1.x() <<", " <<i <<", " <<objMIdx1.z() <<")" <<endl;
          }
          planarHistoryMarkers.insert(FlexMazeIdx(objMIdx1.x(), i, objMIdx1.z()));
        }
      }
    } else if (connFig->typeId() == drcVia) {
      auto obj = static_cast<drVia*>(connFig);
      obj->getOrigin(bp);
      // skip if unfixable obj
      if (!getRouteBox().contains(bp)) {
        continue;
      }
      fixable = true;
      // skip if curr is irrelavant spacing violation
      //if (marker.hasDir() && marker.isH() && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //} else if (marker.hasDir() && !marker.isH() && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      // add history cost
      obj->getMazeIdx(objMIdx1, objMIdx2);
      gridGraph.addMarkerCostVia(objMIdx1.x(), objMIdx1.y(), objMIdx1.z());
      if (enableOutput) {
        cout <<"add marker cost via @(" <<objMIdx1.x() <<", " <<objMIdx1.y() <<", " <<objMIdx1.z() <<")" <<endl;
      }
      viaHistoryMarkers.insert(objMIdx1);
    } else if (connFig->typeId() == drcPatchWire) {
      auto obj = static_cast<drPatchWire*>(connFig);
      obj->getOrigin(bp);
      // skip if unfixable obj
      if (!getRouteBox().contains(bp)) {
        continue;
      }
      fixable = true;
      // skip if curr is irrelavant spacing violation
      //if (marker.hasDir() && marker.isH() && objBox.top() != mBox.bottom() && objBox.bottom() != mBox.top()) {
      //  continue;
      //} else if (marker.hasDir() && !marker.isH() && objBox.right() != mBox.left() && objBox.left() != mBox.right()) {
      //  continue;
      //}
      // add history cost
      gridGraph.getMazeIdx(objMIdx1, bp, lNum);
      //frBox offsetBox;
      //obj->getOffsetBox(offsetBox);
      //gridGraph.get
      gridGraph.addMarkerCostPlanar(objMIdx1.x(), objMIdx1.y(), objMIdx1.z());
      gridGraph.addMarkerCost(objMIdx1.x(), objMIdx1.y(), objMIdx1.z(), frDirEnum::U); // always block upper via in case stack via
      gridGraph.addMarkerCost(objMIdx1.x(), objMIdx1.y(), objMIdx1.z(), frDirEnum::D); // always block upper via in case stack via
      if (enableOutput) {
        cout <<"add marker cost patchwire @(" <<objMIdx1.x() <<", " <<objMIdx1.y() <<", " <<objMIdx1.z() <<")" <<endl;
      }
      planarHistoryMarkers.insert(objMIdx1);
    }
  }
  return fixable;
}

void FlexDRWorker::initMazeCost_marker_fixMode_3_ripupNets(const frMarker &marker) {
  bool enableOutput = false;

  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_rptr_value_t<drConnFig> > results;
  frBox mBox, bloatBox;
  FlexMazeIdx mIdx1, mIdx2;
  frPoint bp, ep;

  marker.getBBox(mBox);
  auto lNum   = marker.getLayerNum();

  // ripup all nets within bloatbox
  frCoord bloatDist = 0;
  // if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::CUT) {
  //   if (getDesign()->getTech()->getTopLayerNum() >= lNum + 1 && 
  //       getDesign()->getTech()->getLayer(lNum + 1)->getType() == frLayerTypeEnum::ROUTING) {
  //     bloatDist = getDesign()->getTech()->getLayer(lNum + 1)->getWidth() * workerMarkerBloatWidth;
  //   } else if (getDesign()->getTech()->getBottomLayerNum() <= lNum - 1 && 
  //              getDesign()->getTech()->getLayer(lNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
  //     bloatDist = getDesign()->getTech()->getLayer(lNum - 1)->getWidth() * workerMarkerBloatWidth;
  //   }
  // } else if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::ROUTING) {
  //   bloatDist = getDesign()->getTech()->getLayer(lNum)->getWidth() * workerMarkerBloatWidth;
  // }
  mBox.bloat(bloatDist, bloatBox);
  if (enableOutput) {
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    cout <<"marker @(" <<mBox.left()  / dbu <<", " <<mBox.bottom() / dbu <<") ("
                       <<mBox.right() / dbu <<", " <<mBox.top()    / dbu <<") "
         <<getDesign()->getTech()->getLayer(lNum)->getName() <<" " <<bloatDist
         <<endl;
  }

  // auto lNumMin = max(getDesign()->getTech()->getBottomLayerNum(), lNum - (int)workerMarkerBloatDepth);
  // auto lNumMax = min(getDesign()->getTech()->getTopLayerNum(),    lNum + (int)workerMarkerBloatDepth);
  // for (auto currLNum = lNumMin; currLNum <= lNumMax; currLNum++) {
  auto currLNum = lNum;
    results.clear();
    workerRegionQuery.query(bloatBox, currLNum, results);
    frBox objBox;
    for (auto &[boostB, connFig]: results) {
      objBox.set(boostB.min_corner().x(), boostB.min_corner().y(), boostB.max_corner().x(), boostB.max_corner().y());
      bool isEnter = false;
      if (getFixMode() == 3) {
        isEnter = true;
      //} else {
      //  if (!marker.hasDir()) { // non spc viol
      //    isEnter = true;
      //  } else {
      //    if (getFixMode() == 4 && (objBox.top() == mBox.bottom() || objBox.right() == mBox.left())) {
      //      isEnter = true;
      //    } else if (getFixMode() == 5 && (objBox.bottom() == mBox.top() || objBox.left() == mBox.right())) {
      //      isEnter = true;
      //    }
      //  }
      }
      if (!isEnter) {
        continue;
      }
      // for pathseg-related marker, bloat marker by half width and add marker cost planar
      if (connFig->typeId() == drcPathSeg) {
        //cout <<"@@pathseg" <<endl;
        auto obj = static_cast<drPathSeg*>(connFig);
        obj->getPoints(bp, ep);
        //bool isPsH = (bp.y() == ep.y());
        //bool isLayerH = (getDesign()->getTech()->getLayer(currLNum)->getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
        //if ((!marker.hasDir()) && 
        //    ((getFixMode() == 4 && (isPsH != isLayerH)) ||
        //     (getFixMode() == 5 && (isPsH == isLayerH)))) {
        //  continue;
        //}
        // update marker dist
        auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
        auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
        auto dz = abs(bloatDist * (lNum - currLNum));
        connFig->getNet()->updateMarkerDist(dx * dx + dy * dy + dz * dz);
        connFig->getNet()->setRipup();
        if (enableOutput) {
          cout <<"ripup pathseg from " <<connFig->getNet()->getFrNet()->getName() <<endl;
        }
      // for via-related marker, add marker cost via
      } else if (connFig->typeId() == drcVia) {
        //cout <<"@@via" <<endl;
        //if ((!marker.hasDir()) && getFixMode() == 4) {
        //  continue;
        //}
        //if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::ROUTING &&
        //    marker.getConstraint()->typeId() == frConstraintTypeEnum::frcShortConstraint) {
        //  continue;
        //}
        // update marker dist
        auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
        auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
        auto dz = abs(bloatDist * (lNum - currLNum));
        connFig->getNet()->updateMarkerDist(dx * dx + dy * dy + dz * dz);

        //auto obj = static_cast<drVia*>(connFig);
        //obj->getMazeIdx(mIdx1, mIdx2);
        connFig->getNet()->setRipup();
        if (enableOutput) {
          cout <<"ripup via from " <<connFig->getNet()->getFrNet()->getName() <<endl;
        }
      } else if (connFig->typeId() == drcPatchWire) {
        // TODO: could add marker // for now we think the other part in the violation would not be patchWire
        //if ((!marker.hasDir()) && getFixMode() == 5) {
        //  continue;
        //}
        // update marker dist
        auto dx = max(max(objBox.left(),   mBox.left())   - min(objBox.right(), mBox.right()), 0);
        auto dy = max(max(objBox.bottom(), mBox.bottom()) - min(objBox.top(),   mBox.top()),   0);
        auto dz = abs(bloatDist * (lNum - currLNum));
        connFig->getNet()->updateMarkerDist(dx * dx + dy * dy + dz * dz);

        //auto obj = static_cast<drPatchWire*>(connFig);
        //obj->getMazeIdx(mIdx1, mIdx2);
        connFig->getNet()->setRipup();
        if (enableOutput) {
          cout <<"ripup pwire from " <<connFig->getNet()->getFrNet()->getName() <<endl;
        }
      } else {
        cout <<"Error: unsupported dr type" <<endl;
      }
    }
  // }
}

// TODO: modify behavior for bloat width != 0
void FlexDRWorker::initMazeCost_marker_route_queue(const frMarker &marker) {
  if (workerMarkerBloatWidth) {
    initMazeCost_marker_route_queue_addHistoryCost(marker);
  } else {
    initMazeCost_marker_route_queue_addHistoryCost(marker);
  }
}


void FlexDRWorker::initMazeCost_marker_fixMode_3(const frMarker &marker) {
  bool fixable = false;
  if (workerMarkerBloatWidth) {
    fixable = initMazeCost_marker_fixMode_3_addHistoryCost1(marker);
  } else {
    fixable = initMazeCost_marker_fixMode_3_addHistoryCost(marker);
  }
  // skip non-fixable markers
  if (!fixable) {
    return;
  }
  initMazeCost_marker_fixMode_3_ripupNets(marker);
}

void FlexDRWorker::route_queue_resetRipup() {
  for (auto &net: nets) {
    net->resetNumReroutes();
  }
}

void FlexDRWorker::route_queue_markerCostDecay() {
  for (auto it = planarHistoryMarkers.begin(); it != planarHistoryMarkers.end();) {
    auto currIt = it;
    auto &mi = *currIt;
    ++it;
    if (gridGraph.decayMarkerCostPlanar(mi.x(), mi.y(), mi.z(), MARKERDECAY)) {
    // if (gridGraph.decayMarkerCostPlanar(mi.x(), mi.y(), mi.z())) {
      planarHistoryMarkers.erase(currIt);
    }
  }
  for (auto it = viaHistoryMarkers.begin(); it != viaHistoryMarkers.end();) {
    auto currIt = it;
    auto &mi = *currIt;
    ++it;
    if (gridGraph.decayMarkerCostVia(mi.x(), mi.y(), mi.z(), MARKERDECAY)) {
    // if (gridGraph.decayMarkerCostVia(mi.x(), mi.y(), mi.z())) {
      viaHistoryMarkers.erase(currIt);
    }
  }
}

void FlexDRWorker::route_queue_addMarkerCost(const vector<unique_ptr<frMarker> > &markers) {
  if (getFixMode() < 9) {
  // if (true) {
    for (auto &uMarker: markers) {
      auto &marker = *(uMarker.get());
      initMazeCost_marker_fixMode_3(marker);
    }
  } else {
    for (auto &uMarker: markers) {
      auto &marker = *(uMarker.get());
      initMazeCost_marker_route_queue(marker);
    }
  }
}

// init from member markers
void FlexDRWorker::route_queue_addMarkerCost() {
  if (getFixMode() < 9) {
  // if (true) {
    for (auto &marker: markers) {
      initMazeCost_marker_fixMode_3(marker);
    }
  } else {
    for (auto &marker: markers) {
      initMazeCost_marker_route_queue(marker);
    }
  }
}

void FlexDRWorker::route_queue_init_queue(deque<pair<frBlockObject*, pair<bool, int> > > &rerouteQueue) {
  set<frBlockObject*> uniqueVictims;
  set<frBlockObject*> uniqueAggressors;
  vector<pair<frBlockObject*, pair<bool, int> > > checks;
  vector<pair<frBlockObject*, pair<bool, int> > > routes;
  
  if (getRipupMode() == 0) {
    for (auto &marker: markers) {
      route_queue_update_from_marker(&marker, uniqueVictims, uniqueAggressors, checks, routes);
    }
  } else if (getRipupMode() == 1 || getRipupMode() == 2) {
    // ripup all nets and clear objs here
    // nets are ripped up during initNets()
    vector<drNet*> ripupNets;
    for (auto &net: nets) {
      ripupNets.push_back(net.get());
    }

    // sort nets
    mazeIterInit_sortRerouteNets(0, ripupNets);
    // if (routeBox.left() == 462000 && routeBox.bottom() == 81100) {
    //   cout << "@@@ debug nets:\n";
    //   for (auto &net: ripupNets) {
    //     cout << net->getFrNet()->getName() << "\n";
    //   }
    // }

    // // expt: for ripup all, first route nets with violations to avoid previous mistakes in net ordering
    // if (getRipupMode() == 1 && getDRIter() > 2) {
    //   for (auto &marker: markers) {
    //     route_queue_update_from_marker(&marker, uniqueVictims, uniqueAggressors, checks, routes);
    //   }
    //   checks.clear();
    // }

    for (auto &net: ripupNets) {
      routes.push_back(make_pair(net, make_pair(true, 0)));
      // reserve via because all nets are ripupped
      initMazeCost_via_helper(net, true);
      // no need to clear the net because route objs are not pushed to the net (See FlexDRWorker::initNet)
    }
  } else {
    cout << "Error: unsupported ripup mode\n";
  }
  route_queue_update_queue(checks, routes, rerouteQueue);
}

void FlexDRWorker::route_queue_update_queue(const vector<pair<frBlockObject*, pair<bool, int> > > &checks,
                                            const vector<pair<frBlockObject*, pair<bool, int> > > &routes,
                                            deque<pair<frBlockObject*, pair<bool, int> > > &rerouteQueue) {
  for (auto &route: routes) {
    rerouteQueue.push_back(route);
  }
  for (auto &check: checks) {
    rerouteQueue.push_back(check);
  }
}

//*****************************************************************************************//
// EXPONENTIAL QUEUE SIZE IF NOT MAKE AGGRESSORS AND VICTIMS UNIQUE FOR A SET OF MARKERS!! //
// NET A --> PUSH ROUTE A --> PUSH CHECK A * 3 -->                                         //
//        |                                      |                                         //
//        ----------------------------------------                                         //
//*****************************************************************************************//
void FlexDRWorker::route_queue_update_from_marker(frMarker *marker, 
                                                  set<frBlockObject*> &uniqueVictims, 
                                                  set<frBlockObject*> &uniqueAggressors,
                                                  vector<pair<frBlockObject*, pair<bool, int> > > &checks,
                                                  vector<pair<frBlockObject*, pair<bool, int> > > &routes) {
  // bool enableOutput = false;

  vector<frBlockObject*> uniqueVictimOwners;    // to maintain order
  vector<frBlockObject*> uniqueAggressorOwners; // to maintain order

  auto &markerAggressors = marker->getAggressors();
  set<frNet*> movableAggressorNets;
  set<frBlockObject*> movableAggressorOwners;

  for (auto &aggressorPair: markerAggressors) {
    auto &aggressor = aggressorPair.first;
    if (aggressor && aggressor->typeId() == frcNet) {
      auto fNet = static_cast<frNet*>(aggressor);
      if (fNet->getType() == frNetEnum::frcNormalNet || fNet->getType() == frNetEnum::frcClockNet) {
        movableAggressorNets.insert(fNet);
        if (getDRNets(fNet)) {
          for (auto dNet: *(getDRNets(fNet))) {
            if (dNet->getNumReroutes() >= getMazeEndIter()) {
              continue;
            }
            movableAggressorOwners.insert(aggressor);
          }
        }
      }
    }
  }

  // push movable aggressors for reroute and other srcs for drc checking
  bool hasRerouteNet = false;
  if (!movableAggressorNets.empty()) {
    for (auto &fNet: movableAggressorNets) {
      if (getDRNets(fNet)) {
        // int subNetIdx = -1;
        for (auto dNet: *(getDRNets(fNet))) {
          // subNetIdx++;
          if (dNet->getNumReroutes() >= getMazeEndIter()) {
            continue;
          }
          // rerouteQueue.push_back(make_pair(dNet, make_pair(true, dNet->getNumReroutes())));
          if (uniqueAggressors.find(fNet) == uniqueAggressors.end()) {
            uniqueAggressors.insert(fNet);
            uniqueAggressorOwners.push_back(fNet);
          }
          // cout << "push route1: " << dNet->getFrNet()->getName() << "(subNetIdx " << subNetIdx << "), NumReroutes = " << dNet->getNumReroutes() <<  "\n";
          hasRerouteNet = true;
        }
      }
    }
  }

  if (hasRerouteNet) {
    set<frBlockObject*> checkDRCOwners;
    for (auto &src: marker->getSrcs()) {
      if (movableAggressorOwners.find(src) == movableAggressorOwners.end()) {
        if (src) {
          checkDRCOwners.insert(src);
        }
      }
    }
    // push checkDRCOwners to queue for DRC
    for (auto &owner: checkDRCOwners) {
      // rerouteQueue.push_back(make_pair(owner, make_pair(false, -1)));
      if (uniqueVictims.find(owner) == uniqueVictims.end()) {
        uniqueVictims.insert(owner);
        uniqueVictimOwners.push_back(owner);
      }
      // if (owner->typeId() == frcNet) {
      //   auto fNet = static_cast<frNet*>(owner);
      //   cout << "push check1: " << fNet->getName() << "\n";
      // } else if (owner->typeId() == frcInstTerm) {
      //   auto instTerm = static_cast<frInstTerm*>(owner);
      //   cout << "push check1: " << instTerm->getInst()->getName() << "/" << instTerm->getTerm()->getName() << "\n";
      // } else if (owner->typeId() == frcTerm) {
      //   auto term = static_cast<frTerm*>(owner);
      //   cout << "push check1: " << term->getName() << "\n";
      // } else if (owner->typeId() == frcInstBlockage) {
      //   auto instBlockage = static_cast<frInstBlockage*>(owner);
      //   cout << "push check1: " << instBlockage->getInst()->getName() << "/OBS \n";
      // } else {
      //   cout << "push check1: typeId = " << (int)owner->typeId() << "\n";
      // }
    }
  } else {
    set<frBlockObject*> owners, otherOwners, routeOwners;
    auto &srcs = marker->getSrcs();
    for (auto &src: srcs) {
      if (src) {
        owners.insert(src);
      }
    }
    std::set_difference(owners.begin(), owners.end(), movableAggressorOwners.begin(), movableAggressorOwners.end(), std::inserter(otherOwners, otherOwners.end()));
    for (auto &owner: otherOwners) {
      if (owner && owner->typeId() == frcNet) {
        auto fNet = static_cast<frNet*>(owner);
        if (fNet->getType() == frNetEnum::frcNormalNet || fNet->getType() == frNetEnum::frcClockNet) {
          if (getDRNets(fNet)) {
            // int subNetIdx = -1;
            for (auto dNet: *(getDRNets(fNet))) {
              // subNetIdx++;
              // if (dNet->getNumReroutes() >= getMazeEndIter() * 2) {
              if (dNet->getNumReroutes() >= getMazeEndIter()) {
                continue;
              }
              // rerouteQueue.push_back(make_pair(dNet, make_pair(true, dNet->getNumReroutes())));
              if (uniqueAggressors.find(fNet) == uniqueAggressors.end()) {
                uniqueAggressors.insert(fNet);
                uniqueAggressorOwners.push_back(fNet);
              }
              // cout << "push route2: " << dNet->getFrNet()->getName() << "(subNetIdx " << subNetIdx << "), NumReroutes = " << dNet->getNumReroutes() <<  "\n";
              routeOwners.insert(owner);
              hasRerouteNet = true;
            }
          }
        }
      }
    }
    if (hasRerouteNet) {
      set<frBlockObject*> checkDRCOwners;
      for (auto &src: marker->getSrcs()) {
        if (routeOwners.find(src) == routeOwners.end()) {
          if (src) {
            checkDRCOwners.insert(src);
          }
        }
      }
      // push checkDRCOwners to queue for DRC
      for (auto &owner: checkDRCOwners) {
        // rerouteQueue.push_back(make_pair(owner, make_pair(false, -1)));
        if (uniqueVictims.find(owner) == uniqueVictims.end()) {
          uniqueVictims.insert(owner);
          uniqueVictimOwners.push_back(owner);
        }
        // if (owner->typeId() == frcNet) {
        //   auto fNet = static_cast<frNet*>(owner);
        //   cout << "push check2: " << fNet->getName() << "\n";
        // } else if (owner->typeId() == frcInstTerm) {
        //   auto instTerm = static_cast<frInstTerm*>(owner);
        //   cout << "push check2: " << instTerm->getInst()->getName() << "/" << instTerm->getTerm()->getName() << "\n";
        // } else if (owner->typeId() == frcTerm) {
        //   auto term = static_cast<frTerm*>(owner);
        //   cout << "push check2: " << term->getName() << "\n";
        // } else if (owner->typeId() == frcInstBlockage) {
        //   auto instBlockage = static_cast<frInstBlockage*>(owner);
        //   cout << "push check2: " << instBlockage->getInst()->getName() << "/OBS \n";
        // } else {
        //   cout << "push check2: typeId = " << (int)owner->typeId() << "\n";
        // }
      }
    }
  }

  // add to victims and aggressors as appropriate
  for (auto &aggressorOwner: uniqueAggressorOwners) {
    if (aggressorOwner && aggressorOwner->typeId() == frcNet) {
      auto fNet = static_cast<frNet*>(aggressorOwner);
      if (fNet->getType() == frNetEnum::frcNormalNet || fNet->getType() == frNetEnum::frcClockNet) {
        if (getDRNets(fNet)) {
          // int subNetIdx = -1;
          for (auto dNet: *(getDRNets(fNet))) {
            // subNetIdx++;
            // if (dNet->getNumReroutes() >= getMazeEndIter() * 2) {
            if (dNet->getNumReroutes() >= getMazeEndIter()) {
              continue;
            }
            routes.push_back(make_pair(dNet, make_pair(true, dNet->getNumReroutes())));
          }
        }
      }
    }
  }

  for (auto &victimOwner: uniqueVictimOwners) {
    checks.push_back(make_pair(victimOwner, make_pair(false, -1)));
  }

}

//*****************************************************************************************//
// EXPONENTIAL QUEUE SIZE IF NOT MAKE AGGRESSORS AND VICTIMS UNIQUE FOR A SET OF MARKERS!! //
// NET A --> PUSH ROUTE A --> PUSH CHECK A * 3 -->                                         //
//        |                                      |                                         //
//        ----------------------------------------                                         //
//*****************************************************************************************//
// void FlexDRWorker::route_queue_update_queue_from_marker(deque<pair<frBlockObject*, pair<bool, int> > > &rerouteQueue,
//                                                         frMarker *marker) {
//   bool enableOutput = false;

//   auto &aggressors = marker->getAggressors();
//   set<frNet*> movableAggressorNets;
//   set<frBlockObject*> movableAggressorOwners;

//   for (auto &aggressorPair: aggressors) {
//     auto &aggressor = aggressorPair.first;
//     auto isFixed = aggressorPair.second;
//     if (aggressor && aggressor->typeId() == frcNet) {
//       auto fNet = static_cast<frNet*>(aggressor);
//       if (fNet->getType() == frNetEnum::frcNormalNet || fNet->getType() == frNetEnum::frcClockNet) {
//         if (!isFixed) {
//           movableAggressorNets.insert(fNet);
//           if (getDRNets(fNet)) {
//             for (auto dNet: *(getDRNets(fNet))) {
//               if (dNet->getNumReroutes() >= getMazeEndIter()) {
//                 continue;
//               }
//               movableAggressorOwners.insert(aggressor);
//             }
//           }
//         }
//       }
//     }
//   }

//   // push movable aggressors for reroute and other srcs for drc checking
//   bool hasRerouteNet = false;
//   if (!movableAggressorNets.empty()) {
//     for (auto &fNet: movableAggressorNets) {
//       if (getDRNets(fNet)) {
//         int subNetIdx = -1;
//         for (auto dNet: *(getDRNets(fNet))) {
//           subNetIdx++;
//           if (dNet->getNumReroutes() >= getMazeEndIter()) {
//             continue;
//           }
//           rerouteQueue.push_back(make_pair(dNet, make_pair(true, dNet->getNumReroutes())));
//           cout << "push route1: " << dNet->getFrNet()->getName() << "(subNetIdx " << subNetIdx << "), NumReroutes = " << dNet->getNumReroutes() <<  "\n";
//           hasRerouteNet = true;
//         }
//       }
//     }
//   }

//   if (hasRerouteNet) {
//     set<frBlockObject*> checkDRCOwners;
//     for (auto &src: marker->getSrcs()) {
//       if (movableAggressorOwners.find(src) == movableAggressorOwners.end()) {
//         if (src) {
//           checkDRCOwners.insert(src);
//         }
//       }
//     }
//     // push checkDRCOwners to queue for DRC
//     for (auto &owner: checkDRCOwners) {
//       rerouteQueue.push_back(make_pair(owner, make_pair(false, -1)));
//       if (owner->typeId() == frcNet) {
//         auto fNet = static_cast<frNet*>(owner);
//         cout << "push check1: " << fNet->getName() << "\n";
//       } else if (owner->typeId() == frcInstTerm) {
//         auto instTerm = static_cast<frInstTerm*>(owner);
//         cout << "push check1: " << instTerm->getInst()->getName() << "/" << instTerm->getTerm()->getName() << "\n";
//       } else if (owner->typeId() == frcTerm) {
//         auto term = static_cast<frTerm*>(owner);
//         cout << "push check1: " << term->getName() << "\n";
//       } else if (owner->typeId() == frcInstBlockage) {
//         auto instBlockage = static_cast<frInstBlockage*>(owner);
//         cout << "push check1: " << instBlockage->getInst()->getName() << "/OBS \n";
//       } else {
//         cout << "push check1: typeId = " << (int)owner->typeId() << "\n";
//       }
//     }
//   } else {
//     set<frBlockObject*> owners, otherOwners, routeOwners;
//     auto &srcs = marker->getSrcs();
//     for (auto &src: srcs) {
//       if (src) {
//         owners.insert(src);
//       }
//     }
//     std::set_difference(owners.begin(), owners.end(), movableAggressorOwners.begin(), movableAggressorOwners.end(), std::inserter(otherOwners, otherOwners.end()));
//     for (auto &owner: otherOwners) {
//       if (owner && owner->typeId() == frcNet) {
//         auto fNet = static_cast<frNet*>(owner);
//         if (fNet->getType() == frNetEnum::frcNormalNet || fNet->getType() == frNetEnum::frcClockNet) {
//           if (getDRNets(fNet)) {
//             int subNetIdx = -1;
//             for (auto dNet: *(getDRNets(fNet))) {
//               subNetIdx++;
//               // if (dNet->getNumReroutes() >= getMazeEndIter() * 2) {
//               if (dNet->getNumReroutes() >= getMazeEndIter()) {
//                 continue;
//               }
//               rerouteQueue.push_back(make_pair(dNet, make_pair(true, dNet->getNumReroutes())));
//               cout << "push route2: " << dNet->getFrNet()->getName() << "(subNetIdx " << subNetIdx << "), NumReroutes = " << dNet->getNumReroutes() <<  "\n";
//               routeOwners.insert(owner);
//               hasRerouteNet = true;
//             }
//           }
//         }
//       }
//     }
//     if (hasRerouteNet) {
//       set<frBlockObject*> checkDRCOwners;
//       for (auto &src: marker->getSrcs()) {
//         if (routeOwners.find(src) == routeOwners.end()) {
//           if (src) {
//             checkDRCOwners.insert(src);
//           }
//         }
//       }
//       // push checkDRCOwners to queue for DRC
//       for (auto &owner: checkDRCOwners) {
//         rerouteQueue.push_back(make_pair(owner, make_pair(false, -1)));
//         if (owner->typeId() == frcNet) {
//           auto fNet = static_cast<frNet*>(owner);
//           cout << "push check2: " << fNet->getName() << "\n";
//         } else if (owner->typeId() == frcInstTerm) {
//           auto instTerm = static_cast<frInstTerm*>(owner);
//           cout << "push check2: " << instTerm->getInst()->getName() << "/" << instTerm->getTerm()->getName() << "\n";
//         } else if (owner->typeId() == frcTerm) {
//           auto term = static_cast<frTerm*>(owner);
//           cout << "push check2: " << term->getName() << "\n";
//         } else if (owner->typeId() == frcInstBlockage) {
//           auto instBlockage = static_cast<frInstBlockage*>(owner);
//           cout << "push check2: " << instBlockage->getInst()->getName() << "/OBS \n";
//         } else {
//           cout << "push check2: typeId = " << (int)owner->typeId() << "\n";
//         }
//       }
//     }
//   }
// }

// void FlexDRWorker::route_queue_init_queue(deque<pair<drNet*, int> > &rerouteNets) {
//   for (auto &marker: markers) {
//     // TODO: only push fixable violations
//     route_queue_update_queue_from_marker(rerouteNets, &marker);
//   }
// }

// void FlexDRWorker::route_queue_update_queue_from_marker(deque<pair<drNet*, int> > &rerouteNets,
//                                                         frMarker *marker,
//                                                         frNet *victim) {
//   bool enableOutput = false;

//   bool foundAggressor = false;
//   // auto &routeBox = getRouteBox();
//   // frBox markerBBox;
//   // marker->getBBox(markerBBox);
//   // if (!routeBox.contains(markerBBox)) {
//   //   return;
//   // }

//   auto &aggressors = marker->getAggressors();
//   vector<frNet*> aggressorNets, victimNets;

//   for (auto &src: marker->getSrcs()) {
//     if (src == nullptr || src->typeId() != frcNet) {
//       continue;
//     }
//     auto fNet = static_cast<frNet*>(src);
//     if (fNet->getType() != frNetEnum::frcNormalNet && fNet->getType() != frNetEnum::frcClockNet) {
//       continue;
//     }

//     if (getDRNets(fNet)) {
//       if (aggressors.find(src) != aggressors.end()) {
//         aggressorNets.push_back(fNet);
//       } else {
//         victimNets.push_back(fNet);
//       }
//     }
//   }
//   for (auto &fNet: victimNets) {
//     for (auto dNet: *(getDRNets(fNet))) {
//       if (dNet->getNumReroutes() >= getMazeEndIter()) {
//         break;
//       }
//       rerouteNets.push_back(make_pair(dNet, dNet->getNumReroutes()));
//     }
//   }
//   for (auto &fNet: aggressorNets) {
//     for (auto dNet: *(getDRNets(fNet))) {
//       if (dNet->getNumReroutes() >= getMazeEndIter()) {
//         break;
//       }
//       rerouteNets.push_back(make_pair(dNet, dNet->getNumReroutes()));
//     }
//   }

// }

// void FlexDRWorker::route_queue_update_queue_from_marker(deque<pair<drNet*, int> > &rerouteNets,
//                                                         frMarker *marker,
//                                                         frNet *victim) {
//   bool enableOutput = false;

//   bool foundAggressor = false;
//   // auto &routeBox = getRouteBox();
//   frBox markerBBox;
//   marker->getBBox(markerBBox);
//   // if (!routeBox.contains(markerBBox)) {
//   //   return;
//   // }
//   frNet *localVictim = nullptr;

//   for (auto &src: marker->getSrcs()) {
//     if (src == nullptr || src->typeId() != frcNet) {
//       continue;
//     }
//     auto fNet = static_cast<frNet*>(src);
//     // only push aggressor nets 
//     if (victim == nullptr) {
//       if (localVictim == nullptr && getDRNets(fNet) != nullptr) {
//         localVictim = fNet;
//         // continue;
//       }
//     } else {
//       if (localVictim == nullptr && victim == fNet && getDRNets(fNet) != nullptr) {
//         localVictim = fNet;
//         continue;
//       }
//     }

//     if (fNet->getType() != frNetEnum::frcNormalNet && fNet->getType() != frNetEnum::frcClockNet) {
//       continue;
//     }
//     // push aggressors
//     if (getDRNets(fNet)) {
//       for (auto dNet: *(getDRNets(fNet))) {
//         if (dNet->getNumReroutes() >= getMazeEndIter()) {
//           break;
//         }
//         rerouteNets.push_back(make_pair(dNet, dNet->getNumReroutes()));
//       }
//     }
//     foundAggressor = true;
//   }
//   if (!foundAggressor && localVictim) {
//     for (auto dNet: *(getDRNets(localVictim))) {
//       if (dNet->getNumReroutes() >= getMazeEndIter()) {
//         break;
//       }   
//       rerouteNets.push_back(make_pair(dNet, dNet->getNumReroutes()));
//     }
//   }
// }

// void FlexDRWorker::route_queue_update_queue_from_marker(deque<pair<drNet*, int> > &rerouteNets,
//                                                         frMarker *marker,
//                                                         frNet *victim) {
//   set<frNet*> ripupNets;

//   bool foundAggressor = false;
//   for (auto &src: marker->getSrcs()) {
//     if (src == nullptr || src->typeId() != frcNet) {
//       continue;
//     }
//     auto fNet = static_cast<frNet*>(src);
//     // only push aggressor nets 
//     if (fNet->getType() != frNetEnum::frcNormalNet && fNet->getType() != frNetEnum::frcClockNet) {
//       continue;
//     }
//     // push aggressors
//     ripupNets.insert(fNet);
//   }
//   if (ripupNets.size() == 1) {
//     for (auto &fNet: ripupNets) {
//       for (auto dNet: *(getDRNets(fNet))) {
//         if (dNet->getNumReroutes() >= getMazeEndIter()) {
//           break;
//         }   
//         rerouteNets.push_back(make_pair(dNet, dNet->getNumReroutes()));
//       }
//     }
//   } else {
//     for (auto &fNet: ripupNets) {
//       if (fNet == victim) {
//         continue;
//       }
//       else {
//         for (auto dNet: *(getDRNets(fNet))) {
//           if (dNet->getNumReroutes() >= getMazeEndIter()) {
//             break;
//           }   
//           rerouteNets.push_back(make_pair(dNet, dNet->getNumReroutes()));
//         }  
//       }
//     }
//   }
// }

void FlexDRWorker::route_queue_update_queue(const vector<unique_ptr<frMarker> > &markers,
                                            deque<pair<frBlockObject*, pair<bool, int> > > &rerouteQueue) {
  set<frBlockObject*> uniqueVictims;
  set<frBlockObject*> uniqueAggressors;
  vector<pair<frBlockObject*, pair<bool, int> > > checks;
  vector<pair<frBlockObject*, pair<bool, int> > > routes;
  
  for (auto &uMarker: markers) {
    auto marker = uMarker.get();
    route_queue_update_from_marker(marker, uniqueVictims, uniqueAggressors, checks, routes);
  }

  route_queue_update_queue(checks, routes, rerouteQueue);
}

// void FlexDRWorker::init_snr_iter() {
//   mazeIterInit_snr_resetRipup();
//   initMazeCost_marker_snr();
// }

// void FlexDRWorker::mazeIterInit_snr_resetRipup() {
//   if (getFixMode() == 9) {
//     for (auto &net: nets) {
//       net->resetNumReroutes();
//     }
//   }
// }

// void FlexDRWorker::initMazeCost_marker_snr() {
//   for (auto it = planarHistoryMarkers.begin(); it != planarHistoryMarkers.end();) {
//     auto currIt = it;
//     auto &mi = *currIt;
//     ++it;
//     if (gridGraph.decayMarkerCostPlanar(mi.x(), mi.y(), mi.z(), MARKERDECAY)) {
//       planarHistoryMarkers.erase(currIt);
//     }
//   }
//   for (auto it = viaHistoryMarkers.begin(); it != viaHistoryMarkers.end();) {
//     auto currIt = it;
//     auto &mi = *currIt;
//     ++it;
//     if (gridGraph.decayMarkerCostVia(mi.x(), mi.y(), mi.z(), MARKERDECAY)) {
//       viaHistoryMarkers.erase(currIt);
//     }
//   }

//   if (getFixMode() == 9) {
//     for (auto &marker: markers) {
//       initMazeCost_marker_fixMode_9(marker);
//     }
//   }
// }

// void FlexDRWorker::initMazeCost_marker_fixMode_9(const frMarker &marker) {
//   bool fixable = false;
//   fixable = initMazeCost_marker_fixMode_9_addHistoryCost(marker, true);
//   if (!fixable) {
//     return;
//   }
// }

// bool FlexDRWorker::initMazeCost_marker_fixMode_9_addHistoryCost(const frMarker &marker, bool isAddCost) {
//   bool enableOutput = false;
//   //bool enableOutput = true;

//   frBox mBox, bloatBox;
//   FlexMazeIdx mIdx1, mIdx2;

//   marker.getBBox(mBox);
//   auto lNum   = marker.getLayerNum();
//   auto topLNum = getDesign()->getTech()->getTopLayerNum();
//   frCoord width;
//   frMIdx zIdx;
//   if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::CUT) {
//     width = getDesign()->getTech()->getLayer(lNum - 1)->getWidth();
//     zIdx = gridGraph.getMazeZIdx(lNum - 1);
//   } else {
//     width = getDesign()->getTech()->getLayer(lNum)->getWidth();
//     zIdx = gridGraph.getMazeZIdx(lNum);
//   }
//   mBox.bloat(width * workerMarkerBloatWidth, bloatBox);
//   gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);

//   frMIdx minZIdx = max(0, int(zIdx - workerMarkerBloatDepth / 2));
//   frMIdx maxZIdx;
//   if (getDesign()->getTech()->getLayer(topLNum)->getType() == frLayerTypeEnum::CUT) {
//     maxZIdx = gridGraph.getMazeZIdx(topLNum- 1);
//   } else {
//     maxZIdx = gridGraph.getMazeZIdx(topLNum);
//   }
//   maxZIdx = min(int(maxZIdx), int(zIdx + workerMarkerBloatDepth / 2));

//   for (int k = minZIdx; k <= maxZIdx; k++) {
//     for (int i = mIdx1.x(); i <= mIdx2.x(); i++) {
//       for (int j = mIdx1.y(); j <= mIdx2.y(); j++) {
//         gridGraph.addMarkerCostPlanar(i, j, k);
//         gridGraph.addMarkerCost(i, j, k, frDirEnum::U); // always block upper via in case stack via
//       }
//     }
//   }
//   return true;
// }

void FlexDRWorker::initMazeCost_marker() {
  //bool enableOutput = true;
  //bool enableOutput = false;
  // decay all existing mi
  // old
  for (auto it = planarHistoryMarkers.begin(); it != planarHistoryMarkers.end();) {
    auto currIt = it;
    auto &mi = *currIt;
    ++it;
    if (gridGraph.decayMarkerCostPlanar(mi.x(), mi.y(), mi.z(), MARKERDECAY)) {
      planarHistoryMarkers.erase(currIt);
    }
  }
  for (auto it = viaHistoryMarkers.begin(); it != viaHistoryMarkers.end();) {
    auto currIt = it;
    auto &mi = *currIt;
    ++it;
    if (gridGraph.decayMarkerCostVia(mi.x(), mi.y(), mi.z(), MARKERDECAY)) {
      viaHistoryMarkers.erase(currIt);
    }
  }
  // new 
  //for (int i = 0; i < 3; i++) {
  //  frDirEnum dir = frDirEnum::UNKNOWN;
  //  switch(i) {
  //    case 0: dir = frDirEnum::E; break;
  //    case 1: dir = frDirEnum::N; break;
  //    case 2: dir = frDirEnum::U; break;
  //    default: ;
  //  }
  //  for (auto it = historyMarkers[i].begin(); it != historyMarkers[i].end();) {
  //    auto currIt = it;
  //    auto &mi = *currIt;
  //    ++it;
  //    if (gridGraph.decayMarkerCost(mi.x(), mi.y(), mi.z(), dir, MARKERDECAY)) {
  //      historyMarkers[i].erase(currIt);
  //    }
  //  }
  //}
  // add new marker mi
  for (auto &marker: markers) {
    switch(getFixMode()) {
      case 0: 
        initMazeCost_marker_fixMode_0(marker);
        //initMazeCost_marker_fixMode_3(marker, false);
        break;
      case 1: 
        initMazeCost_marker_fixMode_1(marker, true);
        break;
      case 2:
        initMazeCost_marker_fixMode_1(marker, false);
        break;
      case 3:
      case 4:
      case 5:
        //initMazeCost_marker_fixMode_3(marker, true);
        initMazeCost_marker_fixMode_3(marker);
        break;
      default:
        ;
    }
  }
}

void FlexDRWorker::initMazeCost_guide_helper(drNet* net, bool isAdd) {
  //bool enableOutput = true;
  bool enableOutput = false;
  FlexMazeIdx mIdx1, mIdx2;
  frBox box, tmpBox;
  frLayerNum lNum;
  frMIdx z;
  for (auto &rect: net->getOrigGuides()) {
    rect.getBBox(tmpBox);
    //tmpBox.bloat(1000, box);
    tmpBox.bloat(0, box);
    lNum = rect.getLayerNum();
    z = gridGraph.getMazeZIdx(lNum);
    gridGraph.getIdxBox(mIdx1, mIdx2, box);
    if (isAdd) {
      gridGraph.setGuide(mIdx1.x(), mIdx1.y(), mIdx2.x(), mIdx2.y(), z);
      if (enableOutput) {
        double dbu = getDesign()->getTopBlock()->getDBUPerUU();
        cout <<"add guide for " <<net->getFrNet()->getName() <<": (" 
             <<box.left()  / dbu <<", " <<box.bottom() / dbu<<") ("
             <<box.right() / dbu <<", " <<box.top()    / dbu <<") ("
             <<mIdx1.x() <<", " <<mIdx1.y() <<", " <<z <<") ("
             <<mIdx2.x() <<", " <<mIdx2.y() <<", " <<z <<") "
             <<getDesign()->getTech()->getLayer(lNum)->getName()
             <<endl <<flush;
      }
    } else {
      gridGraph.resetGuide(mIdx1.x(), mIdx1.y(), mIdx2.x(), mIdx2.y(), z);
      if (enableOutput) {
        double dbu = getDesign()->getTopBlock()->getDBUPerUU();
        cout <<"sub guide for " <<net->getFrNet()->getName() <<": (" 
             <<box.left()  / dbu <<", " <<box.bottom() / dbu <<") ("
             <<box.right() / dbu <<", " <<box.top()    / dbu <<") ("
             <<mIdx1.x() <<", " <<mIdx1.y() <<", " <<z <<") ("
             <<mIdx2.x() <<", " <<mIdx2.y() <<", " <<z <<") "
             <<getDesign()->getTech()->getLayer(lNum)->getName()
             <<endl <<flush;
      }
    }
  }
}

void FlexDRWorker::initMazeCost() {
  // init Maze cost by snet shapes and blockages
  initMazeCost_fixedObj();
  // init Maze Cost by pin shapes and snet shapes and blockages
  // initMazeCost_pin();
  // added in maze route
  // via access cost
  //initMazeCost_via();
  initMazeCost_ap();
  // init Maze Cost by connFig
  initMazeCost_connFig();
  // init Maze Cost by planar access terms (prevent early wrongway / turn)
  initMazeCost_planarTerm();
}

void FlexDRWorker::initMazeCost_pin_helper(const frBox &box, frCoord bloatDist, frMIdx zIdx, bool isAddPathCost) {
  FlexMazeIdx mIdx1, mIdx2;
  frBox bloatBox;
  box.bloat(bloatDist, bloatBox);
  gridGraph.getIdxBox(mIdx1, mIdx2, bloatBox);
  for (int i = mIdx1.x(); i <= mIdx2.x(); i++) {
    for (int j = mIdx1.y(); j <= mIdx2.y(); j++) {
      // gridGraph.setShapePlanar(i, j, zIdx);
      // gridGraph.setShapeVia(i, j, zIdx);
      if (isAddPathCost) {
        gridGraph.addShapeCostPlanar(i, j, zIdx);
        gridGraph.addShapeCostVia(i, j, zIdx);
      } else {
        gridGraph.subShapeCostPlanar(i, j, zIdx);
        gridGraph.subShapeCostVia(i, j, zIdx);
      }
    }
  }
}

// void FlexDRWorker::initMazeCost_pin(drNet *net, bool isAddPathCost) {
//   if (!net->getFrNet()) {
//     return;
//   }
//   auto pins = getNetPins(net->getFrNet());
//   if (!pins) {
//     return;
//   }

//   for (auto pin: *pins) {
//     auto obj = pin.first;
//     auto zIdx = pin.second.first;
//     auto box = pin.second.second;
//     if (obj->typeId() == frcTerm) {
//       // bloat dist is pre calculated and adjusted to box
//       initMazeCost_pin_helper(box, 0, zIdx, isAddPathCost);
//     } else if (obj->typeId() == frcInstTerm) {

//     }
//   }
// }

// void FlexDRWorker::initMazeCost_pin_getFrTerms() {
//   vector<rq_rptr_value_t<frBlockObject> > result;
//   frBox box;
//   //frCoord width = 0;
//   frCoord bloatDist = 0;
//   frMIdx  zIdx = 0;
//   map<frNet*, set<frBlockObject*> > frNet2Terms;
//   for (auto layerNum = getTech()->getBottomLayerNum(); layerNum <= getTech()->getTopLayerNum(); ++layerNum) {
//     bool isRoutingLayer = true;
//     result.clear();
//     if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
//       isRoutingLayer = true;
//       zIdx = gridGraph.getMazeZIdx(layerNum);
//     } else if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::CUT) {
//       isRoutingLayer = false;
//       if (getTech()->getBottomLayerNum() <= layerNum - 1 && getTech()->getLayer(layerNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
//         zIdx = gridGraph.getMazeZIdx(layerNum - 1);
//       } else {
//         continue;
//       }
//     } else {
//       continue;
//     }
//     //width = getTech()->getLayer(layerNum)->getWidth();
//     getRegionQuery()->query(getExtBox(), layerNum, result);
//     for (auto &[boostb, obj]: result) {
//       if (obj->typeId() == frcTerm) {
//         frNet2Terms[static_cast<frTerm*>(obj)->getNet()].insert(obj);
//       } else if (obj->typeId() == frcInstTerm) {
//         frNet2Terms[static_cast<frInstTerm*>(obj)->getNet()].insert(obj);
//       // snet
//       } else if (obj->typeId() == frcPathSeg) {
//         ;
//       // snet
//       } else if (obj->typeId() == frcVia) {
//         ;
//       } else if (obj->typeId() == frcBlockage) {
//         ;
//       } else if (obj->typeId() == frcInstBlockage) {
//         ;
//       } else {
//         cout <<"Warning: unsupported type in initMazeCost_pin" <<endl;
//       }
//     }
//   }

  
// }


// init maze cost for snet objs and blockages
void FlexDRWorker::initMazeCost_fixedObj() {
  vector<rq_rptr_value_t<frBlockObject> > result;
  frBox box;
  //frCoord width = 0;
  // frCoord bloatDist = 0;
  frMIdx zIdx = 0;
  map<frNet*, set<frBlockObject*> > frNet2Terms;
  for (auto layerNum = getTech()->getBottomLayerNum(); layerNum <= getTech()->getTopLayerNum(); ++layerNum) {
    bool isRoutingLayer = true;
    result.clear();
    if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
      isRoutingLayer = true;
      zIdx = gridGraph.getMazeZIdx(layerNum);
    } else if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::CUT) {
      isRoutingLayer = false;
      if (getTech()->getBottomLayerNum() <= layerNum - 1 && getTech()->getLayer(layerNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
        zIdx = gridGraph.getMazeZIdx(layerNum - 1);
      } else {
        continue;
      }
    } else {
      continue;
    }
    //width = getTech()->getLayer(layerNum)->getWidth();
    getRegionQuery()->query(getExtBox(), layerNum, result);
    // process blockage first, then unblock based on pin shape
    for (auto &[boostb, obj]: result) {
      // term no bloat
      // if (obj->typeId() == frcTerm) {
      //   frNet2Terms[static_cast<frTerm*>(obj)->getNet()].insert(obj);
      // } else if (obj->typeId() == frcInstTerm) {
      //   frNet2Terms[static_cast<frInstTerm*>(obj)->getNet()].insert(obj);
      //   box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
      //   if (isRoutingLayer) {
      //     // block via edge, if indeed legal via access, later code will unblock
      //     modBlockedVia(box, zIdx, true);
      //   } else {
      //     modCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, false);
      //   }
      // // snet
      // } else if (obj->typeId() == frcPathSeg) {
      //   if (QUICKDRCTEST) {
      //     auto ps = static_cast<frPathSeg*>(obj);
      //     cout <<"  initMazeCost_snet " <<ps->getNet()->getName() <<endl;
      //   }
      //   box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
      //   // assume only routing layer
      //   modMinSpacingCostPlaner(box, zIdx, 3);
      //   modMinSpacingCostVia(box, zIdx, 3, true,  true);
      //   modMinSpacingCostVia(box, zIdx, 3, false, true);
      //   modEolSpacingCost(box, zIdx, 3);
      // // snet
      // } else if (obj->typeId() == frcVia) {
      //   if (QUICKDRCTEST) {
      //     auto via = static_cast<frVia*>(obj);
      //     cout <<"  initMazeCost_snet " <<via->getNet()->getName() <<endl;
      //   }
      //   box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
      //   if (isRoutingLayer) {
      //     // assume only routing layer
      //     modMinSpacingCostPlaner(box, zIdx, 3);
      //     modMinSpacingCostVia(box, zIdx, 3, true,  false);
      //     modMinSpacingCostVia(box, zIdx, 3, false, false);
      //     modEolSpacingCost(box, zIdx, 3);
      //   } else {
      //     modCutSpacingCost(box, zIdx, 3);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, false);
      //   }
      // } else 
      if (obj->typeId() == frcBlockage) {
        if (QUICKDRCTEST) {
          //auto blk = static_cast<frBlockage*>(obj);
          cout <<"  initMazeCost_blockage block" <<endl;
        }
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
        if (isRoutingLayer) {
          // assume only routing layer
          modMinSpacingCostPlaner(box, zIdx, 3, true);
          modMinSpacingCostVia(box, zIdx, 3, true,  false, true);
          modMinSpacingCostVia(box, zIdx, 3, false, false, true);
          modEolSpacingCost(box, zIdx, 3);
          // block
          modBlockedPlanar(box, zIdx, true);
          modBlockedVia(box, zIdx, true);
        } else {
          modCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, false);
        }
      } else if (obj->typeId() == frcInstBlockage) {
        if (QUICKDRCTEST) {
          auto blk = static_cast<frInstBlockage*>(obj);
          cout <<"  initMazeCost_instBlockage " <<blk->getInst()->getName() <<"/OBS" <<endl;
        }
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
        //double dbu = getDesign()->getTopBlock()->getDBUPerUU();
        //cout <<"inst blk here (" <<box.left()  / dbu <<", " <<box.bottom() / dbu <<") ("
        //                         <<box.right() / dbu <<", " <<box.top()    / dbu <<") "
        //     <<getDesign()->getTech()->getLayer(layerNum)->getName() <<endl;

        if (isRoutingLayer) {
          // assume only routing layer
          modMinSpacingCostPlaner(box, zIdx, 3, true);
          modMinSpacingCostVia(box, zIdx, 3, true,  false, true);
          modMinSpacingCostVia(box, zIdx, 3, false, false, true);
          modEolSpacingCost(box, zIdx, 3);
          // block
          modBlockedPlanar(box, zIdx, true);
          modBlockedVia(box, zIdx, true);
        } else {
          modCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, false);
        }
      } 
      // else {
      //   cout <<"Warning: unsupported type in initMazeCost_fixedObj" <<endl;
      // }
    }
    for (auto &[boostb, obj]: result) {
      // term no bloat
      if (obj->typeId() == frcTerm) {
        frNet2Terms[static_cast<frTerm*>(obj)->getNet()].insert(obj);
      } else if (obj->typeId() == frcInstTerm) {
        frNet2Terms[static_cast<frInstTerm*>(obj)->getNet()].insert(obj);
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
        if (isRoutingLayer) {
          // unblock planar edge for obs over pin, ap will unblock via edge for legal pin access
          modBlockedPlanar(box, zIdx, false);
          if (zIdx <= (VIA_ACCESS_LAYERNUM / 2 - 1)) {
            modMinSpacingCostPlaner(box, zIdx, 3, true);
            modEolSpacingCost(box, zIdx, 3);
          }
        } else {
          modCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, false);
        }
      // snet
      } else if (obj->typeId() == frcPathSeg) {
        auto ps = static_cast<frPathSeg*>(obj);
        if (QUICKDRCTEST) {
          cout <<"  initMazeCost_snet " <<ps->getNet()->getName() <<endl;
        }
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
        // assume only routing layer
        modMinSpacingCostPlaner(box, zIdx, 3);
        modMinSpacingCostVia(box, zIdx, 3, true,  true);
        modMinSpacingCostVia(box, zIdx, 3, false, true);
        modEolSpacingCost(box, zIdx, 3);
        // block for PDN (fixed obj)
        if (ps->getNet()->getType() == frNetEnum::frcPowerNet || ps->getNet()->getType() == frNetEnum::frcGroundNet) {
          modBlockedPlanar(box, zIdx, true);
          modBlockedVia(box, zIdx, true);
        }
      // snet
      } else if (obj->typeId() == frcVia) {
        if (QUICKDRCTEST) {
          auto via = static_cast<frVia*>(obj);
          cout <<"  initMazeCost_snet " <<via->getNet()->getName() <<endl;
        }
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
        if (isRoutingLayer) {
          // assume only routing layer
          modMinSpacingCostPlaner(box, zIdx, 3);
          modMinSpacingCostVia(box, zIdx, 3, true,  false);
          modMinSpacingCostVia(box, zIdx, 3, false, false);
          modEolSpacingCost(box, zIdx, 3);
        } else {
          auto via = static_cast<frVia*>(obj);
          modAdjCutSpacingCost_fixedObj(box, via);

          modCutSpacingCost(box, zIdx, 3);
          modInterLayerCutSpacingCost(box, zIdx, 3, true);
          modInterLayerCutSpacingCost(box, zIdx, 3, false);
        }
      }
      // else if (obj->typeId() == frcBlockage) {
      //   if (QUICKDRCTEST) {
      //     //auto blk = static_cast<frBlockage*>(obj);
      //     cout <<"  initMazeCost_blockage block" <<endl;
      //   }
      //   box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
      //   if (isRoutingLayer) {
      //     // assume only routing layer
      //     modMinSpacingCostPlaner(box, zIdx, 3, true);
      //     modMinSpacingCostVia(box, zIdx, 3, true,  false, true);
      //     modMinSpacingCostVia(box, zIdx, 3, false, false, true);
      //     modEolSpacingCost(box, zIdx, 3);
      //   } else {
      //     modCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, false);
      //   }
      // } else if (obj->typeId() == frcInstBlockage) {
      //   if (QUICKDRCTEST) {
      //     auto blk = static_cast<frInstBlockage*>(obj);
      //     cout <<"  initMazeCost_instBlockage " <<blk->getInst()->getName() <<"/OBS" <<endl;
      //   }
      //   box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
      //   //double dbu = getDesign()->getTopBlock()->getDBUPerUU();
      //   //cout <<"inst blk here (" <<box.left()  / dbu <<", " <<box.bottom() / dbu <<") ("
      //   //                         <<box.right() / dbu <<", " <<box.top()    / dbu <<") "
      //   //     <<getDesign()->getTech()->getLayer(layerNum)->getName() <<endl;

      //   if (isRoutingLayer) {
      //     // assume only routing layer
      //     modMinSpacingCostPlaner(box, zIdx, 3, true);
      //     modMinSpacingCostVia(box, zIdx, 3, true,  false, true);
      //     modMinSpacingCostVia(box, zIdx, 3, false, false, true);
      //     modEolSpacingCost(box, zIdx, 3);
      //   } else {
      //     modCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, true);
      //     modInterLayerCutSpacingCost(box, zIdx, 3, false);
      //   }
      // } else {
      //   cout <<"Warning: unsupported type in initMazeCost_fixedObj" <<endl;
      // }
    }
  }

  // assign terms to each subnet
  for (auto &[net, objs]: frNet2Terms) {
    // to remove once verify error will not be triggered
    if (owner2nets.find(net) == owner2nets.end()) {
      // cout << "Error: frNet with term(s) does not exist in owner2nets\n";
      // continue;
    } else {
      for (auto dNet: owner2nets[net]) {
        dNet->setFrNetTerms(objs);
      }
    }
    initMazeCost_terms(objs, true);
  }
}

void FlexDRWorker::initMazeCost_terms(const set<frBlockObject*> &objs, bool isAddPathCost, bool isSkipVia) {
  for (auto &obj: objs) {
    if (obj->typeId() == frcTerm) {
      auto term = static_cast<frTerm*>(obj);
      for (auto &uPin: term->getPins()) {
        auto pin = uPin.get();
        for (auto &uPinFig: pin->getFigs()) {
          auto pinFig = uPinFig.get();
          if (pinFig->typeId() == frcRect) {
            auto rpinRect = static_cast<frRect*>(pinFig);
            frLayerNum layerNum = rpinRect->getLayerNum();
            if (getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING) {
              continue;
            }
            frMIdx zIdx;
            frRect instPinRect(*rpinRect);
            frBox box;
            instPinRect.getBBox(box);

            // add cost
            // bool isRoutingLayer = true;
            // if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
            //   isRoutingLayer = true;
            //   zIdx = gridGraph.getMazeZIdx(layerNum);
            // } else if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::CUT) {
            //   isRoutingLayer = false;
            //   if (getTech()->getBottomLayerNum() <= layerNum - 1 && getTech()->getLayer(layerNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
            //     zIdx = gridGraph.getMazeZIdx(layerNum - 1);
            //   } else {
            //     continue;
            //   }
            // } else {
            //   continue;
            // }

            // frCoord bloatDist = getDesign()->getTech()->getLayer(layerNum)->getWidth();
            // initMazeCost_pin_helper(box, bloatDist, zIdx, isAddPathCost);
            bool isRoutingLayer = true;
            if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
              isRoutingLayer = true;
              zIdx = gridGraph.getMazeZIdx(layerNum);
            } else if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::CUT) {
              isRoutingLayer = false;
              if (getTech()->getBottomLayerNum() <= layerNum - 1 && getTech()->getLayer(layerNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
                zIdx = gridGraph.getMazeZIdx(layerNum - 1);
              } else {
                continue;
              }
            } else {
              continue;
            }

            int type = isAddPathCost ? 3 : 2;

            if (isRoutingLayer) {
              modMinSpacingCostPlaner(box, zIdx, type);
              if (!isSkipVia) {
                modMinSpacingCostVia(box, zIdx, type, true,  false);
                modMinSpacingCostVia(box, zIdx, type, false, false);
              }
              modEolSpacingCost(box, zIdx, type);
            } else {
              modCutSpacingCost(box, zIdx, type);
              modInterLayerCutSpacingCost(box, zIdx, type, true);
              modInterLayerCutSpacingCost(box, zIdx, type, false);
            }
          } else {
            cout << "Error: initMazeCost_terms unsupported pinFig\n";
          }
        }
      }

    } else if (obj->typeId() == frcInstTerm) {
      auto instTerm = static_cast<frInstTerm*>(obj);
      auto inst = instTerm->getInst();
      frTransform xform;
      inst->getUpdatedXform(xform);

      for (auto &uPin:instTerm->getTerm()->getPins()) {
        auto pin = uPin.get();
        for (auto &uPinFig: pin->getFigs()) {
          auto pinFig = uPinFig.get();
          if (pinFig->typeId() == frcRect) {
            auto rpinRect = static_cast<frRect*>(pinFig);
            frLayerNum layerNum = rpinRect->getLayerNum();
            if (getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING) {
              continue;
            }
            frMIdx zIdx;
            frRect instPinRect(*rpinRect);
            instPinRect.move(xform);
            frBox box;
            instPinRect.getBBox(box);

            // add cost
            bool isRoutingLayer = true;
            if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
              isRoutingLayer = true;
              zIdx = gridGraph.getMazeZIdx(layerNum);
            } else if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::CUT) {
              isRoutingLayer = false;
              if (getTech()->getBottomLayerNum() <= layerNum - 1 && getTech()->getLayer(layerNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
                zIdx = gridGraph.getMazeZIdx(layerNum - 1);
              } else {
                continue;
              }
            } else {
              continue;
            }

            int type = isAddPathCost ? 3 : 2;

            if (isRoutingLayer) {
              modMinSpacingCostPlaner(box, zIdx, type);
              if (!isSkipVia) {
                modMinSpacingCostVia(box, zIdx, type, true,  false);
                modMinSpacingCostVia(box, zIdx, type, false, false);
              }
              modEolSpacingCost(box, zIdx, type);
            } else {
              modCutSpacingCost(box, zIdx, type);
              modInterLayerCutSpacingCost(box, zIdx, type, true);
              modInterLayerCutSpacingCost(box, zIdx, type, false);
            }
            // temporary solution, only add cost around macro pins
            if (inst->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
                inst->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
                inst->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
                inst->getRefBlock()->getMacroClass() == MacroClassEnum::RING) {
              modMinimumcutCostVia(box, zIdx, type, true);
              modMinimumcutCostVia(box, zIdx, type, false);
            }
          } else {
            cout << "Error: initMazeCost_terms unsupported pinFig\n";
          }
        }
      }
    } else {
      cout << "Error: unexpected obj type in initMazeCost_terms\n";
    }
  }
}

void FlexDRWorker::initMazeCost_planarTerm() {
  vector<rq_rptr_value_t<frBlockObject> > result;
  frBox box;
  // frCoord width = 0;
  frMIdx  zIdx = 0;
  for (auto layerNum = getTech()->getBottomLayerNum(); layerNum <= getTech()->getTopLayerNum(); ++layerNum) {
    result.clear();
    if (getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING) {
      continue;
    }
    zIdx = gridGraph.getMazeZIdx(layerNum);
    // auto width = getTech()->getLayer(layerNum)->getWidth();
    getRegionQuery()->query(getExtBox(), layerNum, result);
    for (auto &[boostb, obj]: result) {
      // term no bloat
      if (obj->typeId() == frcTerm) {
        box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
        FlexMazeIdx mIdx1, mIdx2;
        gridGraph.getIdxBox(mIdx1, mIdx2, box);
        bool isPinRectHorz = (box.right() - box.left()) > (box.top() - box.bottom());
        for (int i = mIdx1.x(); i <= mIdx2.x(); i++) {
          for (int j = mIdx1.y(); j <= mIdx2.y(); j++) {
            FlexMazeIdx mIdx(i, j, zIdx);
            gridGraph.setBlocked(i, j, zIdx, frDirEnum::U);
            gridGraph.setBlocked(i, j, zIdx, frDirEnum::D);
            if (isPinRectHorz) {
              gridGraph.setBlocked(i, j, zIdx, frDirEnum::N);
              gridGraph.setBlocked(i, j, zIdx, frDirEnum::S);
              // initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::N, 0, true);
              // initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::S, 0, true);
            } else {
              gridGraph.setBlocked(i, j, zIdx, frDirEnum::W);
              gridGraph.setBlocked(i, j, zIdx, frDirEnum::E);
              // initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::W, 0, true);
              // initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::E, 0, true);
            }
          }
        }
      } 
      // else if (obj->typeId() == frcInstTerm) {
      //   if (layerNum <= VIA_ACCESS_LAYERNUM) {
      //     continue;
      //   }
      //   box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
      //   auto pinWidth = min(box.right() - box.left(), box.top() - box.bottom());
      //   if (pinWidth >= width * 2) {
      //     continue;
      //   }
      //   bool isPinRectHorz = (box.right() - box.left()) > (box.top() - box.bottom());
      //   FlexMazeIdx mIdx1, mIdx2;
      //   gridGraph.getIdxBox(mIdx1, mIdx2, box);
      //   for (int i = mIdx1.x(); i <= mIdx2.x(); i++) {
      //     for (int j = mIdx1.y(); j <= mIdx2.y(); j++) {
      //       FlexMazeIdx mIdx(i, j, zIdx);
      //       gridGraph.setBlocked(i, j, zIdx, frDirEnum::U);
      //       gridGraph.setBlocked(i, j, zIdx, frDirEnum::D);
      //       if (isPinRectHorz) {
      //         gridGraph.setBlocked(i, j, zIdx, frDirEnum::N);
      //         gridGraph.setBlocked(i, j, zIdx, frDirEnum::S);
      //         initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::E, 10 * width, false);
      //         initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::W, 10 * width, false);
      //       } else {
      //         gridGraph.setBlocked(i, j, zIdx, frDirEnum::W);
      //         gridGraph.setBlocked(i, j, zIdx, frDirEnum::E);
      //         initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::N, 10 * width, false);
      //         initMazeCost_ap_planarGrid_helper(mIdx, frDirEnum::S, 10 * width, false);
      //       }
      //     }
      //   }
      // }
    }
  }
}


// void FlexDRWorker::initMazeCost_pin() {
//   vector<rq_rptr_value_t<frBlockObject> > result;
//   frBox box;
//   //frCoord width = 0;
//   frCoord bloatDist = 0;
//   frMIdx  zIdx = 0;
//   for (auto layerNum = getTech()->getBottomLayerNum(); layerNum <= getTech()->getTopLayerNum(); ++layerNum) {
//     bool isRoutingLayer = true;
//     result.clear();
//     if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
//       isRoutingLayer = true;
//       zIdx = gridGraph.getMazeZIdx(layerNum);
//     } else if (getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::CUT) {
//       isRoutingLayer = false;
//       if (getTech()->getBottomLayerNum() <= layerNum - 1 && getTech()->getLayer(layerNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
//         zIdx = gridGraph.getMazeZIdx(layerNum - 1);
//       } else {
//         continue;
//       }
//     } else {
//       continue;
//     }
//     //width = getTech()->getLayer(layerNum)->getWidth();
//     getRegionQuery()->query(getExtBox(), layerNum, result);
//     for (auto &[boostb, obj]: result) {
//       // term no bloat
//       if (obj->typeId() == frcTerm) {
//         // box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
//         box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
//         // bloatDist = 0;
//         bloatDist = getDesign()->getTech()->getLayer(layerNum)->getWidth() / 2;
//         initMazeCost_pin_helper(box, bloatDist, zIdx);
//         // add to owner2pins for net-based shape cost manipulation
//         if (static_cast<frTerm*>(obj)->hasNet()) {
//           owner2pins[static_cast<frTerm*>(obj)->getNet()].push_back(make_pair(obj, make_pair(zIdx, box)));
//         }
//       // instterm bloat
//       } else if (obj->typeId() == frcInstTerm) {
//         box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
//         // assume only routing layer
//         if (QUICKDRCTEST) {
//           auto instTerm = static_cast<frInstTerm*>(obj);
//           cout <<"  initMazeCost_pin " <<instTerm->getInst()->getName() <<"/" <<instTerm->getTerm()->getName() <<endl;
//         }
//         if (isRoutingLayer) {
//           modMinSpacingCostPlaner(box, zIdx, 3);
//           modMinSpacingCostVia(box, zIdx, 3, true,  false);
//           modMinSpacingCostVia(box, zIdx, 3, false, false);
//           modEolSpacingCost(box, zIdx, 3);
//         } else {
//           modCutSpacingCost(box, zIdx, 3);
//           modInterLayerCutSpacingCost(box, zIdx, 3, true);
//           modInterLayerCutSpacingCost(box, zIdx, 3, false);
//         }
//         // temporary solution, only add cost around macro pins
//         if (static_cast<frInstTerm*>(obj)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
//             static_cast<frInstTerm*>(obj)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
//             static_cast<frInstTerm*>(obj)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
//             static_cast<frInstTerm*>(obj)->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::RING) {
//           modMinimumcutCostVia(box, zIdx, 3, true);
//           modMinimumcutCostVia(box, zIdx, 3, false);
//         }
//         // add to owner2pins for net-based shape cost manipulation
//         if (static_cast<frInstTerm*>(obj)->hasNet()) {
//           owner2pins[static_cast<frInstTerm*>(obj)->getNet()].push_back(make_pair(obj, make_pair(zIdx, box)));
//         }
//       // snet
//       } else if (obj->typeId() == frcPathSeg) {
//         if (QUICKDRCTEST) {
//           auto ps = static_cast<frPathSeg*>(obj);
//           cout <<"  initMazeCost_snet " <<ps->getNet()->getName() <<endl;
//         }
//         box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
//         // assume only routing layer
//         modMinSpacingCostPlaner(box, zIdx, 3);
//         modMinSpacingCostVia(box, zIdx, 3, true,  true);
//         modMinSpacingCostVia(box, zIdx, 3, false, true);
//         modEolSpacingCost(box, zIdx, 3);
//       // snet
//       } else if (obj->typeId() == frcVia) {
//         if (QUICKDRCTEST) {
//           auto via = static_cast<frVia*>(obj);
//           cout <<"  initMazeCost_snet " <<via->getNet()->getName() <<endl;
//         }
//         box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
//         if (isRoutingLayer) {
//           // assume only routing layer
//           modMinSpacingCostPlaner(box, zIdx, 3);
//           modMinSpacingCostVia(box, zIdx, 3, true,  false);
//           modMinSpacingCostVia(box, zIdx, 3, false, false);
//           modEolSpacingCost(box, zIdx, 3);
//         } else {
//           modCutSpacingCost(box, zIdx, 3);
//           modInterLayerCutSpacingCost(box, zIdx, 3, true);
//           modInterLayerCutSpacingCost(box, zIdx, 3, false);
//         }
//       } else if (obj->typeId() == frcBlockage) {
//         if (QUICKDRCTEST) {
//           //auto blk = static_cast<frBlockage*>(obj);
//           cout <<"  initMazeCost_blockage block" <<endl;
//         }
//         box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
//         if (isRoutingLayer) {
//           // assume only routing layer
//           modMinSpacingCostPlaner(box, zIdx, 3, true);
//           modMinSpacingCostVia(box, zIdx, 3, true,  false, true);
//           modMinSpacingCostVia(box, zIdx, 3, false, false, true);
//           //modEolSpacingCost(box, zIdx, 3); // OBS do not have eol
//         } else {
//           modCutSpacingCost(box, zIdx, 3, true);
//           modInterLayerCutSpacingCost(box, zIdx, 3, true);
//           modInterLayerCutSpacingCost(box, zIdx, 3, false);
//         }
//       } else if (obj->typeId() == frcInstBlockage) {
//         if (QUICKDRCTEST) {
//           auto blk = static_cast<frInstBlockage*>(obj);
//           cout <<"  initMazeCost_instBlockage " <<blk->getInst()->getName() <<"/OBS" <<endl;
//         }
//         box.set(boostb.min_corner().x(), boostb.min_corner().y(), boostb.max_corner().x(), boostb.max_corner().y());
//         //double dbu = getDesign()->getTopBlock()->getDBUPerUU();
//         //cout <<"inst blk here (" <<box.left()  / dbu <<", " <<box.bottom() / dbu <<") ("
//         //                         <<box.right() / dbu <<", " <<box.top()    / dbu <<") "
//         //     <<getDesign()->getTech()->getLayer(layerNum)->getName() <<endl;

//         if (isRoutingLayer) {
//           // assume only routing layer
//           modMinSpacingCostPlaner(box, zIdx, 3, true);
//           modMinSpacingCostVia(box, zIdx, 3, true,  false, true);
//           modMinSpacingCostVia(box, zIdx, 3, false, false, true);
//           //modEolSpacingCost(box, zIdx, 3); // OBS do not have eol
//         } else {
//           modCutSpacingCost(box, zIdx, 3, true);
//           modInterLayerCutSpacingCost(box, zIdx, 3, true);
//           modInterLayerCutSpacingCost(box, zIdx, 3, false);
//         }
//       } else {
//         cout <<"Warning: unsupported type in initMazeCost_pin" <<endl;
//       }
//     }
//   }
// }

/*
void FlexDRWorker::initMazeCost_pin() {
  vector<rq_rptr_value_t<frBlockObject> > result;
  frBox box;
  frCoord width = 0;
  frCoord bloatDist = 0;
  frMIdx  zIdx = 0;
  for (auto layerNum = getTech()->getBottomLayerNum(); layerNum <= getTech()->getTopLayerNum(); ++layerNum) {
    result.clear();
    if (getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING) {
      continue;
    }
    zIdx = gridGraph.getMazeZIdx(layerNum);
    width = getTech()->getLayer(layerNum)->getWidth();
    getRegionQuery()->query(getExtBox(), layerNum, result);
    for (auto &[boostb, obj]: result) {
      // term no bloat
      if (obj->typeId() == frcTerm) {
        box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
        bloatDist = 0;
        initMazeCost_pin_helper(box, bloatDist, zIdx);
      // instterm bloat
      } else if (obj->typeId() == frcInstTerm) {
        box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
        bloatDist = SHAPEBLOATWIDTH * width;
        initMazeCost_pin_helper(box, bloatDist, zIdx);
      // snet
      } else if (obj->typeId() == frcPathSeg || obj->typeId() == frcVia) {
        box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
        bloatDist = SHAPEBLOATWIDTH * width;
        initMazeCost_pin_helper(box, bloatDist, zIdx);
      } else if (obj->typeId() == frcBlockage || obj->typeId() == frcInstBlockage) {
        if (USEMINSPACING_OBS) {
          box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
          bloatDist = SHAPEBLOATWIDTH * width;
          initMazeCost_pin_helper(box, bloatDist, zIdx);
        } else {
          box.set(boostb.min_corner().x()+1, boostb.min_corner().y()+1, boostb.max_corner().x()-1, boostb.max_corner().y()-1);
          bloatDist = SHAPEBLOATWIDTH * width;
          initMazeCost_pin_helper(box, bloatDist, zIdx);
        }
      } else {
        cout <<"Warning: unsupported type in initMazeCost_pin" <<endl;
      }
    }
  }
}
*/

void FlexDRWorker::addToCostGrids(const Rectangle &region, std::set<std::pair<frMIdx, frMIdx> > &costGrids) {
  frMIdx startXIdx, endXIdx, startYIdx, endYIdx;
  startXIdx = gridGraph.getMazeXIdx(xl(region));
  endXIdx = gridGraph.getMazeXIdx(xh(region));
  startYIdx = gridGraph.getMazeYIdx(yl(region));
  endYIdx = gridGraph.getMazeYIdx(yh(region));
  frMIdx xDim, yDim, zDim;
  gridGraph.getDim(xDim,yDim,zDim);
  xDim -= 1;
  yDim -= 1;
  zDim -= 1;
  for (auto currXIdx = startXIdx; currXIdx < min(endXIdx, xDim); ++currXIdx) {
    for (auto currYIdx = startYIdx; currYIdx < min(endYIdx, yDim); ++currYIdx) {
      costGrids.insert(make_pair(currXIdx, currYIdx));
    }
  }
}

void FlexDRWorker::initMazeCost_connFig() {
  int cnt = 0;
  for (auto &net: nets) {
    for (auto &connFig: net->getExtConnFigs()) {
      if (QUICKDRCTEST) {
        cout <<"  initMazeCost_connFig " <<net->getFrNet()->getName() <<" extConnFigs";
      }
      addPathCost(connFig.get());
      cnt++;
    }
    for (auto &connFig: net->getRouteConnFigs()) {
      if (QUICKDRCTEST) {
        cout <<"  initMazeCost_connFig " <<net->getFrNet()->getName() <<" routeConnFigs";
      }
      addPathCost(connFig.get());
      cnt++;
    }
  }
  //cout <<"init " <<cnt <<" connfig costs" <<endl;
}

void FlexDRWorker::initMazeCost_via_helper(drNet* net, bool isAddPathCost) {
  unique_ptr<drVia> via = nullptr;
  frPoint bp;
  for (auto &pin: net->getPins()) {
    if (pin->getFrTerm() == nullptr) {
      continue;
    }
    // MACRO pin does not prefer via access
    // bool macroPinViaBlock = false;
    auto dPinTerm = pin->getFrTerm();
    if (dPinTerm->typeId() == frcInstTerm) {
      frInstTerm *instTerm = static_cast<frInstTerm*>(dPinTerm);
      frInst *inst = instTerm->getInst();
      if (inst->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK || 
          inst->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
          inst->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
          inst->getRefBlock()->getMacroClass() == MacroClassEnum::RING) {
        continue;
      }
    }

    drAccessPattern *minCostAP = nullptr;
    for (auto &ap: pin->getAccessPatterns()) {
      if (ap->hasAccessViaDef(frDirEnum::U)) {
        if (minCostAP == nullptr) {
          minCostAP = ap.get();
        }
        if (ap->getPinCost() < minCostAP->getPinCost()) {
          minCostAP = ap.get();
          if (ap->getPinCost() == 0) {
            break;
          }
        }
      }
    }

    if (!minCostAP) {
      continue;
    }
    
    // if ((isInitDR() || getDRIter() <= 1) && isAddPathCost == false && ap->getPinCost() != 0) {
    //   continue;
    // }
    minCostAP->getPoint(bp);
    auto lNum = minCostAP->getBeginLayerNum();
    frViaDef* viaDef = minCostAP->getAccessViaDef();
    via = make_unique<drVia>(viaDef);
    via->setOrigin(bp);
    via->addToNet(net);
    initMazeIdx_connFig(via.get());
    FlexMazeIdx bi, ei;
    via->getMazeIdx(bi, ei);
    if (QUICKDRCTEST) {
      cout <<"  initMazeCost_via_helper @(" 
           <<bp.x() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<", "
           <<bp.y() * 1.0 / getDesign()->getTopBlock()->getDBUPerUU() <<") "
           <<getTech()->getLayer(lNum)->getName() <<" " 
           <<net->getFrNet()->getName() <<" " <<via->getViaDef()->getName();
    }
    if (isAddPathCost) {
      addPathCost(via.get());
    } else {
      subPathCost(via.get());
    }
  }
}


// TODO: replace l1Box / l2Box calculation with via get bounding box function
void FlexDRWorker::initMazeCost_minCut_helper(drNet *net, bool isAddPathCost) {
  int modType = isAddPathCost ? 1 : 0;
  for (auto &connFig: net->getExtConnFigs()) {
    if (QUICKDRCTEST) {
      cout << "  initMazeCost_minCut_helper " << net->getFrNet()->getName();
    }
    if (connFig->typeId() == drcVia) {
      auto via = static_cast<drVia*>(connFig.get());
      frBox l1Box, l2Box;
      frTransform xform;
      via->getTransform(xform);

      auto l1Num = via->getViaDef()->getLayer1Num();
      auto l1Fig = (via->getViaDef()->getLayer1Figs()[0].get());
      l1Fig->getBBox(l1Box);
      l1Box.transform(xform);
      modMinimumcutCostVia(l1Box, gridGraph.getMazeZIdx(l1Num), modType, true);
      modMinimumcutCostVia(l1Box, gridGraph.getMazeZIdx(l1Num), modType, false);
      
      auto l2Num = via->getViaDef()->getLayer2Num();
      auto l2Fig = (via->getViaDef()->getLayer2Figs()[0].get());
      l2Fig->getBBox(l2Box);
      l2Box.transform(xform);
      modMinimumcutCostVia(l2Box, gridGraph.getMazeZIdx(l2Num), modType, true);
      modMinimumcutCostVia(l2Box, gridGraph.getMazeZIdx(l2Num), modType, false);
    }
  }
}

void FlexDRWorker::initMazeCost_boundary_helper(drNet* net, bool isAddPathCost) {
  // do not check same-net rules between ext and route objs to avoid pessimism
  for (auto &connFig: net->getExtConnFigs()) {
    if (QUICKDRCTEST) {
      cout <<"  initMazeCost_boundary_helper " <<net->getFrNet()->getName();
    }
    if (isAddPathCost) {
      addPathCost(connFig.get());
    } else {
      subPathCost(connFig.get());
    }
    //if (connFig->typeId() == drcPathSeg) {
    //  auto obj = static_cast<drPathSeg*>(connFig.get());
    //  frPoint bp, ep;
    //  obj->getPoints(bp, ep);
    //  auto &box = getRouteBox();
    //  frCoord dxbp = max(max(box.left()   - bp.x(), bp.x() - box.right()), 0);
    //  frCoord dybp = max(max(box.bottom() - bp.y(), bp.y() - box.top()),   0);
    //  frCoord dxep = max(max(box.left()   - ep.x(), ep.x() - box.right()), 0);
    //  frCoord dyep = max(max(box.bottom() - ep.y(), ep.y() - box.top()),   0);
    //  // boundary segment as if it does not exist
    //  if (dxbp + dybp == 0 || dxep + dyep == 0) {
    //    if (TEST) {
    //      cout <<"  initMazeCost_boundary_helper true bound dist(bp, ep) = (" 
    //           <<dxbp <<", " <<dybp <<") (" 
    //           <<dxep <<", " <<dyep <<") "
    //           <<net->getFrNet()->getName();
    //    }
    //    if (isAddPathCost) {
    //      addPathCost(obj);
    //    } else {
    //      subPathCost(obj);
    //    }
    //  } else {
    //    if (TEST) {
    //      FlexMazeIdx bi, ei;
    //      obj->getMazeIdx(bi, ei);
    //      cout <<"  initMazeCost_boundary_helper true bound not entered dist(bp, ep) = (" 
    //           <<dxbp <<", " <<dybp <<") (" 
    //           <<dxep <<", " <<dyep <<") "
    //           <<net->getFrNet()->getName() <<" "
    //           <<bi <<" -- " <<ei <<endl;
    //    }
    //  }
    // via as if same-net metal spacing does not exist,
    // e.g., via eol on metal layer may be false because metal is connected
    //   --> DRC engine to add cost if true eol 
    // currently prl and tw are also included to avoid pessimistism
    // after adding via spacing rule, addPathCost should add an argument to allow only metal rules
    // via spacing should still be applied
    //} else if (connFig->typeId() == drcVia) {
    //  auto obj = static_cast<drVia*>(connFig.get());
    //  if (TEST) {
    //    cout <<"  initMazeCost_boundary_helper false bound " <<net->getFrNet()->getName();
    //  }
    //  if (isAddPathCost) {
    //    addPathCost(obj);
    //  } else {
    //    subPathCost(obj);
    //  }
    //}
  }
}

void FlexDRWorker::initFixedObjs() {
  bool enableOutput = false;
  //bool enableOutput = true;
  double dbu = getTech()->getDBUPerUU();
  //auto &drcBox = getRouteBox();
  auto &extBox = getExtBox(); // modified 6/18/19
  if (enableOutput) {
    cout << "  extBox (" << extBox.left()  / dbu << ", " << extBox.bottom() / dbu
         << ") -- ("     << extBox.right() / dbu << ", " << extBox.top()    / dbu << "):\n";
  }
  //box_t queryBox(point_t(drcBox.left(), drcBox.bottom()), point_t(drcBox.right(), drcBox.top()));
  box_t queryBox(point_t(extBox.left(), extBox.bottom()), point_t(extBox.right(), extBox.top()));
  set<frBlockObject*> drcObjSet;
  // fixed obj
  int cnt = 0;
  for (auto layerNum = getDesign()->getTech()->getBottomLayerNum(); 
       layerNum <= design->getTech()->getTopLayerNum(); ++layerNum) {
    auto regionQuery = getDesign()->getRegionQuery();
    vector<rq_rptr_value_t<frBlockObject> > queryResult;
    regionQuery->query(queryBox, layerNum, queryResult);
    for (auto &objPair: queryResult) {
      cnt ++;
      if (drcObjSet.find(objPair.second) == drcObjSet.end()) {
        drcObjSet.insert(objPair.second);
        fixedObjs.push_back(objPair.second);
      }
    }
  }
  //for (auto drcObj:drcObjSet) {
  //  fixedObjs.push_back(drcObj);
  //}
  if (enableOutput) {
    cout << "#fixed obj = " << fixedObjs.size() << "\n";
  }
  //cout << "#fixed obj (drworker) = " << cnt << "\n";
}

void FlexDRWorker::initMarkers() {
  vector<frMarker*> result;
  getRegionQuery()->queryMarker(getDrcBox(), result); // get all markers within drc box
  // int numRecheckMarkers = 0;
  for (auto mptr: result) {
    // check recheck， if true， then markers.clear(), set drWorker bit to check drc at start
    // recheck mark is complicated A.F.
    // if (mptr->getConstraint()->typeId() != frConstraintTypeEnum::frcRecheckConstraint) {
      markers.push_back(*mptr);
    // } else {
      // needRecheck = true;
      // numRecheckMarkers++;
    // }
  }
  setInitNumMarkers(getNumMarkers());
  // // this number is tied to how many ripup all iterations at the beginning
  // if (getDRIter() <= 2) {
  // // if (getRipupMode() == 1) {
  //   if (getNumMarkers() == 0) {
  //     setInitNumMarkers(1);
  //   }
  // }
}

void FlexDRWorker::init() {
  // if initDR
  //   get all instterm/term for each net
  // else
  //   1. get all insterm/term based on begin/end of pathseg, via
  //   2. union and find
  //
  //using namespace std::chrono;
  initMarkers();
  // if (!DRCTEST && isEnableDRC() && getDRIter() && getInitNumMarkers() == 0 && !needRecheck) {
  //   return;
  // }
  if (isEnableDRC() && getDRIter() && getInitNumMarkers() == 0 && getRipupMode() != 2) {
    skipRouting = true;
  }
  if (skipRouting) {
    return;
  }
  initFixedObjs();
  //high_resolution_clock::time_point t0 = high_resolution_clock::now();
  initNets();
  //high_resolution_clock::time_point t1 = high_resolution_clock::now();
  initGridGraph();
  //high_resolution_clock::time_point t2 = high_resolution_clock::now();
  initMazeIdx();
  //high_resolution_clock::time_point t3 = high_resolution_clock::now();
  initMazeCost();
  //high_resolution_clock::time_point t4 = high_resolution_clock::now();
}
