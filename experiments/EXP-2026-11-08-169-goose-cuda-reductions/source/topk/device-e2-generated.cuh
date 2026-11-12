#pragma once
#include <stdint.h>
#include <string.h>
typedef struct { void *data; int64_t len; } fg169t2_gg_slice;


#pragma pack(push, 1)
typedef struct { float data[2]; } fg169t2_gg_type_8;
#pragma pack(pop)
static __device__ __forceinline__ void fg169t2_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ fg169t2_gg_slice fg169t2_gg_view(void *p, int64_t n) { fg169t2_gg_slice v; v.data=p; v.len=n; return v; }
static __device__ __forceinline__ double fg169t2_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg169t2_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg169t2_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg169t2_gg_div(int64_t a, int64_t b) { if (!b) fg169t2_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg169t2_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg169t2_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg169t2_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg169t2_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg169t2_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg169t2_gg_umod(uint64_t a,uint64_t b) { if(!b)fg169t2_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ float fg169t2_gg_function_15(float p0, int64_t base);
static __device__ __forceinline__ float fg169t2_gg_function_12(float p0, int64_t base);
static __device__ __forceinline__ float fg169t2_gg_function_4(int64_t base);
static __device__ __forceinline__ float fg169t2_gg_function_3(int64_t base);
static __device__ __forceinline__ void fg169t2_gg_function_1(int64_t p0, uint64_t p1, int32_t p2, int32_t p3, int32_t p4, uint8_t p5, int64_t base);
static __device__ __forceinline__ float fg169t2_gg_function_15(float p0, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  float t3;
  float t4;
  int32_t t5;
  float t6;
  float t7;
  int32_t t8;
  int32_t t9;
  float v0;
  float v1;
  int32_t v2;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = v1;
  t4 = ((float)(v1));
  t5 = ((int32_t)(v2));
  t6 = fg169t_shuffle(t4, t5);
  t7 = ((float)(t6));
  v1 = ((t3) + (t7));
  t8 = v2;
  t9 = ((int32_t)(((int64_t)UINT64_C(1))));
  v2 = ((t8) >> ((uint64_t)(t9) & 31));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto fg169t2_gg_done;
  }
fg169t2_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg169t2_gg_function_12(float p0, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  float t3;
  float t4;
  int32_t t5;
  float t6;
  float t7;
  float t8;
  int32_t t9;
  int32_t t10;
  float v0;
  float v1;
  int32_t v2;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((float)(v1));
  t5 = ((int32_t)(v2));
  t6 = fg169t_shuffle(t4, t5);
  t7 = ((float)(t6));
  t8 = fg169t_fmax(t3, t7);
  v1 = ((float)(t8));
  t9 = v2;
  t10 = ((int32_t)(((int64_t)UINT64_C(1))));
  v2 = ((t9) >> ((uint64_t)(t10) & 31));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto fg169t2_gg_done;
  }
fg169t2_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg169t2_gg_function_4(int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float v0;
  float v1;
  float v2;
  {
  t1 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4665729213955833856))))));
  t2 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4665729213955833856))))));
  v0 = ((float)(((t1) * (t2))));
  t3 = ((float)(v0));
  t4 = ((float)(v0));
  v1 = ((float)(((t3) * (t4))));
  t5 = ((float)(v1));
  t6 = ((float)(v1));
  v2 = ((float)(((t5) * (t6))));
  t7 = ((float)(v2));
  t8 = ((float)(fg169t2_gg_double_bits(UINT64_C(4715268809320038400))));
  t0 = ((float)(((t7) * (t8))));
  goto fg169t2_gg_done;
  }
