/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __c2d2_h_
#define __c2d2_h_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef C2D_API
#define C2D_API /* define API export as needed */
#endif
#if !defined(int32) && !defined(_INT32_DEFINED)
typedef int                     int32;
#define _INT32_DEFINED
#endif
#if !defined(uint32) && !defined(_UINT32_DEFINED)
typedef unsigned int            uint32;
#define _UINT32_DEFINED
#endif

/*****************************************************************************/
/*********************** Blit definitions *****************************/
/*****************************************************************************/

/* Status codes, returned by any blit function */
typedef enum {
    C2D_STATUS_OK              = 0,
    C2D_STATUS_NOT_SUPPORTED   = 1,
    C2D_STATUS_OUT_OF_MEMORY   = 2,
    C2D_STATUS_INVALID_PARAM   = 3,
    C2D_STATUS_SURFACE_IN_USE  = 4,
} C2D_STATUS;


/* Definitions of color format modes, used together with color formats */
typedef enum {
    C2D_FORMAT_PACK_INTO_32BIT   = (1 <<  8), /* pack into dword if set */
    C2D_FORMAT_SWAP_ENDIANNESS   = (1 <<  9), /* swaps the order */
    C2D_FORMAT_LINEAR_SPACE      = (1 << 10), /* linear color space */
    C2D_FORMAT_PREMULTIPLIED     = (1 << 11), /* alpha premultiplied */
    C2D_FORMAT_INVERT_ALPHA      = (1 << 12), /* inverts alpha */
    C2D_FORMAT_DISABLE_ALPHA     = (1 << 13), /* disables alpha */
    C2D_FORMAT_INTERLACED        = (1 << 14), /* YUV line-interlaced */
    C2D_FORMAT_TRANSPARENT       = (1 << 15), /* YUV 1-bit alpha in Y */
    C2D_FORMAT_MACROTILED        = (1 << 16), /* tiled in macro level */
    C2D_FORMAT_TILED_4x4         = (1 << 17), /* 4x4 tiled format */
    C2D_FORMAT_SWAP_RB           = (1 << 18), /* Swap R & B color components */
    C2D_FORMAT_UBWC_COMPRESSED   = (1 << 23), /* UBWC compressed format */
} C2D_FORMAT_MODE;

/* Definitions of supported RGB formats, used in C2D_RGB_SURFACE_DEF.
 * The bits of each color channel are packed into a machine word
 * representing a single pixel from left to right (MSB to LSB) in the
 * order indicated by format name. For the sub-byte formats the pixels
 * are packed into bytes from left to right (MSbit to LSBit).
 * If the C2D_FORMAT_PACK_INTO_32BIT bit is set, the minimal
 * machine word used for pixel storage is 32-bit and the whole word
 * is reversed if endianness is swapped.
 * If the C2D_FORMAT_SWAP_ENDIANNESS bit is set, the order within a
 * minimal machine word representing a pixel
 * is reversed for both sub-byte and multi-byte formats.
 * If the C2D_FORMAT_LINEAR_SPACE bit is set, the color space of
 * the formats below is considered linear, if applicable.
 * If the C2D_FORMAT_PREMULTIPLIED bit is set, the color channels
 * are premultiplied with the alpha, if applicable.
 * If the C2D_FORMAT_INVERT_ALPHA bit is set, the alpha interpretation
 * is inverted: 0 - opaque, 1 - transparent, if applicable.
 * If the C2D_FORMAT_DISABLE_ALPHA bit is set, the alpha channel serves
 * as a placeholder and is ignored during blit, if applicable.
 * If the C2D_FORMAT_MACROTILED bit is set, the surface is in the
 * tiled format : 64x32 for 8bpp, 32x32 for 16bpp formats  */
