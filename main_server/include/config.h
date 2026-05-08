#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#include "request.h"

typedef struct {
	int numDrivers;
	int durationSec;
	int surgeThreshold;
	float surgeMultiplier;
	int timeoutNormal;
	int timeoutVip;
	int timeoutEmergency;
	int agingNormalToVip;
	int agingVipToEmergency;
} Config;

extern Config config;

void configInit(Config *cfg);
int configLoadFile(Config *cfg, const char *path);
int configApplyArgs(Config *cfg, int argc, char **argv);
int configGetTimeout(const Config *cfg, RequestType type);

#endif
