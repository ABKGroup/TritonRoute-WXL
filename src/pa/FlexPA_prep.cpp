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

#include <iostream>
#include <sstream>
#include <chrono>
#include "FlexPA.h"
#include "db/infra/frTime.h"
#include "gc/FlexGC.h"
#include <omp.h>

using namespace std;
using namespace fr;

int gcCallCnt = 0;

void FlexPA::prepPoint_pin_mergePinShapes(vector<gtl::polygon_90_set_data<frCoord> > &pinShapes, frPin* pin, frInstTerm* instTerm, bool isShrink) {
  frInst* inst = nullptr;
  if (instTerm) {
    inst = instTerm->getInst();
  }

  frTransform xform;
  if (inst) {
    inst->getUpdatedXform(xform);
  }

  vector<frCoord> layerWidths;
  if (isShrink) {
    layerWidths.resize(getDesign()->getTech()->getLayers().size(), 0);
    for (int i = 0; i < int(layerWidths.size()); i++) {
      layerWidths[i] = getDesign()->getTech()->getLayer(i)->getWidth();
    }
  }

  pinShapes.clear();
  pinShapes.resize(getDesign()->getTech()->getLayers().size());
  for (auto &shape: pin->getFigs()) {
    if (shape->typeId() == frcRect) {
      auto obj = static_cast<frRect*>(shape.get());
      auto layerNum = obj->getLayerNum();
      if (getDesign()->getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING) {
        continue;
      }
      frBox box;
      obj->getBBox(box);
      box.transform(xform);
      gtl::rectangle_data<frCoord> rect(box.left(), box.bottom(), box.right(), box.top());
      if (isShrink) {
        if (getDesign()->getTech()->getLayer(layerNum)->getDir() == frcHorzPrefRoutingDir) {
          gtl::shrink(rect, gtl::VERTICAL, layerWidths[layerNum] / 2);
        } else if (getDesign()->getTech()->getLayer(layerNum)->getDir() == frcVertPrefRoutingDir) {
          gtl::shrink(rect, gtl::HORIZONTAL, layerWidths[layerNum] / 2);
        }
      }
      using namespace boost::polygon::operators;
      pinShapes[layerNum] += rect;
    } else if (shape->typeId() == frcPolygon) {
      auto obj = static_cast<frPolygon*>(shape.get());
      auto layerNum = obj->getLayerNum();
      vector<gtl::point_data<frCoord> > points;
      // must be copied pts
      for (auto pt: obj->getPoints()) {
        pt.transform(xform);
        points.push_back(gtl::point_data<frCoord>(pt.x(), pt.y()));
      }
      gtl::polygon_90_data<frCoord> poly;
      poly.set(points.begin(), points.end());
      using namespace boost::polygon::operators;
      pinShapes[layerNum] += poly;
    } else {
      cout <<"Error: FlexPA mergePinShapes unsupported shape" <<endl;
      exit(1);
    }
  }
}

void FlexPA::prepPoint_pin_genPoints_rect_genGrid(map<frCoord, int> &coords, const map<frCoord, int> &trackCoords,
                                                  frCoord low, frCoord high) {
  for (auto it = trackCoords.lower_bound(low); it != trackCoords.end(); it++) {
    auto &[coord, cost] = *it;
    if (coord > high) {
      break;
    }
    coords.insert(*it);
  }
}
  
// will not generate center for wider edge
void FlexPA::prepPoint_pin_genPoints_rect_genCenter(map<frCoord, int> &coords, frLayerNum layerNum, frCoord low, frCoord high) {
  //bool enableOutput = true;
  bool enableOutput = false;
  // if touching two tracks, then no center??
  int cnt = 0;
  for (auto it = coords.lower_bound(low); it != coords.end(); it++) {
    auto &[c1, c2] = *it;
    if (c1 > high) {
      break;
    }
    if (c2 == 0) {
      cnt++;
    }
  }
  if (cnt >= 2) {
    return;
  }
  //if (high - low > 3 * (frCoord)(getDesign()->getTech()->getLayer(layerNum)->getWidth())) {
  //  return;
  //}

  frCoord manuGrid = getDesign()->getTech()->getManufacturingGrid();
  frCoord coord = (low + high) / 2 / manuGrid * manuGrid;
  auto it = coords.find(coord);
  if (it == coords.end()) {
    if (enableOutput) {
      double dbu = getDesign()->getTopBlock()->getDBUPerUU();
      cout <<"gen center pt@ " << coord / dbu <<endl;
    }
    coords.insert(make_pair(coord, 2));
  } else {
    coords[coord] = std::min(coords[coord], 2);
  }
}

void FlexPA::prepPoint_pin_genPoints_rect_ap_helper(vector<unique_ptr<frAccessPoint> > &aps, set<pair<frPoint, frLayerNum> > &apset,
                                                    const gtl::rectangle_data<frCoord> &maxrect, frCoord x, frCoord y, 
                                                    frLayerNum layerNum, bool allowPlanar, bool allowVia, 
                                                    int lowCost, int highCost) {
  gtl::point_data<frCoord> pt(x, y);
  if (!gtl::contains(maxrect, pt)) {
    return;
  }
  frPoint fpt(x, y);
  if (apset.find(make_pair(fpt, layerNum)) != apset.end()) {
    return;
  }
  auto ap = make_unique<frAccessPoint>();
  ap->setPoint(fpt);
  ap->setLayerNum(layerNum);
  if (allowPlanar) {
    auto lowerLayer = getDesign()->getTech()->getLayer(layerNum);
    ap->setAccess(frDirEnum::W, true);
    ap->setAccess(frDirEnum::E, true);
    ap->setAccess(frDirEnum::S, true);
    ap->setAccess(frDirEnum::N, true);
    // rectonly forbid wrongway planar access
    // rightway on grid only forbid off track rightway planar access
    // horz layer
    if (lowerLayer->getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir) {
      if (lowerLayer->getLef58RectOnlyConstraint()) {
        ap->setAccess(frDirEnum::S, false);
        ap->setAccess(frDirEnum::N, false);
      }
      if (lowerLayer->getLef58RightWayOnGridOnlyConstraint() && lowCost != 0) {
        ap->setAccess(frDirEnum::W, false);
        ap->setAccess(frDirEnum::E, false);
      } 
    }
    // vert layer
    if (lowerLayer->getDir() == frPrefRoutingDirEnum::frcVertPrefRoutingDir) {
      if (lowerLayer->getLef58RectOnlyConstraint()) {  
        ap->setAccess(frDirEnum::W, false);
        ap->setAccess(frDirEnum::E, false);
      }
      if (lowerLayer->getLef58RightWayOnGridOnlyConstraint() && lowCost != 0) {
        ap->setAccess(frDirEnum::S, false);
        ap->setAccess(frDirEnum::N, false);
      }
    }
  } else {
    ap->setAccess(frDirEnum::W, false);
    ap->setAccess(frDirEnum::E, false);
    ap->setAccess(frDirEnum::S, false);
    ap->setAccess(frDirEnum::N, false);
  }
  ap->setAccess(frDirEnum::D, false);
  if (allowVia) {
    // if (layerNum + 1 <= getDesign()->getTech()->getTopLayerNum()) {
      ap->setAccess(frDirEnum::U, true);
      //cout <<"@@@set U valid, check " <<ap->hasAccess(frDirEnum::U) <<endl;
    // }
  } else {
    ap->setAccess(frDirEnum::U, false);
  }
  ap->setType((frAccessPointEnum)lowCost, true);
  ap->setType((frAccessPointEnum)highCost, false);
  aps.push_back(std::move(ap));
  apset.insert(make_pair(fpt, layerNum));
}

void FlexPA::prepPoint_pin_genPoints_rect_ap(vector<unique_ptr<frAccessPoint> > &aps, set<pair<frPoint, frLayerNum> > &apset,
                                             const gtl::rectangle_data<frCoord> &rect,
                                             frLayerNum layerNum, bool allowPlanar, bool allowVia, bool isLayer1Horz,
                                             const map<frCoord, int> &xCoords, const map<frCoord, int> &yCoords, 
                                             int lowerType, int upperType) {
  // build points;
  for (auto &[xCoord, costX]: xCoords) {
    for (auto &[yCoord, costY]: yCoords) {
      // lower full/half/center
      auto &lowCost  = isLayer1Horz    ? costY : costX;
      auto &highCost = (!isLayer1Horz) ? costY : costX;
      if (lowCost == lowerType && highCost == upperType) {
        prepPoint_pin_genPoints_rect_ap_helper(aps, apset, rect, xCoord, yCoord, layerNum, allowPlanar, allowVia, lowCost, highCost);
      }
    }
  }
}

void FlexPA::prepPoint_pin_genPoints_rect_genEnc(map<frCoord, int> &coords, const gtl::rectangle_data<frCoord> &rect, 
                                                 frLayerNum layerNum, bool isCurrLayerHorz) {
  auto rectWidth  = gtl::delta(rect, gtl::HORIZONTAL);
  auto rectHeight = gtl::delta(rect, gtl::VERTICAL);
  int maxNumViaTrial = 2;
  if (layerNum + 1 > getDesign()->getTech()->getTopLayerNum()) {
    return;
  }
  // hardcode first two single vias
  vector<frViaDef*> viaDefs;
  int cnt = 0;
  for (auto &[tup, via]: layerNum2ViaDefs[layerNum + 1][1]) {
    viaDefs.push_back(via);
    cnt++;
    if (cnt >= maxNumViaTrial) {
      break;
    }
  }
  frBox box;
  for (auto &viaDef: viaDefs) {
    frVia via(viaDef);
    via.getLayer1BBox(box);
    auto viaWidth  = box.right() - box.left();
    auto viaHeight = box.top()   - box.bottom();
    if (viaWidth > rectWidth || viaHeight > rectHeight) {
      //cout <<"@@@" <<viaDef->getName() <<" rect " <<rectWidth <<" " <<rectHeight <<" via " <<viaWidth <<" " <<viaHeight <<endl;
      continue;
    }
    if (isCurrLayerHorz) {
      auto coord = gtl::yh(rect) - (box.top() - 0);
      auto it = coords.find(coord);
      if (it == coords.end()) {
        //cout << coord / 2000.0 <<endl;
        coords.insert(make_pair(coord, 3));
      } else {
        coords[coord] = std::min(coords[coord], 3);
      }
      coord = gtl::yl(rect) + (0 - box.bottom());
      it = coords.find(coord);
      if (it == coords.end()) {
        //cout << coord / 2000.0 <<endl;
        coords.insert(make_pair(coord, 3));
      } else {
        coords[coord] = std::min(coords[coord], 3);
      }
    } else {
      auto coord = gtl::xh(rect) - (box.right() - 0);
      auto it = coords.find(coord);
      if (it == coords.end()) {
        //cout << coord / 2000.0 <<endl;
        coords.insert(make_pair(coord, 3));
      } else {
        coords[coord] = std::min(coords[coord], 3);
      }
      coord = gtl::xl(rect) + (0 - box.left());
      it = coords.find(coord);
      if (it == coords.end()) {
        //cout << coord / 2000.0 <<endl;
        coords.insert(make_pair(coord, 3));
      } else {
        coords[coord] = std::min(coords[coord], 3);
      }
    }
  }
}
void FlexPA::prepPoint_pin_genPoints_rect(vector<unique_ptr<frAccessPoint> > &aps, set<pair<frPoint, frLayerNum> > &apset,
                                          const gtl::rectangle_data<frCoord> &rect,
                                          frLayerNum layerNum, bool allowPlanar, bool allowVia, int lowerType, int upperType, bool isMacroCellPin) {
  if (std::min(gtl::delta(rect, gtl::HORIZONTAL), gtl::delta(rect, gtl::VERTICAL)) <
      getDesign()->getTech()->getLayer(layerNum)->getMinWidth()) {
    return;
  }
  frLayerNum secondLayerNum = 0;
  if (layerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) {
    secondLayerNum = layerNum + 2;
  } else if (layerNum - 2 >= getDesign()->getTech()->getBottomLayerNum()) {
    secondLayerNum = layerNum - 2;
  } else {
    cout <<"Error: prepPoint_pin_genPoints_rect cannot find secondLayerNum" <<endl;
    exit(1);
  }
  auto &layer1TrackCoords = trackCoords[layerNum];
  auto &layer2TrackCoords = trackCoords[secondLayerNum];
  bool isLayer1Horz = (getDesign()->getTech()->getLayer(layerNum)->getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);

  map<frCoord, int> xCoords;
  map<frCoord, int> yCoords;

  bool useCenterLine = false;
  if (isMacroCellPin) {
    auto rectDir = gtl::guess_orientation(rect);
    if ((rectDir == gtl::HORIZONTAL && isLayer1Horz) || 
        (rectDir == gtl::VERTICAL && !isLayer1Horz)) {
      auto layerWidth = getDesign()->getTech()->getLayer(layerNum)->getWidth();
      if ((rectDir == gtl::HORIZONTAL && gtl::delta(rect, gtl::VERTICAL) < 2 * layerWidth) ||
          (rectDir == gtl::VERTICAL && gtl::delta(rect, gtl::HORIZONTAL) < 2 * layerWidth)) {
        useCenterLine = true;
      }
    }
  }

  // gen all full/half grid coords
  if (!isMacroCellPin || !useCenterLine) {
    if (isLayer1Horz) {
      prepPoint_pin_genPoints_rect_genGrid(yCoords, layer1TrackCoords, gtl::yl(rect), gtl::yh(rect));
      prepPoint_pin_genPoints_rect_genGrid(xCoords, layer2TrackCoords, gtl::xl(rect), gtl::xh(rect));
      if (lowerType >= 2) {
        prepPoint_pin_genPoints_rect_genCenter(yCoords, layerNum, gtl::yl(rect), gtl::yh(rect));
      }
      if (lowerType >= 3) {
        prepPoint_pin_genPoints_rect_genEnc(yCoords, rect, layerNum, isLayer1Horz);
      }
      if (upperType >= 2) {
        prepPoint_pin_genPoints_rect_genCenter(xCoords, layerNum, gtl::xl(rect), gtl::xh(rect));
      }
      if (upperType >= 3) {
        prepPoint_pin_genPoints_rect_genEnc(xCoords, rect, layerNum, !isLayer1Horz);
      }
    } else {
      prepPoint_pin_genPoints_rect_genGrid(xCoords, layer1TrackCoords, gtl::xl(rect), gtl::xh(rect));
      prepPoint_pin_genPoints_rect_genGrid(yCoords, layer2TrackCoords, gtl::yl(rect), gtl::yh(rect));
      if (lowerType >= 2) {
        prepPoint_pin_genPoints_rect_genCenter(xCoords, layerNum, gtl::xl(rect), gtl::xh(rect));
      }
      if (lowerType >= 3) {
        prepPoint_pin_genPoints_rect_genEnc(xCoords, rect, layerNum, isLayer1Horz);
      }
      if (upperType >= 2) {
        prepPoint_pin_genPoints_rect_genCenter(yCoords, layerNum, gtl::yl(rect), gtl::yh(rect));
      }
      if (upperType >= 3) {
        prepPoint_pin_genPoints_rect_genEnc(yCoords, rect, layerNum, !isLayer1Horz);
      }
    }
  } else {
    if (isLayer1Horz) {
      lowerType = 0;
      prepPoint_pin_genPoints_rect_genGrid(xCoords, layer2TrackCoords, gtl::xl(rect), gtl::xh(rect));
      if (upperType >= 2) {
        prepPoint_pin_genPoints_rect_genCenter(xCoords, layerNum, gtl::xl(rect), gtl::xh(rect));
      }
      if (upperType >= 3) {
        prepPoint_pin_genPoints_rect_genEnc(xCoords, rect, layerNum, !isLayer1Horz);
      }
      prepPoint_pin_genPoints_rect_genCenter(yCoords, layerNum, gtl::yl(rect), gtl::yh(rect));
      for (auto &[yCoord, cost]: yCoords) {
        yCoords[yCoord] = 0;
      }
    } else {
      // prepPoint_pin_genPoints_rect_genGrid(xCoords, layer1TrackCoords, gtl::xl(rect), gtl::xh(rect));
      prepPoint_pin_genPoints_rect_genGrid(yCoords, layer2TrackCoords, gtl::yl(rect), gtl::yh(rect));
      // if (lowerType >= 2) {
      //   prepPoint_pin_genPoints_rect_genCenter(xCoords, layerNum, gtl::xl(rect), gtl::xh(rect));
      // }
      // if (lowerType >= 3) {
      //   prepPoint_pin_genPoints_rect_genEnc(xCoords, rect, layerNum, isLayer1Horz);
      // }
      if (upperType >= 2) {
        prepPoint_pin_genPoints_rect_genCenter(yCoords, layerNum, gtl::yl(rect), gtl::yh(rect));
      }
      if (upperType >= 3) {
        prepPoint_pin_genPoints_rect_genEnc(yCoords, rect, layerNum, !isLayer1Horz);
      }
      prepPoint_pin_genPoints_rect_genCenter(xCoords, layerNum, gtl::xl(rect), gtl::xh(rect));
      for (auto &[xCoord, cost]: xCoords) {
        xCoords[xCoord] = 0;
      }
    }
  }
  prepPoint_pin_genPoints_rect_ap(aps, apset, rect, layerNum, allowPlanar, allowVia, 
                                  isLayer1Horz, xCoords, yCoords, lowerType, upperType);
}

