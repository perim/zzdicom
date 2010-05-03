#ifndef ZZ_DICOM_H
#define ZZ_DICOM_H

#include <stdint.h>
#include <stdbool.h>

#include "zztags.h"

typedef uint32_t zzKey;
#define ZZ_KEY(m_group, m_element) (((uint32_t)m_group << 16) + (uint32_t)m_element)
#define ZZ_GROUP(m_key) ((uint16_t)(m_key >> 16))
#define ZZ_ELEMENT(m_key) ((uint16_t)(m_key & 0xffff))

#endif
