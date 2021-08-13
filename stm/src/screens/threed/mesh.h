// count = 168
#include "../threed.h"

namespace threed {

struct DefaultMeshHolder {
Tri tris[168];
const static inline size_t tri_count = 168;

constexpr DefaultMeshHolder() : tris{} {
tris[0] = {
    { m::fixed_t{-17696, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41716, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    204,
    78,
    0
};

tris[1] = {
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    204,
    78,
    0
};

tris[2] = {
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{11824, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41714, nullptr} },
    204,
    78,
    0
};

tris[3] = {
    { m::fixed_t{11824, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41714, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    204,
    78,
    0
};

tris[4] = {
    { m::fixed_t{12467, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    204,
    78,
    0
};

tris[5] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    204,
    78,
    0
};

tris[6] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    204,
    78,
    0
};

tris[7] = {
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6222, nullptr} },
    204,
    78,
    0
};

tris[8] = {
    { m::fixed_t{556, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6222, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{-2366, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    204,
    78,
    0
};

tris[9] = {
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6222, nullptr} },
    204,
    78,
    0
};

tris[10] = {
    { m::fixed_t{-2366, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{-3636, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    204,
    78,
    0
};

tris[11] = {
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    204,
    78,
    0
};

tris[12] = {
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    204,
    78,
    0
};

tris[13] = {
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    204,
    78,
    0
};

tris[14] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    { m::fixed_t{-18352, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    204,
    78,
    0
};

tris[15] = {
    { m::fixed_t{-18352, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    { m::fixed_t{-17696, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41716, nullptr} },
    204,
    78,
    0
};

tris[16] = {
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[17] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    204,
    78,
    0
};

tris[18] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43113, nullptr} },
    204,
    78,
    0
};

tris[19] = {
    { m::fixed_t{12467, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43113, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    { m::fixed_t{11824, nullptr}, m::fixed_t{-11279, nullptr}, m::fixed_t{-42674, nullptr} },
    204,
    78,
    0
};

tris[20] = {
    { m::fixed_t{11824, nullptr}, m::fixed_t{-11279, nullptr}, m::fixed_t{-42674, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    204,
    78,
    0
};

tris[21] = {
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-17696, nullptr}, m::fixed_t{-11278, nullptr}, m::fixed_t{-42676, nullptr} },
    204,
    78,
    0
};

tris[22] = {
    { m::fixed_t{-17696, nullptr}, m::fixed_t{-11278, nullptr}, m::fixed_t{-42676, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-18352, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43114, nullptr} },
    204,
    78,
    0
};

tris[23] = {
    { m::fixed_t{-18352, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43114, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    204,
    78,
    0
};

tris[24] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[25] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[26] = {
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    204,
    78,
    0
};

tris[27] = {
    { m::fixed_t{-6558, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{-3636, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    204,
    78,
    0
};

tris[28] = {
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    204,
    78,
    0
};

tris[29] = {
    { m::fixed_t{-3636, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{-2366, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    204,
    78,
    0
};

tris[30] = {
    { m::fixed_t{-18352, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    { m::fixed_t{-17696, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41716, nullptr} },
    { m::fixed_t{-17696, nullptr}, m::fixed_t{-11278, nullptr}, m::fixed_t{-42676, nullptr} },
    204,
    78,
    0
};

tris[31] = {
    { m::fixed_t{13233, nullptr}, m::fixed_t{9895, nullptr}, m::fixed_t{-42316, nullptr} },
    { m::fixed_t{13233, nullptr}, m::fixed_t{-11251, nullptr}, m::fixed_t{-43276, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43113, nullptr} },
    204,
    78,
    0
};

tris[32] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    204,
    78,
    0
};

tris[33] = {
    { m::fixed_t{-18798, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[34] = {
    { m::fixed_t{12973, nullptr}, m::fixed_t{-11867, nullptr}, m::fixed_t{-29710, nullptr} },
    { m::fixed_t{12973, nullptr}, m::fixed_t{9279, nullptr}, m::fixed_t{-28750, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    204,
    78,
    0
};

tris[35] = {
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    204,
    78,
    0
};

tris[36] = {
    { m::fixed_t{-3636, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    204,
    78,
    0
};

tris[37] = {
    { m::fixed_t{-2366, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-3636, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-3636, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    204,
    78,
    0
};

tris[38] = {
    { m::fixed_t{556, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6222, nullptr} },
    { m::fixed_t{-2366, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-2366, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    204,
    78,
    0
};

tris[39] = {
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6222, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    204,
    78,
    0
};

tris[40] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[41] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[42] = {
    { m::fixed_t{11824, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41714, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43113, nullptr} },
    204,
    78,
    0
};

tris[43] = {
    { m::fixed_t{-17696, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41716, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    204,
    78,
    0
};

tris[44] = {
    { m::fixed_t{-1467, nullptr}, m::fixed_t{-12387, nullptr}, m::fixed_t{-18260, nullptr} },
    { m::fixed_t{-1467, nullptr}, m::fixed_t{8759, nullptr}, m::fixed_t{-17300, nullptr} },
    { m::fixed_t{11302, nullptr}, m::fixed_t{-11313, nullptr}, m::fixed_t{-41916, nullptr} },
    204,
    78,
    0
};

tris[45] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{-18352, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    { m::fixed_t{-18352, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43114, nullptr} },
    204,
    78,
    0
};

tris[46] = {
    { m::fixed_t{-18842, nullptr}, m::fixed_t{9277, nullptr}, m::fixed_t{-28704, nullptr} },
    { m::fixed_t{-18842, nullptr}, m::fixed_t{-11869, nullptr}, m::fixed_t{-29664, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    204,
    78,
    0
};

tris[47] = {
    { m::fixed_t{-18352, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    { m::fixed_t{-17696, nullptr}, m::fixed_t{-11278, nullptr}, m::fixed_t{-42676, nullptr} },
    { m::fixed_t{-18352, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43114, nullptr} },
    204,
    78,
    0
};

tris[48] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{14174, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    204,
    78,
    0
};

tris[49] = {
    { m::fixed_t{14174, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{14174, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{13233, nullptr}, m::fixed_t{9895, nullptr}, m::fixed_t{-42316, nullptr} },
    204,
    78,
    0
};

tris[50] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{14174, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{14174, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    204,
    78,
    0
};

tris[51] = {
    { m::fixed_t{13233, nullptr}, m::fixed_t{9895, nullptr}, m::fixed_t{-42316, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43113, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{9888, nullptr}, m::fixed_t{-42153, nullptr} },
    204,
    78,
    0
};

tris[52] = {
    { m::fixed_t{13233, nullptr}, m::fixed_t{-11251, nullptr}, m::fixed_t{-43276, nullptr} },
    { m::fixed_t{13233, nullptr}, m::fixed_t{9895, nullptr}, m::fixed_t{-42316, nullptr} },
    { m::fixed_t{14174, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    204,
    78,
    0
};

tris[53] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[54] = {
    { m::fixed_t{-18798, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[55] = {
    { m::fixed_t{12973, nullptr}, m::fixed_t{9279, nullptr}, m::fixed_t{-28750, nullptr} },
    { m::fixed_t{12973, nullptr}, m::fixed_t{-11867, nullptr}, m::fixed_t{-29710, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{9200, nullptr}, m::fixed_t{-27018, nullptr} },
    204,
    78,
    0
};

tris[56] = {
    { m::fixed_t{12914, nullptr}, m::fixed_t{9200, nullptr}, m::fixed_t{-27018, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-11946, nullptr}, m::fixed_t{-27978, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    204,
    78,
    0
};

tris[57] = {
    { m::fixed_t{12973, nullptr}, m::fixed_t{-11867, nullptr}, m::fixed_t{-29710, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-11946, nullptr}, m::fixed_t{-27978, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{9200, nullptr}, m::fixed_t{-27018, nullptr} },
    204,
    78,
    0
};

tris[58] = {
    { m::fixed_t{12914, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-11946, nullptr}, m::fixed_t{-27978, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[59] = {
    { m::fixed_t{12973, nullptr}, m::fixed_t{-11867, nullptr}, m::fixed_t{-29710, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    204,
    78,
    0
};

tris[60] = {
    { m::fixed_t{-6558, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6223, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    204,
    78,
    0
};

tris[61] = {
    { m::fixed_t{-3636, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-6558, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{-3636, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    204,
    78,
    0
};

tris[62] = {
    { m::fixed_t{-2366, nullptr}, m::fixed_t{8174, nullptr}, m::fixed_t{-4409, nullptr} },
    { m::fixed_t{-3636, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    { m::fixed_t{-2366, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    204,
    78,
    0
};

tris[63] = {
    { m::fixed_t{556, nullptr}, m::fixed_t{8256, nullptr}, m::fixed_t{-6222, nullptr} },
    { m::fixed_t{-2366, nullptr}, m::fixed_t{-12973, nullptr}, m::fixed_t{-5370, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    204,
    78,
    0
};

tris[64] = {
    { m::fixed_t{13025, nullptr}, m::fixed_t{9309, nullptr}, m::fixed_t{-29415, nullptr} },
    { m::fixed_t{556, nullptr}, m::fixed_t{-12890, nullptr}, m::fixed_t{-7183, nullptr} },
    { m::fixed_t{13025, nullptr}, m::fixed_t{-11837, nullptr}, m::fixed_t{-30376, nullptr} },
    204,
    78,
    0
};

tris[65] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{12914, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    204,
    78,
    0
};

tris[66] = {
    { m::fixed_t{20422, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{20422, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    204,
    78,
    0
};

tris[67] = {
    { m::fixed_t{11824, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41714, nullptr} },
    { m::fixed_t{12467, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43113, nullptr} },
    { m::fixed_t{11824, nullptr}, m::fixed_t{-11279, nullptr}, m::fixed_t{-42674, nullptr} },
    204,
    78,
    0
};

tris[68] = {
    { m::fixed_t{-17696, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41716, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    { m::fixed_t{-17696, nullptr}, m::fixed_t{-11278, nullptr}, m::fixed_t{-42676, nullptr} },
    204,
    78,
    0
};

tris[69] = {
    { m::fixed_t{11824, nullptr}, m::fixed_t{9868, nullptr}, m::fixed_t{-41714, nullptr} },
    { m::fixed_t{11824, nullptr}, m::fixed_t{-11279, nullptr}, m::fixed_t{-42674, nullptr} },
    { m::fixed_t{11302, nullptr}, m::fixed_t{9833, nullptr}, m::fixed_t{-40955, nullptr} },
    204,
    78,
    0
};

tris[70] = {
    { m::fixed_t{11302, nullptr}, m::fixed_t{9833, nullptr}, m::fixed_t{-40955, nullptr} },
    { m::fixed_t{11302, nullptr}, m::fixed_t{-11313, nullptr}, m::fixed_t{-41916, nullptr} },
    { m::fixed_t{-1467, nullptr}, m::fixed_t{8759, nullptr}, m::fixed_t{-17300, nullptr} },
    204,
    78,
    0
};

tris[71] = {
    { m::fixed_t{11824, nullptr}, m::fixed_t{-11279, nullptr}, m::fixed_t{-42674, nullptr} },
    { m::fixed_t{11302, nullptr}, m::fixed_t{-11313, nullptr}, m::fixed_t{-41916, nullptr} },
    { m::fixed_t{11302, nullptr}, m::fixed_t{9833, nullptr}, m::fixed_t{-40955, nullptr} },
    204,
    78,
    0
};

tris[72] = {
    { m::fixed_t{-2401, nullptr}, m::fixed_t{8670, nullptr}, m::fixed_t{-15330, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{8603, nullptr}, m::fixed_t{-13872, nullptr} },
    204,
    78,
    0
};

tris[73] = {
    { m::fixed_t{-1467, nullptr}, m::fixed_t{8759, nullptr}, m::fixed_t{-17300, nullptr} },
    { m::fixed_t{-2401, nullptr}, m::fixed_t{-12477, nullptr}, m::fixed_t{-16290, nullptr} },
    { m::fixed_t{-2401, nullptr}, m::fixed_t{8670, nullptr}, m::fixed_t{-15330, nullptr} },
    204,
    78,
    0
};

tris[74] = {
    { m::fixed_t{-2401, nullptr}, m::fixed_t{8670, nullptr}, m::fixed_t{-15330, nullptr} },
    { m::fixed_t{-2401, nullptr}, m::fixed_t{-12477, nullptr}, m::fixed_t{-16290, nullptr} },
    { m::fixed_t{-3000, nullptr}, m::fixed_t{-12543, nullptr}, m::fixed_t{-14832, nullptr} },
    204,
    78,
    0
};

tris[75] = {
    { m::fixed_t{-2401, nullptr}, m::fixed_t{-12477, nullptr}, m::fixed_t{-16290, nullptr} },
    { m::fixed_t{-1467, nullptr}, m::fixed_t{8759, nullptr}, m::fixed_t{-17300, nullptr} },
    { m::fixed_t{-1467, nullptr}, m::fixed_t{-12387, nullptr}, m::fixed_t{-18260, nullptr} },
    204,
    78,
    0
};

tris[76] = {
    { m::fixed_t{-26306, nullptr}, m::fixed_t{9896, nullptr}, m::fixed_t{-42346, nullptr} },
    { m::fixed_t{-18352, nullptr}, m::fixed_t{-11259, nullptr}, m::fixed_t{-43114, nullptr} },
    { m::fixed_t{-26306, nullptr}, m::fixed_t{-11250, nullptr}, m::fixed_t{-43306, nullptr} },
    204,
    78,
    0
};

tris[77] = {
    { m::fixed_t{-18798, nullptr}, m::fixed_t{7940, nullptr}, m::fixed_t{742, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{9200, nullptr}, m::fixed_t{-27017, nullptr} },
    204,
    78,
    0
};

tris[78] = {
    { m::fixed_t{-18798, nullptr}, m::fixed_t{9200, nullptr}, m::fixed_t{-27017, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-11946, nullptr}, m::fixed_t{-27977, nullptr} },
    { m::fixed_t{-18842, nullptr}, m::fixed_t{9277, nullptr}, m::fixed_t{-28704, nullptr} },
    204,
    78,
    0
};

tris[79] = {
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-13206, nullptr}, m::fixed_t{-218, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-11946, nullptr}, m::fixed_t{-27977, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{9200, nullptr}, m::fixed_t{-27017, nullptr} },
    204,
    78,
    0
};

tris[80] = {
    { m::fixed_t{-18842, nullptr}, m::fixed_t{9277, nullptr}, m::fixed_t{-28704, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{-11841, nullptr}, m::fixed_t{-30294, nullptr} },
    { m::fixed_t{-18887, nullptr}, m::fixed_t{9306, nullptr}, m::fixed_t{-29334, nullptr} },
    204,
    78,
    0
};

tris[81] = {
    { m::fixed_t{-18842, nullptr}, m::fixed_t{-11869, nullptr}, m::fixed_t{-29664, nullptr} },
    { m::fixed_t{-18842, nullptr}, m::fixed_t{9277, nullptr}, m::fixed_t{-28704, nullptr} },
    { m::fixed_t{-18798, nullptr}, m::fixed_t{-11946, nullptr}, m::fixed_t{-27977, nullptr} },
    204,
    78,
    0
};

tris[82] = {
    { m::fixed_t{-32499, nullptr}, m::fixed_t{-11863, nullptr}, m::fixed_t{-29808, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    3,
    75,
    162
};

tris[83] = {
    { m::fixed_t{-32294, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    3,
    75,
    162
};

tris[84] = {
    { m::fixed_t{-46338, nullptr}, m::fixed_t{-11066, nullptr}, m::fixed_t{-47361, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{-11162, nullptr}, m::fixed_t{-45252, nullptr} },
    3,
    75,
    162
};

tris[85] = {
    { m::fixed_t{-47801, nullptr}, m::fixed_t{-11162, nullptr}, m::fixed_t{-45252, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{-11289, nullptr}, m::fixed_t{-42436, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{-11384, nullptr}, m::fixed_t{-40342, nullptr} },
    3,
    75,
    162
};

tris[86] = {
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{-11766, nullptr}, m::fixed_t{-31944, nullptr} },
    3,
    75,
    162
};

tris[87] = {
    { m::fixed_t{-46802, nullptr}, m::fixed_t{-11766, nullptr}, m::fixed_t{-31944, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-11849, nullptr}, m::fixed_t{-30112, nullptr} },
    { m::fixed_t{-48241, nullptr}, m::fixed_t{-11788, nullptr}, m::fixed_t{-31449, nullptr} },
    3,
    75,
    162
};

tris[88] = {
    { m::fixed_t{-47801, nullptr}, m::fixed_t{-11162, nullptr}, m::fixed_t{-45252, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{-11289, nullptr}, m::fixed_t{-42436, nullptr} },
    3,
    75,
    162
};

tris[89] = {
    { m::fixed_t{-47909, nullptr}, m::fixed_t{-11384, nullptr}, m::fixed_t{-40342, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    3,
    75,
    162
};

tris[90] = {
    { m::fixed_t{-42452, nullptr}, m::fixed_t{-11289, nullptr}, m::fixed_t{-42436, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{-11384, nullptr}, m::fixed_t{-40342, nullptr} },
    3,
    75,
    162
};

tris[91] = {
    { m::fixed_t{-37723, nullptr}, m::fixed_t{-10997, nullptr}, m::fixed_t{-48877, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{-11193, nullptr}, m::fixed_t{-44562, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{-11002, nullptr}, m::fixed_t{-48770, nullptr} },
    3,
    75,
    162
};

tris[92] = {
    { m::fixed_t{-43977, nullptr}, m::fixed_t{-11002, nullptr}, m::fixed_t{-48770, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    { m::fixed_t{-46338, nullptr}, m::fixed_t{-11066, nullptr}, m::fixed_t{-47361, nullptr} },
    3,
    75,
    162
};

tris[93] = {
    { m::fixed_t{-37723, nullptr}, m::fixed_t{-10997, nullptr}, m::fixed_t{-48877, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{-11193, nullptr}, m::fixed_t{-44562, nullptr} },
    3,
    75,
    162
};

tris[94] = {
    { m::fixed_t{-33368, nullptr}, m::fixed_t{-11152, nullptr}, m::fixed_t{-45456, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    { m::fixed_t{-35189, nullptr}, m::fixed_t{-11054, nullptr}, m::fixed_t{-47611, nullptr} },
    3,
    75,
    162
};

tris[95] = {
    { m::fixed_t{-35189, nullptr}, m::fixed_t{-11054, nullptr}, m::fixed_t{-47611, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{-10997, nullptr}, m::fixed_t{-48877, nullptr} },
    3,
    75,
    162
};

tris[96] = {
    { m::fixed_t{-37208, nullptr}, m::fixed_t{-11319, nullptr}, m::fixed_t{-41788, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{-11152, nullptr}, m::fixed_t{-45456, nullptr} },
    { m::fixed_t{-32299, nullptr}, m::fixed_t{-11301, nullptr}, m::fixed_t{-42184, nullptr} },
    3,
    75,
    162
};

tris[97] = {
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{-11152, nullptr}, m::fixed_t{-45456, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{-11319, nullptr}, m::fixed_t{-41788, nullptr} },
    3,
    75,
    162
};

tris[98] = {
    { m::fixed_t{-46802, nullptr}, m::fixed_t{-11766, nullptr}, m::fixed_t{-31944, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-11849, nullptr}, m::fixed_t{-30112, nullptr} },
    3,
    75,
    162
};

tris[99] = {
    { m::fixed_t{-40529, nullptr}, m::fixed_t{-11193, nullptr}, m::fixed_t{-44562, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{-11002, nullptr}, m::fixed_t{-48770, nullptr} },
    3,
    75,
    162
};

tris[100] = {
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    { m::fixed_t{-32294, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    3,
    75,
    162
};

tris[101] = {
    { m::fixed_t{-32294, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    { m::fixed_t{-32499, nullptr}, m::fixed_t{-11863, nullptr}, m::fixed_t{-29808, nullptr} },
    3,
    75,
    162
};

tris[102] = {
    { m::fixed_t{-46338, nullptr}, m::fixed_t{10081, nullptr}, m::fixed_t{-46401, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{10145, nullptr}, m::fixed_t{-47810, nullptr} },
    3,
    75,
    162
};

tris[103] = {
    { m::fixed_t{-43977, nullptr}, m::fixed_t{10145, nullptr}, m::fixed_t{-47810, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{9953, nullptr}, m::fixed_t{-43602, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{10149, nullptr}, m::fixed_t{-47916, nullptr} },
    3,
    75,
    162
};

tris[104] = {
    { m::fixed_t{-40529, nullptr}, m::fixed_t{9953, nullptr}, m::fixed_t{-43602, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{10149, nullptr}, m::fixed_t{-47916, nullptr} },
    3,
    75,
    162
};

tris[105] = {
    { m::fixed_t{-37723, nullptr}, m::fixed_t{10149, nullptr}, m::fixed_t{-47916, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-35189, nullptr}, m::fixed_t{10092, nullptr}, m::fixed_t{-46651, nullptr} },
    3,
    75,
    162
};

tris[106] = {
    { m::fixed_t{-35189, nullptr}, m::fixed_t{10092, nullptr}, m::fixed_t{-46651, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{9994, nullptr}, m::fixed_t{-44496, nullptr} },
    3,
    75,
    162
};

tris[107] = {
    { m::fixed_t{-33368, nullptr}, m::fixed_t{9994, nullptr}, m::fixed_t{-44496, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{9828, nullptr}, m::fixed_t{-40828, nullptr} },
    { m::fixed_t{-32299, nullptr}, m::fixed_t{9846, nullptr}, m::fixed_t{-41224, nullptr} },
    3,
    75,
    162
};

tris[108] = {
    { m::fixed_t{-33368, nullptr}, m::fixed_t{9994, nullptr}, m::fixed_t{-44496, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{9828, nullptr}, m::fixed_t{-40828, nullptr} },
    3,
    75,
    162
};

tris[109] = {
    { m::fixed_t{-47909, nullptr}, m::fixed_t{9762, nullptr}, m::fixed_t{-39382, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{9857, nullptr}, m::fixed_t{-41476, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{9985, nullptr}, m::fixed_t{-44292, nullptr} },
    3,
    75,
    162
};

tris[110] = {
    { m::fixed_t{-47801, nullptr}, m::fixed_t{9985, nullptr}, m::fixed_t{-44292, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-46338, nullptr}, m::fixed_t{10081, nullptr}, m::fixed_t{-46401, nullptr} },
    3,
    75,
    162
};

tris[111] = {
    { m::fixed_t{-42452, nullptr}, m::fixed_t{9857, nullptr}, m::fixed_t{-41476, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{9985, nullptr}, m::fixed_t{-44292, nullptr} },
    3,
    75,
    162
};

tris[112] = {
    { m::fixed_t{-48241, nullptr}, m::fixed_t{9358, nullptr}, m::fixed_t{-30488, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9297, nullptr}, m::fixed_t{-29152, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{9380, nullptr}, m::fixed_t{-30984, nullptr} },
    3,
    75,
    162
};

tris[113] = {
    { m::fixed_t{-46802, nullptr}, m::fixed_t{9380, nullptr}, m::fixed_t{-30984, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    3,
    75,
    162
};

tris[114] = {
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9297, nullptr}, m::fixed_t{-29152, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{9380, nullptr}, m::fixed_t{-30984, nullptr} },
    3,
    75,
    162
};

tris[115] = {
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{9735, nullptr}, m::fixed_t{-38785, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{9762, nullptr}, m::fixed_t{-39382, nullptr} },
    3,
    75,
    162
};

tris[116] = {
    { m::fixed_t{-43977, nullptr}, m::fixed_t{10145, nullptr}, m::fixed_t{-47810, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{9953, nullptr}, m::fixed_t{-43602, nullptr} },
    3,
    75,
    162
};

tris[117] = {
    { m::fixed_t{-47909, nullptr}, m::fixed_t{9762, nullptr}, m::fixed_t{-39382, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{9735, nullptr}, m::fixed_t{-38785, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{9857, nullptr}, m::fixed_t{-41476, nullptr} },
    3,
    75,
    162
};

tris[118] = {
    { m::fixed_t{-41580, nullptr}, m::fixed_t{9735, nullptr}, m::fixed_t{-38785, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-32499, nullptr}, m::fixed_t{9284, nullptr}, m::fixed_t{-28848, nullptr} },
    3,
    75,
    162
};

tris[119] = {
    { m::fixed_t{-32499, nullptr}, m::fixed_t{9284, nullptr}, m::fixed_t{-28848, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-32294, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    3,
    75,
    162
};

tris[120] = {
    { m::fixed_t{-32299, nullptr}, m::fixed_t{9846, nullptr}, m::fixed_t{-41224, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{9828, nullptr}, m::fixed_t{-40828, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{-11319, nullptr}, m::fixed_t{-41788, nullptr} },
    3,
    75,
    162
};

tris[121] = {
    { m::fixed_t{-47909, nullptr}, m::fixed_t{9762, nullptr}, m::fixed_t{-39382, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{9985, nullptr}, m::fixed_t{-44292, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{-11162, nullptr}, m::fixed_t{-45252, nullptr} },
    3,
    75,
    162
};

tris[122] = {
    { m::fixed_t{-46338, nullptr}, m::fixed_t{10081, nullptr}, m::fixed_t{-46401, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{10145, nullptr}, m::fixed_t{-47810, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{-11002, nullptr}, m::fixed_t{-48770, nullptr} },
    3,
    75,
    162
};

tris[123] = {
    { m::fixed_t{-47801, nullptr}, m::fixed_t{9985, nullptr}, m::fixed_t{-44292, nullptr} },
    { m::fixed_t{-46338, nullptr}, m::fixed_t{10081, nullptr}, m::fixed_t{-46401, nullptr} },
    { m::fixed_t{-46338, nullptr}, m::fixed_t{-11066, nullptr}, m::fixed_t{-47361, nullptr} },
    3,
    75,
    162
};

tris[124] = {
    { m::fixed_t{-43977, nullptr}, m::fixed_t{10145, nullptr}, m::fixed_t{-47810, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{10149, nullptr}, m::fixed_t{-47916, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{-10997, nullptr}, m::fixed_t{-48877, nullptr} },
    3,
    75,
    162
};

tris[125] = {
    { m::fixed_t{-48241, nullptr}, m::fixed_t{9358, nullptr}, m::fixed_t{-30488, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{9380, nullptr}, m::fixed_t{-30984, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{-11766, nullptr}, m::fixed_t{-31944, nullptr} },
    3,
    75,
    162
};

tris[126] = {
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9297, nullptr}, m::fixed_t{-29152, nullptr} },
    { m::fixed_t{-48241, nullptr}, m::fixed_t{9358, nullptr}, m::fixed_t{-30488, nullptr} },
    { m::fixed_t{-48241, nullptr}, m::fixed_t{-11788, nullptr}, m::fixed_t{-31449, nullptr} },
    3,
    75,
    162
};

tris[127] = {
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9297, nullptr}, m::fixed_t{-29152, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-11849, nullptr}, m::fixed_t{-30112, nullptr} },
    3,
    75,
    162
};

tris[128] = {
    { m::fixed_t{-32294, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    3,
    75,
    162
};

tris[129] = {
    { m::fixed_t{-33205, nullptr}, m::fixed_t{-11813, nullptr}, m::fixed_t{-30906, nullptr} },
    { m::fixed_t{-33205, nullptr}, m::fixed_t{9333, nullptr}, m::fixed_t{-29946, nullptr} },
    { m::fixed_t{-32499, nullptr}, m::fixed_t{-11863, nullptr}, m::fixed_t{-29808, nullptr} },
    3,
    75,
    162
};

tris[130] = {
    { m::fixed_t{-46802, nullptr}, m::fixed_t{9380, nullptr}, m::fixed_t{-30984, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    3,
    75,
    162
};

tris[131] = {
    { m::fixed_t{-37208, nullptr}, m::fixed_t{9828, nullptr}, m::fixed_t{-40828, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    3,
    75,
    162
};

tris[132] = {
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{9762, nullptr}, m::fixed_t{-39382, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{-11384, nullptr}, m::fixed_t{-40342, nullptr} },
    3,
    75,
    162
};

tris[133] = {
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{9857, nullptr}, m::fixed_t{-41476, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{-11289, nullptr}, m::fixed_t{-42436, nullptr} },
    3,
    75,
    162
};

tris[134] = {
    { m::fixed_t{-40529, nullptr}, m::fixed_t{9953, nullptr}, m::fixed_t{-43602, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    3,
    75,
    162
};

tris[135] = {
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{9953, nullptr}, m::fixed_t{-43602, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{-11193, nullptr}, m::fixed_t{-44562, nullptr} },
    3,
    75,
    162
};

tris[136] = {
    { m::fixed_t{-32499, nullptr}, m::fixed_t{9284, nullptr}, m::fixed_t{-28848, nullptr} },
    { m::fixed_t{-32294, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-32294, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    3,
    75,
    162
};

tris[137] = {
    { m::fixed_t{-42452, nullptr}, m::fixed_t{9857, nullptr}, m::fixed_t{-41476, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{9735, nullptr}, m::fixed_t{-38785, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    3,
    75,
    162
};

tris[138] = {
    { m::fixed_t{-33368, nullptr}, m::fixed_t{9994, nullptr}, m::fixed_t{-44496, nullptr} },
    { m::fixed_t{-32299, nullptr}, m::fixed_t{9846, nullptr}, m::fixed_t{-41224, nullptr} },
    { m::fixed_t{-32299, nullptr}, m::fixed_t{-11301, nullptr}, m::fixed_t{-42184, nullptr} },
    3,
    75,
    162
};

tris[139] = {
    { m::fixed_t{-35189, nullptr}, m::fixed_t{10092, nullptr}, m::fixed_t{-46651, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{9994, nullptr}, m::fixed_t{-44496, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{-11152, nullptr}, m::fixed_t{-45456, nullptr} },
    3,
    75,
    162
};

tris[140] = {
    { m::fixed_t{-37723, nullptr}, m::fixed_t{10149, nullptr}, m::fixed_t{-47916, nullptr} },
    { m::fixed_t{-35189, nullptr}, m::fixed_t{10092, nullptr}, m::fixed_t{-46651, nullptr} },
    { m::fixed_t{-35189, nullptr}, m::fixed_t{-11054, nullptr}, m::fixed_t{-47611, nullptr} },
    3,
    75,
    162
};

tris[141] = {
    { m::fixed_t{-32299, nullptr}, m::fixed_t{9846, nullptr}, m::fixed_t{-41224, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{-11319, nullptr}, m::fixed_t{-41788, nullptr} },
    { m::fixed_t{-32299, nullptr}, m::fixed_t{-11301, nullptr}, m::fixed_t{-42184, nullptr} },
    3,
    75,
    162
};

tris[142] = {
    { m::fixed_t{-47909, nullptr}, m::fixed_t{9762, nullptr}, m::fixed_t{-39382, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{-11162, nullptr}, m::fixed_t{-45252, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{-11384, nullptr}, m::fixed_t{-40342, nullptr} },
    3,
    75,
    162
};

tris[143] = {
    { m::fixed_t{-46338, nullptr}, m::fixed_t{10081, nullptr}, m::fixed_t{-46401, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{-11002, nullptr}, m::fixed_t{-48770, nullptr} },
    { m::fixed_t{-46338, nullptr}, m::fixed_t{-11066, nullptr}, m::fixed_t{-47361, nullptr} },
    3,
    75,
    162
};

tris[144] = {
    { m::fixed_t{-47801, nullptr}, m::fixed_t{9985, nullptr}, m::fixed_t{-44292, nullptr} },
    { m::fixed_t{-46338, nullptr}, m::fixed_t{-11066, nullptr}, m::fixed_t{-47361, nullptr} },
    { m::fixed_t{-47801, nullptr}, m::fixed_t{-11162, nullptr}, m::fixed_t{-45252, nullptr} },
    3,
    75,
    162
};

tris[145] = {
    { m::fixed_t{-43977, nullptr}, m::fixed_t{10145, nullptr}, m::fixed_t{-47810, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{-10997, nullptr}, m::fixed_t{-48877, nullptr} },
    { m::fixed_t{-43977, nullptr}, m::fixed_t{-11002, nullptr}, m::fixed_t{-48770, nullptr} },
    3,
    75,
    162
};

tris[146] = {
    { m::fixed_t{-48241, nullptr}, m::fixed_t{9358, nullptr}, m::fixed_t{-30488, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{-11766, nullptr}, m::fixed_t{-31944, nullptr} },
    { m::fixed_t{-48241, nullptr}, m::fixed_t{-11788, nullptr}, m::fixed_t{-31449, nullptr} },
    3,
    75,
    162
};

tris[147] = {
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9297, nullptr}, m::fixed_t{-29152, nullptr} },
    { m::fixed_t{-48241, nullptr}, m::fixed_t{-11788, nullptr}, m::fixed_t{-31449, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-11849, nullptr}, m::fixed_t{-30112, nullptr} },
    3,
    75,
    162
};

tris[148] = {
    { m::fixed_t{-48759, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-11849, nullptr}, m::fixed_t{-30112, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    3,
    75,
    162
};

tris[149] = {
    { m::fixed_t{-32294, nullptr}, m::fixed_t{9143, nullptr}, m::fixed_t{-25750, nullptr} },
    { m::fixed_t{-48759, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    { m::fixed_t{-32294, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    3,
    75,
    162
};

tris[150] = {
    { m::fixed_t{-33205, nullptr}, m::fixed_t{9333, nullptr}, m::fixed_t{-29946, nullptr} },
    { m::fixed_t{-39379, nullptr}, m::fixed_t{-11537, nullptr}, m::fixed_t{-36985, nullptr} },
    { m::fixed_t{-39379, nullptr}, m::fixed_t{9609, nullptr}, m::fixed_t{-36025, nullptr} },
    3,
    75,
    162
};

tris[151] = {
    { m::fixed_t{-39379, nullptr}, m::fixed_t{9609, nullptr}, m::fixed_t{-36025, nullptr} },
    { m::fixed_t{-40600, nullptr}, m::fixed_t{-11476, nullptr}, m::fixed_t{-38336, nullptr} },
    { m::fixed_t{-40600, nullptr}, m::fixed_t{9671, nullptr}, m::fixed_t{-37376, nullptr} },
    3,
    75,
    162
};

tris[152] = {
    { m::fixed_t{-40600, nullptr}, m::fixed_t{9671, nullptr}, m::fixed_t{-37376, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{9735, nullptr}, m::fixed_t{-38785, nullptr} },
    3,
    75,
    162
};

tris[153] = {
    { m::fixed_t{-40600, nullptr}, m::fixed_t{-11476, nullptr}, m::fixed_t{-38336, nullptr} },
    { m::fixed_t{-39379, nullptr}, m::fixed_t{9609, nullptr}, m::fixed_t{-36025, nullptr} },
    { m::fixed_t{-39379, nullptr}, m::fixed_t{-11537, nullptr}, m::fixed_t{-36985, nullptr} },
    3,
    75,
    162
};

tris[154] = {
    { m::fixed_t{-39379, nullptr}, m::fixed_t{-11537, nullptr}, m::fixed_t{-36985, nullptr} },
    { m::fixed_t{-33205, nullptr}, m::fixed_t{9333, nullptr}, m::fixed_t{-29946, nullptr} },
    { m::fixed_t{-33205, nullptr}, m::fixed_t{-11813, nullptr}, m::fixed_t{-30906, nullptr} },
    3,
    75,
    162
};

tris[155] = {
    { m::fixed_t{-40600, nullptr}, m::fixed_t{9671, nullptr}, m::fixed_t{-37376, nullptr} },
    { m::fixed_t{-40600, nullptr}, m::fixed_t{-11476, nullptr}, m::fixed_t{-38336, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    3,
    75,
    162
};

tris[156] = {
    { m::fixed_t{-32499, nullptr}, m::fixed_t{-11863, nullptr}, m::fixed_t{-29808, nullptr} },
    { m::fixed_t{-33205, nullptr}, m::fixed_t{9333, nullptr}, m::fixed_t{-29946, nullptr} },
    { m::fixed_t{-32499, nullptr}, m::fixed_t{9284, nullptr}, m::fixed_t{-28848, nullptr} },
    3,
    75,
    162
};

tris[157] = {
    { m::fixed_t{-46802, nullptr}, m::fixed_t{9380, nullptr}, m::fixed_t{-30984, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    { m::fixed_t{-46802, nullptr}, m::fixed_t{-11766, nullptr}, m::fixed_t{-31944, nullptr} },
    3,
    75,
    162
};

tris[158] = {
    { m::fixed_t{-37208, nullptr}, m::fixed_t{9828, nullptr}, m::fixed_t{-40828, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    { m::fixed_t{-37208, nullptr}, m::fixed_t{-11319, nullptr}, m::fixed_t{-41788, nullptr} },
    3,
    75,
    162
};

tris[159] = {
    { m::fixed_t{-40678, nullptr}, m::fixed_t{9366, nullptr}, m::fixed_t{-30675, nullptr} },
    { m::fixed_t{-47909, nullptr}, m::fixed_t{-11384, nullptr}, m::fixed_t{-40342, nullptr} },
    { m::fixed_t{-40678, nullptr}, m::fixed_t{-11780, nullptr}, m::fixed_t{-31635, nullptr} },
    3,
    75,
    162
};

tris[160] = {
    { m::fixed_t{-41936, nullptr}, m::fixed_t{9927, nullptr}, m::fixed_t{-43021, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{-11289, nullptr}, m::fixed_t{-42436, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    3,
    75,
    162
};

tris[161] = {
    { m::fixed_t{-40529, nullptr}, m::fixed_t{9953, nullptr}, m::fixed_t{-43602, nullptr} },
    { m::fixed_t{-41936, nullptr}, m::fixed_t{-11219, nullptr}, m::fixed_t{-43982, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{-11193, nullptr}, m::fixed_t{-44562, nullptr} },
    3,
    75,
    162
};

tris[162] = {
    { m::fixed_t{-39143, nullptr}, m::fixed_t{9934, nullptr}, m::fixed_t{-43171, nullptr} },
    { m::fixed_t{-40529, nullptr}, m::fixed_t{-11193, nullptr}, m::fixed_t{-44562, nullptr} },
    { m::fixed_t{-39143, nullptr}, m::fixed_t{-11212, nullptr}, m::fixed_t{-44131, nullptr} },
    3,
    75,
    162
};

tris[163] = {
    { m::fixed_t{-32499, nullptr}, m::fixed_t{9284, nullptr}, m::fixed_t{-28848, nullptr} },
    { m::fixed_t{-32294, nullptr}, m::fixed_t{-12003, nullptr}, m::fixed_t{-26711, nullptr} },
    { m::fixed_t{-32499, nullptr}, m::fixed_t{-11863, nullptr}, m::fixed_t{-29808, nullptr} },
    3,
    75,
    162
};

tris[164] = {
    { m::fixed_t{-42452, nullptr}, m::fixed_t{9857, nullptr}, m::fixed_t{-41476, nullptr} },
    { m::fixed_t{-41580, nullptr}, m::fixed_t{-11412, nullptr}, m::fixed_t{-39746, nullptr} },
    { m::fixed_t{-42452, nullptr}, m::fixed_t{-11289, nullptr}, m::fixed_t{-42436, nullptr} },
    3,
    75,
    162
};

tris[165] = {
    { m::fixed_t{-33368, nullptr}, m::fixed_t{9994, nullptr}, m::fixed_t{-44496, nullptr} },
    { m::fixed_t{-32299, nullptr}, m::fixed_t{-11301, nullptr}, m::fixed_t{-42184, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{-11152, nullptr}, m::fixed_t{-45456, nullptr} },
    3,
    75,
    162
};

tris[166] = {
    { m::fixed_t{-35189, nullptr}, m::fixed_t{10092, nullptr}, m::fixed_t{-46651, nullptr} },
    { m::fixed_t{-33368, nullptr}, m::fixed_t{-11152, nullptr}, m::fixed_t{-45456, nullptr} },
    { m::fixed_t{-35189, nullptr}, m::fixed_t{-11054, nullptr}, m::fixed_t{-47611, nullptr} },
    3,
    75,
    162
};

tris[167] = {
    { m::fixed_t{-37723, nullptr}, m::fixed_t{10149, nullptr}, m::fixed_t{-47916, nullptr} },
    { m::fixed_t{-35189, nullptr}, m::fixed_t{-11054, nullptr}, m::fixed_t{-47611, nullptr} },
    { m::fixed_t{-37723, nullptr}, m::fixed_t{-10997, nullptr}, m::fixed_t{-48877, nullptr} },
    3,
    75,
    162
};

} }; }