void FlexPA::prepPoint_pin_genPoints_layerShapes(vector<unique_ptr<frAccessPoint> > &aps, set<pair<frPoint, frLayerNum> > &apset,
                                                 frPin* pin, frInstTerm* instTerm,
                                                 const gtl::polygon_90_set_data<frCoord> &layerShapes,
                                                 frLayerNum layerNum, bool allowVia, int lowerType, int upperType) {
  //bool enableOutput = false;
  //bool enableOutput = true;
  if (getDesign()->getTech()->getLayer(layerNum)->getType() != frLayerTypeEnum::ROUTING) {
    return;
  }
  bool allowPlanar = true;
  bool isMacroCellPin = false;
  // bool isIOPin = false;
  if (instTerm) {
    if (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE ||
        instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIEHIGH ||
        instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIELOW ||
        instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_ANTENNACELL) {
      if ((layerNum >= VIAINPIN_BOTTOMLAYERNUM && layerNum <= VIAINPIN_TOPLAYERNUM) || 
          layerNum <= VIA_ACCESS_LAYERNUM) {
        // if (DBPROCESSNODE == "GF14_13M_3Mx_2Cx_4Kx_2Hx_2Gx_LB") {
          allowPlanar = false;
        // }
      }
    } else if (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
               instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
               instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
               instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::RING) {
      isMacroCellPin = true;
      // if (layerNum >= VIAONLY_MACROCELLPIN_BOTTOMLAYERNUM && layerNum <= VIAONLY_MACROCELLPIN_TOPLAYERNUM) {
      //   allowPlanar = false;
      // }
    }
  } else {
    // isIOPin = true;
    // IO term is treated as the MacroCellPin as the top block
    isMacroCellPin = true;
    allowPlanar = true;
    allowVia = false;
  }
  // lower layer is current layer
  // righway on grid only forbid off track up via access on upper layer
  auto upperLayer = (layerNum + 2 <= getDesign()->getTech()->getTopLayerNum()) ? getDesign()->getTech()->getLayer(layerNum + 2) : nullptr;
  if (!isMacroCellPin && upperLayer && upperLayer->getLef58RightWayOnGridOnlyConstraint() && upperType != 0) {
    return;
  }
  vector<gtl::rectangle_data<frCoord> > maxrects;
  gtl::get_max_rectangles(maxrects, layerShapes);
  for (auto &bboxRect: maxrects) {
    prepPoint_pin_genPoints_rect(aps, apset, bboxRect, layerNum, allowPlanar, allowVia, lowerType, upperType, isMacroCellPin);
  }
}


// filter off-grid coordinate
// lower on-grid 0, upper on-grid 0 = 0
// lower 1/2     1, upper on-grid 0 = 1
// lower center  2, upper on-grid 0 = 2
// lower center  2, upper center  2 = 4
void FlexPA::prepPoint_pin_genPoints(vector<unique_ptr<frAccessPoint> > &aps, set<pair<frPoint, frLayerNum> > &apset,
                                     frPin* pin, frInstTerm *instTerm,
                                     const vector<gtl::polygon_90_set_data<frCoord> > &pinShapes,
                                     int lowerType, int upperType) {
  bool enableOutput = false;
  //bool enableOutput = true;
  // only VIA_ACCESS_LAYERNUM layer can have via access
  bool allowVia = true;
  frLayerNum layerNum = (int)pinShapes.size() - 1;
  for (auto it = pinShapes.rbegin(); it != pinShapes.rend(); it++) {
    if ((layerNum == VIA_ACCESS_LAYERNUM || layerNum == VIA_ACCESS_LAYERNUM + 2) && instTerm != nullptr) {
      allowVia = true;
    } else {
      allowVia = false;
    }
    if (!it->empty() && getDesign()->getTech()->getLayer(layerNum)->getType() == frLayerTypeEnum::ROUTING) {
      //cout <<"via layernum = " <<layerNum <<endl;
      prepPoint_pin_genPoints_layerShapes(aps, apset, pin, instTerm, *it, layerNum, allowVia, lowerType, upperType);
      // allowVia = false;
    }
    layerNum--;
  }
  if (enableOutput) {
    if (aps.empty()) {
      cout <<"Warning: prepPoint_pin_genPoints zero ap" <<endl;
    }
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    frPoint pt;
    for (auto &ap: aps) {
      ap->getPoint(pt);
      cout <<"generated ap@(" <<pt.x() / dbu <<", " <<pt.y() / dbu <<") "
           <<getDesign()->getTech()->getLayer(ap->getLayerNum())->getName() <<" , cost=" 
           <<ap->getCost() <<endl;
    }
  }
}

bool FlexPA::prepPoint_pin_checkPoint_planar_ep(frPoint &ep, 
                                                const vector<gtl::polygon_90_data<frCoord> > &layerPolys,
                                                const frPoint &bp, frLayerNum layerNum, frDirEnum dir, int stepSizeMultiplier) {
  frCoord x = bp.x();
  frCoord y = bp.y();
  frCoord width = getDesign()->getTech()->getLayer(layerNum)->getWidth();
  frCoord stepSize = stepSizeMultiplier * width;
  switch (dir) {
    case (frDirEnum::W):
      x -= stepSize;
      break;
    case (frDirEnum::E):
      x += stepSize;
      break;
    case (frDirEnum::S):
      y -= stepSize;
      break;
    case (frDirEnum::N):
      y += stepSize;
      break;
    default:
      std::cout << "unexpected direction in getPlanarEP\n";
  }
  ep.set(x, y);
  gtl::point_data<frCoord> pt(x, y);
  bool outside = true;
  for (auto &layerPoly: layerPolys) {
    if (gtl::contains(layerPoly, pt)) {
      outside = false;
      break;
    }
  }

  // width enclosure
  // bool isWidthEnclosed = false;
  // frCoord widthX1 = bp.x();
  // frCoord widthX2 = bp.x();
  // frCoord widthY1 = bp.y();
  // frCoord widthY2 = bp.y();

  // if (dir == frDirEnum::E || dir == frDirEnum::W) {
  //   widthY1 += width / 2;
  //   widthY2 -= width / 2;
  // } else if (dir == frDirEnum::N || dir == frDirEnum::S) {
  //   widthX1 += width / 2;
  //   widthX2 -= width / 2;
  // }
  // gtl::point_data<frCoord> widthPt1(widthX1, widthY1);
  // gtl::point_data<frCoord> widthPt2(widthX2, widthY2);
  // for (auto &layerPoly: layerPolys) {
  //   if (gtl::contains(layerPoly, widthPt1) && gtl::contains(layerPoly, widthPt2)) {
  //     isWidthEnclosed = true;
  //     break;
  //   }
  // }

  // check width
  // return (outside && isWidthEnclosed);
  return outside;
}

void FlexPA::prepPoint_pin_checkPoint_print_helper(frAccessPoint* ap, frPin* pin, frInstTerm* instTerm,
                                                   frDirEnum dir, int typeGC, int typeDRC,
                                                   frPoint bp, frPoint ep, frViaDef* viaDef) {
  //double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  // type == 1 clean; type == 2 drc
  if (typeDRC != -1) { // compare mode
    if (typeGC == 1 && typeDRC == 2) {
      cout <<"  gcmisses";
    } else if (typeGC == 2 && typeDRC == 1) {
      cout <<"  gcfalsealarm";
    } else {
      return;
    }
  } else { // pure gc mode
    if (typeGC == 1) {
      cout <<"  gcclean";
    } else if (typeGC == 2) {
      cout <<"  gcdirty";
    } else {
      return;
    }
  }
  if (viaDef) {
    cout <<"via";
  } else {
    cout <<"seg";
  }
  frTransform xform, revertCellXForm;
  if (instTerm) {
    frInst* inst = instTerm->getInst();
    inst->getTransform(xform);
    revertCellXForm.set(-xform.xOffset(), -xform.yOffset());
    revertCellXForm.set(frcR0);
    bp.transform(revertCellXForm);
    ep.transform(revertCellXForm);
  }
  frPoint transSP, transEP;
  if (bp < ep) {
    transSP = bp;
    transEP = ep;
  } else {
    transSP = ep;
    transEP = bp;
  }
  map<frOrientEnum, string> orient2Name = {{frcR0, "R0"}, 
                                           {frcR90, "R90"}, 
                                           {frcR180, "R180"}, 
                                           {frcR270, "R270"}, 
                                           {frcMY, "MY"}, 
                                           {frcMXR90, "MX90"},
                                           {frcMX, "MX"},
                                           {frcMYR90, "MY90"}};
  if (viaDef) {
    cout <<" " << instTerm->getInst()->getRefBlock()->getName() << " "
         << instTerm->getTerm()->getName() << " " << viaDef->getName() << " "
         << bp.x() << " " << bp.y() 
         << " " << orient2Name[instTerm->getInst()->getOrient()] << "\n";
  } else {
    frCoord layerWidth = getDesign()->getTech()->getLayer(ap->getLayerNum())->getWidth();
    if (transSP.x() == transEP.x()) {
      cout << " " << instTerm->getInst()->getRefBlock()->getName() << " "
           << instTerm->getTerm()->getName() << " "
           << transSP.x() << " " << transSP.y() - layerWidth / 2 << " "
           << transEP.x() << " " << transEP.y() + layerWidth / 2 << " "
           << orient2Name[instTerm->getInst()->getOrient()]      << " "
           << getDesign()->getTech()->getLayer(ap->getLayerNum())->getName() << "\n";
    } else {
      cout << " " << instTerm->getInst()->getRefBlock()->getName() << " "
           << instTerm->getTerm()->getName() << " "
           << transSP.x() - layerWidth / 2 << " " << transSP.y() << " "
           << transEP.x() + layerWidth / 2 << " " << transEP.y() << " "
           << orient2Name[instTerm->getInst()->getOrient()]      << " "
           << getDesign()->getTech()->getLayer(ap->getLayerNum())->getName() << "\n";
    }
  }
}

