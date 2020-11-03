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

//#include "FlexRoute.h"
#include <iostream>
#include <fstream>
#include "global.h"
#include "frDesign.h"
using namespace std;
using namespace fr;

void frDesign::printAllComps() {
  cout <<endl <<"print all components: ";
  for (auto &m: topBlock->getInsts()) {
    cout <<endl <<*m <<endl;
  }
}

void frDesign::printAllMacros() {
  cout <<endl;
  cout <<"print all macros: " <<endl;
  for (auto &m: refBlocks) {
    cout <<endl <<*(m) <<endl;
  }
}

////void FlexRoute::printAllNets() {
////  cout <<endl;
////  cout <<"print all nets: " <<endl;
////  for (auto &m: nets) {
////    cout <<endl <<*(m.second) <<endl;
////  }
////}

void frDesign::printAllTerms() {
  cout <<endl <<"print all terminals: ";
  for (auto &m: topBlock->getTerms()) {
    cout <<endl <<*m <<endl;
  }
}

void frTechObject::printAllVias() {
  cout <<endl <<"print all vias: " <<endl;
  for (auto &m: vias) {
    cout <<endl <<*(m) <<endl;
  }
}

void frDesign::printCMap() {
  cout <<endl <<"print cmap: ";
  int xCnt = 0;
  int yCnt = 0;
  for (auto &gcp: topBlock->getGCellPatterns()) {
    if (gcp.isHorizontal()) {
      yCnt = gcp.getCount();
    } else {
      xCnt = gcp.getCount();
    }
  }
  cout <<"x/y = " <<xCnt <<" " <<yCnt <<endl;
  auto &cmap = topBlock->getCMap();
  for (int lNum = 0; lNum <= (int)getTech()->getLayers().size(); lNum += 2) {
    for (int i = 0; i < xCnt; i++) {
      for (int j = 0; j < yCnt; j++) {
        cout <<"x/y/z/s/t/l/e1/e2/u = " 
             <<i <<" " <<j <<" " <<lNum <<" "
             <<cmap.getSupply(i,j,lNum) <<" "
             <<cmap.getThroughDemand(i,j,lNum) <<" "
             <<cmap.getLocalDemand(i,j,lNum) <<" "
             <<cmap.getEdge1Demand(i,j,lNum) <<" "
             <<cmap.getEdge2Demand(i,j,lNum) <<" "
             <<cmap.getUpDemand(i,j,lNum) <<" ";
        if (cmap.getThroughDemand(i,j,lNum) + cmap.getLocalDemand(i,j,lNum) +
            cmap.getEdge1Demand(i,j,lNum)   > cmap.getSupply(i,j,lNum) ||
            cmap.getThroughDemand(i,j,lNum) + cmap.getLocalDemand(i,j,lNum) +
            cmap.getEdge2Demand(i,j,lNum)   > cmap.getSupply(i,j,lNum)) {
          cout <<"GCell overflow";
        }
        cout <<endl;
      }
    }
  }
}
