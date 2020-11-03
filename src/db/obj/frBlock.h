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

#ifndef _FR_BLOCK_H_
#define _FR_BLOCK_H_

#include <algorithm>
#include "frBaseTypes.h"
#include "db/obj/frTrackPattern.h"
#include "db/obj/frBlockage.h"
#include "db/obj/frInst.h"
#include "db/obj/frTerm.h"
#include "db/obj/frNet.h"
#include "db/obj/frBoundary.h"
#include "db/obj/frCMap.h"
#include "db/obj/frMarker.h"

namespace fr {
  namespace io {
    class Parser;
  }
  class FlexGR;
  class frGuide;
  class frBlock: public frBlockObject {
  public:
    // constructors
    frBlock(): frBlockObject(), name(), dbUnit(0)/*, manufacturingGrid(0)*/, macroClass(MacroClassEnum::UNKNOWN) {};
    // getters
    frUInt4 getDBUPerUU() const {
      return dbUnit;
    }
    //frUInt4 getManufacturingGrid() const {
    //  return manufacturingGrid;
    //}
    void getBBox(frBox &boxIn) const {
      if (boundaries.size()) {
        boundaries.begin()->getBBox(boxIn);
      }
      frCoord llx = boxIn.left();
      frCoord lly = boxIn.bottom();
      frCoord urx = boxIn.right();
      frCoord ury = boxIn.top();
      frBox tmpBox;
      for (auto &boundary: boundaries) {
        boundary.getBBox(tmpBox);
        llx = llx < tmpBox.left()   ? llx : tmpBox.left();
        lly = lly < tmpBox.bottom() ? lly : tmpBox.bottom();
        urx = urx > tmpBox.right()  ? urx : tmpBox.right();
        ury = ury > tmpBox.top()    ? ury : tmpBox.top();
      }
      //std::cout <<"dieBox after bound " <<llx <<" " <<lly <<" " <<urx <<" " <<ury <<std::endl;
      for (auto &inst: getInsts()) {
        inst->getBBox(tmpBox);
        llx = llx < tmpBox.left()   ? llx : tmpBox.left();
        lly = lly < tmpBox.bottom() ? lly : tmpBox.bottom();
        urx = urx > tmpBox.right()  ? urx : tmpBox.right();
        ury = ury > tmpBox.top()    ? ury : tmpBox.top();
      }
      //std::cout <<"dieBox after inst " <<llx <<" " <<lly <<" " <<urx <<" " <<ury <<std::endl;
      for (auto &term: getTerms()) {
        for (auto &pin: term->getPins()) {
          for (auto &fig: pin->getFigs()) {
            fig->getBBox(tmpBox);
            llx = llx < tmpBox.left()   ? llx : tmpBox.left();
            lly = lly < tmpBox.bottom() ? lly : tmpBox.bottom();
            urx = urx > tmpBox.right()  ? urx : tmpBox.right();
            ury = ury > tmpBox.top()    ? ury : tmpBox.top();
          }
        }
      }
      //std::cout <<"dieBox after term " <<llx <<" " <<lly <<" " <<urx <<" " <<ury <<std::endl;
      boxIn.set(llx, lly, urx, ury);
    }
    void getBoundaryBBox(frBox &boxIn) const {
      if (boundaries.size()) {
        boundaries.begin()->getBBox(boxIn);
      }
      frCoord llx = boxIn.left();
      frCoord lly = boxIn.bottom();
      frCoord urx = boxIn.right();
      frCoord ury = boxIn.top();
      frBox tmpBox;
      for (auto &boundary: boundaries) {
        boundary.getBBox(tmpBox);
        llx = llx < tmpBox.left()   ? llx : tmpBox.left();
        lly = lly < tmpBox.bottom() ? lly : tmpBox.bottom();
        urx = urx > tmpBox.right()  ? urx : tmpBox.right();
        ury = ury > tmpBox.top()    ? ury : tmpBox.top();
      }
      boxIn.set(llx, lly, urx, ury);
    }
    const std::vector<frBoundary>& getBoundaries() const {
      return boundaries;
    }
    std::vector<frBoundary>& getBoundaries() {
      return boundaries;
    }
    std::vector< std::unique_ptr<frBlockage> >& getBlockages() {
      return blockages;
    }
    const std::vector< std::unique_ptr<frBlockage> >& getBlockages() const {
      return blockages;
    }
    const frCMap& getCMap() const {
      return cMap;
    }
    frCMap& getCMap() {
      return cMap;
    }
    const std::vector<frGCellPattern>& getGCellPatterns() const {
      return gCellPatterns;
    }
    std::vector<frGCellPattern>& getGCellPatterns() {
      return gCellPatterns;
    }
    std::vector<frGuide*> getGuides() const {
      std::vector<frGuide*> sol;
      for(auto &net: getNets()) {
        for (auto &guide: net->getGuides()) {
          sol.push_back(guide.get());
        }
      }
      return sol;
    }
    const frString& getName() const {
      return name;
    }
    const std::vector<std::unique_ptr<frInst> >& getInsts() const {
      return insts;
    }
    std::vector<std::unique_ptr<frInst> >& getInsts() {
      return insts;
    }
    std::vector<std::unique_ptr<frNet> >& getNets() {
      return nets;
    }
    const std::vector<std::unique_ptr<frNet> >& getNets() const {
      return nets;
    }
    std::vector<std::unique_ptr<frNet> >& getSNets() {
      return snets;
    }
    const std::vector<std::unique_ptr<frNet> >& getSNets() const {
      return snets;
    }
    std::vector<frTrackPattern*> getTrackPatterns() const {
      std::vector<frTrackPattern*> sol;
      for (auto &m: trackPatterns) {
        for (auto &n: m) {
          sol.push_back(n.get());
        }
      }
      return sol;
    }
    const std::vector<std::unique_ptr<frTrackPattern> >& getTrackPatterns(frLayerNum lNum) const {
      return trackPatterns.at(lNum);
    }
    std::vector<std::unique_ptr<frTrackPattern> >& getTrackPatterns(frLayerNum lNum) {
      return trackPatterns.at(lNum);
    }
    std::vector<std::unique_ptr<frTerm> >& getTerms() {
      return terms;
    }
    const std::vector<std::unique_ptr<frTerm> >& getTerms() const {
      return terms;
    }
    frTerm* getTerm(const std::string &in) const {
      auto it = name2term.find(in);
      if (it == name2term.end()) {
        return nullptr;
      } else {
        return it->second;
      }
    }
    // idx must be legal
    void getGCellBox(const frPoint &idx1, frBox &box) const {
      frPoint idx(idx1);
      frBox dieBox;
      getBoundaryBBox(dieBox);
      //double dbu = getDBUPerUU();
      //std::cout <<"diebox (" <<dieBox.left() / dbu <<", " <<dieBox.bottom() / dbu <<") ("
      //                       <<dieBox.right()/ dbu <<", " <<dieBox.top()    / dbu <<")" <<std::endl;
      auto &gp = getGCellPatterns();
      auto &xgp = gp[0];
      auto &ygp = gp[1];
      if (idx.x() <= 0) {
        idx.set(0, idx.y());
      }
      if (idx.y() <= 0) {
        idx.set(idx.x(), 0);
      }
      if (idx.x() >= (int)xgp.getCount() - 1) {
        idx.set((int)xgp.getCount() - 1, idx.y());
      }
      if (idx.y() >= (int)ygp.getCount() - 1) {
        idx.set(idx.x(), (int)ygp.getCount() - 1);
      }
      frCoord xl = (frCoord)xgp.getSpacing() * idx.x()       + xgp.getStartCoord();
      frCoord yl = (frCoord)ygp.getSpacing() * idx.y()       + ygp.getStartCoord();
      frCoord xh = (frCoord)xgp.getSpacing() * (idx.x() + 1) + xgp.getStartCoord();
      frCoord yh = (frCoord)ygp.getSpacing() * (idx.y() + 1) + ygp.getStartCoord();
      if (idx.x() <= 0) {
        xl = dieBox.left();
        //std::cout <<"xl reset to dieBox" <<std::endl;
      }
      if (idx.y() <= 0) {
        yl = dieBox.bottom();
        //std::cout <<"yl reset to dieBox" <<std::endl;
      }
      if (idx.x() >= (int)xgp.getCount() - 1) {
        xh = dieBox.right();
        //std::cout <<"xh reset to dieBox" <<std::endl;
      }
      if (idx.y() >= (int)ygp.getCount() - 1) {
        yh = dieBox.top();
        //std::cout <<"yh reset to dieBox" <<std::endl;
      }
      //std::cout <<"xl/yl/xh/yh@" <<xl <<"/" <<yl <<"/" <<xh <<"/" <<yh <<std::endl;
      box.set(xl, yl, xh, yh);
    }
    void getGCellCenter(const frPoint &idx, frPoint &pt) const {
      frBox dieBox;
      getBoundaryBBox(dieBox);
      auto &gp = getGCellPatterns();
      auto &xgp = gp[0];
      auto &ygp = gp[1];
      frCoord xl = (frCoord)xgp.getSpacing() * idx.x()       + xgp.getStartCoord();
      frCoord yl = (frCoord)ygp.getSpacing() * idx.y()       + ygp.getStartCoord();
      frCoord xh = (frCoord)xgp.getSpacing() * (idx.x() + 1) + xgp.getStartCoord();
      frCoord yh = (frCoord)ygp.getSpacing() * (idx.y() + 1) + ygp.getStartCoord();
      if (idx.x() == 0) {
        xl = dieBox.left();
      }
      if (idx.y() == 0) {
        yl = dieBox.bottom();
      }
      if (idx.x() == (int)xgp.getCount() - 1) {
        xh = dieBox.right();
      }
      if (idx.y() == (int)ygp.getCount() - 1) {
        yh = dieBox.top();
      }
      pt.set((xl + xh) / 2, (yl + yh) / 2);
    }
    void getGCellIdx(const frPoint &pt, frPoint &idx) const {
      auto &gp = getGCellPatterns();
      auto &xgp = gp[0];
      auto &ygp = gp[1];
      frCoord idxX = (pt.x() - xgp.getStartCoord()) / (frCoord)xgp.getSpacing();
      frCoord idxY = (pt.y() - ygp.getStartCoord()) / (frCoord)ygp.getSpacing();
      if (idxX < 0) {
        idxX = 0;
      }
      if (idxY < 0) {
        idxY = 0;
      }
      if (idxX >= (int)xgp.getCount()) {
        idxX = (int)xgp.getCount() - 1;
      }
      if (idxY >= (int)ygp.getCount()) {
        idxY = (int)ygp.getCount() - 1;
      }
      idx.set(idxX, idxY);
    }
    MacroClassEnum getMacroClass() {
      return macroClass;
    }
    const frList<std::unique_ptr<frMarker> >& getMarkers() const {
      return markers;
    }
    frList<std::unique_ptr<frMarker> >& getMarkers() {
      return markers;
    }
    int getNumMarkers() const {
      return markers.size();
    }
    frNet* getFakeVSSNet() {
      return fakeSNets[0].get();
    }
    frNet* getFakeVDDNet() {
      return fakeSNets[1].get();
    }