void FlexPA::prepPoint_pin_checkPoint_planar(frAccessPoint* ap, 
                                             const vector<gtl::polygon_90_data<frCoord> > &layerPolys,
                                             frDirEnum dir,
                                             frPin* pin, frInstTerm* instTerm) {
  bool enableOutput = false;
  //bool enableOutput = true;
  //bool compMode = false;
  frPoint bp, ep;
  ap->getPoint(bp);
  // skip viaonly access
  if (!ap->hasAccess(dir)) {
    //if (enableOutput) {
    //  prepPoint_pin_checkPoint_print_helper(ap, pin, instTerm, dir, 0, -1, bp, ep, nullptr);
    //}
    return;
  }
  bool isStdCellPin = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE ||
                                    instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIEHIGH ||
                                    instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIELOW ||
                                    instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_ANTENNACELL));
  // if (DBPROCESSNODE == "GF14_13M_3Mx_2Cx_4Kx_2Hx_2Gx_LB") {
     
  // } else {
    bool isOutSide = prepPoint_pin_checkPoint_planar_ep(ep, layerPolys, bp, ap->getLayerNum(), dir);
    // skip if two width within shape for standard cell
    if (isStdCellPin && !isOutSide) {
      ap->setAccess(dir, false);
      if (enableOutput) {
        prepPoint_pin_checkPoint_print_helper(ap, pin, instTerm, dir, 0, -1, bp, ep, nullptr);
      }
      return;
    }
  // }

  auto ps = make_unique<frPathSeg>();
  auto style = getDesign()->getTech()->getLayer(ap->getLayerNum())->getDefaultSegStyle();
  if (dir == frDirEnum::W || dir == frDirEnum::S) {
    ps->setPoints(ep, bp);
    style.setEndStyle(frcTruncateEndStyle, 0);
  } else {
    ps->setPoints(bp, ep);
    style.setBeginStyle(frcTruncateEndStyle, 0);
  }
  ps->setLayerNum(ap->getLayerNum());
  ps->setStyle(style);
  if (instTerm && instTerm->hasNet()) {
    ps->addToNet(instTerm->getNet());
  } else {
    ps->addToPin(pin);
  }

  // old drcWorker
  int typeDRC = -1;
  // if (compMode) {
  //   std::vector<frBlockObject*> pinObjs;
  //   std::set<frBlockObject*> pinObjSet;
  //   {
  //     frBox box;
  //     frInst* inst = nullptr;
  //     if (instTerm) {
  //       inst = instTerm->getInst();
  //       instTerm->getInst()->getBoundaryBBox(box);
  //     }
  //     auto regionQuery = design->getRegionQuery();
  //     std::vector<rq_rptr_value_t<frBlockObject> > queryResult;
  //     for (auto layerNum = design->getTech()->getBottomLayerNum(); layerNum <= design->getTech()->getTopLayerNum(); ++layerNum) {
  //       queryResult.clear();
  //       regionQuery->query(box, layerNum, queryResult);
  //       for (auto &[objBox, objPtr]: queryResult) {
  //         auto it = pinObjSet.find(objPtr);
  //         if (it == pinObjSet.end()) {
  //           if ((objPtr->typeId() == frcInstTerm && (static_cast<frInstTerm*>(objPtr))->getInst() == inst) || 
  //               (objPtr->typeId() == frcInstBlockage && (static_cast<frInstBlockage*>(objPtr)->getInst() == inst))) {
  //             pinObjs.push_back(objPtr);
  //           }
  //           pinObjSet.insert(objPtr);
  //         }
  //       }
  //     }
  //   }
  //   pinObjs.push_back(ps.get());
  //   DRCWorker drcWorker(design, pinObjs);
  //   drcWorker.init();
  //   drcWorker.setup();
  //   drcWorker.check();
  //   if (drcWorker.getViolations().empty()) {
  //     typeDRC = 1;
  //   } else {
  //     typeDRC = 2;
  //   }
  // }

  // new gcWorker
  FlexGCWorker gcWorker(getDesign());
  gcWorker.setIgnoreMinArea();
  frBox extBox(bp.x() - 3000, bp.y() - 3000, bp.x() + 3000, bp.y() + 3000);
  gcWorker.setExtBox(extBox);
  gcWorker.setDrcBox(extBox);
  if (instTerm) {
    gcWorker.setTargetObj(instTerm->getInst());
  } else {
    gcWorker.setTargetObj(pin->getTerm());
  }
  gcWorker.initPA0();
  if (instTerm) {
    if (instTerm->hasNet()) {
      gcWorker.addPAObj(ps.get(), instTerm->getNet());
    } else {
      gcWorker.addPAObj(ps.get(), instTerm);
    }
  } else {
    if (pin->getTerm()->hasNet()) {
      gcWorker.addPAObj(ps.get(), pin->getTerm()->getNet());
    } else {
      gcWorker.addPAObj(ps.get(), pin->getTerm());
    }
  }
  gcWorker.initPA1();
  gcWorker.main();
  gcWorker.end();

  int typeGC  = 0;
  if (gcWorker.getMarkers().empty()) {
    ap->setAccess(dir, true);
    // if (instTerm->getInst()->getRefBlock()->getName() == string("gf14_1rw_d128_w116_m2_bit_mod") && pin->getTerm()->getName() == string("EMAS")) {
    //     cout << " @@@ debug @@@ here1" << endl;
    //     cout << " bp (" << bp.x() / 2000.0 << ", " << bp.y() / 2000.0 << ")\n";
    //     cout << " ep (" << ep.x() / 2000.0 << ", " << ep.y() / 2000.0 << ")\n";
    // }
    // if (instTerm->getInst()->getRefBlock()->getName() == string("gf14_1rw_d128_w116_m2_bit_mod") && pin->getTerm()->getName() == string("CEN")) {
    //     cout << " @@@ debug @@@ here1 CEN" << endl;
    //     cout << " bp (" << bp.x() / 2000.0 << ", " << bp.y() / 2000.0 << ")\n";
    //     cout << " ep (" << ep.x() / 2000.0 << ", " << ep.y() / 2000.0 << ")\n";
    // }
    typeGC = 1;
  } else {
    // if (instTerm->getInst()->getRefBlock()->getName() == string("gf14_1rw_d128_w116_m2_bit_mod") && pin->getTerm()->getName() == string("EMAS")) {
    //   cout << "inst: " << instTerm->getInst()->getName() << ", isStdCellPin = " << isStdCellPin << endl;
    //   cout << " bp (" << bp.x() / 2000.0 << ", " << bp.y() / 2000.0 << ")\n";
    //   cout << " ep (" << ep.x() / 2000.0 << ", " << ep.y() / 2000.0 << ")\n";
    //   for (auto &marker: gcWorker.getMarkers()) {
    //     cout << " @@@ debug @@@ " << (int)marker->getConstraint()->typeId() << endl;
    //     frBox bbox;
    //     marker->getBBox(bbox);
    //     cout << "    (" << bbox.left() / 2000.0 << ", " << bbox.bottom() / 2000.0 << ") - ("
    //          << bbox.right() / 2000.0 << ", " << bbox.top() / 2000.0 << ")\n";
    //     for (auto src: marker->getSrcs()) {
    //       if (src) {
    //         cout << "      " << (int)src->typeId() << endl;
    //         if (src->typeId() == frcNet) {
    //           cout << "        " << static_cast<frNet*>(src)->getName() << endl;
    //         }
    //       } else {
    //         cout << "      nullptr\n";
    //       }
    //     }
    //   }
    // }
    ap->setAccess(dir, false);
    typeGC = 2;
  }
  if (enableOutput) {
    prepPoint_pin_checkPoint_print_helper(ap, pin, instTerm, dir, typeGC, typeDRC, bp, ep, nullptr);
  }
}

void FlexPA::prepPoint_pin_checkPoint_via(frAccessPoint* ap, 
                                          const gtl::polygon_90_set_data<frCoord> &polyset,
                                          frDirEnum dir,
                                          frPin* pin, frInstTerm* instTerm) {
  //bool enableOutput = false;
  //bool enableOutput = true;
  //bool compMode = false;
  frPoint bp;
  ap->getPoint(bp);
  auto layerNum = ap->getLayerNum();
  //cout <<"@@@mmm" <<endl;
  // skip planar only access
  if (!ap->hasAccess(dir)) {
    //cout <<"no via access, check " <<ap->hasAccess(frDirEnum::U) <<endl;
    return;
  }

  bool viainpin = false;
  if (layerNum >= VIAINPIN_BOTTOMLAYERNUM && layerNum <= VIAINPIN_TOPLAYERNUM) {
    viainpin = true;
  } else if (ap->getType(true) == frAccessPointEnum::frcEncOptAP || ap->getType(false) == frAccessPointEnum::frcEncOptAP) {
    viainpin = true;
  }

  int maxNumViaTrial = 2;
  // use std:pair to ensure deterministic behavior
  vector<pair<int, frViaDef*> > viaDefs;
  // hardcode first two single vias
  int cnt = 0;
  for (auto &[tup, viaDef]: layerNum2ViaDefs[layerNum + 1][1]) {
    viaDefs.push_back(make_pair(viaDefs.size(), viaDef));
    cnt++;
    if (cnt >= maxNumViaTrial) {
      break;
    }
  }

  // check if ap is on the left/right boundary of the cell
  bool isLRBound = false;
  if (instTerm) {
    frBox boundaryBBox;
    instTerm->getInst()->getBoundaryBBox(boundaryBBox);
    frCoord width = getDesign()->getTech()->getLayer(layerNum)->getWidth();
    if (bp.x() <= boundaryBBox.left() + 3 * width || bp.x() >= boundaryBBox.right() - 3 * width) {
      isLRBound = true;
    }
  }

  set<tuple<frCoord, int, frViaDef*> > validViaDefs;
  frBox box;
  //for (auto &[idx, viaDef]: viaDefs) {
  //  auto via = make_unique<frVia>(viaDef);
  //  via->setOrigin(bp);
  //  via->getLayer1BBox(box);
  //  gtl::rectangle_data<frCoord> viarect(box.left(), box.bottom(), box.right(), box.top());
  //  auto viarectArea = gtl::area(viarect);
  //  using namespace boost::polygon::operators;
  //  auto intersection = polyset & viarect;
  //  auto intersectionArea = gtl::area(intersection);
  //  //cout <<"@@@?????" <<endl;
  //  if (viainpin && intersectionArea < viarectArea) {
  //      continue;
  //  }
  //  auto nonEnclosedArea = viarectArea - intersectionArea;
  //  //cout <<"@@@start" <<endl;
  //  if (prepPoint_pin_checkPoint_via_helper(ap, via.get(), pin, instTerm)) {
  //    //cout <<"@@@clean" <<endl;
  //    validViaDefs.insert(make_tuple(nonEnclosedArea, idx, viaDef));
  //  }
  //}
  for (auto &[idx, viaDef]: viaDefs) {
    auto via = make_unique<frVia>(viaDef);
    via->setOrigin(bp);
    via->getLayer1BBox(box);
    gtl::rectangle_data<frCoord> viarect(box.left(), box.bottom(), box.right(), box.top());
    //auto viarectArea = gtl::area(viarect);
    using namespace boost::polygon::operators;
    gtl::polygon_90_set_data<frCoord> intersection = polyset & viarect;
    gtl::rectangle_data<frCoord> intersection_extRect;
    intersection.extents(intersection_extRect);
    frCoord leftExt   = std::max(0,   gtl::xl(intersection_extRect) - gtl::xl(viarect));
    frCoord rightExt  = std::max(0, - gtl::xh(intersection_extRect) + gtl::xh(viarect));
    frCoord bottomExt = std::max(0,   gtl::yl(intersection_extRect) - gtl::yl(viarect));
    frCoord topExt    = std::max(0, - gtl::yh(intersection_extRect) + gtl::yh(viarect));
    // via ranking criteria: max extension distance beyond pin shape
    frCoord maxExt = std::max(std::max(leftExt, rightExt), std::max(bottomExt, topExt));
    // via ranking criteria for boundary pin: max horizontal extension distance beyond pin shape
    if (isLRBound && !viainpin) {
      maxExt = std::max(leftExt, rightExt);
    }
    //cout <<"@@@?????" <<endl;
    if (viainpin && maxExt > 0) {
        continue;
    }
    if (instTerm) {
      frBox boundaryBBox;
      instTerm->getInst()->getBoundaryBBox(boundaryBBox);
      if (!boundaryBBox.contains(box)) {
        continue;
      }
    }
    
    // avoid layer2BBox outside of cell
    if (instTerm) {
      frBox layer2BBox, boundaryBBox;
      via->getLayer2BBox(layer2BBox);
      instTerm->getInst()->getBoundaryBBox(boundaryBBox);
      if (!boundaryBBox.contains(layer2BBox)) {
        continue;
      }
    }

    //cout <<"@@@start" <<endl;
    if (prepPoint_pin_checkPoint_via_helper(ap, via.get(), pin, instTerm)) {
      //cout <<"@@@clean" <<endl;
      validViaDefs.insert(make_tuple(maxExt, idx, viaDef));
    }
  }
  if (validViaDefs.empty()) {
    ap->setAccess(dir, false);
  }
  for (auto &[area, idx, viaDef]: validViaDefs) {
    ap->addViaDef(viaDef);
  }
}