fg169t2_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg169t2_gg_function_3(int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float v0;
  {
  t1 = fg169t2_gg_function_4(base + 0);
  t2 = ((float)(t1));
  t3 = ((float)(fg169t2_gg_double_bits(UINT64_C(4611686018427387904))));
  v0 = ((float)(((t2) * (t3))));
  t4 = v0;
  t0 = ((float)((-(t4))));
  goto fg169t2_gg_done;
  }
fg169t2_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg169t2_gg_function_1(int64_t p0, uint64_t p1, int32_t p2, int32_t p3, int32_t p4, uint8_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  uint8_t t44;
  float t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint64_t t60;
  uint64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  float t80;
  float t81;
  int64_t t82;
  int32_t t83;
  int32_t t84;
  int32_t t85;
  float * t86;
  int32_t t87;
  int32_t t88;
  int32_t t89;
  int32_t t90;
  int32_t t91;
  int32_t t92;
  int32_t t93;
  int32_t t94;
  int32_t t95;
  float * t96;
  int32_t t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  uint8_t t101;
  uint8_t t102;
  int32_t t103;
  int32_t t104;
  uint8_t t105;
  uint64_t t106;
  uint64_t t107;
  float t108;
  int32_t t109;
  int32_t t110;
  uint8_t t111;
  uint8_t t112;
  uint8_t t113;
  int32_t t114;
  int32_t t115;
  int32_t t116;
  float * t117;
  float t118;
  float t119;
  int32_t t120;
  float * t121;
  float t122;
  float t123;
  float t124;
  float t125;
  float t126;
  int32_t t127;
  int32_t t128;
  uint8_t t129;
  int32_t t130;
  int32_t t131;
  int32_t t132;
  float * t133;
  int32_t t134;
  float * t135;
  float t136;
  float t137;
  float t138;
  uint8_t t139;
  float t140;
  float t141;
  float t142;
  float t143;
  float t144;
  float t145;
  float t146;
  float t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  int32_t t151;
  float t152;
  int32_t t153;
  float * t154;
  float t155;
  float t156;
  int32_t t157;
  int32_t t158;
  float t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  float * t163;
  float t164;
  float t165;
  float t166;
  float t167;
  int32_t t168;
  float * t169;
  float t170;
  float t171;
  int32_t t172;
  int32_t t173;
  float t174;
  float t175;
  float t176;
  int32_t t177;
  int32_t t178;
  int32_t t179;
  float * t180;
  int32_t t181;
  float * t182;
  float t183;
  float t184;
  int32_t t185;
  int32_t t186;
  int32_t t187;
  int32_t t188;
  int32_t t189;
  float * t190;
  float t191;
  int32_t t192;
  float * t193;
  float t194;
  uint8_t t195;
  int32_t t196;
  float * t197;
  int32_t t198;
  int32_t t199;
  float t200;
  float t201;
  int64_t t202;
  uint8_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  int32_t t207;
  int32_t t208;
  int32_t t209;
  int32_t t210;
  float * t211;
  int32_t t212;
  int32_t t213;
  int32_t t214;
  int32_t t215;
  uint8_t t216;
  uint8_t t217;
  int32_t t218;
  int32_t t219;
  uint8_t t220;
  int32_t t221;
  float * t222;
  float t223;
  uint64_t t224;
  uint64_t t225;
  float t226;
  float t227;
  int32_t t228;
  int32_t t229;
  int32_t t230;
  int32_t t231;
  uint8_t t232;
  int32_t t233;
  int32_t t234;
  int32_t t235;
  int32_t t236;
  int32_t t237;
  int32_t t238;
  int32_t t239;
  int32_t t240;
  int32_t t241;
  int32_t t242;
  int32_t t243;
  int32_t t244;
  int32_t t245;
  int32_t t246;
  uint8_t t247;
  uint8_t t248;
  int32_t t249;
  float * t250;
  int32_t t251;
  float * t252;
  float t253;
  float t254;
  float t255;
  float t256;
  float t257;
  float t258;
  float t259;
  float t260;
  float t261;
  int32_t t262;
  int32_t t263;
  int32_t t264;
  int32_t t265;
  float t266;
  int32_t t267;
  float t268;
  float t269;
  int32_t t270;
  float t271;
  float t272;
  float t273;
  float t274;
  float t275;
  float t276;
  float t277;
  float t278;
  float t279;
  float t280;
  float t281;
  float t282;
  float t283;
  int32_t t284;
  int32_t t285;
  int32_t t286;
  int32_t t287;
  uint8_t t288;
  float t289;
  float t290;
  int32_t t291;
  int32_t t292;
  int32_t t293;
  int32_t t294;
  float t295;
  int32_t t296;
  float t297;
  float t298;
  float t299;
  uint8_t t300;
  uint8_t t301;
  float t302;
  float t303;
  uint8_t t304;
  uint8_t t305;
  int32_t t306;
  int32_t t307;
  uint8_t t308;
  int32_t t309;
  int32_t t310;
  int32_t t311;
  int32_t t312;
  int32_t t313;
  int32_t t314;
  uint8_t t315;
  uint8_t t316;
  int32_t t317;
  int32_t t318;
  uint8_t t319;
  uint64_t t320;
  int32_t t321;
  int32_t t322;
  int32_t t323;
  int32_t t324;
  int32_t t325;
  int32_t t326;
  int32_t t327;
  int32_t t328;
  int32_t t329;
  int32_t t330;
  uint8_t t331;
  uint8_t t332;
  uint64_t t333;
  uint64_t t334;
  uint64_t t335;
  uint64_t t336;
  uint64_t t337;
  uint64_t t338;
  uint8_t t339;
  uint8_t t340;
  int32_t t341;
  float * t342;
  int32_t t343;
  float * t344;
  int32_t t345;
  int32_t t346;
  float t347;
  float t348;
  int64_t t349;
  int32_t t350;
  int32_t t351;
  int32_t t352;
  float * t353;
  int32_t t354;
  int32_t t355;
  int32_t t356;
  int32_t t357;
  int32_t t358;
  int32_t t359;
  uint8_t t360;
  int64_t t361;
  float * t362;
  int32_t t363;
  int32_t t364;
  uint8_t t365;
  uint8_t t366;
  int64_t t367;
  float * t368;
  int64_t t369;
  float * t370;
  int32_t t371;
  int32_t t372;
  int32_t t373;
  int32_t t374;
  int32_t t375;
  int32_t t376;
  uint8_t t377;
  int32_t t378;
  float * t379;
  int32_t t380;
  float * t381;
  int32_t t382;
  int32_t t383;
  int32_t t384;
  int32_t t385;
  uint8_t t386;
  uint8_t t387;
  int32_t t388;
  int32_t t389;
  uint8_t t390;
  uint8_t t391;
  float t392;
  float t393;
  uint8_t t394;
  int32_t t395;
  float * t396;
  int32_t t397;
  int32_t t398;
  int32_t t399;
  int32_t t400;
  float t401;
  int32_t t402;
  float t403;
  float t404;
  int32_t t405;
  float t406;
  int32_t t407;
  int32_t t408;
  int32_t t409;
  float t410;
  float t411;
  uint8_t t412;
  uint8_t t413;
  float t414;
  float t415;
  uint8_t t416;
  uint8_t t417;
  int32_t t418;
  int32_t t419;
  uint8_t t420;
  int32_t t421;
  int32_t t422;
  int32_t t423;
  int32_t t424;
  int32_t t425;
  int32_t t426;
  uint8_t t427;
  uint8_t t428;
  fg169t2_gg_slice t429;
  int32_t t430;
  int32_t t431;
  int32_t t432;
  float * t433;
  fg169t2_gg_slice t434;
  int32_t t435;
  int32_t t436;
  int32_t t437;
  float * t438;
  int32_t t439;
  int32_t t440;
  int32_t t441;
  int32_t t442;
  uint8_t t443;
  fg169t2_gg_slice t444;
  int32_t t445;
  int32_t t446;
  int32_t t447;
  float * t448;
  int32_t t449;
  int32_t t450;
  int32_t t451;
  int32_t t452;
  uint8_t t453;
  uint64_t t454;
  uint64_t t455;
  int32_t t456;
  uint8_t t457;
  float t458;
  float t459;
  int32_t t460;
  int32_t t461;
  uint8_t t462;
  float t463;
  int64_t t464;
  int64_t t465;
  float t466;
  float t467;
  float t468;
  float t469;
  float t470;
  float t471;
  int32_t t472;
  int32_t t473;
  int32_t t474;
  float * t475;
  int32_t t476;
  float * t477;
  float t478;
  float t479;
  int32_t t480;
  int32_t t481;
  uint8_t t482;
  int32_t t483;
  int32_t t484;
  int32_t t485;
  int32_t t486;
  int32_t t487;
  int32_t t488;
  int32_t t489;
  int32_t t490;
  uint8_t t491;
  float t492;
  int32_t t493;
  float * t494;
  float t495;
  float t496;
  int32_t t497;
  int32_t t498;
  float t499;
  int32_t t500;
  int32_t t501;
  int32_t t502;
  int32_t t503;
  int32_t t504;
  int32_t t505;
  int32_t t506;
  int32_t t507;
  uint8_t t508;
  int32_t t509;
  float * t510;
  float t511;
  float t512;
  float t513;
  float t514;
  int32_t t515;
  float * t516;
  float t517;
  float t518;
  int32_t t519;
  float * t520;
  int32_t t521;
  int32_t t522;
  float t523;
  float t524;
  float t525;
  int32_t t526;
  int32_t t527;
  int32_t t528;
  int32_t t529;
  int32_t t530;
  int32_t t531;
  int32_t t532;
  int32_t t533;
  uint8_t t534;
  int32_t t535;
  float * t536;
  int32_t t537;
  float * t538;
  float t539;
  float t540;
  int32_t t541;
  int32_t t542;
  int64_t t543;
  int64_t t544;
  float t545;
  int32_t t546;
  int32_t t547;
  int32_t t548;
  int32_t t549;
  int32_t t550;
  int32_t t551;
  int32_t t552;
  int32_t t553;
  uint8_t t554;
  uint64_t t555;
  uint64_t t556;
  int32_t t557;
  float * t558;
  float t559;
  float t560;
  float t561;
  int32_t t562;
  int32_t t563;
  int64_t v0;
  uint64_t v1;
  int32_t v2;
  int32_t v3;
  int32_t v4;
  uint8_t v5;
  int32_t v6;
  int32_t v7;
  uint64_t v8;
  uint8_t v9;
  uint8_t v10;
  uint8_t v11;
  uint8_t v12;
  int32_t v13;
  int32_t v14;
  float v15;
  float v16;
  float v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  fg169t2_gg_type_8 v22;
  int32_t v23;
  int32_t v24;
  float v25;
  float v26;
  float v27;
  float v28;
  float v29;
  fg169t2_gg_type_8 v30;
  int32_t v31;
  int32_t v32;
  float v33;
  int32_t v34;
  float v35;
  float v36;
  float v37;
  int32_t v38;
  float v39;
  float v40;
  int32_t v41;
  float v42;
  uint64_t v43;
  int32_t v44;
  float v45;
  fg169t2_gg_type_8 v46;
  int32_t v47;
  float v48;
  float v49;
  int32_t v50;
  int32_t v51;
  float v52;
  int32_t v53;
  float v54;
  float v55;
  int32_t v56;
  float v57;
  float v58;
  float v59;
  float v60;
  int32_t v61;
  float v62;
  int32_t v63;
  float v64;
  float v65;
  int32_t v66;
  float v67;
  int32_t v68;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v0));
  t1 = ((int64_t)(((int64_t)UINT64_C(3))));
  t2 = ((int64_t)(((int64_t)UINT64_C(1))));
  t3 = fg169t_di(t0, t1, t2);
  v6 = ((int32_t)(((int32_t)(t3))));
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)UINT64_C(3))));
  t6 = ((int64_t)(((int64_t)UINT64_C(2))));
  t7 = fg169t_di(t4, t5, t6);
  v7 = ((int32_t)(((int32_t)(t7))));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(((int64_t)UINT64_C(3))));
  t10 = ((int64_t)(((int64_t)UINT64_C(3))));
  t11 = fg169t_di(t8, t9, t10);
  v8 = ((uint64_t)(t11));
  t12 = ((uint64_t)(v8));
  t13 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t14 = ((uint64_t)(((uint64_t)fg169t2_gg_umod(t12, t13))));
  t15 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v9 = ((t14) == (t15));
  t16 = ((uint64_t)(v8));
  t17 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t18 = ((uint64_t)(((uint64_t)fg169t2_gg_udiv(t16, t17))));
  t19 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t20 = ((uint64_t)(((uint64_t)fg169t2_gg_umod(t18, t19))));
  t21 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v10 = ((t20) == (t21));
  t22 = ((uint64_t)(v8));
  t23 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t24 = ((uint64_t)(((uint64_t)fg169t2_gg_udiv(t22, t23))));
  t25 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t26 = ((uint64_t)(((uint64_t)fg169t2_gg_umod(t24, t25))));
  t27 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v11 = ((t26) == (t27));
  t28 = ((uint64_t)(v8));
  t29 = ((uint64_t)(((int64_t)UINT64_C(8))));
  t30 = ((uint64_t)(((uint64_t)fg169t2_gg_udiv(t28, t29))));
  t31 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t32 = ((uint64_t)(((uint64_t)fg169t2_gg_umod(t30, t31))));
  t33 = ((uint64_t)(((int64_t)UINT64_C(1))));
  v12 = ((t32) == (t33));
  t34 = ((int64_t)(v0));
  t35 = ((int64_t)(((int64_t)UINT64_C(3))));
  t36 = ((int64_t)(((int64_t)UINT64_C(4))));
  t37 = fg169t_di(t34, t35, t36);
  v13 = ((int32_t)(((int32_t)(t37))));
  t38 = ((int64_t)(v0));
  t39 = ((int64_t)(((int64_t)UINT64_C(3))));
  t40 = ((int64_t)(((int64_t)UINT64_C(5))));
  t41 = fg169t_di(t38, t39, t40);
  v14 = ((int32_t)(((int32_t)(t41))));
  t42 = ((uint64_t)(v1));
  t43 = ((uint64_t)(((uint64_t)(v6))));
  t44 = ((t42) >= (t43));
  if (t44) {
  {
  goto fg169t2_gg_done;
  }
  }
  v15 = fg169t2_gg_function_3(base + 0);
  v16 = fg169t2_gg_function_4(base + 0);
  t45 = v16;
  v17 = ((float)((-(t45))));
  t46 = ((int64_t)(v0));
  t47 = ((int64_t)(((int64_t)UINT64_C(0))));
  t48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t49 = fg169t_di(t46, t47, t48);
  t50 = ((uint64_t)(t49));
  t51 = ((uint64_t)(((uint64_t)(v3))));
  t52 = ((uint64_t)(v1));
  t53 = ((uint64_t)(((uint64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t55 = ((uint64_t)(((uint64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  v18 = ((uint64_t)(((uint64_t)((uint64_t)(t50) + (uint64_t)(t55)))));
  t56 = ((int64_t)(v0));
  t57 = ((int64_t)(((int64_t)UINT64_C(0))));
  t58 = ((int64_t)(((int64_t)UINT64_C(1))));
  t59 = fg169t_di(t56, t57, t58);
  t60 = ((uint64_t)(t59));
  t61 = ((uint64_t)(((uint64_t)(v7))));
  t62 = ((uint64_t)(v1));
  t63 = ((uint64_t)(((uint64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t65 = ((uint64_t)(((uint64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  v19 = ((uint64_t)(((uint64_t)((uint64_t)(t60) + (uint64_t)(t65)))));
  t66 = ((int64_t)(v0));
  t67 = ((int64_t)(((int64_t)UINT64_C(0))));
  t68 = ((int64_t)(((int64_t)UINT64_C(2))));
  t69 = fg169t_di(t66, t67, t68);
  t70 = ((uint64_t)(t69));
  t71 = ((uint64_t)(((uint64_t)(v3))));
  t72 = ((uint64_t)(v1));
  t73 = ((uint64_t)(((uint64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t75 = ((uint64_t)(((uint64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  v20 = ((uint64_t)(((uint64_t)((uint64_t)(t70) + (uint64_t)(t75)))));
  t76 = ((int64_t)(v0));
  t77 = ((int64_t)(((int64_t)UINT64_C(0))));
  t78 = ((int64_t)(((int64_t)UINT64_C(3))));
  t79 = fg169t_di(t76, t77, t78);
  v21 = ((uint64_t)(t79));
  memset(&t80, 0, sizeof(t80));
  t81 = ((float)(t80));
  for (t82=0; t82<2; t82++) (v22).data[t82] = t81;
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t83 = ((int32_t)(v23));
  t84 = ((int32_t)(v4));
  if (!(((t83) < (t84)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t85 = v23;
  t86 = ((float *)(v22).data) + fg169t2_gg_index(t85, ((int64_t)UINT64_C(2)));
  (*t86) = ((float)(v15));
  t87 = v23;
  t88 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t87) + (uint64_t)(t88)));
  }
  goto label1;
label2:;
  fg169t_sync();
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t89 = ((int32_t)(v23));
  t90 = ((int32_t)(v4));
  if (!(((t89) < (t90)))) goto label5;
  {
  t91 = ((int32_t)(v23));
  t92 = ((int32_t)(((int64_t)UINT64_C(32))));
  t93 = ((int32_t)(((int32_t)((uint64_t)(t91) * (uint64_t)(t92)))));
  t94 = ((int32_t)(v2));
  v24 = ((int32_t)(((int32_t)((uint64_t)(t93) + (uint64_t)(t94)))));
  /* FG_DIRECT_SLICE_INDEX */
  t95 = v23;
  t96 = ((float *)(v22).data) + fg169t2_gg_index(t95, ((int64_t)UINT64_C(2)));
  t97 = ((int32_t)(v3));
  t98 = ((int32_t)(((int64_t)UINT64_C(32))));
  t99 = ((int32_t)(((int32_t)fg169t2_gg_mod(t97, t98))));
  t100 = ((int32_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) == (t100));
  t102 = t101;
  if (!t102) {
  t103 = ((int32_t)(v24));
  t104 = ((int32_t)(v3));
  t102 = ((t103) < (t104));
  }
  t105 = t102;
  if (t105) {
  {
  t106 = ((uint64_t)(v18));
  t107 = ((uint64_t)(((uint64_t)(v24))));
  t108 = fg169t_load(t106, t107);
  (*t96) = ((float)(t108));
  }
  } else {
  {
  (*t96) = ((float)(v15));
  }
  }
  t109 = v23;
  t110 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t109) + (uint64_t)(t110)));
  }
  goto label4;
label5:;
  fg169t_barrier();
  t111 = v12;
  t112 = (!(t111));
  if (t112) {
  {
  t113 = v9;
  if (t113) {
  {
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t114 = ((int32_t)(v23));
  t115 = ((int32_t)(v4));
  if (!(((t114) < (t115)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t116 = v23;
  t117 = ((float *)(v22).data) + fg169t2_gg_index(t116, ((int64_t)UINT64_C(2)));
  t118 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4607182418800017408))))));
  t119 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4607182418800017408))))));
  /* FG_DIRECT_SLICE_INDEX */
  t120 = v23;
  t121 = ((float *)(v22).data) + fg169t2_gg_index(t120, ((int64_t)UINT64_C(2)));
  t122 = (*t121);
  t123 = ((float)((-(t122))));
  t124 = fg169t_exp(t123);
  t125 = ((float)(t124));
  t126 = ((float)(((t119) + (t125))));
  (*t117) = ((float)(((t118) / (t126))));
  t127 = v23;
  t128 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t127) + (uint64_t)(t128)));
  }
  goto label7;
label8:;
  }
  } else {
  {
  t129 = v10;
  if (t129) {
  {
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t130 = ((int32_t)(v23));
  t131 = ((int32_t)(v4));
  if (!(((t130) < (t131)))) goto label11;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t132 = v23;
  t133 = ((float *)(v22).data) + fg169t2_gg_index(t132, ((int64_t)UINT64_C(2)));
  v25 = ((float)((*t133)));
  /* FG_DIRECT_SLICE_INDEX */
  t134 = v23;
  t135 = ((float *)(v22).data) + fg169t2_gg_index(t134, ((int64_t)UINT64_C(2)));
  t137 = ((float)(v25));
  t138 = ((float)(fg169t2_gg_double_bits(UINT64_C(4626322717216342016))));
  t139 = ((t137) > (t138));
  if (t139) {
  {
  t136 = ((float)(v25));
  }
  } else {
  {
  t140 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4607182418800017408))))));
  t141 = ((float)(v25));
  t142 = fg169t_exp(t141);
  t143 = ((float)(t142));
  t144 = ((float)(((t140) + (t143))));
  t145 = fg169t_log(t144);
  t136 = ((float)(t145));
  }
  }
  t146 = ((float)(t136));
  t147 = fg169t_sqrt(t146);
  (*t135) = ((float)(t147));
  t148 = v23;
  t149 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t148) + (uint64_t)(t149)));
  }
  goto label10;
label11:;
  }
  } else {
  {
  v26 = ((float)(v15));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label13:;
  t150 = ((int32_t)(v23));
  t151 = ((int32_t)(v4));
  if (!(((t150) < (t151)))) goto label14;
  {
  t152 = ((float)(v26));
  /* FG_DIRECT_SLICE_INDEX */
  t153 = v23;
  t154 = ((float *)(v22).data) + fg169t2_gg_index(t153, ((int64_t)UINT64_C(2)));
  t155 = ((float)((*t154)));
  t156 = fg169t_max(t152, t155);
  v26 = ((float)(t156));
  t157 = v23;
  t158 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t157) + (uint64_t)(t158)));
  }
  goto label13;