typedef enum {
    C2D_COLOR_FORMAT_1            = 0,   /* 1-bit alpha/color expansion */

    C2D_COLOR_FORMAT_2_PALETTE    = 1,   /* 2-bit indices for palette */
    C2D_COLOR_FORMAT_4_PALETTE    = 2,   /* 4-bit indices for palette */
    C2D_COLOR_FORMAT_8_PALETTE    = 3,   /* 8-bit indices for palette */

    C2D_COLOR_FORMAT_2_L          = 4,   /* 2-bit grayscale */
    C2D_COLOR_FORMAT_4_L          = 5,   /* 4-bit grayscale */
    C2D_COLOR_FORMAT_8_L          = 6,   /* 8-bit grayscale */

    C2D_COLOR_FORMAT_2_A          = 7,   /* 2-bit alpha only */
    C2D_COLOR_FORMAT_4_A          = 8,   /* 4-bit alpha only */
    C2D_COLOR_FORMAT_8_A          = 9,   /* 8-bit alpha only */

    C2D_COLOR_FORMAT_444_RGB      = 10,  /* 12-bit colors */
    C2D_COLOR_FORMAT_565_RGB      = 11,  /* 16-bit colors */
    C2D_COLOR_FORMAT_888_RGB      = 12,  /* 24-bit colors */

    C2D_COLOR_FORMAT_1555_ARGB    = 13,  /* 16-bit colors (1-bit alpha) */
    C2D_COLOR_FORMAT_4444_ARGB    = 14,  /* 16-bit colors (4-bit alpha) */
    C2D_COLOR_FORMAT_8565_ARGB    = 15,  /* 24-bit colors (8-bit alpha) */
    C2D_COLOR_FORMAT_8888_ARGB    = 16,  /* 32-bit colors (8-bit alpha) */

    C2D_COLOR_FORMAT_5551_RGBA    = 17,  /* 16-bit colors (1-bit alpha) */
    C2D_COLOR_FORMAT_4444_RGBA    = 18,  /* 16-bit colors (4-bit alpha) */
    C2D_COLOR_FORMAT_5658_RGBA    = 19,  /* 24-bit colors (8-bit alpha) */
    C2D_COLOR_FORMAT_8888_RGBA    = 20,  /* 32-bit colors (8-bit alpha) */

    /* derived RGB color formats (base format + mode bits) */

} C2D_RGB_FORMAT;

/* Definitions of supported YUV formats, used in C2D_YUV_SURFACE_DEF.
 * Each of Y,U,V channels usually takes 1 byte and therefore is
 * individually addressable. The definitions below show how Y,U,V
 * channels are packed into macropixels for each particular format.
 * The order is from left (smaller byte addresses) to right (larger
 * byte addresses). The first three digits (4xx) denote the chroma
 * subsampling in standard YUV notation. The digits in the macropixel
 * denote that the whole block (from the previous digit or from the
 * beginning) has to be repeated the number of times. Underscores
 * between Y,U,V channels are used to describe separate planes for
 * planar YUV formats. Formats are mapped to numbers so that future
 * versions with various YUV permutations are easy to add.
 * If the C2D_FORMAT_INTERLACED bit is set, the line order is
 * interlaced: 0,2,4,...1,3,5... if applicable.
 * If the C2D_FORMAT_TRANSPARENT bit is set, the least significant
 * bit of Y channel serves as alpha: 0 - transparent, 1 - opaque. */