bool FlexPA::prepPoint_pin_checkPoint_via_helper(frAccessPoint* ap, frVia* via, frPin* pin, frInstTerm* instTerm) {
  bool enableOutput = false;
  // bool enableOutput = true;
  //bool compMode = false;
  frPoint bp, ep;
  ap->getPoint(bp);
  //auto layerNum = ap->getLayerNum();

  if (instTerm && instTerm->hasNet()) {
    via->addToNet(instTerm->getNet());
  } else {
    via->addToPin(pin);
  }

  // old drcWorker
  int typeDRC = -1;
  // if (compMode) {
  //   std::vector<frBlockObject*> pinObjs;
  //   std::set<frBlockObject*> pinObjSet;
  //   {
  //     frBox box;
  //     frInst* inst = nullptr;
  //     if (instTerm) {
  //       inst = instTerm->getInst();
  //       instTerm->getInst()->getBoundaryBBox(box);
  //     }
  //     auto regionQuery = design->getRegionQuery();
  //     std::vector<rq_rptr_value_t<frBlockObject> > queryResult;
  //     for (auto layerNum = design->getTech()->getBottomLayerNum(); layerNum <= design->getTech()->getTopLayerNum(); ++layerNum) {
  //       queryResult.clear();
  //       regionQuery->query(box, layerNum, queryResult);
  //       for (auto &[objBox, objPtr]: queryResult) {
  //         auto it = pinObjSet.find(objPtr);
  //         if (it == pinObjSet.end()) {
  //           if ((objPtr->typeId() == frcInstTerm && (static_cast<frInstTerm*>(objPtr))->getInst() == inst) || 
  //               (objPtr->typeId() == frcInstBlockage && (static_cast<frInstBlockage*>(objPtr)->getInst() == inst))) {
  //             pinObjs.push_back(objPtr);
  //           }
  //           pinObjSet.insert(objPtr);
  //         }
  //       }
  //     }
  //   }
  //   pinObjs.push_back(via);
  //   DRCWorker drcWorker(design, pinObjs);
  //   drcWorker.addIgnoredConstraintType(frConstraintTypeEnum::frcAreaConstraint);
  //   drcWorker.init();
  //   drcWorker.setup();
  //   drcWorker.check();
  //   if (drcWorker.getViolations().empty()) {
  //     typeDRC = 1;
  //   } else {
  //     typeDRC = 2;
  //   }
  // }

  // new gcWorker
  FlexGCWorker gcWorker(getDesign());
  gcWorker.setIgnoreMinArea();
  frBox extBox(bp.x() - 3000, bp.y() - 3000, bp.x() + 3000, bp.y() + 3000);
  gcWorker.setExtBox(extBox);
  gcWorker.setDrcBox(extBox);
  if (instTerm) {
    gcWorker.setTargetObj(instTerm->getInst());
  } else {
    gcWorker.setTargetObj(pin->getTerm());
  }
  //cout <<flush;
  gcWorker.initPA0();
  if (instTerm) {
    if (instTerm->hasNet()) {
      gcWorker.addPAObj(via, instTerm->getNet());
    } else {
      gcWorker.addPAObj(via, instTerm);
    }
  } else {
    gcWorker.addPAObj(via, pin->getTerm());
  }
  gcWorker.initPA1();
  gcWorker.main();
  gcWorker.end();

  int typeGC  = 0;
  bool sol = false;
  if (gcWorker.getMarkers().empty()) {
    typeGC = 1;
    sol = true;
  } else {
    typeGC = 2;
  }
  if (enableOutput) {
    // if (instTerm->getInst()->getName() == string("_14797_") && instTerm->getTerm()->getName() == string("Y")) {
      prepPoint_pin_checkPoint_print_helper(ap, pin, instTerm, frDirEnum::U, typeGC, typeDRC, bp, ep, via->getViaDef());
      //if (typeGC == 2) {
      //  for (auto &marker: gcWorker.getMarkers()) {
      //    frBox bbox;
      //    marker.get()->getBBox(bbox);
      //    cout << "  violation on layer " << marker.get()->getLayerNum() 
      //         << ", bbox = (" << bbox.left() / 2000.0 << ", " << bbox.bottom() / 2000.0 << ") - (" << bbox.right() / 2000.0 << ", " << bbox.top() / 2000.0 << ")"
      //         << ", constraint enum = " << int(marker.get()->getConstraint()->typeId()) << "\n";
      //  }
      //}
    // }
  }
  return sol;
}

void FlexPA::prepPoint_pin_checkPoint(frAccessPoint* ap, 
                                      const gtl::polygon_90_set_data<frCoord> &polyset,
                                      const vector<gtl::polygon_90_data<frCoord> > &polys,
                                      frPin* pin, frInstTerm* instTerm) {
  bool enableOutput = false;
  // bool enableOutput = true;
  if (enableOutput) {
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    frPoint pt;
    ap->getPoint(pt);
    cout <<"checking ap@( " <<pt.x() / dbu <<" " <<pt.y() / dbu <<" ) "
         <<getDesign()->getTech()->getLayer(ap->getLayerNum())->getName() <<" , cost=" 
         <<ap->getCost() <<endl;
    //if (pt.x() != 28.8 * 2000 && pt.y() != 125.26 * 2000) {
    //  return;
    //}
  }
  /*
  frPoint bp, ep;
  ap->getPoint(bp);
  bool boundaryPlanrFound = false;
  if (!prepPoint_pin_checkPoint_planar_ep(ep, polys, bp, ap->getLayerNum(), frDirEnum::W, 1)) {
    ap->setAccess(frDirEnum::W, false);
  } else {
    boundaryPlanrFound = true;
  }
  if (!prepPoint_pin_checkPoint_planar_ep(ep, polys, bp, ap->getLayerNum(), frDirEnum::E, 1)) {
    ap->setAccess(frDirEnum::E, false);
  } else {
    boundaryPlanrFound = true;
  }
  if (!prepPoint_pin_checkPoint_planar_ep(ep, polys, bp, ap->getLayerNum(), frDirEnum::S, 1)) {
    ap->setAccess(frDirEnum::S, false);    
  } else {
    boundaryPlanrFound = true;
  }
  if (!prepPoint_pin_checkPoint_planar_ep(ep, polys, bp, ap->getLayerNum(), frDirEnum::N, 1)) {
    ap->setAccess(frDirEnum::N, false);
  } else {
    boundaryPlanrFound = true;
  }

  if (!boundaryPlanrFound) {
    ap->setAccess(frDirEnum::W, true);
    ap->setAccess(frDirEnum::E, true);
    ap->setAccess(frDirEnum::S, true);
    ap->setAccess(frDirEnum::N, true);
  }
  */

  prepPoint_pin_checkPoint_planar(ap, polys, frDirEnum::W, pin, instTerm);
  prepPoint_pin_checkPoint_planar(ap, polys, frDirEnum::E, pin, instTerm);
  prepPoint_pin_checkPoint_planar(ap, polys, frDirEnum::S, pin, instTerm);
  prepPoint_pin_checkPoint_planar(ap, polys, frDirEnum::N, pin, instTerm);
  prepPoint_pin_checkPoint_via(ap, polyset, frDirEnum::U, pin, instTerm);
}

void FlexPA::prepPoint_pin_checkPoints(vector<unique_ptr<frAccessPoint> > &aps, 
                                       const vector<gtl::polygon_90_set_data<frCoord> > &layerPolysets,
                                       frPin* pin, frInstTerm* instTerm) {
  vector<vector<gtl::polygon_90_data<frCoord> > > layerPolys(layerPolysets.size());
  for (int i = 0; i < (int)layerPolysets.size(); i++) {
    layerPolysets[i].get_polygons(layerPolys[i]);
  }
  for (auto &ap: aps) {
    auto layerNum = ap->getLayerNum();
    frPoint pt;
    ap->getPoint(pt);
    //if (pt.x() == 189.3 * 2000 && pt.y() == 175.7 * 2000) {
    //  ;
    //} else {
    //  continue;
    //}
    prepPoint_pin_checkPoint(ap.get(), layerPolysets[layerNum], layerPolys[layerNum], pin, instTerm);
  }
}

void FlexPA::prepPoint_pin_updateStat(const vector<unique_ptr<frAccessPoint> > &tmpAps, frPin* pin, frInstTerm* instTerm) {
  bool isStdCellPin   = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIEHIGH ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIELOW ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_ANTENNACELL));
  bool isMacroCellPin = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::RING));
  //bool hasAccess = false;
  for (auto &ap: tmpAps) {
    //if (ap->hasAccess()) {
    //  hasAccess = true;
    //}
    if (ap->hasAccess(frDirEnum::W) || ap->hasAccess(frDirEnum::E) || 
        ap->hasAccess(frDirEnum::S) || ap->hasAccess(frDirEnum::N)) { 
      if (isStdCellPin) {
        #pragma omp atomic
        stdCellPinValidPlanarApCnt++;
      }
      if (isMacroCellPin) {
        #pragma omp atomic
        macroCellPinValidPlanarApCnt++;
      }
    }
    if (ap->hasAccess(frDirEnum::U)) { 
      if (isStdCellPin) {
        #pragma omp atomic
        stdCellPinValidViaApCnt++;
      }
      if (isMacroCellPin) {
        #pragma omp atomic
        macroCellPinValidViaApCnt++;
      }
    }
  }
}

bool FlexPA::prepPoint_pin_helper(vector<unique_ptr<frAccessPoint> > &aps,
                                  set<pair<frPoint, frLayerNum> > &apset,
                                  vector<gtl::polygon_90_set_data<frCoord> > &pinShapes,
                                  frPin* pin, frInstTerm* instTerm, int lowerType, int upperType) {
  bool isStdCellPin   = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIEHIGH ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIELOW || 
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_ANTENNACELL));
  bool isMacroCellPin = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::RING));
  bool isIOPin = (instTerm == nullptr);
  vector<unique_ptr<frAccessPoint> > tmpAps;
  // int prevSize = apset.size();
  // set<pair<frPoint, frLayerNum> > prevApset = apset, diffApset;
  prepPoint_pin_genPoints(tmpAps, apset, pin, instTerm, pinShapes, lowerType, upperType);
  // std::set_difference(apset.begin(), apset.end(), prevApset.begin(), prevApset.end(), std::inserter(diffApset, diffApset.end()));
  // int currSize = apset.size();
  // if (instTerm->getInst()->getName() == string("dcnt_reg_3_") && instTerm->getTerm()->getName() == string("Q")) {
  //   cout << "dcnt_reg_3_/" << instTerm->getTerm()->getName() <<" has " << currSize - prevSize 
  //        << " new access points with lowerType = " << lowerType 
  //        << " and upperType = " << upperType << "\n";
  //   // for (auto ap: diffApset) {
  //   //   cout << "  (" << ap.first.x() / 2000.0 << ", " << ap.first.y() / 2000.0 << ", " << ap.second << ")\n";
  //   // }
  // }
  // if (instTerm->getInst()->getRefBlock()->getName() == string("gf14_1rw_d128_w116_m2_bit_mod") && pin->getTerm()->getName() == string("EMAS")) {
  //   cout << "apset.size() " << apset.size() << endl;
  //   for (auto ap: apset) {
  //     cout << " ap (" << ap.first.x() / 2000.0 << ", " << ap.first.y() / 2000.0 << ")\n";
  //   }
  // }
  prepPoint_pin_checkPoints(tmpAps, pinShapes, pin, instTerm);
  if (isStdCellPin) {
    #pragma omp atomic
    stdCellPinGenApCnt += tmpAps.size();
  }
  if (isMacroCellPin) {
    #pragma omp atomic
    macroCellPinGenApCnt += tmpAps.size();
  }
  for (auto &ap: tmpAps) {
    // for stdcell, add (i) planar access if layerNum != VIA_ACCESS_LAYERNUM, and (ii) access if exist access
    // for macro, allow pure planar ap
    if (isStdCellPin) {
      auto layerNum = ap->getLayerNum();
      if ((layerNum == VIA_ACCESS_LAYERNUM && ap->hasAccess(frDirEnum::U)) ||
          (layerNum != VIA_ACCESS_LAYERNUM && ap->hasAccess())) {
      // if (ap->hasAccess(frDirEnum::U)) {
        aps.push_back(std::move(ap));
      }
    } else if ((isMacroCellPin || isIOPin) && ap->hasAccess()) {
      aps.push_back(std::move(ap));
    }
  }
  if (isStdCellPin && (int)aps.size() >= MINNUMACCESSPOINT_STDCELLPIN) {
    prepPoint_pin_updateStat(aps, pin, instTerm);
    // write to pa
    auto it = unique2paidx.find(instTerm->getInst());
    if (it == unique2paidx.end()) {
      cout <<"Error: prepPoint_pin_helper unique2paidx not found" <<endl;
      exit(1);
    } else {
      for (auto &ap: aps) {
        pin->getPinAccess(it->second)->addAccessPoint(ap);
      }
    }
    return true;
  }
  if (isMacroCellPin && (int)aps.size() >= MINNUMACCESSPOINT_MACROCELLPIN) {
    prepPoint_pin_updateStat(aps, pin, instTerm);
    // write to pa
    auto it = unique2paidx.find(instTerm->getInst());
    if (it == unique2paidx.end()) {
      cout <<"Error: prepPoint_pin_helper unique2paidx not found" <<endl;
      exit(1);
    } else {
      for (auto &ap: aps) {
        pin->getPinAccess(it->second)->addAccessPoint(ap);
      }
    }
    return true;
  }
  if (isIOPin && (int)aps.size() > 0) {
    // IO term pin always only have one access
    for (auto &ap: aps) {
      pin->getPinAccess(0)->addAccessPoint(ap);
    }
    return true;
  }
  return false;
}

