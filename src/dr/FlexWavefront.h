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

#ifndef _FLEX_WF_H
#define _FLEX_WF_H

#include <queue>
#include <bitset>
#include <vector>
#include "frBaseTypes.h"
#include "dr/FlexMazeTypes.h"
#include <memory>
#include "global.h"
//#include <boost/pool/pool_alloc.hpp>

namespace fr {
  class FlexWavefrontGrid {
  public:
    FlexWavefrontGrid(): xIdx(-1), yIdx(-1), zIdx(-1), pathCost(0), cost(0), layerPathArea(0), 
                         vLengthX(std::numeric_limits<frCoord>::max()), 
                         vLengthY(std::numeric_limits<frCoord>::max()), 
                         dist(0), prevViaUp(false), 
                         tLength(std::numeric_limits<frCoord>::max()), /*preTurnDir(frDirEnum::UNKNOWN),*/ backTraceBuffer() {}
    //FlexWavefrontGrid(int xIn, int yIn, int zIn, frCost pathCostIn, frCost costIn, std::bitset<WAVEFRONTBITSIZE> backTraceBufferIn): 
    //                  xIdx(xIn), yIdx(yIn), zIdx(zIn), pathCost(pathCostIn), cost(costIn), layerPathLength(0), backTraceBuffer(backTraceBufferIn) {}
    FlexWavefrontGrid(int xIn, int yIn, int zIn, frCoord layerPathAreaIn, 
                      frCoord vLengthXIn, frCoord vLengthYIn,
                      bool prevViaUpIn, frCoord tLengthIn,
                      frCoord distIn, frCost pathCostIn, frCost costIn/*, frDirEnum preTurnDirIn*/): 
                      xIdx(xIn), yIdx(yIn), zIdx(zIn), pathCost(pathCostIn), cost(costIn), 
                      layerPathArea(layerPathAreaIn), vLengthX(vLengthXIn), vLengthY(vLengthYIn),
                      dist(distIn), prevViaUp(prevViaUpIn), tLength(tLengthIn), /*preTurnDir(preTurnDirIn),*/ backTraceBuffer() {}
    FlexWavefrontGrid(int xIn, int yIn, int zIn, frCoord layerPathAreaIn, 
                      frCoord vLengthXIn, frCoord vLengthYIn,
                      bool prevViaUpIn, frCoord tLengthIn,
                      frCoord distIn, frCost pathCostIn, frCost costIn, /*frDirEnum preTurnDirIn,*/
                      std::bitset<WAVEFRONTBITSIZE> backTraceBufferIn): 
                      xIdx(xIn), yIdx(yIn), zIdx(zIn), pathCost(pathCostIn), cost(costIn), 
                      layerPathArea(layerPathAreaIn), vLengthX(vLengthXIn), vLengthY(vLengthYIn),
                      dist(distIn), prevViaUp(prevViaUpIn), tLength(tLengthIn), /*preTurnDir(preTurnDirIn),*/ backTraceBuffer(backTraceBufferIn) {}
    // bool operator<(const FlexWavefrontGrid &b) const {
    //   return this->cost > b.cost;
    // }
    bool operator<(const FlexWavefrontGrid &b) const {
      if (this->cost != b.cost) {
        return this->cost > b.cost; // prefer smaller cost
      } else {
        if (this->dist != b.dist) {
          return this->dist > b.dist; // prefer routing close to pin gravity center (centerPt)
        } else {
          if (this->zIdx != b.zIdx) {
            return this->zIdx < b.zIdx; // prefer upper layer
          } else {
            return this->pathCost < b.pathCost; //prefer larger pathcost, DFS-style
            // return this->pathCost > b.pathCost; //prefer smaller pathcost, BFS-style, test1 shows better results
          }
        }
      }
    }
    // getters
    frMIdx x() const {
      return xIdx;
    }
    frMIdx y() const {
      return yIdx;
    }
    frMIdx z() const {
      return zIdx;
    }
    frCost getPathCost() const {
      return pathCost;
    }
    frCost getCost() const {
      return cost;
    }
    std::bitset<WAVEFRONTBITSIZE> getBackTraceBuffer() const {
      return backTraceBuffer;
    }
    frCoord getLayerPathArea() const {
      return layerPathArea;
    }
    frCoord getLength() const {
      return vLengthX;
    }
    void getVLength(frCoord &vLengthXIn, frCoord &vLengthYIn) const {
      vLengthXIn = vLengthX;
      vLengthYIn = vLengthY;
    }
    bool isPrevViaUp() const {
      return prevViaUp;
    }
    frCoord getTLength() const {
      return tLength;
    }
    // setters
    void addLayerPathArea(frCoord in) {
      layerPathArea += in;
    }
    void resetLayerPathArea() {
      layerPathArea = 0;
    }