label14:;
  t159 = ((float)(v26));
  v26 = fg169t2_gg_function_12(t159, base + 0);
  v27 = ((float)(fg169t2_gg_double_bits(UINT64_C(0))));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label16:;
  t160 = ((int32_t)(v23));
  t161 = ((int32_t)(v4));
  if (!(((t160) < (t161)))) goto label17;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t162 = v23;
  t163 = ((float *)(v22).data) + fg169t2_gg_index(t162, ((int64_t)UINT64_C(2)));
  t164 = ((float)((*t163)));
  t165 = ((float)(v26));
  t166 = ((float)(((t164) - (t165))));
  t167 = fg169t_exp(t166);
  v28 = ((float)(t167));
  /* FG_DIRECT_SLICE_INDEX */
  t168 = v23;
  t169 = ((float *)(v22).data) + fg169t2_gg_index(t168, ((int64_t)UINT64_C(2)));
  (*t169) = ((float)(v28));
  t170 = v27;
  t171 = ((float)(v28));
  v27 = ((t170) + (t171));
  t172 = v23;
  t173 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t172) + (uint64_t)(t173)));
  }
  goto label16;
label17:;
  t174 = ((float)(v27));
  v27 = fg169t2_gg_function_15(t174, base + 0);
  t175 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4607182418800017408))))));
  t176 = ((float)(v27));
  v29 = ((float)(((t175) / (t176))));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label19:;
  t177 = ((int32_t)(v23));
  t178 = ((int32_t)(v4));
  if (!(((t177) < (t178)))) goto label20;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t179 = v23;
  t180 = ((float *)(v22).data) + fg169t2_gg_index(t179, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t181 = v23;
  t182 = ((float *)(v22).data) + fg169t2_gg_index(t181, ((int64_t)UINT64_C(2)));
  t183 = ((float)((*t182)));
  t184 = ((float)(v29));
  (*t180) = ((float)(((t183) * (t184))));
  t185 = v23;
  t186 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t185) + (uint64_t)(t186)));
  }
  goto label19;