typedef enum {
    C2D_COLOR_FORMAT_411_YYUYYV   = 110, /* packed, 12-bit         */
    C2D_COLOR_FORMAT_411_YUYYVY   = 111, /* packed, 12-bit         */
    C2D_COLOR_FORMAT_411_UYYVYY   = 112, /* packed, 12-bit, "Y411" */
    C2D_COLOR_FORMAT_411_YUYV2Y4  = 116, /* packed, 12-bit         */
    C2D_COLOR_FORMAT_411_UYVY2Y4  = 117, /* packed, 12-bit, "Y41P" */

    C2D_COLOR_FORMAT_422_YUYV     = 120, /* packed, 16-bit, "YUY2" */
    C2D_COLOR_FORMAT_422_UYVY     = 121, /* packed, 16-bit, "UYVY" */
    C2D_COLOR_FORMAT_422_YVYU     = 122, /* packed, 16-bit, "YVYU" */
    C2D_COLOR_FORMAT_422_VYUY     = 123, /* packed, 16-bit         */

    C2D_COLOR_FORMAT_444_YUV      = 130, /* packed, 24-bit         */
    C2D_COLOR_FORMAT_444_UYV      = 131, /* packed, 24-bit, "IYU2" */
    C2D_COLOR_FORMAT_444_AYUV     = 136, /* packed, 24-bit, "AYUV" */

    C2D_COLOR_FORMAT_410_Y_UV     = 150, /* planar, Y + interleaved UV */
    C2D_COLOR_FORMAT_411_Y_UV     = 151, /* planar, Y + interleaved UV */
    C2D_COLOR_FORMAT_420_Y_UV     = 152, /* planar, Y + interleaved UV */
    C2D_COLOR_FORMAT_422_Y_UV     = 153, /* planar, Y + interleaved UV */
    C2D_COLOR_FORMAT_444_Y_UV     = 154, /* planar, Y + interleaved UV */

    C2D_COLOR_FORMAT_410_Y_VU     = 160, /* planar, Y + interleaved VU */
    C2D_COLOR_FORMAT_411_Y_VU     = 161, /* planar, Y + interleaved VU */
    C2D_COLOR_FORMAT_420_Y_VU     = 162, /* planar, Y + interleaved VU */
    C2D_COLOR_FORMAT_422_Y_VU     = 163, /* planar, Y + interleaved VU */
    C2D_COLOR_FORMAT_444_Y_VU     = 164, /* planar, Y + interleaved VU */

    C2D_COLOR_FORMAT_410_Y_U_V    = 170, /* planar, Y + U + V separate */
    C2D_COLOR_FORMAT_411_Y_U_V    = 171, /* planar, Y + U + V separate */
    C2D_COLOR_FORMAT_420_Y_V_U    = 172, /* planar, Y + V + U separate */
    C2D_COLOR_FORMAT_420_Y_U_V    = 173, /* planar, Y + U + V separate */
    C2D_COLOR_FORMAT_422_Y_U_V    = 174, /* planar, Y + U + V separate */
    C2D_COLOR_FORMAT_444_Y_U_V    = 175, /* planar, Y + U + V separate */

    C2D_COLOR_FORMAT_800_Y        = 190, /* planar, Y only, grayscale */

    /* derived YUV color formats (base format + mode bits), FOURCC */

    C2D_COLOR_FORMAT_411_Y411     = 112,
    C2D_COLOR_FORMAT_411_Y41P     = 117,
    C2D_COLOR_FORMAT_411_IY41     = 117 | (1 << 14),
    C2D_COLOR_FORMAT_411_Y41T     = 117 | (1 << 15),

    C2D_COLOR_FORMAT_422_YUY2     = 120,
    C2D_COLOR_FORMAT_422_IUYV     = 121 | (1 << 14),
    C2D_COLOR_FORMAT_422_Y42T     = 121 | (1 << 15),
    C2D_COLOR_FORMAT_444_IYU2     = 131,

    C2D_COLOR_FORMAT_420_NV12     = 152,
    C2D_COLOR_FORMAT_420_NV21     = 162,

    C2D_COLOR_FORMAT_410_YUV9     = 170,
    C2D_COLOR_FORMAT_410_YVU9     = 170,
    C2D_COLOR_FORMAT_411_Y41B     = 171,
    C2D_COLOR_FORMAT_420_YV12     = 172,
    C2D_COLOR_FORMAT_420_IYUV     = 173,
    C2D_COLOR_FORMAT_420_I420     = 173,
    C2D_COLOR_FORMAT_422_YV16     = 174,
    C2D_COLOR_FORMAT_422_Y42B     = 174,

    C2D_COLOR_FORMAT_800_Y800     = 190,

} C2D_YUV_FORMAT;


/* Configuration bits, used in the config_mask field of C2D_OBJECT struct */
typedef enum {
    C2D_SOURCE_RECT_BIT      = (1 <<  0), /* enables source_rect field */
    C2D_MIRROR_H_BIT         = (1 <<  1), /* enables horizontal flipping */
    C2D_MIRROR_V_BIT         = (1 <<  2), /* enables vertical flipping */
    C2D_SOURCE_TILE_BIT      = (1 <<  3), /* enables source surface tiling */
    C2D_TARGET_RECT_BIT      = (1 <<  4), /* enables target_rect field */
    C2D_ROTATE_BIT           = (1 <<  5), /* enables all rotation fields */
    C2D_SCISSOR_RECT_BIT     = (1 <<  6), /* enables scissor_rect field */
    C2D_MASK_SURFACE_BIT     = (1 <<  7), /* enables mask_surface_id field */
    C2D_MASK_ALIGN_BIT       = (1 <<  8), /* aligns mask to source_rect */
    C2D_MASK_SCALE_BIT       = (1 <<  9), /* enables mask surface scaling */
    C2D_MASK_TILE_BIT        = (1 << 10), /* enables mask surface tiling */
    C2D_GLOBAL_ALPHA_BIT     = (1 << 11), /* enables global_alpha field */
    C2D_COLOR_KEY_BIT        = (1 << 12), /* enables color_key field */
    C2D_NO_PIXEL_ALPHA_BIT   = (1 << 13), /* disables source alpha channel */
    C2D_NO_BILINEAR_BIT      = (1 << 14), /* disables bilinear on scaling */
    C2D_NO_ANTIALIASING_BIT  = (1 << 15), /* disables antialiasing on edges */
    C2D_DRAW_LINE_BIT        = (1 << 16), /* enables line drawing with source rectangle */
    C2D_DRAW_LINE_NOLAST     = (1 << 17), /* disable last pixel draw for line */
} C2D_SOURCE_CONFIG;


/* Target configuration bits, defines rotation + mirroring.
 * Mirror is applied prior to rotation if enabled. */
