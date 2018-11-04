/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// See the header for documentation on the meaning of this data.

#include "tensorflow/contrib/lite/experimental/micro/examples/micro_speech/CMSIS/no_30ms_sample_data.h"

const int g_no_30ms_sample_data_size = 480;
const int16_t g_no_30ms_sample_data[480] = {
    5713,  5735,  5735,  5737,  5701,  5691,  5656,  5633,  5611,  5552,  5475,
    5394,  5293,  5177,  5064,  4924,  4737,  4599,  4420,  4237,  4048,  3828,
    3623,  3413,  3183,  2915,  2622,  2308,  1980,  1657,  1261,  901,   549,
    205,   -85,   -383,  -688,  -969,  -1246, -1530, -1850, -2206, -2561, -2915,
    -3224, -3482, -3713, -3921, -4107, -4287, -4470, -4660, -4850, -5057, -5239,
    -5395, -5540, -5619, -5697, -5724, -5697, -5675, -5633, -5590, -5579, -5530,
    -5486, -5442, -5426, -5391, -5348, -5276, -5197, -5124, -5039, -4925, -4808,
    -4677, -4581, -4479, -4343, -4218, -4087, -3970, -3858, -3729, -3570, -3384,
    -3206, -3020, -2839, -2636, -2453, -2287, -2185, -2154, -1926, -1562, -1223,
    -758,  -473,  -64,   395,   599,   880,   814,   938,   1172,  1498,  1928,
    2127,  2422,  2608,  2841,  2937,  2886,  2815,  2985,  3324,  3757,  4152,
    4481,  4652,  4917,  4965,  4766,  4583,  4328,  4503,  4815,  5118,  5408,
    5682,  5956,  6082,  6055,  5744,  5426,  5341,  5427,  5606,  5882,  6065,
    6226,  6428,  6477,  6385,  6009,  5728,  5552,  5439,  5339,  5200,  5008,
    4947,  4835,  4614,  4330,  3887,  3521,  3111,  2460,  1983,  1297,  650,
    279,   -353,  -720,  -1044, -1518, -1668, -2117, -2496, -2743, -3266, -3607,
    -3790, -4149, -4075, -4042, -4096, -3981, -4138, -4226, -4214, -4503, -4455,
    -4577, -4642, -4346, -4351, -4270, -4263, -4522, -4521, -4673, -4814, -4731,
    -4950, -5011, -5004, -5288, -5341, -5566, -5833, -5783, -5929, -5847, -5765,
    -5828, -5644, -5613, -5615, -5428, -5291, -5014, -4554, -4277, -3964, -3854,
    -3829, -3612, -3603, -3438, -3137, -2831, -2164, -1438, -939,  -330,  -156,
    46,    242,   73,    242,   220,   239,   542,   565,   739,   872,   801,
    857,   676,   543,   586,   567,   828,   1142,  1490,  1985,  2508,  2982,
    3438,  3699,  3939,  4069,  4178,  4420,  4622,  4917,  5338,  5801,  6285,
    6658,  6963,  7213,  7233,  7328,  7176,  7038,  7031,  6860,  6957,  6767,
    6599,  6523,  6212,  6147,  6063,  5860,  6020,  6015,  6033,  6184,  5722,
    5607,  5016,  4337,  4063,  3229,  3080,  3006,  2804,  3035,  2541,  2136,
    1879,  1012,  401,   -575,  -1584, -1930, -2278, -2485, -2477, -2712, -2747,
    -2766, -3320, -3592, -4188, -4669, -4672, -4939, -4789, -4426, -4203, -3674,
    -3563, -3656, -3759, -4067, -4257, -4522, -4970, -5204, -5237, -5139, -4907,
    -4911, -4917, -4921, -5007, -5230, -5654, -6122, -6464, -6733, -6948, -7067,
    -6972, -6800, -6520, -6132, -5830, -5382, -5091, -4797, -4546, -4472, -4362,
    -4350, -4235, -3851, -3454, -3144, -2735, -2341, -1845, -1262, -958,  -549,
    -166,  66,    382,   366,   352,   341,   85,    -13,   -176,  -303,  -235,
    -341,  -309,  -227,  -249,  -50,   143,   384,   874,   1149,  1552,  2155,
    2767,  3499,  3994,  4460,  4920,  5288,  5569,  5704,  5881,  6094,  6461,
    6653,  6803,  7115,  7311,  7521,  7612,  7443,  7380,  7124,  6742,  6495,
    5964,  5656,  5415,  5167,  5656,  5813,  6027,  6401,  6351,  6787,  7019,
    6581,  6512,  5965,  5308,  5140,  4336,  4147,  3899,  3398,  3360,  2830,
    2624,  1968,  1026,  395,   -699,  -1424, -2327, -3006, -3192, -3435, -3337,
    -3686, -3513, -3350, -3502, -3261, -3878, -4005, -4063, -4187, -3767, -3598,
    -3384, -3300, -3094, -2857, -3023, -3274, -3851, -4352, -4523, -4943, -5477,
    -5612, -5682, -5733, -5714, -5965, -6110, -5950, -6158, -6548, -6897, -7165,
    -7281, -7352, -7258, -7185, -6659, -5946, -5470,
};