label20:;
  }
  }
  }
  }
  }
  }
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label22:;
  t187 = ((int32_t)(v23));
  t188 = ((int32_t)(v4));
  if (!(((t187) < (t188)))) goto label23;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t189 = v23;
  t190 = ((float *)(v22).data) + fg169t2_gg_index(t189, ((int64_t)UINT64_C(2)));
  t191 = ((float)((*t190)));
  /* FG_DIRECT_SLICE_INDEX */
  t192 = v23;
  t193 = ((float *)(v22).data) + fg169t2_gg_index(t192, ((int64_t)UINT64_C(2)));
  t194 = ((float)((*t193)));
  t195 = ((t191) != (t194));
  if (t195) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t196 = v23;
  t197 = ((float *)(v22).data) + fg169t2_gg_index(t196, ((int64_t)UINT64_C(2)));
  (*t197) = ((float)(v17));
  }
  }
  t198 = v23;
  t199 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t198) + (uint64_t)(t199)));
  }
  goto label22;
label23:;
  memset(&t200, 0, sizeof(t200));
  t201 = ((float)(t200));
  for (t202=0; t202<2; t202++) (v30).data[t202] = t201;
  t203 = v5;
  if (t203) {
  {
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label25:;
  t204 = ((int32_t)(v23));
  t205 = ((int32_t)(v4));
  if (!(((t204) < (t205)))) goto label26;
  {
  t206 = ((int32_t)(v23));
  t207 = ((int32_t)(((int64_t)UINT64_C(32))));
  t208 = ((int32_t)(((int32_t)((uint64_t)(t206) * (uint64_t)(t207)))));
  t209 = ((int32_t)(v2));
  v31 = ((int32_t)(((int32_t)((uint64_t)(t208) + (uint64_t)(t209)))));
  /* FG_DIRECT_SLICE_INDEX */
  t210 = v23;
  t211 = ((float *)(v30).data) + fg169t2_gg_index(t210, ((int64_t)UINT64_C(2)));
  t212 = ((int32_t)(v3));
  t213 = ((int32_t)(((int64_t)UINT64_C(32))));
  t214 = ((int32_t)(((int32_t)fg169t2_gg_mod(t212, t213))));
  t215 = ((int32_t)(((int64_t)UINT64_C(0))));
  t216 = ((t214) == (t215));
  t217 = t216;
  if (!t217) {
  t218 = ((int32_t)(v31));
  t219 = ((int32_t)(v3));
  t217 = ((t218) < (t219));
  }
  t220 = t217;
  if (t220) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t221 = v23;
  t222 = ((float *)(v22).data) + fg169t2_gg_index(t221, ((int64_t)UINT64_C(2)));
  t223 = ((float)((*t222)));
  t224 = ((uint64_t)(v21));
  t225 = ((uint64_t)(((uint64_t)(v31))));
  t226 = fg169t_load(t224, t225);
  t227 = ((float)(t226));
  (*t211) = ((float)(((t223) + (t227))));
  }
  } else {
  {
  (*t211) = ((float)(v15));
  }
  }
  t228 = v23;
  t229 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t228) + (uint64_t)(t229)));
  }
  goto label25;
