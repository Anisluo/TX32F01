#ifndef _FPDEFINE_H
#define _FPDEFINE_H

#include <stdint.h>

#ifdef __cplusplus
  #define   __I     volatile             /*!< Defines 'read only' permissions                 */
#else
  #define   __I     volatile const       /*!< Defines 'read only' permissions                 */
#endif
#define     __O     volatile             /*!< Defines 'write only' permissions                */
#define     __IO    volatile             /*!< Defines 'read / write' permissions              */


typedef int32_t  s32;
typedef int16_t s16;
typedef int8_t  s8;

typedef const int32_t sc32;  /*!< Read Only */
typedef const int16_t sc16;  /*!< Read Only */
typedef const int8_t sc8;   /*!< Read Only */

typedef __IO int32_t  vs32;
typedef __IO int16_t  vs16;
typedef __IO int8_t   vs8;

typedef __I int32_t vsc32;  /*!< Read Only */
typedef __I int16_t vsc16;  /*!< Read Only */
typedef __I int8_t vsc8;   /*!< Read Only */

typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef const uint32_t uc32;  /*!< Read Only */
typedef const uint16_t uc16;  /*!< Read Only */
typedef const uint8_t uc8;   /*!< Read Only */

typedef __IO uint32_t  vu32;
typedef __IO uint16_t vu16;
typedef __IO uint8_t  vu8;

typedef __I uint32_t vuc32;  /*!< Read Only */
typedef __I uint16_t vuc16;  /*!< Read Only */
typedef __I uint8_t vuc8;   /*!< Read Only */

typedef enum {FALSE = 0, TRUE = !FALSE} bool;

typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;

typedef enum {DISABLE = 0, ENABLE = !DISABLE} FunctionalState;
#define IS_FUNCTIONAL_STATE(STATE) (((STATE) == DISABLE) || ((STATE) == ENABLE))

typedef enum {ERROR = 0, SUCCESS = !ERROR} ErrorStatus;


typedef enum
{ falsefp = 0,
  truefp=1
}booltyped;


#define B0000 0x00
#define B0001 0x01
#define B0010 0x02
#define B0011 0x03
#define B0100 0x04
#define B0101 0x05
#define B0110 0x06
#define B0111 0x07
#define B1000 0x08
#define B1001 0x09
#define B1010 0x0A
#define B1011 0x0B
#define B1100 0x0C
#define B1101 0x0D
#define B1110 0x0E
#define B1111 0x0F

