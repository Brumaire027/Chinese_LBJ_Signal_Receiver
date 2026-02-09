//
// Created by FLN1021 on 2023/9/4.
//

#ifndef ESP32_TEST_UNICON_H
#define ESP32_TEST_UNICON_H

#include <cstdint>
typedef unsigned int	UINT;	/* int must be 16-bit or 32-bit */
typedef unsigned char	BYTE;	/* char must be 8-bit */
typedef uint16_t		WORD;	/* 16-bit unsigned integer */
typedef uint16_t		WCHAR;	/* 16-bit unsigned integer */
typedef uint32_t		DWORD;	/* 32-bit unsigned integer */
typedef uint64_t		QWORD;	/* 64-bit unsigned integer */
#define MERGE2(a, b) a ## b
#define CVTBL(tbl, cp) MERGE2(tbl, cp)
#define FF_CODE_PAGE 936
WCHAR ff_oem2uni (WCHAR oem, WORD cp);

#endif //ESP32_TEST_UNICON_H