typedef enum {
    C2D_TARGET_MIRROR_H        = (1 << 0), /* horizontal flip */
    C2D_TARGET_MIRROR_V        = (1 << 1), /* vertical flip */
    C2D_TARGET_ROTATE_0        = (0 << 2), /* no rotation */
    C2D_TARGET_ROTATE_90       = (1 << 2), /* 90 degree rotation */
    C2D_TARGET_ROTATE_180      = (2 << 2), /* 180 degree rotation */
    C2D_TARGET_ROTATE_270      = (3 << 2), /* 270 degree rotation, 90 + 180 */
    C2D_TARGET_MASK_ALIGN      = (1 << 4), /* aligns mask to target scissor */
    C2D_TARGET_MASK_SCALE      = (1 << 5), /* enables mask scaling */
    C2D_TARGET_MASK_TILE       = (1 << 6), /* enables mask tiling */
    C2D_TARGET_COLOR_KEY       = (1 << 7), /* enables target_color_key */
    C2D_TARGET_NO_PIXEL_ALPHA  = (1 << 8), /* disables target alpha channel */
} C2D_TARGET_CONFIG;

#define C2D_TARGET_ROTATION_MASK  (C2D_TARGET_ROTATE_90*3)

/* Additional blend modes, can be used with both source and target configs.
   If none of the below is set, the default "SRC over DST" is applied. */
typedef enum {
    C2D_ALPHA_BLEND_SRC_OVER   = (0  << 20), /* Default, Porter-Duff "SRC over DST" */
    C2D_ALPHA_BLEND_SRC        = (1  << 20), /* Porter-Duff "SRC" */
    C2D_ALPHA_BLEND_SRC_IN     = (2  << 20), /* Porter-Duff "SRC in DST" */
    C2D_ALPHA_BLEND_DST_IN     = (3  << 20), /* Porter-Duff "DST in SRC" */
    C2D_ALPHA_BLEND_SRC_OUT    = (4  << 20), /* Porter-Duff "SRC out DST" */
    C2D_ALPHA_BLEND_DST_OUT    = (5  << 20), /* Porter-Duff "DST out SRC" */
    C2D_ALPHA_BLEND_DST_OVER   = (6  << 20), /* Porter-Duff "DST over SRC" */
    C2D_ALPHA_BLEND_SRC_ATOP   = (7  << 20), /* Porter-Duff "SRC ATOP" */
    C2D_ALPHA_BLEND_DST_ATOP   = (8  << 20), /* Porter-Duff "DST ATOP" */
    C2D_ALPHA_BLEND_XOR        = (9  << 20), /* Xor */
    C2D_ALPHA_BLEND_MULTIPLY   = (10 << 20), /* OpenVG "MULTIPLY" */
    C2D_ALPHA_BLEND_SCREEN     = (11 << 20), /* OpenVG "SCREEN" */
    C2D_ALPHA_BLEND_DARKEN     = (12 << 20), /* OpenVG "DARKEN" */
    C2D_ALPHA_BLEND_LIGHTEN    = (13 << 20), /* OpenVG "LIGHTEN" */
    C2D_ALPHA_BLEND_ADDITIVE   = (14 << 20), /* OpenVG "ADDITIVE" */
    C2D_ALPHA_BLEND_DIRECT     = (15 << 20), /* Direct alpha blitting */
    C2D_ALPHA_BLEND_INVERTC    = (16 << 20), /* Invert color */
    C2D_ALPHA_BLEND_NONE       = (1  << 25), /* disables alpha blending */
} C2D_ALPHA_BLEND_MODE;

/* Configuration bits, used in the config_mask field of C2D_OBJECT struct */
typedef enum {
    C2D_OVERRIDE_GLOBAL_TARGET_ROTATE_CONFIG = (1 << 27), /* Overrides TARGET Config */
    C2D_OVERRIDE_TARGET_ROTATE_0             = (0 << 28), /* no rotation             */
    C2D_OVERRIDE_TARGET_ROTATE_90            = (1 << 28), /* 90 degree rotation      */
    C2D_OVERRIDE_TARGET_ROTATE_180           = (2 << 28), /* 180 degree rotation     */
    C2D_OVERRIDE_TARGET_ROTATE_270           = (3 << 28), /* 270 degree rotation     */
} C2D_SOURCE_TARGET_CONFIG;

#define C2D_OVERRIDE_SOURCE_CONFIG_TARGET_ROTATION_SHIFT_MASK  28
#define C2D_OVERRIDE_TARGET_CONFIG_TARGET_ROTATION_SHIFT_MASK  2


