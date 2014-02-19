#ifndef _CAMERAPROPERTIES_H
#define _CAMERAPROPERTIES_H

#include <map>

typedef std::map<int, std::pair<std::string, float>> propStrValMap;
typedef propStrValMap::value_type propStrValMapVal;
typedef propStrValMap::mapped_type propStrValMapMap;

// Camera Tv (shutter speed) settings
extern const propStrValMap tvStrValMap;
#define CAMTV_BULB	0x0C
#define CAMTV_30S	0x10
#define CAMTV_25S	0x13
#define CAMTV_20S	0x14
#define CAMTV_20SCF	0x15
#define CAMTV_15S	0x18
#define CAMTV_13S	0x1B
#define CAMTV_10S	0x1C
#define CAMTV_10SCF	0x1D
#define CAMTV_8S	0x20
#define CAMTV_6SCF	0x23
#define CAMTV_6S	0x24
#define CAMTV_5S	0x25
#define CAMTV_4S	0x28
#define CAMTV_3S2	0x2B
#define CAMTV_3S	0x2C
#define CAMTV_2S5	0x2D
#define CAMTV_2S	0x30
#define CAMTV_1S6	0x33
#define CAMTV_1S5	0x34
#define CAMTV_1S3	0x35
#define CAMTV_1S	0x38
#define CAMTV_0S8	0x3B
#define CAMTV_0S7	0x3C
#define CAMTV_0S6	0x3D
#define CAMTV_0S5	0x40
#define CAMTV_0S4	0x43
#define CAMTV_0S3	0x44
#define CAMTV_0S3CF	0x45
#define CAMTV_4		0x48
#define CAMTV_5		0x4B
#define CAMTV_6		0x4C
#define CAMTV_6CF	0x4D
#define CAMTV_8		0x50
#define CAMTV_10CF	0x53
#define CAMTV_10	0x54
#define CAMTV_13	0x55
#define CAMTV_15	0x58
#define CAMTV_20CF	0x5B
#define CAMTV_20	0x5C
#define CAMTV_25	0x5D
#define CAMTV_30	0x60
#define CAMTV_40	0x63
#define CAMTV_45	0x64
#define CAMTV_50	0x65
#define CAMTV_60	0x68
#define CAMTV_80	0x6B
#define CAMTV_90	0x6C
#define CAMTV_100	0x6D
#define CAMTV_125	0x70
#define CAMTV_160	0x73
#define CAMTV_180	0x74
#define CAMTV_200	0x75
#define CAMTV_250	0x78
#define CAMTV_320	0x7B
#define CAMTV_350	0x7C
#define CAMTV_400	0x7D
#define CAMTV_500	0x80
#define CAMTV_640	0x83
#define CAMTV_750	0x84
#define CAMTV_800	0x85
#define CAMTV_1000	0x88
#define CAMTV_1250	0x8B
#define CAMTV_1500	0x8C
#define CAMTV_1600	0x8D
#define CAMTV_2000	0x90
#define CAMTV_2500	0x93
#define CAMTV_3000	0x94
#define CAMTV_3200	0x95
#define CAMTV_4000	0x98
#define CAMTV_5000	0x9B
#define CAMTV_6000	0x9C
#define CAMTV_6400	0x9D
#define CAMTV_8000	0xA0

#endif