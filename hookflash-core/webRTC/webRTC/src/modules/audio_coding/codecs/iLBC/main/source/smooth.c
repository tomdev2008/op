/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_Smooth.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "smooth_out_data.h"

/*----------------------------------------------------------------*
 * find the smoothed output data
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_Smooth(
    WebRtc_Word16 *odata,   /* (o) smoothed output */
    WebRtc_Word16 *current,  /* (i) the un enhanced residual for
                                this block */
    WebRtc_Word16 *surround  /* (i) The approximation from the
                                surrounding sequences */
                          ) {
  WebRtc_Word16 maxtot, scale, scale1, scale2;
  WebRtc_Word16 A, B, C, denomW16;
  WebRtc_Word32 B_W32, denom, num;
  WebRtc_Word32 errs;
  WebRtc_Word32 w00,w10,w11, endiff, crit;
  WebRtc_Word32 w00prim, w10prim, w11_div_w00;
  WebRtc_Word16 w11prim;
  WebRtc_Word16 bitsw00, bitsw10, bitsw11;
  WebRtc_Word32 w11w00, w10w10, w00w00;
  WebRtc_Word16 max1, max2;

  /* compute some inner products (ensure no overflow by first calculating proper scale factor) */

  w00 = w10 = w11 = 0;
  current=current;

  max1=WebRtcSpl_MaxAbsValueW16(current, ENH_BLOCKL);
  max2=WebRtcSpl_MaxAbsValueW16(surround, ENH_BLOCKL);
  maxtot=WEBRTC_SPL_MAX(max1, max2);

  scale=WebRtcSpl_GetSizeInBits(maxtot);
  scale = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(2,scale)-26;
  scale=WEBRTC_SPL_MAX(0, scale);

  w00=WebRtcSpl_DotProductWithScale(current,current,ENH_BLOCKL,scale);
  w11=WebRtcSpl_DotProductWithScale(surround,surround,ENH_BLOCKL,scale);
  w10=WebRtcSpl_DotProductWithScale(surround,current,ENH_BLOCKL,scale);

  if (w00<0) w00 = WEBRTC_SPL_WORD32_MAX;
  if (w11<0) w11 = WEBRTC_SPL_WORD32_MAX;

  /* Rescale w00 and w11 to w00prim and w11prim, so that w00prim/w11prim
     is in Q16 */

  bitsw00 = WebRtcSpl_GetSizeInBits(w00);
  bitsw11 = WebRtcSpl_GetSizeInBits(w11);
  bitsw10 = WebRtcSpl_GetSizeInBits(WEBRTC_SPL_ABS_W32(w10));
  scale1 = 31 - bitsw00;
  scale2 = 15 - bitsw11;

  if (scale2>(scale1-16)) {
    scale2 = scale1 - 16;
  } else {
    scale1 = scale2 + 16;
  }

  w00prim = WEBRTC_SPL_LSHIFT_W32(w00, scale1);
  w11prim = (WebRtc_Word16) WEBRTC_SPL_SHIFT_W32(w11, scale2);

  /* Perform C = sqrt(w11/w00) (C is in Q11 since (16+6)/2=11) */
  if (w11prim>64) {
    endiff = WEBRTC_SPL_LSHIFT_W32(
        (WebRtc_Word32)WebRtcSpl_DivW32W16(w00prim, w11prim), 6);
    C = (WebRtc_Word16)WebRtcSpl_Sqrt(endiff); /* C is in Q11 */
  } else {
    C = 1;
  }

  /* first try enhancement without power-constraint */

  errs = WebRtcIlbcfix_Smooth_odata(odata, current, surround, C);



  /* if constraint violated by first try, add constraint */

  if ( (6-scale+scale1) > 31) {
    crit=0;
  } else {
    /* crit = 0.05 * w00 (Result in Q-6) */
    crit = WEBRTC_SPL_SHIFT_W32(
        WEBRTC_SPL_MUL(ENH_A0, WEBRTC_SPL_RSHIFT_W32(w00prim, 14)),
        -(6-scale+scale1));
  }

  if (errs > crit) {

    if( w00 < 1) {
      w00=1;
    }

    /* Calculate w11*w00, w10*w10 and w00*w00 in the same Q domain */

    scale1 = bitsw00-15;
    scale2 = bitsw11-15;

    if (scale2>scale1) {
      scale = scale2;
    } else {
      scale = scale1;
    }

    w11w00 = WEBRTC_SPL_MUL_16_16(
        (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(w11, -scale),
        (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(w00, -scale));

    w10w10 = WEBRTC_SPL_MUL_16_16(
        (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(w10, -scale),
        (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(w10, -scale));

    w00w00 = WEBRTC_SPL_MUL_16_16(
        (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(w00, -scale),
        (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(w00, -scale));

    /* Calculate (w11*w00-w10*w10)/(w00*w00) in Q16 */
    if (w00w00>65536) {
      endiff = (w11w00-w10w10);
      endiff = WEBRTC_SPL_MAX(0, endiff);
      /* denom is in Q16 */
      denom = WebRtcSpl_DivW32W16(endiff, (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(w00w00, 16));
    } else {
      denom = 65536;
    }

    if( denom > 7){ /* eliminates numerical problems
                       for if smooth */

      scale=WebRtcSpl_GetSizeInBits(denom)-15;

      if (scale>0) {
        /* denomW16 is in Q(16+scale) */
        denomW16=(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(denom, scale);

        /* num in Q(34-scale) */
        num=WEBRTC_SPL_RSHIFT_W32(ENH_A0_MINUS_A0A0DIV4, scale);
      } else {
        /* denomW16 is in Q16 */
        denomW16=(WebRtc_Word16)denom;

        /* num in Q34 */
        num=ENH_A0_MINUS_A0A0DIV4;
      }

      /* A sqrt( (ENH_A0-(ENH_A0^2)/4)*(w00*w00)/(w11*w00 + w10*w10) ) in Q9 */
      A = (WebRtc_Word16)WebRtcSpl_Sqrt(WebRtcSpl_DivW32W16(num, denomW16));

      /* B_W32 is in Q30 ( B = 1 - ENH_A0/2 - A * w10/w00 ) */
      scale1 = 31-bitsw10;
      scale2 = 21-scale1;
      w10prim = WEBRTC_SPL_LSHIFT_W32(w10, scale1);
      w00prim = WEBRTC_SPL_SHIFT_W32(w00, -scale2);
      scale = bitsw00-scale2-15;

      if (scale>0) {
        w10prim=WEBRTC_SPL_RSHIFT_W32(w10prim, scale);
        w00prim=WEBRTC_SPL_RSHIFT_W32(w00prim, scale);
      }

      if ((w00prim>0)&&(w10prim>0)) {
        w11_div_w00=WebRtcSpl_DivW32W16(w10prim, (WebRtc_Word16)w00prim);

        if (WebRtcSpl_GetSizeInBits(w11_div_w00)+WebRtcSpl_GetSizeInBits(A)>31) {
          B_W32 = 0;
        } else {
          B_W32 = (WebRtc_Word32)1073741824 - (WebRtc_Word32)ENH_A0DIV2 -
              WEBRTC_SPL_MUL(A, w11_div_w00);
        }
        B = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(B_W32, 16); /* B in Q14 */
      } else {
        /* No smoothing */
        A = 0;
        B = 16384; /* 1 in Q14 */
      }
    }
    else{ /* essentially no difference between cycles;
             smoothing not needed */

      A = 0;
      B = 16384; /* 1 in Q14 */
    }

    /* create smoothed sequence */

    WebRtcSpl_ScaleAndAddVectors(surround, A, 9,
                                current, B, 14,
                                odata, ENH_BLOCKL);
  }
  return;
}