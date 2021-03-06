/*******************************************************************************
* Copyright 2015-2018 Intel Corporation
* All Rights Reserved.
*
* If this  software was obtained  under the  Intel Simplified  Software License,
* the following terms apply:
*
* The source code,  information  and material  ("Material") contained  herein is
* owned by Intel Corporation or its  suppliers or licensors,  and  title to such
* Material remains with Intel  Corporation or its  suppliers or  licensors.  The
* Material  contains  proprietary  information  of  Intel or  its suppliers  and
* licensors.  The Material is protected by  worldwide copyright  laws and treaty
* provisions.  No part  of  the  Material   may  be  used,  copied,  reproduced,
* modified, published,  uploaded, posted, transmitted,  distributed or disclosed
* in any way without Intel's prior express written permission.  No license under
* any patent,  copyright or other  intellectual property rights  in the Material
* is granted to  or  conferred  upon  you,  either   expressly,  by implication,
* inducement,  estoppel  or  otherwise.  Any  license   under such  intellectual
* property rights must be express and approved by Intel in writing.
*
* Unless otherwise agreed by Intel in writing,  you may not remove or alter this
* notice or  any  other  notice   embedded  in  Materials  by  Intel  or Intel's
* suppliers or licensors in any way.
*
*
* If this  software  was obtained  under the  Apache License,  Version  2.0 (the
* "License"), the following terms apply:
*
* You may  not use this  file except  in compliance  with  the License.  You may
* obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
*
*
* Unless  required  by   applicable  law  or  agreed  to  in  writing,  software
* distributed under the License  is distributed  on an  "AS IS"  BASIS,  WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
* See the   License  for the   specific  language   governing   permissions  and
* limitations under the License.
*******************************************************************************/

/* 
// 
//  Purpose:
//     Cryptography Primitive.
//     Decrypt 128-bit data block according to Rijndael
//     (compact S-box based implementation)
// 
//  Contents:
//     Safe2Decrypt_RIJ128()
// 
// 
*/

#include "owncp.h"

#if ((_IPP <_IPP_V8) && (_IPP32E <_IPP32E_U8)) /* no pshufb instruction */

#if (_ALG_AES_SAFE_==_ALG_AES_SAFE_COMPACT_SBOX_)
#pragma message("_ALG_AES_SAFE_COMPACT_SBOX_ enabled")

#include "pcprij128safe2.h"
#include "pcprijtables.h"


#include "pcpbnuimpl.h"
#define SELECTION_BITS  ((sizeof(BNU_CHUNK_T)/sizeof(Ipp8u)) -1)

#if defined(__INTEL_COMPILER)
__INLINE Ipp8u getInvSboxValue(Ipp8u x)
{
   BNU_CHUNK_T selection = 0;
   const BNU_CHUNK_T* SboxEntry = (BNU_CHUNK_T*)RijDecSbox;

   BNU_CHUNK_T i_sel = x/sizeof(BNU_CHUNK_T);  /* selection index */
   BNU_CHUNK_T i;
   for(i=0; i<sizeof(RijEncSbox)/sizeof(BNU_CHUNK_T); i++) {
      BNU_CHUNK_T mask = (i==i_sel)? (BNU_CHUNK_T)(-1) : 0; /* ipp and IPP build specific avoid jump instruction here */
      selection |= SboxEntry[i] & mask;
   }
   selection >>= (x & SELECTION_BITS)*8;
   return (Ipp8u)(selection & 0xFF);
}

#else
#include "pcpmask_ct.h"
__INLINE Ipp8u getInvSboxValue(Ipp8u x)
{
   BNU_CHUNK_T selection = 0;
   const BNU_CHUNK_T* SboxEntry = (BNU_CHUNK_T*)RijDecSbox;

   Ipp32u _x = x/sizeof(BNU_CHUNK_T);
   Ipp32u i;
   for(i=0; i<sizeof(RijEncSbox)/sizeof(BNU_CHUNK_T); i++) {
      BNS_CHUNK_T mask = cpIsEqu_ct(_x, i);
      selection |= SboxEntry[i] & mask;
   }
   selection >>= (x & SELECTION_BITS)*8;
   return (Ipp8u)(selection & 0xFF);
}
#endif

__INLINE void invSubBytes(Ipp8u state[])
{
   int i;
   for(i=0;i<16;i++)
      state[i] = getInvSboxValue(state[i]);
}

__INLINE void invShiftRows(Ipp32u* state)
{
   state[1] =  ROR32(state[1], 24);
   state[2] =  ROR32(state[2], 16);
   state[3] =  ROR32(state[3],  8);
}

__INLINE void invMixColumns(Ipp32u* state)
{
   Ipp32u y0 = state[1] ^ state[2] ^ state[3];
   Ipp32u y1 = state[0] ^ state[2] ^ state[3];
   Ipp32u y2 = state[0] ^ state[1] ^ state[3];
   Ipp32u y3 = state[0] ^ state[1] ^ state[2];
   Ipp32u t02, t13, t0123;

   state[0] = xtime4(state[0]);
   state[1] = xtime4(state[1]);
   state[2] = xtime4(state[2]);
   state[3] = xtime4(state[3]);

   y0 ^= state[0] ^ state[1];
   y1 ^= state[1] ^ state[2];
   y2 ^= state[2] ^ state[3];
   y3 ^= state[3] ^ state[0];

   t02 = state[0] ^ state[2];
   t13 = state[1] ^ state[3];
   t02 = xtime4(t02);
   t13 = xtime4(t13);

   t0123 = t02^t13;
   t0123 = xtime4(t0123);

   state[0] = y0 ^t02 ^t0123;
   state[1] = y1 ^t13 ^t0123;
   state[2] = y2 ^t02 ^t0123;
   state[3] = y3 ^t13 ^t0123;
}


void Safe2Decrypt_RIJ128(const Ipp8u* in,
                               Ipp8u* out,
                               int Nr,
                               const Ipp8u* RoundKey,
                               const void* sbox)
{
   Ipp32u state[4];

   int round=0;

   UNREFERENCED_PARAMETER(sbox);

   // copy input to the state array
   TRANSPOSE((Ipp8u*)state, in);

   // add the round key to the state before starting the rounds.
   XorRoundKey((Ipp32u*)state, (Ipp32u*)(RoundKey+Nr*16));

   // there will be Nr rounds
   for(round=Nr-1;round>0;round--) {
      invShiftRows(state);
      invSubBytes((Ipp8u*)state);
      XorRoundKey(state,(Ipp32u*)(RoundKey+round*16));
      invMixColumns(state);
    }

   // last round
   invShiftRows(state);
   invSubBytes((Ipp8u*)state);
   XorRoundKey(state,(Ipp32u*)(RoundKey+0*16));

   // copy from the state to output
   TRANSPOSE(out, (Ipp8u*)state);
}
#endif /* _ALG_AES_SAFE_COMPACT_SBOX_ */

#endif /* (_IPP <_IPP_V8) && (_IPP32E <_IPP32E_U8) */