/* Surface caps enumeration */
typedef enum {
    C2D_SOURCE          = (1 << 0), /* allows to use as a source */
    C2D_TARGET          = (1 << 1), /* allows to use as a target */
    C2D_MASK            = (1 << 2), /* allows to use as a mask */
    C2D_PALETTE         = (1 << 3), /* allows to use as a palette */
} C2D_SURFACE_BITS;

/* Surface type enumeration */
typedef enum {
    C2D_SURFACE_RGB_HOST        = 1, /* Host memory RGB surface */
    C2D_SURFACE_RGB_EXT         = 2, /* External memory RGB surface */
    C2D_SURFACE_YUV_HOST        = 3, /* Host memory YUV surface */
    C2D_SURFACE_YUV_EXT         = 4, /* External memory YUV surface */
    C2D_SURFACE_WITH_PHYS       = (1<<3), /* physical address already mapped */
                                        /* this bit is valid with HOST types */
    C2D_SURFACE_WITH_PHYS_DUMMY = (1<<4), /* physical address already mapped */
                                        /* this bit is valid with HOST types */
} C2D_SURFACE_TYPE;

/* Structure for registering a RGB buffer as a blit surface */
typedef struct {
    uint32 format;   /* RGB color format plus additional mode bits */
    uint32 width;    /* defines width in pixels */
    uint32 height;   /* defines height in pixels */
    void  *buffer;   /* pointer to the RGB buffer */
    void  *phys;     /* physical address */
    int32  stride;   /* defines stride in bytes, negative stride is allowed */
} C2D_RGB_SURFACE_DEF;

/* Structure for registering a YUV plane(s) as a blit surface */
typedef struct {
    uint32 format;   /* YUV color format plus additional mode bits */
    uint32 width;    /* defines width in pixels */
    uint32 height;   /* defines height in pixels */
    void  *plane0;  /* holds the whole buffer if YUV format is not planar */
    void  *phys0;   /* physical address */
    int32  stride0; /* stride in bytes if YUV format is not planar */
    void  *plane1;  /* holds UV or VU plane for planar interleaved */
    void  *phys1;   /* physical address */
    int32  stride1; /* stride for UV or VU plane for planar interleaved */
    void  *plane2;  /* holds the 3. plane, ignored if YUV format is not planar */
    void  *phys2;    /* physical address */
    int32  stride2; /* stride for the 3. plane, ignored if YUV format is not planar */
} C2D_YUV_SURFACE_DEF;


/* Rectangle definition */
typedef struct {
    int32 x;        /* upper-left x */
    int32 y;        /* upper-left y */
    int32 width;    /* width */
    int32 height;   /* height */
} C2D_RECT;

/* C2D_OBJECT encapsulates the blit parameters for a source surface.
 * The fg_color defines color in target format for bits equal to 1
 * in the source C2D_COLOR_FORMAT_1 format. It also defines rendering
 * color for all alpha-only source formats. If the surface_id is 0
 * the fg_color defines a constant fill color used instead of the surface.
 * The bg_color defines color in target format for bits equal to 0
 * in the source C2D_COLOR_FORMAT_1 format, otherwise both are ignored.
 * The palette_id is used for all palette source formats, otherwise ignored.

 * The source_rect first defines the content of the source surface,
 * it is then horizontally/vertically flipped if C2D_MIRROR_*_BIT is set,
 * then scaled with bilinear interpolation to exactly fit target_rect
 * or repeated across target_rect if C2D_SOURCE_TILE_BIT is set,
 * target_rect is then rotated clockwise by an arbitrary angle in degrees
 * around the rot_orig_x/y, defined relative to target_rect's top left point,
 * and then clipped to scissor_rect defined in target coordinate system.

 * Finally alpha blending is applied before pixels get written into the target.
 * Surface's pixel alpha is combined with mask alpha and with global alpha.
 * Mask surface follows all transformations applied to the source surface.
 * Source color key defines transparent color, applied together with alpha. */
typedef struct C2D_OBJECT_STR {
    uint32 surface_id;      /* source surface */

    uint32 fg_color;        /* foreground color */
    uint32 bg_color;        /* background color */
    uint32 palette_id;      /* one-dimensional horizontal palette surface */

    uint32 config_mask;     /* defines which fields below are enabled */

    C2D_RECT source_rect;  /* region of the source surface,   16.16 fp */
    C2D_RECT target_rect;  /* position and scaling in target, 16.16 fp */

    int32 rot_orig_x;       /* rotation origin relative to target_rect's... */
    int32 rot_orig_y;       /* ...top left point,     both are 16.16 fp */
    int32 rotation;         /* clock-wise rotation in degrees, 16.16 fp */

    C2D_RECT scissor_rect; /* defines the clip rectangle in target surface */

    uint32 mask_surface_id; /* source alpha-mask surface */
    uint32 global_alpha;    /* 0 = fully transparent, 255 = fully opaque */
    uint32 color_key;       /* transparent color for the source surface */

    struct C2D_OBJECT_STR *next; /* pointer to the next object or NULL */
} C2D_OBJECT;