label26:;
  }
  }
  t230 = ((int32_t)(v13));
  t231 = ((int32_t)(((int64_t)UINT64_C(0))));
  t232 = ((t230) > (t231));
  if (t232) {
  {
  t233 = ((int32_t)(v3));
  t234 = ((int32_t)(v13));
  v32 = ((int32_t)(((int32_t)fg169t2_gg_div(t233, t234))));
  v33 = ((float)(v15));
  v34 = ((int32_t)(((int64_t)UINT64_C(0))));
label28:;
  t235 = ((int32_t)(v34));
  t236 = ((int32_t)(v13));
  if (!(((t235) < (t236)))) goto label29;
  {
  v35 = ((float)(v15));
  v36 = ((float)(v15));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label31:;
  t237 = ((int32_t)(v23));
  t238 = ((int32_t)(v4));
  if (!(((t237) < (t238)))) goto label32;
  {
  t239 = ((int32_t)(v23));
  t240 = ((int32_t)(((int64_t)UINT64_C(32))));
  t241 = ((int32_t)(((int32_t)((uint64_t)(t239) * (uint64_t)(t240)))));
  t242 = ((int32_t)(v2));
  t243 = ((int32_t)(((int32_t)((uint64_t)(t241) + (uint64_t)(t242)))));
  t244 = ((int32_t)(v32));
  t245 = ((int32_t)(((int32_t)fg169t2_gg_div(t243, t244))));
  t246 = ((int32_t)(v34));
  t247 = ((t245) == (t246));
  if (t247) {
  {
  t248 = v5;
  if (t248) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t249 = v23;
  t250 = ((float *)(v30).data) + fg169t2_gg_index(t249, ((int64_t)UINT64_C(2)));
  v37 = ((float)((*t250)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t251 = v23;
  t252 = ((float *)(v22).data) + fg169t2_gg_index(t251, ((int64_t)UINT64_C(2)));
  v37 = ((float)((*t252)));
  }
  }
  t253 = ((float)(v36));
  t254 = ((float)(v35));
  t255 = ((float)(v37));
  t256 = fg169t_fmin(t254, t255);
  t257 = ((float)(t256));
  t258 = fg169t_fmax(t253, t257);
  v36 = ((float)(t258));
  t259 = ((float)(v35));
  t260 = ((float)(v37));
  t261 = fg169t_fmax(t259, t260);
  v35 = ((float)(t261));
  }
  }
  t262 = v23;
  t263 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t262) + (uint64_t)(t263)));
  }
  goto label31;
label32:;
  v38 = ((int32_t)(((int64_t)UINT64_C(16))));
label34:;
  t264 = ((int32_t)(v38));
  t265 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t264) > (t265)))) goto label35;
  {
  t266 = ((float)(v35));
  t267 = ((int32_t)(v38));
  t268 = fg169t_shuffle(t266, t267);
  v39 = ((float)(t268));
  t269 = ((float)(v36));
  t270 = ((int32_t)(v38));
  t271 = fg169t_shuffle(t269, t270);
  v40 = ((float)(t271));
  t272 = ((float)(v35));
  t273 = ((float)(v39));
  t274 = fg169t_fmin(t272, t273);
  t275 = ((float)(t274));
  t276 = ((float)(v36));
  t277 = ((float)(v40));
  t278 = fg169t_fmax(t276, t277);
  t279 = ((float)(t278));
  t280 = fg169t_fmax(t275, t279);
  v36 = ((float)(t280));
  t281 = ((float)(v35));
  t282 = ((float)(v39));
  t283 = fg169t_fmax(t281, t282);
  v35 = ((float)(t283));
  t284 = v38;
  t285 = ((int32_t)(((int64_t)UINT64_C(1))));
  v38 = ((t284) >> ((uint64_t)(t285) & 31));
  }
  goto label34;
