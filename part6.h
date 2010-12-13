#ifndef ZZ_PART6_H
#define ZZ_PART6_H

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
	uint16_t group;		// private
	uint16_t element;	// private
	const char *VR;
	const char *VM;
	bool retired;
	const char *description;
};

const struct privatedic *zzprivtag(uint16_t group, uint16_t element, const char *label);
const struct part6 *zztag(uint16_t group, uint16_t element);

#ifdef __cplusplus
}
#endif

#endif