// first create all access points with costs
void FlexPA::prepPoint_pin(frPin* pin, frInstTerm* instTerm) {
  // aps are after xform
  // before checkPoints, ap->hasAccess(dir) indicates whether to check drc 
  vector<unique_ptr<frAccessPoint> > aps;
  set<pair<frPoint, frLayerNum> > apset;
  bool isStdCellPin   = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIEHIGH ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_TIELOW ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::CORE_ANTENNACELL));
  bool isMacroCellPin = (instTerm && (instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::PAD_POWER ||
                                      instTerm->getInst()->getRefBlock()->getMacroClass() == MacroClassEnum::RING));
  
  vector<gtl::polygon_90_set_data<frCoord> > pinShapes;
  if (isMacroCellPin) {
    prepPoint_pin_mergePinShapes(pinShapes, pin, instTerm);
    // prepPoint_pin_mergePinShapes(pinShapes, pin, instTerm, true);
  } else {
    prepPoint_pin_mergePinShapes(pinShapes, pin, instTerm);
  }

  // 0 iter, gen on-grid, on-grid points
  //cout <<"iter0" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 0, 0)) {
    return;
  }
  // 1st iter, gen 1/2-grid, on-grid points
  //cout <<"iter1" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 1, 0)) {
    return;
  }
  // 2nd iter, gen center, on-grid points
  //cout <<"iter2" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 2, 0)) {
    return;
  }
  // 3rd iter, gen enc-opt, on-grid points
  //cout <<"iter3" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 3, 0)) {
    return;
  }
  // 4th iter, gen on-grid, 1/2-grid points
  //cout <<"iter4" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 0, 1)) {
    return;
  }
  // 5th iter, gen 1/2-grid, 1/2-grid points
  //cout <<"iter5" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 1, 1)) {
    return;
  }
  // 6th iter, gen center, 1/2-grid points
  //cout <<"iter6" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 2, 1)) {
    return;
  }
  // 7th iter, gen enc-opt, 1/2-grid points
  //cout <<"iter7" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 3, 1)) {
    return;
  }
  // 8th iter, gen on-grid, center points
  //cout <<"iter8" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 0, 2)) {
    return;
  }
  // 9th iter, gen 1/2-grid, center points
  //cout <<"iter9" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 1, 2)) {
    return;
  }
  // 10th iter, gen center, center points
  //cout <<"iter10" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 2, 2)) {
    return;
  }
  // 11th iter, gen enc-opt, center points
  //cout <<"iter11" <<endl;
  if (prepPoint_pin_helper(aps, apset, pinShapes, pin, instTerm, 3, 2)) {
    return;
  }

  // instTerm aps are written back here if not early stopped
  // IO term aps are are written back in prepPoint_pin_helper and always early stopped
  prepPoint_pin_updateStat(aps, pin, instTerm);
  if (aps.empty()) {
    if (instTerm) {
      cout <<"Error: no ap for " <<instTerm->getInst()->getName() <<"/" <<instTerm->getTerm()->getName() <<endl;
    } else {
      cout <<"Error: no ap for PIN/" <<pin->getTerm()->getName() <<endl;
    }
    if (isStdCellPin) {
      stdCellPinNoApCnt++;
    }
    if (isMacroCellPin) {
      macroCellPinNoApCnt++;
    }
  } else {
    // write to pa
    auto it = unique2paidx.find(instTerm->getInst());
    if (it == unique2paidx.end()) {
      cout <<"Error: prepPoint_pin unique2paidx not found" <<endl;
      exit(1);
    } else {
      for (auto &ap: aps) {
        // if (instTerm->getInst()->getRefBlock()->getName() == string("INVP_X1F_A9PP84TR_C14") && instTerm->getTerm()->getName() == string("Y")) {
        //  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
        //  frPoint pt;
        //  for (auto &ap: aps) {
        //    ap->getPoint(pt);
        //    cout <<"checked ap@(" <<pt.x() / dbu <<", " <<pt.y() / dbu <<") "
        //         <<getDesign()->getTech()->getLayer(ap->getLayerNum())->getName() <<" , cost=" 
        //         <<ap->getCost() <<endl;
        //  }
        // }
        pin->getPinAccess(it->second)->addAccessPoint(ap);
      }
    }
  }
}

void FlexPA::prepPoint() {
  // bool enableOutput = true;
  bool enableOutput = false;
  if (enableOutput) {
    cout <<endl;
  }
  int cnt = 0;

  omp_set_num_threads(MAX_THREADS);
  #pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < (int)uniqueInstances.size(); i++) {
    auto &inst = uniqueInstances[i];
    // only do for core and block cells
    if (inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_TIEHIGH && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_TIELOW && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_ANTENNACELL && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::BLOCK &&
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::PAD &&
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::PAD_POWER &&
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::RING) {
      continue;
    }
    // temporary for testing ap
    // if (inst->getRefBlock()->getMacroClass() == MacroClassEnum::BLOCK) {
    //   continue;
    // }
    for (auto &instTerm: inst->getInstTerms()) {
      // only do for normal and clock terms
      if (isSkipInstTerm(instTerm.get())) {
        continue;
      }
      for (auto &pin: instTerm->getTerm()->getPins()) {
        // tmporary
        //if (inst->getName() == string(R"(u0\/u0\/U81)") && instTerm->getTerm()->getName() == string("Y")) {
        //  ;
        //} else {
        //  continue;
        //}
        if (enableOutput) {
          cout <<"pin prep for " <<inst->getName() <<" / " <<instTerm->getTerm()->getName() 
               <<" " <<inst->getRefBlock()->getName() <<" " <<inst->getOrient() <<endl;
        }
        prepPoint_pin(pin.get(), instTerm.get());
      }
      #pragma omp critical 
      {
        cnt++;
        if (VERBOSE > 0) {
          if (cnt < 1000) {
            if (cnt % 100 == 0) {
              cout <<"  complete " <<cnt <<" pins" <<endl;
            }
          } else {
            if (cnt % 1000 == 0) {
              cout <<"  complete " <<cnt <<" pins" <<endl;
            }
          }
        }
      }
    }
  }

  //cout << "PA for IO terms\n" << flush;

  // PA for IO terms
  omp_set_num_threads(MAX_THREADS);
  #pragma omp parallel for schedule(dynamic)
  for (unsigned i = 0; i < getDesign()->getTopBlock()->getTerms().size(); i++) {
    //for (auto &term: getDesign()->getTopBlock()->getTerms()) { // only gcc 9  support this
    auto &term = getDesign()->getTopBlock()->getTerms()[i];
    if (term->getType() != frTermEnum::frcNormalTerm && term->getType() != frTermEnum::frcClockTerm) {
      continue;
    }
    auto net = term->getNet();
    if (net == nullptr) {
      continue;
    }
    // cout << term->getName() << endl;
    for (auto &pin: term->getPins()) {
      if (enableOutput) {
        cout << "pin prep for PIN/" << term->getName() << " " << endl;
      }
      prepPoint_pin(pin.get(), nullptr);
    }
  }


  if (VERBOSE > 0) {
    cout <<"  complete " <<cnt <<" pins" <<endl;
  }
}

void FlexPA::prepPattern() {
  //bool enableOutput = true;
  bool enableOutput = false;
  if (enableOutput) {
    cout <<endl;
  }

  // revert access points to origin
  uniqueInstPatterns.resize(uniqueInstances.size());

  // int cnt = 0;
  // currUniqueInstIdx = 0;
  int cnt = 0;

  omp_set_num_threads(MAX_THREADS);
  #pragma omp parallel for schedule(dynamic)
  for (int currUniqueInstIdx = 0; currUniqueInstIdx < (int)uniqueInstances.size(); currUniqueInstIdx++) {
    auto &inst = uniqueInstances[currUniqueInstIdx];
    // only do for core and block cells
    if (inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_TIEHIGH && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_TIELOW &&
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_ANTENNACELL) {
      // currUniqueInstIdx++;
      continue;
    }

    prepPattern_inst(inst, currUniqueInstIdx);
    // currUniqueInstIdx++;
    #pragma omp critical 
    {
      cnt++;
      if (VERBOSE > 0) {
        if (cnt < 1000) {
          if (cnt % 100 == 0) {
            cout <<"  complete " <<cnt <<" unique inst patterns" <<endl;
          }
        } else {
          if (cnt % 1000 == 0) {
            cout <<"  complete " <<cnt <<" unique inst patterns" <<endl;
          }
        }
      }
    }
  }
  if (VERBOSE > 0) {
    cout <<"  complete " <<cnt <<" unique inst patterns" <<endl;
  }

  

  // prep pattern for each row
  std::vector<frInst*> insts;
  std::vector<std::vector<frInst*> > instRows;
  std::vector<frInst*> rowInsts;

  auto instLocComp = [](frInst* const &a, frInst* const &b) {
    frPoint originA, originB;
    a->getOrigin(originA);
    b->getOrigin(originB);
    if (originA.y() == originB.y()) {
      return (originA.x() < originB.x());
    } else {
      return (originA.y() < originB.y());
    }
  };

  getInsts(insts);
  std::sort(insts.begin(), insts.end(), instLocComp);

  // gen rows of insts
  int prevYCoord = INT_MIN;
  int prevXEndCoord = INT_MIN;
  //if (enableOutput) {
  //  cout << "sorted insts...\n" << flush;
  //}
  frBox instBoundaryBox;
  for (auto inst: insts) {
    frPoint origin;
    inst->getOrigin(origin);
    // cout << inst->getName() << " (" << origin.x() << ", " << origin.y() << ") -- ";
    if (origin.y() != prevYCoord || origin.x() > prevXEndCoord) {
      // cout << "\n";
      if (!rowInsts.empty()) {
        instRows.push_back(rowInsts);
        rowInsts.clear();
      }
    }
    rowInsts.push_back(inst);
    prevYCoord = origin.y();
    inst->getBoundaryBBox(instBoundaryBox);
    prevXEndCoord = instBoundaryBox.right();
  }
  if (!rowInsts.empty()) {
    instRows.push_back(rowInsts);
  }

  if (enableOutput) {
    cout << "gen inst row access patterns...\n" << flush;
  }
  // choose access pattern of a row of insts
  int rowIdx = 0;
  cnt = 0;
  // for (auto &instRow: instRows) {
  omp_set_num_threads(MAX_THREADS);
  #pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < (int)instRows.size(); i++) {
    auto &instRow = instRows[i];
    genInstPattern(instRow);
    #pragma omp critical
    {
      if (enableOutput) {
        cout << "Row " << rowIdx << "\n" << flush;
      }
      rowIdx++;
      cnt++;
      if (VERBOSE > 0) {
        if (cnt < 10000) {
          if (cnt % 1000 == 0) {
            cout <<"  complete " <<cnt <<" groups" <<endl;
          }
        } else {
          if (cnt % 10000 == 0) {
            cout <<"  complete " <<cnt <<" groups" <<endl;
          }
        }
      }
    }
  }
  if (VERBOSE > 0) {
    cout <<"  complete " <<cnt <<" groups" <<endl;
  }

  if (enableOutput) {
    cout << "GC called " << gcCallCnt << " times\n";
  }
}

void FlexPA::revertAccessPoints() {
  for (auto &inst: uniqueInstances) {
    frTransform xform, revertXform;
    inst->getTransform(xform);
    revertXform.set(-xform.xOffset(), -xform.yOffset());
    revertXform.set(frcR0);

    auto paIdx = unique2paidx[inst];
    for (auto &instTerm: inst->getInstTerms()) {
      // if (isSkipInstTerm(instTerm.get())) {
      //   continue;
      // }

      for (auto &pin: instTerm->getTerm()->getPins()) {
        auto pinAccess = pin->getPinAccess(paIdx);
        for (auto &accessPoint: pinAccess->getAccessPoints()) {
          frPoint uniqueAPPoint(accessPoint->getPoint());
          uniqueAPPoint.transform(revertXform);
          accessPoint->setPoint(uniqueAPPoint);
        }
      }
    }
  }
}

// calculate which pattern to be used for each inst
// the insts must be in the same row and sorted from left to right
void FlexPA::genInstPattern(std::vector<frInst*> &insts) {
  // bool enableOutput = false;
  if (insts.empty()) {
    return;
  }

  // TODO: change to a global constant
  maxAccessPatternSize = 5;
  
  int numNode = (insts.size() + 2) * maxAccessPatternSize;
  // int numEdge = numNode *maxAccessPatternSize;

  std::vector<FlexDPNode> nodes(numNode);

  genInstPattern_init(nodes, insts);
  genInstPattern_perform(nodes, insts);
  genInstPattern_commit(nodes, insts);

}

// init dp node array for valide access patterns
void FlexPA::genInstPattern_init(std::vector<FlexDPNode> &nodes,
                                 const std::vector<frInst*> &insts) {
  // bool enableOutput = true;
  bool enableOutput = false;
  if (enableOutput) {
    cout << "  genInstPattern_init\n" << flush;
  }

  // init virutal nodes
  int startNodeIdx = getFlatIdx(-1, 0, maxAccessPatternSize);
  int endNodeIdx = getFlatIdx(insts.size(), 0, maxAccessPatternSize);
  nodes[startNodeIdx].setNodeCost(0);
  nodes[startNodeIdx].setPathCost(0);
  nodes[endNodeIdx].setNodeCost(0);
  
  // init inst nodes
  for (int idx1 = 0; idx1 < (int)insts.size(); idx1++) {
    auto &inst = insts[idx1];
    auto &uniqueInst = inst2unique[inst];
    auto uniqueInstIdx = unique2Idx[uniqueInst];
    auto &instPatterns = uniqueInstPatterns[uniqueInstIdx];
    for (int idx2 = 0; idx2 < (int)instPatterns.size(); idx2++) {
      int nodeIdx = getFlatIdx(idx1, idx2, maxAccessPatternSize);
      auto accessPattern = instPatterns[idx2].get();
      nodes[nodeIdx].setNodeCost(accessPattern->getCost());
    }
  }
}