/* Configuration bits, driver capabilities used by 2Dapplications */
typedef enum {
    C2D_DRIVER_SUPPORTS_GLOBAL_ALPHA_OP           = (1 << 0),
    C2D_DRIVER_SUPPORTS_TILE_OP                   = (1 << 1),
    C2D_DRIVER_SUPPORTS_COLOR_KEY_OP              = (1 << 2),
    C2D_DRIVER_SUPPORTS_NO_PIXEL_ALPHA_OP         = (1 << 3),
    C2D_DRIVER_SUPPORTS_TARGET_ROTATE_OP          = (1 << 4),
    C2D_DRIVER_SUPPORTS_ANTI_ALIASING_OP          = (1 << 5), /* antialiasing */
    C2D_DRIVER_SUPPORTS_BILINEAR_FILTER_OP        = (1 << 6),
    C2D_DRIVER_SUPPORTS_LENS_CORRECTION_OP        = (1 << 7),
    C2D_DRIVER_SUPPORTS_OVERRIDE_TARGET_ROTATE_OP = (1 << 8),
    C2D_DRIVER_SUPPORTS_SHADER_BLOB_OP            = (1 << 9),
    C2D_DRIVER_SUPPORTS_MASK_SURFACE_OP           = (1 << 10), /* mask surface */
    C2D_DRIVER_SUPPORTS_MIRROR_H_OP               = (1 << 11), /* horizontal flip */
    C2D_DRIVER_SUPPORTS_MIRROR_V_OP               = (1 << 12), /* vertical flip */
    C2D_DRIVER_SUPPORTS_SCISSOR_RECT_OP           = (1 << 13),
    C2D_DRIVER_SUPPORTS_SOURCE_RECT_OP            = (1 << 14),
    C2D_DRIVER_SUPPORTS_TARGET_RECT_OP            = (1 << 15),
    C2D_DRIVER_SUPPORTS_ROTATE_OP                 = (1 << 16), /* all rotations */
    C2D_DRIVER_SUPPORTS_FLUSH_WITH_FENCE_FD_OP    = (1 << 17), /* all rotations */
    C2D_DRIVER_SUPPORTS_UBWC_COMPRESSED_OP        = (1 << 18), /* UBWC Compression */
    C2D_DRIVER_SUPPORTS_ALL_CAPABILITIES_OP       = ((0xFFFFFFFF) >> (31 - 18)) /* mask for all capabilities supported */
} C2D_DRIVER_CAPABILITIES;

/* 2D driver workaround bits used by the 2D applications */
typedef enum {
    C2D_DRIVER_WORKAROUND_NONE  = 0, /* NO workaround */
    C2D_DRIVER_WORKAROUND_SWAP_UV_FOR_YUV_TARGET  = (1 << 0), /* Swap UV when this flag set */
} C2D_DRIVER_WORKAROUND;

/* Structure to query Driver information */
typedef struct {
    uint32 capabilities_mask;
    uint32 workaround_mask;
    uint32 reserved1;
    uint32 reserved2;
    uint32 reserved3;
} C2D_DRIVER_INFO;

/* Structure to query Driver information */
typedef struct {
    uint32          max_surface_template_needed;
    uint32          reserved1;
    uint32          reserved2;
    uint32          reserved3;
} C2D_DRIVER_SETUP_INFO;

/*****************************************************************************/
/**************************** C2D API 2.0 ********************************/
/*****************************************************************************/

/******************************************************************************
 * Functions to create/destroy surfaces */

/* Creates a generic blit surface according to its type.
 * Pass a combination of desired surface bits according to planned usage.
 * Accepted values for surface_bits may include bits from C2D_SURFACE_BITS,
 * and also from C2D_DISPLAY for compatibility with HW display controller.
 * For host memory types the memory is preallocated outside the API
 * and should remain valid until surface is destroyed.
 * For external memory types the memory is allocated within API.
 * On success, the non-zero surface identifier is returned.
 * All numbers greater that 0 are valid surface identifiers, 0 is invalid.

 * Host memory RGB surface:
 * surface_type       = C2D_SURFACE_RGB_HOST
 * surface_definition = C2D_RGB_SURFACE_DEF
 * all fields in definition structure should be set

 * External memory RGB surface:
 * surface_type       = C2D_SURFACE_RGB_EXT
 * surface_definition = C2D_RGB_SURFACE_DEF
 * buffer field in definition structure is ignored

 * Host memory YUV surface:
 * surface_type       = C2D_SURFACE_YUV_HOST
 * surface_definition = C2D_YUV_SURFACE_DEF
 * one or all plane and stride fields in definition structure
 * should be set depending on whether the format is planar or not

 * External memory YUV surface:
 * surface_type       = C2D_SURFACE_YUV_EXT
 * surface_definition = C2D_YUV_SURFACE_DEF
 * all plane and stride fields in definition structure are ignored */