#define B0000B0000 0x00
#define B0000B0001 0x01
#define B0000B0010 0x02
#define B0000B0011 0x03
#define B0000B0100 0x04
#define B0000B0101 0x05
#define B0000B0110 0x06
#define B0000B0111 0x07
#define B0000B1000 0x08
#define B0000B1001 0x09
#define B0000B1010 0x0A
#define B0000B1011 0x0B
#define B0000B1100 0x0C
#define B0000B1101 0x0D
#define B0000B1110 0x0E
#define B0000B1111 0x0F
#define B0001B0000 0x10
#define B0001B0001 0x11
#define B0001B0010 0x12
#define B0001B0011 0x13
#define B0001B0100 0x14
#define B0001B0101 0x15
#define B0001B0110 0x16
#define B0001B0111 0x17
#define B0001B1000 0x18
#define B0001B1001 0x19
#define B0001B1010 0x1A
#define B0001B1011 0x1B
#define B0001B1100 0x1C
#define B0001B1101 0x1D
#define B0001B1110 0x1E
#define B0001B1111 0x1F
#define B0010B0000 0x20
#define B0010B0001 0x21
#define B0010B0010 0x22
#define B0010B0011 0x23
#define B0010B0100 0x24
#define B0010B0101 0x25
#define B0010B0110 0x26
#define B0010B0111 0x27
#define B0010B1000 0x28
#define B0010B1001 0x29
#define B0010B1010 0x2A
#define B0010B1011 0x2B
#define B0010B1100 0x2C
#define B0010B1101 0x2D
#define B0010B1110 0x2E
#define B0010B1111 0x2F
#define B0011B0000 0x30
#define B0011B0001 0x31
#define B0011B0010 0x32
#define B0011B0011 0x33
#define B0011B0100 0x34
#define B0011B0101 0x35
#define B0011B0110 0x36
#define B0011B0111 0x37
#define B0011B1000 0x38
#define B0011B1001 0x39
#define B0011B1010 0x3A
#define B0011B1011 0x3B
#define B0011B1100 0x3C
#define B0011B1101 0x3D
#define B0011B1110 0x3E
#define B0011B1111 0x3F
#define B0100B0000 0x40
#define B0100B0001 0x41
#define B0100B0010 0x42
#define B0100B0011 0x43
#define B0100B0100 0x44
#define B0100B0101 0x45
#define B0100B0110 0x46
#define B0100B0111 0x47
#define B0100B1000 0x48
#define B0100B1001 0x49
#define B0100B1010 0x4A
#define B0100B1011 0x4B
#define B0100B1100 0x4C
#define B0100B1101 0x4D
#define B0100B1110 0x4E
#define B0100B1111 0x4F
#define B0101B0000 0x50
#define B0101B0001 0x51
#define B0101B0010 0x52
#define B0101B0011 0x53
#define B0101B0100 0x54
#define B0101B0101 0x55
#define B0101B0110 0x56
#define B0101B0111 0x57
#define B0101B1000 0x58
#define B0101B1001 0x59
#define B0101B1010 0x5A
#define B0101B1011 0x5B
#define B0101B1100 0x5C
#define B0101B1101 0x5D
#define B0101B1110 0x5E
#define B0101B1111 0x5F
#define B0110B0000 0x60
#define B0110B0001 0x61
#define B0110B0010 0x62
#define B0110B0011 0x63
#define B0110B0100 0x64
#define B0110B0101 0x65
#define B0110B0110 0x66
#define B0110B0111 0x67
#define B0110B1000 0x68
#define B0110B1001 0x69
#define B0110B1010 0x6A
#define B0110B1011 0x6B
#define B0110B1100 0x6C
#define B0110B1101 0x6D
#define B0110B1110 0x6E
#define B0110B1111 0x6F
#define B0111B0000 0x70
#define B0111B0001 0x71
#define B0111B0010 0x72
#define B0111B0011 0x73
#define B0111B0100 0x74
#define B0111B0101 0x75
#define B0111B0110 0x76
#define B0111B0111 0x77
#define B0111B1000 0x78
#define B0111B1001 0x79
#define B0111B1010 0x7A
#define B0111B1011 0x7B
#define B0111B1100 0x7C
#define B0111B1101 0x7D
#define B0111B1110 0x7E
#define B0111B1111 0x7F
#define B1000B0000 0x80
#define B1000B0001 0x81
#define B1000B0010 0x82
#define B1000B0011 0x83
#define B1000B0100 0x84
#define B1000B0101 0x85
#define B1000B0110 0x86
#define B1000B0111 0x87
#define B1000B1000 0x88
#define B1000B1001 0x89
#define B1000B1010 0x8A
#define B1000B1011 0x8B
#define B1000B1100 0x8C
#define B1000B1101 0x8D
#define B1000B1110 0x8E
#define B1000B1111 0x8F
#define B1001B0000 0x90
#define B1001B0001 0x91
#define B1001B0010 0x92
#define B1001B0011 0x93
#define B1001B0100 0x94
#define B1001B0101 0x95
#define B1001B0110 0x96
#define B1001B0111 0x97
#define B1001B1000 0x98
#define B1001B1001 0x99
#define B1001B1010 0x9A
#define B1001B1011 0x9B
#define B1001B1100 0x9C
#define B1001B1101 0x9D
#define B1001B1110 0x9E
#define B1001B1111 0x9F
#define B1010B0000 0xA0
#define B1010B0001 0xA1
#define B1010B0010 0xA2
#define B1010B0011 0xA3
#define B1010B0100 0xA4
#define B1010B0101 0xA5
#define B1010B0110 0xA6
#define B1010B0111 0xA7
#define B1010B1000 0xA8
#define B1010B1001 0xA9
#define B1010B1010 0xAA
#define B1010B1011 0xAB
#define B1010B1100 0xAC
#define B1010B1101 0xAD
#define B1010B1110 0xAE
#define B1010B1111 0xAF
#define B1011B0000 0xB0
#define B1011B0001 0xB1
#define B1011B0010 0xB2
#define B1011B0011 0xB3
#define B1011B0100 0xB4
#define B1011B0101 0xB5
#define B1011B0110 0xB6
#define B1011B0111 0xB7
#define B1011B1000 0xB8
#define B1011B1001 0xB9
#define B1011B1010 0xBA
#define B1011B1011 0xBB
#define B1011B1100 0xBC
#define B1011B1101 0xBD
#define B1011B1110 0xBE
#define B1011B1111 0xBF
#define B1100B0000 0xC0
#define B1100B0001 0xC1
#define B1100B0010 0xC2
#define B1100B0011 0xC3
#define B1100B0100 0xC4
#define B1100B0101 0xC5
#define B1100B0110 0xC6
#define B1100B0111 0xC7
#define B1100B1000 0xC8
#define B1100B1001 0xC9
#define B1100B1010 0xCA
#define B1100B1011 0xCB
#define B1100B1100 0xCC
#define B1100B1101 0xCD
#define B1100B1110 0xCE
#define B1100B1111 0xCF
#define B1101B0000 0xD0
#define B1101B0001 0xD1
#define B1101B0010 0xD2
#define B1101B0011 0xD3
#define B1101B0100 0xD4
#define B1101B0101 0xD5
#define B1101B0110 0xD6
#define B1101B0111 0xD7
#define B1101B1000 0xD8
#define B1101B1001 0xD9
#define B1101B1010 0xDA
#define B1101B1011 0xDB
#define B1101B1100 0xDC
#define B1101B1101 0xDD
#define B1101B1110 0xDE
#define B1101B1111 0xDF
#define B1110B0000 0xE0
#define B1110B0001 0xE1
#define B1110B0010 0xE2
#define B1110B0011 0xE3
#define B1110B0100 0xE4
#define B1110B0101 0xE5
#define B1110B0110 0xE6
#define B1110B0111 0xE7
#define B1110B1000 0xE8
#define B1110B1001 0xE9
#define B1110B1010 0xEA
#define B1110B1011 0xEB
#define B1110B1100 0xEC
#define B1110B1101 0xED
#define B1110B1110 0xEE
#define B1110B1111 0xEF
#define B1111B0000 0xF0
#define B1111B0001 0xF1
#define B1111B0010 0xF2
#define B1111B0011 0xF3
#define B1111B0100 0xF4
#define B1111B0101 0xF5
#define B1111B0110 0xF6
#define B1111B0111 0xF7
#define B1111B1000 0xF8
#define B1111B1001 0xF9
#define B1111B1010 0xFA
#define B1111B1011 0xFB
#define B1111B1100 0xFC
#define B1111B1101 0xFD
#define B1111B1110 0xFE
#define B1111B1111 0xFF

#endif