label35:;
  t286 = ((int32_t)(v2));
  t287 = ((int32_t)(v34));
  t288 = ((t286) == (t287));
  if (t288) {
  {
  t289 = ((float)(v35));
  t290 = ((float)(v36));
  v33 = ((float)(((t289) + (t290))));
  }
  }
  t291 = v34;
  t292 = ((int32_t)(((int64_t)UINT64_C(1))));
  v34 = ((int32_t)((uint64_t)(t291) + (uint64_t)(t292)));
  }
  goto label28;
label29:;
  v41 = ((int32_t)(((int64_t)UINT64_C(0))));
  v34 = ((int32_t)(((int64_t)UINT64_C(0))));
label37:;
  t293 = ((int32_t)(v34));
  t294 = ((int32_t)(v13));
  if (!(((t293) < (t294)))) goto label38;
  {
  t295 = ((float)(v33));
  t296 = ((int32_t)(v34));
  t297 = fg169t_shufflei(t295, t296);
  v42 = ((float)(t297));
  t298 = ((float)(v42));
  t299 = ((float)(v33));
  t300 = ((t298) > (t299));
  t301 = t300;
  if (!t301) {
  t302 = ((float)(v42));
  t303 = ((float)(v33));
  t304 = ((t302) == (t303));
  t305 = t304;
  if (t305) {
  t306 = ((int32_t)(v34));
  t307 = ((int32_t)(v2));
  t305 = ((t306) < (t307));
  }
  t301 = t305;
  }
  t308 = t301;
  if (t308) {
  {
  t309 = v41;
  t310 = ((int32_t)(((int64_t)UINT64_C(1))));
  v41 = ((int32_t)((uint64_t)(t309) + (uint64_t)(t310)));
  }
  }
  t311 = v34;
  t312 = ((int32_t)(((int64_t)UINT64_C(1))));
  v34 = ((int32_t)((uint64_t)(t311) + (uint64_t)(t312)));
  }
  goto label37;
label38:;
  t313 = ((int32_t)(v2));
  t314 = ((int32_t)(v13));
  t315 = ((t313) < (t314));
  t316 = t315;
  if (t316) {
  t317 = ((int32_t)(v41));
  t318 = ((int32_t)(v14));
  t316 = ((t317) < (t318));
  }
  t319 = t316;
  t320 = fg169t_ballot(t319);
  v43 = ((uint64_t)(t320));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label40:;
  t321 = ((int32_t)(v23));
  t322 = ((int32_t)(v4));
  if (!(((t321) < (t322)))) goto label41;
  {
  t323 = ((int32_t)(v23));
  t324 = ((int32_t)(((int64_t)UINT64_C(32))));
  t325 = ((int32_t)(((int32_t)((uint64_t)(t323) * (uint64_t)(t324)))));
  t326 = ((int32_t)(v2));
  t327 = ((int32_t)(((int32_t)((uint64_t)(t325) + (uint64_t)(t326)))));
  t328 = ((int32_t)(v32));
  v44 = ((int32_t)(((int32_t)fg169t2_gg_div(t327, t328))));
  t329 = ((int32_t)(v44));
  t330 = ((int32_t)(v13));
  t331 = ((t329) >= (t330));
  t332 = t331;
  if (!t332) {
  t333 = ((uint64_t)(v43));
  t334 = ((uint64_t)(((uint64_t)(v44))));
  t335 = ((uint64_t)(((t333) >> ((uint64_t)(t334) & 63))));
  t336 = ((uint64_t)(((int64_t)UINT64_C(2))));
  t337 = ((uint64_t)(((uint64_t)fg169t2_gg_umod(t335, t336))));
  t338 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t332 = ((t337) == (t338));
  }
  t339 = t332;
  if (t339) {
  {
  t340 = v5;
  if (t340) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t341 = v23;
  t342 = ((float *)(v30).data) + fg169t2_gg_index(t341, ((int64_t)UINT64_C(2)));
  (*t342) = ((float)(v15));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t343 = v23;
  t344 = ((float *)(v22).data) + fg169t2_gg_index(t343, ((int64_t)UINT64_C(2)));
  (*t344) = ((float)(v15));
  }
  }
  }
  }
  t345 = v23;
  t346 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t345) + (uint64_t)(t346)));
  }
  goto label40;
label41:;
  }
  }
  v45 = ((float)(fg169t2_gg_double_bits(UINT64_C(0))));
  memset(&t347, 0, sizeof(t347));
  t348 = ((float)(t347));
  for (t349=0; t349<2; t349++) (v46).data[t349] = t348;
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label43:;
  t350 = ((int32_t)(v23));
  t351 = ((int32_t)(v4));
  if (!(((t350) < (t351)))) goto label44;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t352 = v23;
  t353 = ((float *)(v46).data) + fg169t2_gg_index(t352, ((int64_t)UINT64_C(2)));
  (*t353) = ((float)(fg169t2_gg_double_bits(UINT64_C(0))));
  t354 = v23;
  t355 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t354) + (uint64_t)(t355)));
  }
  goto label43;
label44:;
  fg169t_lc();
  v47 = ((int32_t)(((int64_t)UINT64_C(0))));
label46:;
  t356 = ((int32_t)(v47));
  t357 = ((int32_t)(v7));
  if (!(((t356) < (t357)))) goto label47;
  {
  t358 = ((int32_t)(v2));
  t359 = ((int32_t)(v3));
  t360 = ((t358) < (t359));
  if (t360) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t361 = ((int64_t)UINT64_C(0));
  t362 = ((float *)(v22).data) + fg169t2_gg_index(t361, ((int64_t)UINT64_C(2)));
  v48 = ((float)((*t362)));
  }
  } else {
  {
  v48 = ((float)(v15));
  }
  }
  t363 = ((int32_t)(v2));
  t364 = ((int32_t)(v3));
  t365 = ((t363) < (t364));
  if (t365) {
  {
  t366 = v5;
  if (t366) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t367 = ((int64_t)UINT64_C(0));
  t368 = ((float *)(v30).data) + fg169t2_gg_index(t367, ((int64_t)UINT64_C(2)));
  v49 = ((float)((*t368)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t369 = ((int64_t)UINT64_C(0));
  t370 = ((float *)(v22).data) + fg169t2_gg_index(t369, ((int64_t)UINT64_C(2)));
  v49 = ((float)((*t370)));
  }
  }
  }
  } else {
  {
  v49 = ((float)(v15));
  }
  }
  v50 = ((int32_t)(v2));
  v23 = ((int32_t)(((int64_t)UINT64_C(1))));
