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

#ifndef _FR_TRACKPATTERN_H_
#define _FR_TRACKPATTERN_H_

#include "frBaseTypes.h"
#include "db/obj/frBlockObject.h"

namespace fr {
  class frTrackPattern: public frBlockObject {
  public:
    // constructors
    frTrackPattern() = default;
    frTrackPattern(const frTrackPattern& tmpTrackPattern) : 
      horizontal(tmpTrackPattern.horizontal), startCoord(tmpTrackPattern.startCoord), 
      numTracks(tmpTrackPattern.numTracks), trackSpacing(tmpTrackPattern.trackSpacing), layerNum(tmpTrackPattern.layerNum) {}
    frTrackPattern(bool tmpIsH, frCoord tmpSC, frUInt4 tmpNT, frUInt4 tmpTS, frLayerNum tmpLN) :
      horizontal(tmpIsH), startCoord(tmpSC), numTracks(tmpNT), trackSpacing(tmpTS), layerNum(tmpLN) {}
    // getters
    // vertical track has horizontal = true;
    bool isHorizontal() const {
      return horizontal;
    }
    frCoord getStartCoord() const {
      return startCoord;
    }
    frUInt4 getNumTracks() const {
      return numTracks;
    }
    frUInt4 getTrackSpacing() const {
      return trackSpacing;
    }
    frLayerNum getLayerNum() const {
      return layerNum;
    }
    // setters
    // vertical track has horizontal = true;
    void setHorizontal(bool tmpIsHorizontal) {
      horizontal = tmpIsHorizontal;
    }
    void setStartCoord(frCoord tmpStartCoord) {
      startCoord = tmpStartCoord;
    }
    void setNumTracks(frUInt4 tmpNumTracks) {
      numTracks = tmpNumTracks;
    }
    void setTrackSpacing(frUInt4 tmpTrackSpacing) {
      trackSpacing = tmpTrackSpacing;
    }
    void setLayerNum(frLayerNum tmpLayerNum) {
      layerNum = tmpLayerNum;
    }
  protected:
    bool       horizontal;
    frCoord    startCoord;
    frUInt4    numTracks;
    frUInt4    trackSpacing;
    frLayerNum layerNum;
  };

}

#endif
