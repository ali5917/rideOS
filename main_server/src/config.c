#include "../include/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config config;

static void trimWhitespace(char *text) {
	char *end = NULL;
	while (*text != '\0' && isspace((unsigned char)*text)) {
		text++;
	}

	if (*text == '\0') {
		return;
	}   

	end = text + strlen(text) - 1;
	while (end > text && isspace((unsigned char)*end)) {
		*end = '\0';
		end--;
	}
}

static void clampConfig(Config *cfg) {
	if (cfg->numDrivers < 1) cfg->numDrivers = 1;
	if (cfg->surgeThreshold < 0) cfg->surgeThreshold = 0;
	if (cfg->surgeMultiplier < 1.0f) cfg->surgeMultiplier = 1.0f;
	if (cfg->timeoutNormal < 1) cfg->timeoutNormal = 1;
	if (cfg->timeoutVip < 1) cfg->timeoutVip = 1;
	if (cfg->timeoutEmergency < 1) cfg->timeoutEmergency = 1;
	if (cfg->agingNormalToVip < 1) cfg->agingNormalToVip = 1;
	if (cfg->agingVipToEmergency < 1) cfg->agingVipToEmergency = 1;
}

void configInit(Config *cfg) {
	if (cfg == NULL) {
		return;
	}

	cfg->numDrivers = 10;
	cfg->durationSec = 120;
	cfg->surgeThreshold = 10;
	cfg->surgeMultiplier = 1.5f;
	cfg->timeoutNormal = 30;
	cfg->timeoutVip = 20;
	cfg->timeoutEmergency = 10;
	cfg->agingNormalToVip = 5;
	cfg->agingVipToEmergency = 3;
}

static void applyKeyValue(Config *cfg, const char *key, const char *value) {
	if (strcmp(key, "drivers") == 0) cfg->numDrivers = atoi(value);
	else if (strcmp(key, "duration") == 0) cfg->durationSec = atoi(value);
	else if (strcmp(key, "surge_threshold") == 0) cfg->surgeThreshold = atoi(value);
	else if (strcmp(key, "surge_multiplier") == 0) cfg->surgeMultiplier = (float)atof(value);
	else if (strcmp(key, "timeout_normal") == 0) cfg->timeoutNormal = atoi(value);
	else if (strcmp(key, "timeout_vip") == 0) cfg->timeoutVip = atoi(value);
	else if (strcmp(key, "timeout_emergency") == 0) cfg->timeoutEmergency = atoi(value);
	else if (strcmp(key, "aging_normal_to_vip") == 0) cfg->agingNormalToVip = atoi(value);
	else if (strcmp(key, "aging_vip_to_emergency") == 0) cfg->agingVipToEmergency = atoi(value);
}

int configLoadFile(Config *cfg, const char *path) {
	if (cfg == NULL || path == NULL) {
		return -1;
	}

	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return -1;
	}

	char line[256];
	while (fgets(line, sizeof(line), file) != NULL) {
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}

		char *eq = strchr(line, '=');
		if (eq == NULL) {
			continue;
		}

		*eq = '\0';
		char *key = line;
		char *value = eq + 1;
		trimWhitespace(key);
		trimWhitespace(value);
		if (key[0] == '\0' || value[0] == '\0') {
			continue;
		}
		applyKeyValue(cfg, key, value);
	}

	fclose(file);
	clampConfig(cfg);
	return 0;
}

int configApplyArgs(Config *cfg, int argc, char **argv) {
	if (cfg == NULL || argv == NULL) {
		return -1;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--drivers") == 0 && i + 1 < argc) {
			cfg->numDrivers = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
			cfg->durationSec = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--surge-threshold") == 0 && i + 1 < argc) {
			cfg->surgeThreshold = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--surge-multiplier") == 0 && i + 1 < argc) {
			cfg->surgeMultiplier = (float)atof(argv[++i]);
		} else if (strcmp(argv[i], "--timeout-normal") == 0 && i + 1 < argc) {
			cfg->timeoutNormal = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--timeout-vip") == 0 && i + 1 < argc) {
			cfg->timeoutVip = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--timeout-emergency") == 0 && i + 1 < argc) {
			cfg->timeoutEmergency = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--aging-normal") == 0 && i + 1 < argc) {
			cfg->agingNormalToVip = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--aging-vip") == 0 && i + 1 < argc) {
			cfg->agingVipToEmergency = atoi(argv[++i]);
		}
	}

	clampConfig(cfg);
	return 0;
}

int configGetTimeout(const Config *cfg, RequestType type) {
	if (cfg == NULL) {
		return 10;
	}
	if (type == EMERGENCY) return cfg->timeoutEmergency;
	if (type == VIP) return cfg->timeoutVip;
	return cfg->timeoutNormal;
}