label49:;
  t371 = ((int32_t)(v23));
  t372 = ((int32_t)(v4));
  if (!(((t371) < (t372)))) goto label50;
  {
  t373 = ((int32_t)(v2));
  t374 = ((int32_t)(v23));
  t375 = ((int32_t)(((int64_t)UINT64_C(32))));
  t376 = ((int32_t)(((int32_t)((uint64_t)(t374) * (uint64_t)(t375)))));
  v51 = ((int32_t)(((int32_t)((uint64_t)(t373) + (uint64_t)(t376)))));
  t377 = v5;
  if (t377) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t378 = v23;
  t379 = ((float *)(v30).data) + fg169t2_gg_index(t378, ((int64_t)UINT64_C(2)));
  v52 = ((float)((*t379)));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t380 = v23;
  t381 = ((float *)(v22).data) + fg169t2_gg_index(t380, ((int64_t)UINT64_C(2)));
  v52 = ((float)((*t381)));
  }
  }
  t382 = ((int32_t)(v3));
  t383 = ((int32_t)(((int64_t)UINT64_C(32))));
  t384 = ((int32_t)(((int32_t)fg169t2_gg_mod(t382, t383))));
  t385 = ((int32_t)(((int64_t)UINT64_C(0))));
  t386 = ((t384) == (t385));
  t387 = t386;
  if (!t387) {
  t388 = ((int32_t)(v51));
  t389 = ((int32_t)(v3));
  t387 = ((t388) < (t389));
  }
  t390 = t387;
  t391 = t390;
  if (t391) {
  t392 = ((float)(v52));
  t393 = ((float)(v49));
  t391 = ((t392) > (t393));
  }
  t394 = t391;
  if (t394) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t395 = v23;
  t396 = ((float *)(v22).data) + fg169t2_gg_index(t395, ((int64_t)UINT64_C(2)));
  v48 = ((float)((*t396)));
  v49 = ((float)(v52));
  v50 = ((int32_t)(v51));
  }
  }
  t397 = v23;
  t398 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t397) + (uint64_t)(t398)));
  }
  goto label49;
label50:;
  v53 = ((int32_t)(((int64_t)UINT64_C(16))));
label52:;
  t399 = ((int32_t)(v53));
  t400 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t399) > (t400)))) goto label53;
  {
  t401 = ((float)(v48));
  t402 = ((int32_t)(v53));
  t403 = fg169t_shuffle(t401, t402);
  v54 = ((float)(t403));
  t404 = ((float)(v49));
  t405 = ((int32_t)(v53));
  t406 = fg169t_shuffle(t404, t405);
  v55 = ((float)(t406));
  t407 = ((int32_t)(v50));
  t408 = ((int32_t)(v53));
  t409 = fg169t_ishuffle(t407, t408);
  v56 = ((int32_t)(t409));
  t410 = ((float)(v55));
  t411 = ((float)(v49));
  t412 = ((t410) > (t411));
  t413 = t412;
  if (!t413) {
  t414 = ((float)(v55));
  t415 = ((float)(v49));
  t416 = ((t414) == (t415));
  t417 = t416;
  if (t417) {
  t418 = ((int32_t)(v56));
  t419 = ((int32_t)(v50));
  t417 = ((t418) < (t419));
  }
  t413 = t417;
  }
  t420 = t413;
  if (t420) {
  {
  v48 = ((float)(v54));
  v49 = ((float)(v55));
  v50 = ((int32_t)(v56));
  }
  }
  t421 = v53;
  t422 = ((int32_t)(((int64_t)UINT64_C(1))));
  v53 = ((t421) >> ((uint64_t)(t422) & 31));
  }
  goto label52;
label53:;
  t423 = ((int32_t)(v50));
  t424 = ((int32_t)(((int64_t)UINT64_C(32))));
  t425 = ((int32_t)(((int32_t)fg169t2_gg_mod(t423, t424))));
  t426 = ((int32_t)(v2));
  t427 = ((t425) == (t426));
  if (t427) {
  {
  t428 = v5;
  if (t428) {
  {
  t429 = fg169t2_gg_view((v30).data, 2);
  t430 = ((int32_t)(v50));
  t431 = ((int32_t)(((int64_t)UINT64_C(32))));
  t432 = ((int32_t)fg169t2_gg_div(t430, t431));
  t433 = ((float *)t429.data) + fg169t2_gg_index(t432, t429.len);
  (*t433) = ((float)(v15));
  }
  } else {
  {
  t434 = fg169t2_gg_view((v22).data, 2);
  t435 = ((int32_t)(v50));
  t436 = ((int32_t)(((int64_t)UINT64_C(32))));
  t437 = ((int32_t)fg169t2_gg_div(t435, t436));
  t438 = ((float *)t434.data) + fg169t2_gg_index(t437, t434.len);
  (*t438) = ((float)(v15));
  }
  }
  }
  }
  t439 = ((int32_t)(v47));
  t440 = ((int32_t)(((int64_t)UINT64_C(32))));
  t441 = ((int32_t)(((int32_t)fg169t2_gg_mod(t439, t440))));
  t442 = ((int32_t)(v2));
  t443 = ((t441) == (t442));
  if (t443) {
  {
  t444 = fg169t2_gg_view((v46).data, 2);
  t445 = ((int32_t)(v47));
  t446 = ((int32_t)(((int64_t)UINT64_C(32))));
  t447 = ((int32_t)fg169t2_gg_div(t445, t446));
  t448 = ((float *)t444.data) + fg169t2_gg_index(t447, t444.len);
  (*t448) = ((float)(v48));
  }
  }
  t449 = ((int32_t)(v50));
  t450 = ((int32_t)(((int64_t)UINT64_C(32))));
  t451 = ((int32_t)(((int32_t)fg169t2_gg_mod(t449, t450))));
  t452 = ((int32_t)(v2));
  t453 = ((t451) == (t452));
  if (t453) {
  {
  t454 = ((uint64_t)(v20));
  t455 = ((uint64_t)(((uint64_t)(v47))));
  t456 = ((int32_t)(v50));
  fg169t_storei(t454, t455, t456);
  t457 = v11;
  if (t457) {
  {
  t458 = v45;
  t459 = ((float)(v48));
  v45 = ((t458) + (t459));
  }
  }
  }
  }
  t460 = v47;
  t461 = ((int32_t)(((int64_t)UINT64_C(1))));
  v47 = ((int32_t)((uint64_t)(t460) + (uint64_t)(t461)));
  }
  goto label46;
label47:;
  t462 = v11;
  if (t462) {
  {
  t463 = ((float)(v45));
  v45 = fg169t2_gg_function_15(t463, base + 0);
  t464 = ((int64_t)(v0));
  t465 = ((int64_t)(((int64_t)UINT64_C(0))));
  t466 = fg169t_df(t464, t465);
  v57 = ((float)(t466));
  t467 = ((float)(v45));
  t468 = ((float)(v57));
  t469 = fg169t_max(t467, t468);
  v58 = ((float)(t469));
  t470 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4607182418800017408))))));
  t471 = ((float)(v58));
  v59 = ((float)(((t470) / (t471))));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label55:;
  t472 = ((int32_t)(v23));
  t473 = ((int32_t)(v4));
  if (!(((t472) < (t473)))) goto label56;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t474 = v23;
  t475 = ((float *)(v46).data) + fg169t2_gg_index(t474, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t476 = v23;
  t477 = ((float *)(v46).data) + fg169t2_gg_index(t476, ((int64_t)UINT64_C(2)));
  t478 = ((float)((*t477)));
  t479 = ((float)(v59));
  (*t475) = ((float)(((t478) * (t479))));
  t480 = v23;
  t481 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t480) + (uint64_t)(t481)));
  }
  goto label55;