void FlexPA::genInstPattern_perform(std::vector<FlexDPNode> &nodes,
                                    const std::vector<frInst*> &insts) {
  //bool enableOutput = true;
  bool enableOutput = false;
  //if (enableOutput) {
  //  cout << "  genInstPattern_perform\n" << flush;
  //}
  for (int currIdx1 = 0; currIdx1 <= (int)insts.size(); currIdx1++) {
    bool isSet = false;
    if (enableOutput) {
      if (currIdx1 != int(insts.size())) {
        cout << "  genInstPattern_perform " <<insts[currIdx1]->getName() << " uniqueInst " 
             << inst2unique[insts[currIdx1]]->getName() <<endl <<flush;
      }
    }
    for (int currIdx2 = 0; currIdx2 < maxAccessPatternSize; currIdx2++) {
      auto currNodeIdx = getFlatIdx(currIdx1, currIdx2, maxAccessPatternSize);
      auto &currNode = nodes[currNodeIdx];
      if (currNode.getNodeCost() == std::numeric_limits<int>::max()) {
        continue;
      }
      int prevIdx1 = currIdx1 - 1;
      for (int prevIdx2 = 0; prevIdx2 < maxAccessPatternSize; prevIdx2++) {
        int prevNodeIdx = getFlatIdx(prevIdx1, prevIdx2, maxAccessPatternSize);
        auto &prevNode = nodes[prevNodeIdx];
        if (prevNode.getPathCost() == std::numeric_limits<int>::max()) {
          continue;
        }

        int edgeCost = getEdgeCost(prevNodeIdx, currNodeIdx, nodes, insts);
        if (currNode.getPathCost() == std::numeric_limits<int>::max() || 
            currNode.getPathCost() > prevNode.getPathCost() + edgeCost) {
          currNode.setPathCost(prevNode.getPathCost() + edgeCost);
          currNode.setPrevNodeIdx(prevNodeIdx);
          isSet = true;
        }
      }
    }
    if (!isSet) {
      cout << "@@@ dead end inst\n";
    }
  }
}

void FlexPA::genInstPattern_commit(std::vector<FlexDPNode> &nodes,
                                   const std::vector<frInst*> &insts) {
  //bool enableOutput = true;
  bool enableOutput = false;
  //bool isDebugMode = true;
  bool isDebugMode = false;
  if (enableOutput) {
    cout << "  genInstPattern_commit\n" << flush;
  }
  int currNodeIdx = getFlatIdx(insts.size(), 0, maxAccessPatternSize);
  auto currNode = &(nodes[currNodeIdx]);
  int instCnt = insts.size();
  std::vector<int> instAccessPatternIdx(insts.size(), -1);
  while (currNode->getPrevNodeIdx() != -1) {
    // non-virtual node
    if (instCnt != (int)insts.size()) {
      int currIdx1, currIdx2;
      getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPatternSize);
      instAccessPatternIdx[currIdx1] = currIdx2;
    
      auto &inst = insts[currIdx1];
      if (enableOutput) {
        cout << inst->getName() << " ";
      }
      int accessPointIdx = 0;
      auto &uniqueInst = inst2unique[inst];
      auto &uniqueInstIdx = unique2Idx[uniqueInst];
      auto accessPattern = uniqueInstPatterns[uniqueInstIdx][currIdx2].get();
      auto &accessPoints = accessPattern->getPattern();
      
      // update instTerm ap
      for (auto &instTerm: inst->getInstTerms()) {
        if (isSkipInstTerm(instTerm.get())) {
          continue;
        }

        int pinIdx = 0;
        //for (auto &pin: instTerm->getTerm()->getPins()) {
        // to avoid unused variable warning in GCC
        for (int i = 0; i < (int)(instTerm->getTerm()->getPins().size()); i++) {
          auto &accessPoint = accessPoints[accessPointIdx];
          instTerm->setAccessPoint(pinIdx, accessPoint);
          pinIdx++;
          accessPointIdx++;
        }


      }

    }
    currNodeIdx = currNode->getPrevNodeIdx();
    currNode = &(nodes[currNode->getPrevNodeIdx()]);
    instCnt--;
  }
  if (enableOutput) {
    cout <<endl;
  }

  if (instCnt != -1) {
    cout << "Error: valid access pattern combination not found\n";
  }

  // for (int i = 0; i < (int)instAccessPatternIdx.size(); i++) {
  //   cout << instAccessPatternIdx[i] << " ";
  // }
  // cout << "\n";

  if (isDebugMode) {
    genInstPattern_print(nodes, insts);
  }


}

void FlexPA::genInstPattern_print(std::vector<FlexDPNode> &nodes,
                                 const std::vector<frInst*> &insts) {
  int currNodeIdx = getFlatIdx(insts.size(), 0, maxAccessPatternSize);
  auto currNode = &(nodes[currNodeIdx]);
  int instCnt = insts.size();
  std::vector<int> instAccessPatternIdx(insts.size(), -1);

  map<frOrientEnum, string> orient2Name = {{frcR0, "R0"}, 
                                           {frcR90, "R90"}, 
                                           {frcR180, "R180"}, 
                                           {frcR270, "R270"}, 
                                           {frcMY, "MY"}, 
                                           {frcMXR90, "MX90"},
                                           {frcMX, "MX"},
                                           {frcMYR90, "MY90"}};

  while (currNode->getPrevNodeIdx() != -1) {
    // non-virtual node
    if (instCnt != (int)insts.size()) {
      int currIdx1, currIdx2;
      getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPatternSize);
      instAccessPatternIdx[currIdx1] = currIdx2;

      // print debug information
      auto &inst = insts[currIdx1];
      int accessPointIdx = 0;
      auto &uniqueInst = inst2unique[inst];
      auto &uniqueInstIdx = unique2Idx[uniqueInst];
      auto accessPattern = uniqueInstPatterns[uniqueInstIdx][currIdx2].get();
      auto &accessPoints = accessPattern->getPattern();

      for (auto &instTerm: inst->getInstTerms()) {
        if (isSkipInstTerm(instTerm.get())) {
          continue;
        }

        //for (auto &pin: instTerm->getTerm()->getPins()) {
        // to avoid unused variable warning in GCC
        for (int i = 0; i < (int)(instTerm->getTerm()->getPins().size()); i++) {
          auto &accessPoint = accessPoints[accessPointIdx];
          
          frPoint pt(accessPoint->getPoint());
          if (instTerm->hasNet()) {
            cout << " gcclean2via " << inst->getName() << " " 
                 << instTerm->getTerm()->getName() << " " << accessPoint->getViaDef()->getName() << " "
                 << pt.x() << " " << pt.y() << " "
                 << orient2Name[inst->getOrient()] << "\n";
            instTermValidViaApCnt++;
            // cout << instTermValidViaApCnt << endl;
          }
          accessPointIdx++;
        }
      }
    }
    currNodeIdx = currNode->getPrevNodeIdx();
    currNode = &(nodes[currNode->getPrevNodeIdx()]);
    instCnt--;
  }

  cout << flush;

  if (instCnt != -1) {
    cout << "Error: valid access pattern combination not found\n";
  }
}

int FlexPA::getEdgeCost(int prevNodeIdx, int currNodeIdx, 
                        std::vector<FlexDPNode> &nodes,
                        const std::vector<frInst*> &insts) {
  bool enableOutput = false;
  if (enableOutput) {
    cout << "  getEdgeCost\n";
  }
  int edgeCost = 0;
  int prevIdx1, prevIdx2, currIdx1, currIdx2;
  getNestedIdx(prevNodeIdx, prevIdx1, prevIdx2, maxAccessPatternSize);
  getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPatternSize);
  if (prevIdx1 == -1 || currIdx1 == (int)insts.size()) {
    return edgeCost;
  }

  bool hasVio = false;

  // check DRC
  std::vector<std::unique_ptr<frVia> > tempVias;
  std::vector<std::pair<frConnFig*, frBlockObject*> > objs;
  // push the vias from prev inst access pattern and curr inst access pattern
  auto prevInst = insts[prevIdx1];
  auto prevUniqueInstIdx = unique2Idx[inst2unique[prevInst]];
  auto currInst = insts[currIdx1];
  auto currUniqueInstIdx = unique2Idx[inst2unique[currInst]];
  auto prevPinAccessPattern = uniqueInstPatterns[prevUniqueInstIdx][prevIdx2].get();
  auto currPinAccessPattern = uniqueInstPatterns[currUniqueInstIdx][currIdx2].get();
  addAccessPatternObj(prevInst, prevPinAccessPattern, objs, tempVias, true);
  addAccessPatternObj(currInst, currPinAccessPattern, objs, tempVias, false);

  hasVio = !genPatterns_gc(nullptr, objs);
  if (!hasVio) {
    int prevNodeCost = nodes[prevNodeIdx].getNodeCost();
    int currNodeCost = nodes[currNodeIdx].getNodeCost();
    edgeCost = (prevNodeCost + currNodeCost) / 2;
  } else {
    edgeCost = 1000;
  }

  return edgeCost;



}

void FlexPA::addAccessPatternObj(frInst* inst,
                                 FlexPinAccessPattern *accessPattern,
                                 std::vector<std::pair<frConnFig*, frBlockObject*> > &objs,
                                 std::vector<std::unique_ptr<frVia> > &vias, bool isPrev) {
  frTransform xform;
  inst->getUpdatedXform(xform, true);
  int accessPointIdx = 0;
  auto &accessPoints = accessPattern->getPattern();

  for (auto &instTerm: inst->getInstTerms()) {
    if (isSkipInstTerm(instTerm.get())) {
      continue;
    }

    //for (auto &pin: instTerm->getTerm()->getPins()) {
    // to avoid unused variable warning in GCC
    for (int i = 0; i < (int)(instTerm->getTerm()->getPins().size()); i++) {
      auto &accessPoint = accessPoints[accessPointIdx];
      //cout <<"@@@test: isPrev = " <<isPrev <<", apoint " 
      //     <<accessPoint <<", leftAPoint " 
      //     <<accessPattern->getBoundaryAP(true) <<", rightAPoint " 
      //     <<accessPattern->getBoundaryAP(false) <<endl <<flush;
      if (isPrev && accessPoint != accessPattern->getBoundaryAP(false)) {
        accessPointIdx++;
        continue;
      }
      if ((!isPrev) && accessPoint != accessPattern->getBoundaryAP(true)) {
        accessPointIdx++;
        continue;
      }
      //cout <<"@@@find something" <<endl;
      if (accessPoint->hasAccess(frDirEnum::U)) {
        std::unique_ptr<frVia> via = std::make_unique<frVia>(accessPoint->getViaDef());
        frPoint pt(accessPoint->getPoint());
        pt.transform(xform);
        via->setOrigin(pt);
        auto rvia = via.get();
        if (instTerm->hasNet()) {
          objs.push_back(std::make_pair(rvia, instTerm->getNet()));
        } else {
          objs.push_back(std::make_pair(rvia, instTerm.get()));
        }
        vias.push_back(std::move(via));
      }
      accessPointIdx++;
    }
  }
}

void FlexPA::getInsts(std::vector<frInst*> &insts) {
  for (auto &inst: design->getTopBlock()->getInsts()) {
    if (inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_TIEHIGH && 
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_TIELOW &&
        inst->getRefBlock()->getMacroClass() != MacroClassEnum::CORE_ANTENNACELL) {
      continue;
    }
    bool isSkip = true;
    for (auto &instTerm: inst->getInstTerms()) {
      if (!isSkipInstTerm(instTerm.get())) {
        isSkip = false;
        break;
      }
    }
    if (!isSkip) {
      insts.push_back(inst.get());
    }
  }
}

bool FlexPA::isSkipInstTerm(frInstTerm *in) {
  if (in->getTerm()->getType() != frTermEnum::frcNormalTerm &&
      in->getTerm()->getType() != frTermEnum::frcClockTerm) {
    return true;
  } else {
    return false;
  }
}

// the input inst must be unique instance
void FlexPA::prepPattern_inst(frInst *inst, int currUniqueInstIdx) {
  //bool enableOutput = true;
  bool enableOutput = false;
  std::vector<std::pair<frCoord, std::pair<frPin*, frInstTerm*> > > pins;
  if (enableOutput) {
    cout << "start prepPattern " << inst->getName() << "\n" << flush;
  }
  // TODO: add assert in case input inst is not unique inst
  int paIdx = unique2paidx[inst];
  for (auto &instTerm: inst->getInstTerms()) {
    if (isSkipInstTerm(instTerm.get())) {
      continue;
    }

    for (auto &pin: instTerm->getTerm()->getPins()) {
      // container of access points
      auto pinAccess = pin->getPinAccess(paIdx);
      int sumXCoord = 0;
      int sumYCoord = 0;
      int cnt = 0;
      // get avg x coord for sort
      for (auto &accessPoint: pinAccess->getAccessPoints()) {
        sumXCoord += accessPoint->getPoint().x();
        sumYCoord += accessPoint->getPoint().y();
        cnt++;
      }
      if (cnt != 0) {
        // pins.push_back(std::make_pair(sumXCoord / cnt, std::make_pair(pin.get(), instTerm.get())));
        // pins.push_back(std::make_pair((sumXCoord + 0.5 * sumYCoord) / cnt, std::make_pair(pin.get(), instTerm.get())));
        pins.push_back(std::make_pair((sumXCoord + 0.0 * sumYCoord) / cnt, std::make_pair(pin.get(), instTerm.get())));
      } else {
        std::cout << "Error: pin does not have access point\n"; 
      }
    }
  }
  std::sort(pins.begin(), pins.end(), [](const std::pair<frCoord, std::pair<frPin*, frInstTerm*> > &lhs, 
                                         const std::pair<frCoord, std::pair<frPin*, frInstTerm*> > &rhs) 
                                        {return lhs.first < rhs.first;} );
  if (enableOutput) {
    cout <<inst->getName() <<":";
    for (auto &[x, m]: pins) {
      auto &[pin, instTerm] = m;
      cout << " " <<pin->getTerm()->getName() << "(" << pin->getPinAccess(paIdx)->getNumAccessPoints() << ")";
    }
    cout << endl;
  }

  std::vector<std::pair<frPin*, frInstTerm*> > pinInstTermPairs;
  for (auto &[x, m]: pins) {
    pinInstTermPairs.push_back(m);
  }

  genPatterns(pinInstTermPairs, currUniqueInstIdx);
}