    //void addLength(frCoord in) {
    //  length += in;
    //}
    void resetLength() {
      vLengthX = 0;
      vLengthY = 0;
    }
    void setPrevViaUp(bool in) {
      prevViaUp = in;
    }
    // void setPreTurnDir(frDirEnum &in) {
    //   preTurnDir = in;
    // }

    //void addTLength(frCoord tLengthIn, bool isX) {
    //  if (isX) {
    //    tLengthX += tLengthIn;
    //  } else {
    //    tLengthY += tLengthIn;
    //  }
    //}
    //void addTLength(frCoord tLengthIn) {
    //  if (tLength != std::numeric_limits<frCoord>::max()) {
    //    tLength += tLengthIn;
    //  }
    //}
    //void resetTLength() {
    //  tLength = std::numeric_limits<frCoord>::max();
    //  //tLengthY = 0;
    //}

    frDirEnum getLastDir() const {
      auto currDirVal = backTraceBuffer.to_ulong() & 0b111u;
      return static_cast<frDirEnum>(currDirVal);
    }
    bool isBufferFull() const {
      std::bitset<WAVEFRONTBITSIZE> mask = WAVEFRONTBUFFERHIGHMASK;
      return (mask & backTraceBuffer).any();
    }
    //std::bitset<DIRBITSIZE> shiftAddBuffer(const frDirEnum &dir) {
    frDirEnum shiftAddBuffer(const frDirEnum &dir) {
      //std::bitset<WAVEFRONTBITSIZE> mask = WAVEFRONTBUFFERHIGHMASK;
      //std::bitset<DIRBITSIZE> retBS = (int)((mask & backTraceBuffer) >> (WAVEFRONTBITSIZE - DIRBITSIZE)).to_ulong();
      auto retBS = static_cast<frDirEnum>((backTraceBuffer >> (WAVEFRONTBITSIZE - DIRBITSIZE)).to_ulong());
      backTraceBuffer <<= DIRBITSIZE;
      std::bitset<WAVEFRONTBITSIZE> newBS = (unsigned)dir;
      backTraceBuffer |= newBS;
      return retBS;
    }
    // frDirEnum getPreTurnDir() const {
    //   return preTurnDir;
    // }
  protected:
    frMIdx xIdx, yIdx, zIdx;
    frCost pathCost; // path cost
    frCost cost; // path + est cost
    frCoord layerPathArea;
    frCoord vLengthX;
    frCoord vLengthY;
    frCoord dist; // to maze center
    bool    prevViaUp;
    frCoord tLength; // length since last turn
    // frDirEnum preTurnDir; // direction before last turn
    std::bitset<WAVEFRONTBITSIZE> backTraceBuffer;
  };

  class myPriorityQueue: public std::priority_queue<FlexWavefrontGrid> {
  public:
    void cleanup() {
      this->c.clear();
    }
    void fit() {
      this->c.clear();
      this->c.shrink_to_fit();
    }
    void init(int val) {
      this->c.reserve(val);
    }
  };

  class FlexWavefront {
  public:
    // void init(std::shared_ptr<FlexMazePin> rootPin);
    // FlexWavefront(int val) {
    //   wavefrontPQ.init(val);
    // }
    bool empty() const {
      return wavefrontPQ.empty();
    }
    const FlexWavefrontGrid& top() const {
      return wavefrontPQ.top();
    }
    void pop() {
      wavefrontPQ.pop();
    }
    void push(const FlexWavefrontGrid &in) {
      wavefrontPQ.push(in);
    }
    unsigned int size() const {
      return wavefrontPQ.size();
    }
    void cleanup() {
      //wavefrontPQ = std::priority_queue<FlexWavefrontGrid>();
      wavefrontPQ.cleanup();
    }
    void fit() {
      wavefrontPQ.fit();
    }
  protected:
    //std::priority_queue<FlexWavefrontGrid> wavefrontPQ;
    myPriorityQueue wavefrontPQ;
    //std::priority_queue<FlexWavefrontGrid, std::vector<FlexWavefrontGrid, boost::fast_pool_allocator<FlexWavefrontGrid> > > wavefrontPQ; // slower always
  };
}


#endif