label56:;
  }
  }
  t482 = v12;
  if (t482) {
  {
  v60 = ((float)(v15));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label58:;
  t483 = ((int32_t)(v23));
  t484 = ((int32_t)(v4));
  if (!(((t483) < (t484)))) goto label59;
  {
  t485 = ((int32_t)(v2));
  t486 = ((int32_t)(v23));
  t487 = ((int32_t)(((int64_t)UINT64_C(32))));
  t488 = ((int32_t)(((int32_t)((uint64_t)(t486) * (uint64_t)(t487)))));
  v61 = ((int32_t)(((int32_t)((uint64_t)(t485) + (uint64_t)(t488)))));
  t489 = ((int32_t)(v61));
  t490 = ((int32_t)(v7));
  t491 = ((t489) < (t490));
  if (t491) {
  {
  t492 = ((float)(v60));
  /* FG_DIRECT_SLICE_INDEX */
  t493 = v23;
  t494 = ((float *)(v46).data) + fg169t2_gg_index(t493, ((int64_t)UINT64_C(2)));
  t495 = ((float)((*t494)));
  t496 = fg169t_max(t492, t495);
  v60 = ((float)(t496));
  }
  }
  t497 = v23;
  t498 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t497) + (uint64_t)(t498)));
  }
  goto label58;
label59:;
  t499 = ((float)(v60));
  v60 = fg169t2_gg_function_12(t499, base + 0);
  v62 = ((float)(fg169t2_gg_double_bits(UINT64_C(0))));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label61:;
  t500 = ((int32_t)(v23));
  t501 = ((int32_t)(v4));
  if (!(((t500) < (t501)))) goto label62;
  {
  t502 = ((int32_t)(v2));
  t503 = ((int32_t)(v23));
  t504 = ((int32_t)(((int64_t)UINT64_C(32))));
  t505 = ((int32_t)(((int32_t)((uint64_t)(t503) * (uint64_t)(t504)))));
  v63 = ((int32_t)(((int32_t)((uint64_t)(t502) + (uint64_t)(t505)))));
  t506 = ((int32_t)(v63));
  t507 = ((int32_t)(v7));
  t508 = ((t506) < (t507));
  if (t508) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t509 = v23;
  t510 = ((float *)(v46).data) + fg169t2_gg_index(t509, ((int64_t)UINT64_C(2)));
  t511 = ((float)((*t510)));
  t512 = ((float)(v60));
  t513 = ((float)(((t511) - (t512))));
  t514 = fg169t_exp(t513);
  v64 = ((float)(t514));
  /* FG_DIRECT_SLICE_INDEX */
  t515 = v23;
  t516 = ((float *)(v46).data) + fg169t2_gg_index(t515, ((int64_t)UINT64_C(2)));
  (*t516) = ((float)(v64));
  t517 = v62;
  t518 = ((float)(v64));
  v62 = ((t517) + (t518));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t519 = v23;
  t520 = ((float *)(v46).data) + fg169t2_gg_index(t519, ((int64_t)UINT64_C(2)));
  (*t520) = ((float)(fg169t2_gg_double_bits(UINT64_C(0))));
  }
  }
  t521 = v23;
  t522 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t521) + (uint64_t)(t522)));
  }
  goto label61;
label62:;
  t523 = ((float)(v62));
  v62 = fg169t2_gg_function_15(t523, base + 0);
  t524 = ((float)(((float)(fg169t2_gg_double_bits(UINT64_C(4607182418800017408))))));
  t525 = ((float)(v62));
  v65 = ((float)(((t524) / (t525))));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label64:;
  t526 = ((int32_t)(v23));
  t527 = ((int32_t)(v4));
  if (!(((t526) < (t527)))) goto label65;
  {
  t528 = ((int32_t)(v2));
  t529 = ((int32_t)(v23));
  t530 = ((int32_t)(((int64_t)UINT64_C(32))));
  t531 = ((int32_t)(((int32_t)((uint64_t)(t529) * (uint64_t)(t530)))));
  v66 = ((int32_t)(((int32_t)((uint64_t)(t528) + (uint64_t)(t531)))));
  t532 = ((int32_t)(v66));
  t533 = ((int32_t)(v7));
  t534 = ((t532) < (t533));
  if (t534) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t535 = v23;
  t536 = ((float *)(v46).data) + fg169t2_gg_index(t535, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t537 = v23;
  t538 = ((float *)(v46).data) + fg169t2_gg_index(t537, ((int64_t)UINT64_C(2)));
  t539 = ((float)((*t538)));
  t540 = ((float)(v65));
  (*t536) = ((float)(((t539) * (t540))));
  }
  }
  t541 = v23;
  t542 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t541) + (uint64_t)(t542)));
  }
  goto label64;
label65:;
  }
  }
  t543 = ((int64_t)(v0));
  t544 = ((int64_t)(((int64_t)UINT64_C(1))));
  t545 = fg169t_df(t543, t544);
  v67 = ((float)(t545));
  v23 = ((int32_t)(((int64_t)UINT64_C(0))));
label67:;
  t546 = ((int32_t)(v23));
  t547 = ((int32_t)(v4));
  if (!(((t546) < (t547)))) goto label68;
  {
  t548 = ((int32_t)(v23));
  t549 = ((int32_t)(((int64_t)UINT64_C(32))));
  t550 = ((int32_t)(((int32_t)((uint64_t)(t548) * (uint64_t)(t549)))));
  t551 = ((int32_t)(v2));
  v68 = ((int32_t)(((int32_t)((uint64_t)(t550) + (uint64_t)(t551)))));
  t552 = ((int32_t)(v68));
  t553 = ((int32_t)(v7));
  t554 = ((t552) < (t553));
  if (t554) {
  {
  t555 = ((uint64_t)(v19));
  t556 = ((uint64_t)(((uint64_t)(v68))));
  /* FG_DIRECT_SLICE_INDEX */
  t557 = v23;
  t558 = ((float *)(v46).data) + fg169t2_gg_index(t557, ((int64_t)UINT64_C(2)));
  t559 = ((float)((*t558)));
  t560 = ((float)(v67));
  t561 = ((float)(((t559) * (t560))));
  fg169t_store(t555, t556, t561);
  }
  }
  t562 = v23;
  t563 = ((int32_t)(((int64_t)UINT64_C(1))));
  v23 = ((int32_t)((uint64_t)(t562) + (uint64_t)(t563)));
  }
  goto label67;
label68:;
  }
fg169t2_gg_done:;
}
#define FG169T_LANE_2 fg169t2_gg_function_1