C2D_API C2D_STATUS c2dCreateSurface( uint32 *surface_id,
                         uint32 surface_bits,
                         C2D_SURFACE_TYPE surface_type,
                         void *surface_definition );

/* Requests properties of the specified surface. */
C2D_API C2D_STATUS c2dQuerySurface( uint32 surface_id,
                         uint32 *surface_bits,
                         C2D_SURFACE_TYPE *surface_type,
                         uint32 *width, uint32 *height,
                         uint32 *format );

/* Destroys a generic blit surface.
 * For external memory surfaces also deallocates the memory.
 * It is safe to free any external resources associated with a given
 * surface on c2dCreateSurface call after this function returns. */
C2D_API C2D_STATUS c2dDestroySurface( uint32 surface_id );


/******************************************************************************
 * Functions to modify/exchange surface data */

/* The format of fill_color is the same as color format being used
 * for specified surface. If fill_rect is NULL the whole surface is filled.
 * Alpha-blending is not performed while filling.
 * The operation is complete when function returns. */
C2D_API C2D_STATUS c2dFillSurface( uint32 surface_id,
                         uint32 fill_color,
                         C2D_RECT *fill_rect );

/* Writes data located in host memory into the specified surface.
 * The chunk of host memory is identified with surface_type and
 * surface_definition, no surface registration needed in this case.
 * Only C2D_SURFACE_RGB_HOST, C2D_SURFACE_YUV_HOST are accepted.
 * If only part of the host memory buffer should be loaded, it should
 * be configured in surface_definition using width, height and stride.
 * The x and y are defined in target surface coordinate space.
 * Color conversion has to be done, if color formats differ.
 * Alpha-blending is not performed while writing.
 * The operation is complete when function returns. */
C2D_API C2D_STATUS c2dWriteSurface( uint32 surface_id,
                         C2D_SURFACE_TYPE surface_type,
                         void *surface_definition,
                         int32 x, int32 y );

/* Reads data from the specified surface into the host memory.
 * The chunk of host memory is identified with surface_type and
 * surface_definition, no surface registration needed in this case.
 * Only C2D_SURFACE_RGB_HOST, C2D_SURFACE_YUV_HOST are accepted.
 * If only part of the surface should be read, it should
 * be configured in surface_definition using width, height and stride.
 * The x and y are defined in source surface coordinate space.
 * Color conversion has to be done, if color formats differ.
 * Alpha-blending is not performed while reading.
 * The operation is complete when function returns. */
C2D_API C2D_STATUS c2dReadSurface( uint32 surface_id,
                         C2D_SURFACE_TYPE surface_type,
                         void *surface_definition,
                         int32 x, int32 y );

/* Notifies c2d imlementation that surface has been updated from outside the API,
 * if updated_rect is NULL then the whole surface has been updated. */
C2D_API C2D_STATUS c2dSurfaceUpdated( uint32 surface_id,
                         C2D_RECT *updated_rect );

/* Updates surface information.
 * Could be called only for host surfaces set with parameter "C2D_SURFACE_WITH_PHYS".
 * Count for surface planes have to be same than for already allocated surface */
C2D_API C2D_STATUS c2dUpdateSurface( uint32 surface_id,
                         uint32 surface_bits,
                         C2D_SURFACE_TYPE surface_type,
                         void *surface_definition );

/******************************************************************************
 * Functions to do actual blit */

/* Draw a list of blit objects into the given target.
 * The target_config is a bitwise OR of values from C2D_TARGET_CONFIG.
 * The target transformation creates the effect that target surface
 * is transformed before the blit and then transformed back
 * after blit, however no physical target transform is performed.
 * The objects_list is a linked list of blit objects, no more
 * than num_objects is drawn from the given list.
 * If num_objects is 0, the whole list is drawn.
 * The blit is not guaranteed to complete after function returns. */
C2D_API C2D_STATUS c2dDraw( uint32 target_id,
                         uint32 target_config, C2D_RECT *target_scissor,
                         uint32 target_mask_id, uint32 target_color_key,
                         C2D_OBJECT *objects_list, uint32 num_objects );


/* timstamp set in the blit commands flush */
typedef void*                   c2d_ts_handle;

