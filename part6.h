#ifndef ZZ_PART6_H
#define ZZ_PART6_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct privatedic
{
	uint16_t group;
	const char *privateLabel;
	uint16_t element;
	const char *VR;
	const char *description;
	const char *VM;
};

struct part6
{
	uint16_t group;
	uint16_t element;
	const char *VR;
	const char *VM;
	bool retired;
	const char *description;
};

const struct privatedic *zzprivtag(uint16_t group, uint16_t element, const char *label, uint16_t domain);
const struct part6 *zztag(uint16_t group, uint16_t element);

#ifdef __cplusplus
}
#endif

#endif