void FlexPA::genPatterns(const std::vector<std::pair<frPin*, frInstTerm*> > &pins, int currUniqueInstIdx) {
  bool enableOutput = false;
  // bool enableOutput = true;

  if (pins.empty()) {
    return;
  }

  int maxAccessPointSize = 0;
  int paIdx = unique2paidx[pins[0].second->getInst()];
  for (auto &[pin, instTerm]: pins) {
    maxAccessPointSize = std::max(maxAccessPointSize, pin->getPinAccess(paIdx)->getNumAccessPoints());
  }
  int numNode = (pins.size() + 2) * maxAccessPointSize;
  int numEdge = numNode * maxAccessPointSize;

  std::vector<FlexDPNode> nodes(numNode);
  std::vector<int> vioEdge(numEdge, -1);
  // moved for mt
  std::set<std::vector<int> > instAccessPatterns;
  std::set<std::pair<int, int> > usedAccessPoints;
  std::set<std::pair<int, int> > violAccessPoints;

  if (enableOutput) {
    auto &[pin, instTerm] = pins[0];
    auto inst = instTerm->getInst();
    cout << "generate access pattern for uniqueInst" << currUniqueInstIdx << " (" << inst->getName() << ")\n";
  }
  
  genPatterns_init(nodes, pins, instAccessPatterns, usedAccessPoints, violAccessPoints, maxAccessPointSize);
  int numValidPattern = 0;
  for (int i = 0; i < ACCESS_PATTERN_END_ITERATION_NUM; i++) {
    genPatterns_reset(nodes, pins, maxAccessPointSize);
    genPatterns_perform(nodes, pins, vioEdge, usedAccessPoints, violAccessPoints, currUniqueInstIdx, maxAccessPointSize);
    bool isValid = false;
    if (genPatterns_commit(nodes, pins, isValid, instAccessPatterns, usedAccessPoints, violAccessPoints, currUniqueInstIdx, maxAccessPointSize)) {
      if (isValid) {
        numValidPattern++;
      } else {
        if (enableOutput) {
          genPatterns_print_debug(nodes, pins, maxAccessPointSize);
        }
      }
    } else {
      break;
    }
  }

  // try reverse order if no valid pattern
  if (numValidPattern == 0) {
    auto reversedPins = pins;
    reverse(reversedPins.begin(), reversedPins.end());

    std::vector<FlexDPNode> nodes(numNode);
    std::vector<int> vioEdge(numEdge, -1);

    genPatterns_init(nodes, reversedPins, instAccessPatterns, usedAccessPoints, violAccessPoints, maxAccessPointSize);
    for (int i = 0; i < ACCESS_PATTERN_END_ITERATION_NUM; i++) {
      genPatterns_reset(nodes, reversedPins, maxAccessPointSize);
      genPatterns_perform(nodes, reversedPins, vioEdge, usedAccessPoints, violAccessPoints, currUniqueInstIdx, maxAccessPointSize);
      bool isValid = false;
      if (genPatterns_commit(nodes, reversedPins, isValid, instAccessPatterns, usedAccessPoints, violAccessPoints, currUniqueInstIdx, maxAccessPointSize)) {
        if (isValid) {
          numValidPattern++;
        } else {
          if (enableOutput) {
            genPatterns_print_debug(nodes, reversedPins, maxAccessPointSize);
          }
        }
      } else {
        break;
      }
    }
  }

  if (numValidPattern == 0) {
    auto inst = pins[0].second->getInst();
    cout << "Error: no valid pattern for unique instance " 
         << inst->getName() <<", refBlock is " 
         << inst->getRefBlock()->getName() <<endl;
    //int paIdx = unique2paidx[pins[0].second->getInst()];
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    frTransform shiftXform;
    inst->getTransform(shiftXform);
    shiftXform.set(frOrient(frcR0));
    cout <<"  pin ordering (with ap): " <<endl;
    for (auto &[pin, instTerm]: pins) {
      cout <<"    " <<instTerm->getTerm()->getName();
      for (auto &ap: pin->getPinAccess(paIdx)->getAccessPoints()) {
        frPoint pt;
        ap->getPoint(pt);
        //auto bNum = ap->getLayerNum();
        //auto bLayer = getDesign()->getTech()->getLayer(bNum);
        pt.transform(shiftXform);
        cout <<" (" <<pt.x() / dbu <<", " <<pt.y() / dbu <<")";
      }
      cout <<endl;
    }
    //cout << "Error: no valid pattern for unique instance " ;
  }

}

// init dp node array for valid access points
void FlexPA::genPatterns_init(std::vector<FlexDPNode> &nodes,
                              const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                              std::set<std::vector<int> > &instAccessPatterns,
                              std::set<std::pair<int, int> > &usedAccessPoints,
                              std::set<std::pair<int, int> > &violAccessPoints,
                              int maxAccessPointSize) {
  bool enableOutput = false;
  if (enableOutput) {
    cout << "  init\n" << flush;
  }
  // clear temp storage and flag
  instAccessPatterns.clear();
  usedAccessPoints.clear();
  violAccessPoints.clear();

  // init virtual nodes
  int startNodeIdx = getFlatIdx(-1, 0, maxAccessPointSize);
  int endNodeIdx = getFlatIdx(pins.size(), 0, maxAccessPointSize);
  nodes[startNodeIdx].setNodeCost(0);
  nodes[startNodeIdx].setPathCost(0);
  nodes[endNodeIdx].setNodeCost(0);
  // init pin nodes
  int pinIdx = 0;
  int apIdx = 0;
  int paIdx = unique2paidx[pins[0].second->getInst()];

  for (auto &[pin, instTerm]: pins) {
    apIdx = 0;
    for (auto &ap: pin->getPinAccess(paIdx)->getAccessPoints()) {
      int nodeIdx = getFlatIdx(pinIdx, apIdx, maxAccessPointSize);
      nodes[nodeIdx].setNodeCost(ap->getCost());
      apIdx++;
    }
    pinIdx++;
  }
}

void FlexPA::genPatterns_reset(std::vector<FlexDPNode> &nodes,
                               const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                               int maxAccessPointSize) {
  for (int i = 0; i < (int)nodes.size(); i++) {
    auto node = &nodes[i];
    node->setPathCost(std::numeric_limits<int>::max());
    node->setPrevNodeIdx(-1);
  }

  int startNodeIdx = getFlatIdx(-1, 0, maxAccessPointSize);
  int endNodeIdx = getFlatIdx(pins.size(), 0, maxAccessPointSize);
  nodes[startNodeIdx].setNodeCost(0);
  nodes[startNodeIdx].setPathCost(0);
  nodes[endNodeIdx].setNodeCost(0);
}

// objs must hold at least 1 obj
bool FlexPA::genPatterns_gc(frBlockObject* targetObj, vector<pair<frConnFig*, frBlockObject*> > &objs, 
                            std::set<frBlockObject*> *owners) {
  gcCallCnt++;
  if (objs.empty()) {
    if (VERBOSE > 1) {
      cout <<"Warning: genPattern_gc objs empty" <<endl;
    }
    return false;
  }

  FlexGCWorker gcWorker(getDesign());
  gcWorker.setIgnoreMinArea();
  
  frCoord llx = std::numeric_limits<frCoord>::max();
  frCoord lly = std::numeric_limits<frCoord>::max();
  frCoord urx = std::numeric_limits<frCoord>::min();
  frCoord ury = std::numeric_limits<frCoord>::min();
  frBox bbox;
  for (auto &[connFig, owner]: objs) {
    connFig->getBBox(bbox);
    llx = std::min(llx, bbox.left());
    lly = std::min(llx, bbox.bottom());
    urx = std::max(llx, bbox.right());
    ury = std::max(llx, bbox.top());
  }
  frBox extBox(llx - 3000, lly - 3000, urx + 3000, ury + 3000);
  // frBox extBox(llx - 1000, lly - 1000, urx + 1000, ury + 1000);
  gcWorker.setExtBox(extBox);
  gcWorker.setDrcBox(extBox);

  gcWorker.setTargetObj(targetObj);
  gcWorker.setIgnoreDB();
  //cout <<flush;
  gcWorker.initPA0();
  for (auto &[connFig, owner]: objs) {
    gcWorker.addPAObj(connFig, owner);
  }
  gcWorker.initPA1();
  gcWorker.main();
  gcWorker.end();

  //int typeGC  = 0;
  bool sol = false;
  if (gcWorker.getMarkers().empty()) {
    //typeGC = 1;
    sol = true;
  } else {
    //typeGC = 2;
  }
  //if (enableOutput) {
    //prepPoint_pin_checkPoint_print_helper(ap, pin, instTerm, frDirEnum::U, typeGC, typeDRC, bp, ep, via->getViaDef());
  //}
  if (owners) {
    for (auto &marker: gcWorker.getMarkers()) {
      for (auto &src: marker->getSrcs()) {
        owners->insert(src);
      }
    }
  }
  return sol;
}

void FlexPA::genPatterns_perform(std::vector<FlexDPNode> &nodes,
                                 const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                                 std::vector<int> &vioEdges,
                                 std::set<std::pair<int, int> > &usedAccessPoints,
                                 std::set<std::pair<int, int> > &violAccessPoints,
                                 int currUniqueInstIdx,
                                 int maxAccessPointSize) {
  bool enableOutput = false;
  if (enableOutput) {
    cout << "  perform\n" << flush;
  }
  for (int currIdx1 = 0; currIdx1 <= (int)pins.size(); currIdx1++) {
    for (int currIdx2 = 0; currIdx2 < maxAccessPointSize; currIdx2++) {
      auto currNodeIdx = getFlatIdx(currIdx1, currIdx2, maxAccessPointSize);
      auto &currNode = nodes[currNodeIdx];
      if (currNode.getNodeCost() == std::numeric_limits<int>::max()) {
        continue;
      }
      int prevIdx1 = currIdx1 - 1;
      for (int prevIdx2 = 0; prevIdx2 < maxAccessPointSize; prevIdx2++) {
        int prevNodeIdx = getFlatIdx(prevIdx1, prevIdx2, maxAccessPointSize);
        auto &prevNode = nodes[prevNodeIdx];
        if (prevNode.getPathCost() == std::numeric_limits<int>::max()) {
          continue;
        }

        int edgeCost = getEdgeCost(prevNodeIdx, currNodeIdx, nodes, pins, vioEdges, usedAccessPoints, violAccessPoints, currUniqueInstIdx, maxAccessPointSize);
        if (currNode.getPathCost() == std::numeric_limits<int>::max() ||
            currNode.getPathCost() > prevNode.getPathCost() + edgeCost) {
          currNode.setPathCost(prevNode.getPathCost() + edgeCost);
          currNode.setPrevNodeIdx(prevNodeIdx);
        }
      }
    }
  }
}

int FlexPA::getEdgeCost(int prevNodeIdx, 
                        int currNodeIdx,
                        const std::vector<FlexDPNode> &nodes,
                        const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                        std::vector<int> &vioEdges,
                        std::set<std::pair<int, int> > &usedAccessPoints,
                        std::set<std::pair<int, int> > &violAccessPoints,
                        int currUniqueInstIdx,
                        int maxAccessPointSize) {
  int edgeCost = 0;
  int prevIdx1, prevIdx2, currIdx1, currIdx2;
  getNestedIdx(prevNodeIdx, prevIdx1, prevIdx2, maxAccessPointSize);
  getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPointSize);
  if (prevIdx1 == -1 || currIdx1 == (int)pins.size()) {
    return edgeCost;
  }

  bool hasVio = false;
  // check if the edge has been calculated
  int edgeIdx = getFlatEdgeIdx(prevIdx1, prevIdx2, currIdx2, maxAccessPointSize);
  if (vioEdges[edgeIdx] != -1) {
    hasVio = (vioEdges[edgeIdx] == 1);
  } else {
    auto &currUniqueInst = uniqueInstances[currUniqueInstIdx];
    frTransform xform;
    currUniqueInst->getUpdatedXform(xform, true);
    // check DRC
    vector<pair<frConnFig*, frBlockObject*> > objs;
    auto &[pin1, instTerm1] = pins[prevIdx1];
    auto targetObj = instTerm1->getInst();
    int paIdx = unique2paidx[targetObj];
    auto pa1 = pin1->getPinAccess(paIdx);
    // unique_ptr<frVia> via1 = make_unique<frVia>(pa1->getAccessPoint(prevIdx2)->getViaDef());
    unique_ptr<frVia> via1;
    if (pa1->getAccessPoint(prevIdx2)->hasAccess(frDirEnum::U)) {
      via1 = make_unique<frVia>(pa1->getAccessPoint(prevIdx2)->getViaDef());
      frPoint pt1(pa1->getAccessPoint(prevIdx2)->getPoint());
      pt1.transform(xform);
      via1->setOrigin(pt1);
      if (instTerm1->hasNet()) {
        objs.push_back(make_pair(via1.get(), instTerm1->getNet()));
      } else {
        objs.push_back(make_pair(via1.get(), instTerm1));
      }
    }

    auto &[pin2, instTerm2] = pins[currIdx1];
    auto pa2 = pin2->getPinAccess(paIdx);
    // unique_ptr<frVia> via2 = make_unique<frVia>(pa2->getAccessPoint(currIdx2)->getViaDef());
    unique_ptr<frVia> via2;
    if (pa2->getAccessPoint(currIdx2)->hasAccess(frDirEnum::U)) {
      via2 = make_unique<frVia>(pa2->getAccessPoint(currIdx2)->getViaDef());
      frPoint pt2(pa2->getAccessPoint(currIdx2)->getPoint());
      pt2.transform(xform);
      via2->setOrigin(pt2);
      if (instTerm2->hasNet()) {
        objs.push_back(make_pair(via2.get(), instTerm2->getNet()));
      } else {
        objs.push_back(make_pair(via2.get(), instTerm2));
      }
    }

    hasVio = !genPatterns_gc(targetObj, objs);
    vioEdges[edgeIdx] = hasVio;

    // look back for GN14
    if (!hasVio && prevNodeIdx != -1) {
      // check one more back
      auto prevPrevNodeIdx = nodes[prevNodeIdx].getPrevNodeIdx();
      if (prevPrevNodeIdx != -1) {
        int prevPrevIdx1, prevPrevIdx2;
        getNestedIdx(prevPrevNodeIdx, prevPrevIdx1, prevPrevIdx2, maxAccessPointSize);
        if (prevPrevIdx1 != -1) {
          auto &[pin3, instTerm3] = pins[prevPrevIdx1];
          auto pa3 = pin3->getPinAccess(paIdx);
          // unique_ptr<frVia> via3 = make_unique<frVia>(pa3->getAccessPoint(prevPrevIdx2)->getViaDef());
          unique_ptr<frVia> via3;
          if (pa3->getAccessPoint(prevPrevIdx2)->hasAccess(frDirEnum::U)) {
            via3 = make_unique<frVia>(pa3->getAccessPoint(prevPrevIdx2)->getViaDef());
            frPoint pt3(pa3->getAccessPoint(prevPrevIdx2)->getPoint());
            pt3.transform(xform);
            via3->setOrigin(pt3);
            if (instTerm3->hasNet()) {
              objs.push_back(make_pair(via3.get(), instTerm3->getNet()));
            } else {
              objs.push_back(make_pair(via3.get(), instTerm3));
            }
          }

          hasVio = !genPatterns_gc(targetObj, objs);
        }
      }
    }
  }

  if (!hasVio) {
  // if (true) {
    // edgeCost = std::abs(pt1.x() - pt2.x()) + std::abs(pt1.y() + pt2.y());
    // add penalty to cost if either boundary pin access points has been used before
    if ((prevIdx1 == 0 && usedAccessPoints.find(std::make_pair(prevIdx1, prevIdx2)) != usedAccessPoints.end()) || 
        (currIdx1 == (int)pins.size() - 1 && usedAccessPoints.find(std::make_pair(currIdx1, currIdx2)) != usedAccessPoints.end())) {
      edgeCost = 100;
    } else if (violAccessPoints.find(make_pair(prevIdx1, prevIdx2)) != violAccessPoints.end() || 
               violAccessPoints.find(make_pair(currIdx1, currIdx2)) != violAccessPoints.end()) {
      edgeCost = 1000;
    } else {
      int prevNodeCost = nodes[prevNodeIdx].getNodeCost();
      int currNodeCost = nodes[currNodeIdx].getNodeCost();
      edgeCost = (prevNodeCost + currNodeCost) / 2;  
    }
  } else {
    edgeCost = 1000/*violation cost*/;
  }

  return edgeCost;
}