/* Forces any pending blit to complete for a given target.
 * Non-blocking. All input surfaces for this target except those
 * which are shared with other targets are expected to be immediately
 * writable after client has been waiting returned timestamp with
 * c2dWaitTimestamp funtion or c2dFinish has been called for same target */
C2D_API C2D_STATUS c2dFlush( uint32 target_id, c2d_ts_handle *timestamp);


/* Waits the pending timestamp */
C2D_API C2D_STATUS c2dWaitTimestamp( c2d_ts_handle timestamp );


/* Forces any pending blit to complete for a given target.
 * Blocking version, returns when blit is done.
 * All input surfaces for this target except those which are shared with
 * other targets are expected to be immediately
 * writable after this function returns. */
C2D_API C2D_STATUS c2dFinish( uint32 target_id );


/*****************************************************************************/
/****************************** Display API **********************************/
/*****************************************************************************/


/* Display input enumeration */
typedef enum {
    C2D_DISPLAY_INPUT_0      = 0,       /*!< default input */
    C2D_DISPLAY_INPUT_1      = (1<<16), /*!< Overlay 1     */
    C2D_DISPLAY_INPUT_2      = (1<<17), /*!< Overlay 2...    */
} C2D_DISPLAY_INPUT;


/******************************************************************************
 * Functions for display output. */

/* Functionality described in this section is optional and is
 * provided only for the cases when blit HW
 * is tightly bound to the display controller. */

/* Display enumeration, may also be used in surface caps */
typedef enum {
    C2D_DISPLAY_MAIN         = (1 << 10), /* main display */
    C2D_DISPLAY_SECONDARY    = (1 << 11), /* secondary display */
    C2D_DISPLAY_TV_OUT       = (1 << 12), /* tv-out */
} C2D_DISPLAY;

/* Display window enumeration */
typedef enum {
    C2D_DISPLAY_OVERLAY      = C2D_DISPLAY_INPUT_1, /*!< Overlay window bit. This defines display input.
                                                When defined the surface is set on the overlay window
                                                otherwise the surface is set on the background window. */
} C2D_DISPLAY_WINDOW;                    /*!< Window bit set with display parameter */


/* Display update modes */
typedef enum {
    C2D_DISPLAY_MODE_TEAR_SYNC   = (1 << 0), /* enables tearing sync */
    C2D_DISPLAY_MODE_SURF_REMOVE = (1 << 1), /* Remove surface from given display + input */
} C2D_DISPLAY_MODE;


/* Sets the given surface as a current display front buffer.
 * Several displays can be specified as an output if supported.
 * Still only one input can be specified at a time fro display/displays.
 * The surface remains shown until it gets replaced with another one. */
C2D_API C2D_STATUS c2dDisplaySetSurface( uint32 display,
                         uint32 surface_id, uint32 mode );

/* Returns the current surface for a particular display.
 * Only one display can be specified at a time.
 * The latest surface set with compDisplaySetSurface or
 * the default pre-allocated surface is returned. */
C2D_API C2D_STATUS c2dDisplayGetSurface( uint32 display,
                         uint32 *surface_id );

/* Returns the properties for a particular display.
 * Only one display can be specified at a time. */
C2D_API C2D_STATUS c2dDisplayGetProperties( uint32 display,
                         uint32 *width, uint32 *height,
                         uint32 *format );

/* Sets the properties for a particular display input.
 * Only one display + input can be specified at a time.
 * C2D_OBJECT used to set input rect(target rect),
 * blending operations, rotation...etc for display source */
C2D_API C2D_STATUS c2dDisplaySetObject( uint32 display,
                         uint32 target_config, uint32 target_color_key,
                         C2D_OBJECT * c2dObject, uint32 mode);

/* allows user to map a memory region to the gpu. only supported on linux
 * mem_fd is the fd of the memory region, hostptr is the host pointer to the region,
 * len and offset are the size and offset of the memory.
 * flags is one of the memory types supported by gsl
 * gpaddr is passed by refernce back to the user
 */
C2D_API C2D_STATUS c2dMapAddr ( int mem_fd, void * hostptr, uint32 len, uint32 offset, uint32 flags, void ** gpuaddr);

/* allows user to unmap memory region mapped by c2dMapAddr.
 * gpaddr is the gpuaddr to unmap */
C2D_API C2D_STATUS c2dUnMapAddr (void * gpuaddr);

/* allows user to query driver capabilities.
 * driver_info is the information about driver */
C2D_API C2D_STATUS c2dGetDriverCapabilities( C2D_DRIVER_INFO * driver_info);

/* create a fence fd for the timestamp */
C2D_API C2D_STATUS c2dCreateFenceFD( uint32 target_id, c2d_ts_handle timestamp, int32 *fd);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __c2d2_h_ */
