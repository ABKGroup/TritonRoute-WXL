////////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2018, Iowa State University All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////

#ifndef _FLUTE_H_
#define _FLUTE_H_

namespace flute {
    /*****************************/
    /*  User-Defined Parameters  */
    /*****************************/
    // #define MAXD 1000    // max. degree that can be handled
    #define FLUTE_ACCURACY 10  // Default accuracy
    #define FLUTE_ROUTING 1   // 1 to construct routing, 0 to estimate WL only
    #define FLUTE_LOCAL_REFINEMENT 1      // Suggestion: Set to 1 if ACCURACY >= 5
    #define FLUTE_REMOVE_DUPLICATE_PIN 1  // Remove dup. pin for flute_wl() & flute()
    
    #ifndef DTYPE   // Data type for distance
    #define DTYPE int
    #endif
    
    
    /*****************************/
    /*  User-Callable Functions  */
    /*****************************/
    // void readLUT();
    // DTYPE flute_wl(int d, DTYPE x[], DTYPE y[], int acc);
    // DTYPE flutes_wl(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    // Tree flute(int d, DTYPE x[], DTYPE y[], int acc);
    // Tree flutes(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    // DTYPE wirelength(Tree t);
    // void printtree(Tree t);
    // void plottree(Tree t);
    
    
    /*************************************/
    /* Internal Parameters and Functions */
    /*************************************/
    #define FLUTE_POWVFILE "POWV9.dat"        // LUT for POWV (Wirelength Vector)
    #define FLUTE_POSTFILE "POST9.dat"        // LUT for POST (Steiner Tree)
    #define FLUTE_D 9                         // LUT is used for d <= D, D <= 9
    #define FLUTE_TAU(A) (8+1.3*(A))
    #define FLUTE_D1(A) (25+120/((A)*(A)))     // flute_mr is used for D1 < d <= D2
    #define FLUTE_D2(A) ((A)<=6 ? 500 : 75+5*(A))

  
    typedef struct
    {
        DTYPE x, y;   // starting point of the branch
        int n;   // index of neighbor
    } Branch;
    
    typedef struct
    {
        int deg;   // degree
        DTYPE length;   // total wirelength
        Branch *branch;   // array of tree branches
    } Tree;
    
    // User-Callable Functions
    void readLUT();
    DTYPE flute_wl(int d, DTYPE x[], DTYPE y[], int acc);
    // DTYPE flutes_wl(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    Tree flute(int d, DTYPE x[], DTYPE y[], int acc);
    // Tree flutes(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    DTYPE wirelength(Tree t);
    void printtree(Tree t);
    void plottree(Tree t);
    void freetree(Tree t);
    
    // Other useful functions
    // void init_param();
    DTYPE flutes_wl_LD(int d, DTYPE xs[], DTYPE ys[], int s[]);
    DTYPE flutes_wl_MD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    DTYPE flutes_wl_RDP(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    Tree flutes_LD(int d, DTYPE xs[], DTYPE ys[], int s[]);
    Tree flutes_MD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    Tree flutes_HD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    Tree flutes_RDP(int d, DTYPE xs[], DTYPE ys[], int s[], int acc);
    


    // #if REMOVE_DUPLICATE_PIN==1
    //   #define flutes_wl(d, xs, ys, s, acc) flutes_wl_RDP(d, xs, ys, s, acc) 
    //   #define flutes(d, xs, ys, s, acc) flutes_RDP(d, xs, ys, s, acc) 
    // #else
    //   #define flutes_wl(d, xs, ys, s, acc) flutes_wl_ALLD(d, xs, ys, s, acc) 
    //   #define flutes(d, xs, ys, s, acc) flutes_ALLD(d, xs, ys, s, acc) 
    // #endif
    
    
    inline Tree flutes_ALLD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc) {
        if (d <= FLUTE_D) {
            return flutes_LD(d, xs, ys, s);
        } else {
            return flutes_MD(d, xs, ys, s, acc);
        }
    }

    // #define flutes_wl_ALLD(d, xs, ys, s, acc) flutes_wl_LMD(d, xs, ys, s, acc)
    // #define flutes_ALLD(d, xs, ys, s, acc) \
    //     (d<=D ? flutes_LD(d, xs, ys, s) \
    // 			: flutes_MD(d, xs, ys, s, acc))
    
    inline DTYPE flutes_wl_LMD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc) {
        if (d <= FLUTE_D) {
            return flutes_wl_LD(d, xs, ys, s);
        } else {
            return flutes_wl_MD(d, xs, ys, s, acc);
        }
    }

    inline DTYPE flutes_wl_ALLD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc) {
        return flutes_wl_LMD(d, xs, ys, s, acc);
    }

    inline DTYPE flutes_wl(int d, DTYPE xs[], DTYPE ys[], int s[], int acc) {
        if (FLUTE_REMOVE_DUPLICATE_PIN == 1) {
            return flutes_wl_RDP(d, xs, ys, s, acc);
        } else {
            return flutes_wl_ALLD(d, xs, ys, s, acc);
        }
    }
    
    inline Tree flutes_LMD(int d, DTYPE xs[], DTYPE ys[], int s[], int acc) {
        if (d <= FLUTE_D) {
            return flutes_LD(d, xs, ys, s);
        } else {
            return flutes_MD(d, xs, ys, s, acc);
        }
    }

    inline Tree flutes(int d, DTYPE xs[], DTYPE ys[], int s[], int acc) {
        if (FLUTE_REMOVE_DUPLICATE_PIN == 1) {
            return flutes_RDP(d, xs, ys, s, acc);
        } else {
            return flutes_ALLD(d, xs, ys, s, acc);
        }
    }

    // #define flutes_wl_LMD(d, xs, ys, s, acc) \
    //     (d<=D ? flutes_wl_LD(d, xs, ys, s) : flutes_wl_MD(d, xs, ys, s, acc))
    // #define flutes_LMD(d, xs, ys, s, acc) \
    //     (d<=D ? flutes_LD(d, xs, ys, s) : flutes_MD(d, xs, ys, s, acc))
    
    inline int ADIFF(int x, int y) {
        return ((x)>(y)?(x-y):(y-x));
    }

    // #define max(x,y) ((x)>(y)?(x):(y))
    // #define min(x,y) ((x)<(y)?(x):(y))
    // #define abs(x) ((x)<0?(-x):(x))
    // #define ADIFF(x,y) ((x)>(y)?(x-y):(y-x))  // Absolute difference

}
#endif