bool FlexPA::genPatterns_commit(std::vector<FlexDPNode> &nodes,
                                const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                                bool &isValid,
                                std::set<std::vector<int> > &instAccessPatterns,
                                std::set<std::pair<int, int> > &usedAccessPoints,
                                std::set<std::pair<int, int> > &violAccessPoints,
                                int currUniqueInstIdx,
                                int maxAccessPointSize) {
  bool hasNewPattern = false;
  bool enableOutput = false;
  //bool enableOutput = true;
  if (enableOutput) {
    cout << "  commit\n" << flush;
  }
  int currNodeIdx = getFlatIdx(pins.size(), 0, maxAccessPointSize);
  auto currNode = &(nodes[currNodeIdx]);
  int pinCnt = pins.size();
  std::vector<int> accessPattern(pinCnt, -1);
  while (currNode->getPrevNodeIdx() != -1) {
    // non-virtual node
    if (pinCnt != (int)pins.size()) {
      int currIdx1, currIdx2;
      getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPointSize);
      accessPattern[currIdx1] = currIdx2;
      usedAccessPoints.insert(std::make_pair(currIdx1, currIdx2));
    }

    currNodeIdx = currNode->getPrevNodeIdx();
    currNode = &(nodes[currNode->getPrevNodeIdx()]);
    pinCnt--;
  }

  if (pinCnt != -1) {
    cout << "Error: valid access pattern not found\n";
  }

  // if (enableOutput) {
  //   for (auto &apIdx: accessPattern) {
  //     cout << " " << apIdx;
  //   }
  //   cout << "\n";
  // }
  // add to pattern set if unique
  if (instAccessPatterns.find(accessPattern) == instAccessPatterns.end()) {
    instAccessPatterns.insert(accessPattern);
    // create new access pattern and push to uniqueInstances
    auto pinAccessPattern = std::make_unique<FlexPinAccessPattern>();
    std::map<frPin*, frAccessPoint*> pin2AP;
    // check DRC for the whole pattern
    vector<pair<frConnFig*, frBlockObject*> > objs;
    vector<unique_ptr<frVia> > tempVias;
    frInst *targetObj = nullptr;
    for (int idx1 = 0; idx1 < (int)pins.size(); idx1++) {
      auto idx2 = accessPattern[idx1];
      auto &[pin, instTerm] = pins[idx1];
      auto inst = instTerm->getInst();
      targetObj = inst;
      int paIdx = unique2paidx[inst];
      auto pa = pin->getPinAccess(paIdx);
      auto accessPoint = pa->getAccessPoint(idx2);
      pin2AP[pin] = accessPoint;
      // pinAccessPattern->addAccessPoint(accessPoint);

      // add objs
      unique_ptr<frVia> via;
      if (accessPoint->hasAccess(frDirEnum::U)) {
        via = make_unique<frVia>(accessPoint->getViaDef());
        auto rvia = via.get();
        tempVias.push_back(std::move(via));

        frTransform xform;
        inst->getUpdatedXform(xform, true);
        frPoint pt(accessPoint->getPoint());
        pt.transform(xform);
        rvia->setOrigin(pt);
        if (instTerm->hasNet()) {
          objs.push_back(make_pair(rvia, instTerm->getNet()));
        } else {
          objs.push_back(make_pair(rvia, instTerm));
        }
      }
    }

    frAccessPoint* leftAP = nullptr;
    frAccessPoint* rightAP = nullptr;
    frCoord leftPt  = std::numeric_limits<frCoord>::max();
    frCoord rightPt = std::numeric_limits<frCoord>::min();
    frPoint tmpPt;

    auto &[pin, instTerm] = pins[0];
    auto inst = instTerm->getInst();
    for (auto &instTerm: inst->getInstTerms()) {
      if (isSkipInstTerm(instTerm.get())) {
        continue;
      }
      for (auto &pin: instTerm->getTerm()->getPins()) {
        if (pin2AP.find(pin.get()) == pin2AP.end()) {
          cout << "Error: pin does not have valid ap\n";
        } else {
          auto &ap = pin2AP[pin.get()];
          ap->getPoint(tmpPt);
          if (tmpPt.x() < leftPt) {
            leftAP = ap;
            leftPt = tmpPt.x();
          }
          if (tmpPt.x() > rightPt) {
            rightAP = ap;
            rightPt = tmpPt.x();
          }
          pinAccessPattern->addAccessPoint(ap);
        }
      }
    }
    //if (leftAP == nullptr) {
    //  cout <<"@@@Warning leftAP == nullptr " <<instTerm->getInst()->getName() <<endl;
    //}
    //if (rightAP == nullptr) {
    //  cout <<"@@@Warning rightAP == nullptr " <<instTerm->getInst()->getName() <<endl;
    //}
    pinAccessPattern->setBoundaryAP(true,  leftAP);
    pinAccessPattern->setBoundaryAP(false, rightAP);
    
    set<frBlockObject*> owners;
    if (targetObj != nullptr && genPatterns_gc(targetObj, objs, &owners)) {
      pinAccessPattern->updateCost();
      //cout <<"commit ap:";
      //for (auto &ap: pinAccessPattern->getPattern()) {
      //  cout <<" " <<ap;
      //}
      //cout <<endl <<"l/r = " 
      //     <<pinAccessPattern->getBoundaryAP(true) <<"/" 
      //     <<pinAccessPattern->getBoundaryAP(false) <<", "
      //     <<currUniqueInstIdx <<", " <<uniqueInstances[currUniqueInstIdx]->getName() <<endl;
      uniqueInstPatterns[currUniqueInstIdx].push_back(std::move(pinAccessPattern));
      // genPatterns_print(nodes, pins, maxAccessPointSize);
      isValid = true;
    } else {
      for (int idx1 = 0; idx1 < (int)pins.size(); idx1++) {
        auto idx2 = accessPattern[idx1];
        auto &[pin, instTerm] = pins[idx1];
        if (instTerm->hasNet()) {
          if (owners.find(instTerm->getNet()) != owners.end()) {
            violAccessPoints.insert(make_pair(idx1, idx2));// idx ;
          }
        } else {
          if (owners.find(instTerm) != owners.end()) {
            violAccessPoints.insert(make_pair(idx1, idx2));// idx ;
          }
        }
      }
    }

    hasNewPattern = true;
  } else {
    hasNewPattern = false;
  }

  return hasNewPattern;
}

void FlexPA::genPatterns_print_debug(std::vector<FlexDPNode> &nodes,
                               const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                               int maxAccessPointSize) {
  // bool enableOutput = false;
  int currNodeIdx = getFlatIdx(pins.size(), 0, maxAccessPointSize);
  auto currNode = &(nodes[currNodeIdx]);
  int pinCnt = pins.size();

  frTransform xform;
  auto &[pin, instTerm] = pins[0];
  if (instTerm) {
    frInst* inst = instTerm->getInst();
    inst->getTransform(xform);
  //   revertCellXForm.set(-xform.xOffset(), -xform.yOffset());
    xform.set(frcR0);
  }

  // auto inst = instTerm->getInst();
  cout << "failed pattern:";


  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  while (currNode->getPrevNodeIdx() != -1) {
    // non-virtual node
    if (pinCnt != (int)pins.size()) {
      auto &[pin, instTerm] = pins[pinCnt];
      auto inst = instTerm->getInst();
      cout <<" " <<instTerm->getTerm()->getName();
      int paIdx = unique2paidx[inst];
      auto pa = pin->getPinAccess(paIdx);
      int currIdx1, currIdx2;
      getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPointSize);
      //unique_ptr<frVia> via = make_unique<frVia>(pa->getAccessPoint(currIdx2)->getViaDef());
      frPoint pt(pa->getAccessPoint(currIdx2)->getPoint());
      pt.transform(xform);
      cout << " ("
           << pt.x() / dbu << ", " << pt.y() / dbu << ")";
    }

    currNodeIdx = currNode->getPrevNodeIdx();
    currNode = &(nodes[currNode->getPrevNodeIdx()]);
    pinCnt--;
  }
  cout <<endl;
  if (pinCnt != -1) {
    cout << "Error: valid access pattern not found\n";
  }
}

void FlexPA::genPatterns_print(std::vector<FlexDPNode> &nodes,
                               const std::vector<std::pair<frPin*, frInstTerm*> > &pins,
                               int maxAccessPointSize) {
  // bool enableOutput = false;
  int currNodeIdx = getFlatIdx(pins.size(), 0, maxAccessPointSize);
  auto currNode = &(nodes[currNodeIdx]);
  int pinCnt = pins.size();

  map<frOrientEnum, string> orient2Name = {{frcR0, "R0"}, 
                                           {frcR90, "R90"}, 
                                           {frcR180, "R180"}, 
                                           {frcR270, "R270"}, 
                                           {frcMY, "MY"}, 
                                           {frcMXR90, "MX90"},
                                           {frcMX, "MX"},
                                           {frcMYR90, "MY90"}};

  // frTransform xform, revertCellXForm;
  // auto &[pin, instTerm] = pins[0];
  // if (instTerm) {
  //   frInst* inst = instTerm->getInst();
  //   inst->getTransform(xform);
  //   revertCellXForm.set(-xform.xOffset(), -xform.yOffset());
  //   revertCellXForm.set(frcR0);
  // }

  // auto inst = instTerm->getInst();
  cout << "new pattern\n";


  while (currNode->getPrevNodeIdx() != -1) {
    // non-virtual node
    if (pinCnt != (int)pins.size()) {
      auto &[pin, instTerm] = pins[pinCnt];
      auto inst = instTerm->getInst();
      int paIdx = unique2paidx[inst];
      auto pa = pin->getPinAccess(paIdx);
      int currIdx1, currIdx2;
      getNestedIdx(currNodeIdx, currIdx1, currIdx2, maxAccessPointSize);
      unique_ptr<frVia> via = make_unique<frVia>(pa->getAccessPoint(currIdx2)->getViaDef());
      frPoint pt(pa->getAccessPoint(currIdx2)->getPoint());
      // pt.transform(revertCellXForm);
      cout << " gccleanvia " << instTerm->getInst()->getRefBlock()->getName() << " " 
           << instTerm->getTerm()->getName() << " " << via->getViaDef()->getName() << " "
           << pt.x() << " " << pt.y() << " "
           << orient2Name[instTerm->getInst()->getOrient()] << "\n";
    }

    currNodeIdx = currNode->getPrevNodeIdx();
    currNode = &(nodes[currNode->getPrevNodeIdx()]);
    pinCnt--;
  }
  if (pinCnt != -1) {
    cout << "Error: valid access pattern not found\n";
  }
}

// get flat index
// idx1 is outer index and idx2 is inner index dpNodes[idx1][idx2]
int FlexPA::getFlatIdx(int idx1, int idx2, int idx2Dim) {
  return ((idx1 + 1) * idx2Dim + idx2);
}

// get idx1 and idx2 from flat index
void FlexPA::getNestedIdx(int flatIdx, int &idx1, int &idx2, int idx2Dim) {
  idx1 = flatIdx / idx2Dim - 1;
  idx2 = flatIdx % idx2Dim;
}

// get flat edge index
int FlexPA::getFlatEdgeIdx(int prevIdx1, int prevIdx2, int currIdx2, int idx2Dim) {
  return (((prevIdx1 + 1) * idx2Dim + prevIdx2) * idx2Dim + currIdx2);
}