    // setters
    void setDBUPerUU(frUInt4 uIn) {
      dbUnit = uIn;
    }
    //void setManufacturingGrid(frUInt4 in) {
    //  manufacturingGrid = in;
    //}
    void setName(const frString &nameIn) {
      name = nameIn;
    }
    void addTerm(std::unique_ptr<frTerm> &in) {
      name2term[in->getName()] = in.get();
      terms.push_back(std::move(in));
    }
    void addInst(std::unique_ptr<frInst> &in) {
      name2inst[in->getName()] = in.get();
      //in->setId(insts.size());
      insts.push_back(std::move(in));
    }
    void addNet(std::unique_ptr<frNet> &in) {
      name2net[in->getName()] = in.get();
      //in->setId(snets.size() + nets.size());
      nets.push_back(std::move(in));
    }
    void addSNet(std::unique_ptr<frNet> &in) {
      name2snet[in->getName()] = in.get();
      //in->setId(snets.size() + nets.size());
      snets.push_back(std::move(in));
    }
    void setBoundaries(const std::vector<frBoundary> &in) {
      boundaries = in;
    }
    void setBlockages(std::vector<std::unique_ptr<frBlockage> > &in) {
      for (auto &blk : in) {
        blockages.push_back(std::move(blk));
      }
    }
    void addBlockage(std::unique_ptr<frBlockage> &in) {
      blockages.push_back(std::move(in));
    }
    void setCMap(const frCMap &cIn) {
      cMap = cIn;
    }
    void setGCellPatterns(const std::vector<frGCellPattern> &gpIn) {
      gCellPatterns = gpIn;
    }
    void setMacroClass(const MacroClassEnum &in) {
      macroClass = in;
    }
    void addMarker(std::unique_ptr<frMarker> &in) {
      auto rptr = in.get();
      markers.push_back(std::move(in));
      rptr->setIter(--(markers.end()));
    }
    void removeMarker(frMarker* in) {
      markers.erase(in->getIter());
    }
    void addFakeSNet(std::unique_ptr<frNet> &in) {
      fakeSNets.push_back(std::move(in));
    }
    // others
    frBlockObjectEnum typeId() const override {
      return frcBlock;
    }
    friend class io::Parser;
    friend class FlexGR;

  protected:
    frString                                                      name;
    frUInt4                                                       dbUnit;
    //frUInt4                                                       manufacturingGrid;

    MacroClassEnum                                                macroClass;

    std::map<std::string, frInst*>                                name2inst;
    std::vector<std::unique_ptr<frInst> >                         insts;

    std::map<std::string, frTerm*>                                name2term;
    std::vector<std::unique_ptr<frTerm> >                         terms;

    std::map<std::string, frNet*>                                 name2net;
    std::vector<std::unique_ptr<frNet> >                          nets;
    
    std::map<std::string, frNet*>                                 name2snet;
    std::vector<std::unique_ptr<frNet> >                          snets;

    std::vector<std::unique_ptr<frBlockage> >                     blockages;
    
    std::vector<frBoundary>                                       boundaries;
    std::vector<std::vector<std::unique_ptr<frTrackPattern> > >   trackPatterns;
    frCMap                                                        cMap;
    std::vector<frGCellPattern>                                   gCellPatterns;

    frList<std::unique_ptr<frMarker> >                            markers;

    std::vector<std::unique_ptr<frNet> >                          fakeSNets; // 0 is floating VSS, 1 is floating VDD
  };
}

#endif
