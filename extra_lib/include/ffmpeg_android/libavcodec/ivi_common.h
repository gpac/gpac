/*
 * common functions for Indeo Video Interactive codecs (Indeo4 and Indeo5)
 *
 * Copyright (c) 2009 Maxim Poliakovski
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file libavcodec/ivi_common.h
 * This file contains structures and macros shared by both Indeo4 and
 * Indeo5 decoders.
 */

#ifndef AVCODEC_IVI_COMMON_H
#define AVCODEC_IVI_COMMON_H

#include "avcodec.h"
#include "get_bits.h"
#include <stdint.h>

#define IVI_DEBUG 0

#define IVI_VLC_BITS 13 ///< max number of bits of the ivi's huffman codes

/**
 *  huffman codebook descriptor
 */
typedef struct {
    int32_t     num_rows;
    uint8_t     xbits[16];
} IVIHuffDesc;

/**
 *  macroblock/block huffman table descriptor
 */
typedef struct {
    int32_t     tab_sel;    /// index of one of the predefined tables
                            /// or "7" for custom one
    VLC         *tab;       /// pointer to the table associated with tab_sel

    //! the following are used only when tab_sel == 7
    IVIHuffDesc cust_desc;  /// custom Huffman codebook descriptor
    VLC         cust_tab;   /// vlc table for custom codebook
} IVIHuffTab;

enum {
    IVI_MB_HUFF   = 0,      /// Huffman table is used for coding macroblocks
    IVI_BLK_HUFF  = 1       /// Huffman table is used for coding blocks
};

extern VLC ff_ivi_mb_vlc_tabs [8]; ///< static macroblock Huffman tables
extern VLC ff_ivi_blk_vlc_tabs[8]; ///< static block Huffman tables


/**
 *  run-value (RLE) table descriptor
 */
typedef struct {
    uint8_t     eob_sym; ///< end of block symbol
    uint8_t     esc_sym; ///< escape symbol
    uint8_t     runtab[256];
    int8_t      valtab[256];
} RVMapDesc;

extern const RVMapDesc ff_ivi_rvmap_tabs[9];


/**
 *  information for Indeo macroblock (16x16, 8x8 or 4x4)
 */
typedef struct {
    int16_t     xpos;
    int16_t     ypos;
    uint32_t    buf_offs; ///< address in the output buffer for this mb
    uint8_t     type;     ///< macroblock type: 0 - INTRA, 1 - INTER
    uint8_t     cbp;      ///< coded block pattern
    uint8_t     q_delta;  ///< quant delta
    int8_t      mv_x;     ///< motion vector (x component)
    int8_t      mv_y;     ///< motion vector (y component)
} IVIMbInfo;


/**
 *  information for Indeo tile
 */
typedef struct {
    int         xpos;
    int         ypos;
    int         width;
    int         height;
    int         is_empty;  ///< = 1 if this tile doesn't contain any data
    int         data_size; ///< size of the data in bytes
    int         num_MBs;   ///< number of macroblocks in this tile
    IVIMbInfo   *mbs;      ///< array of macroblock descriptors
    IVIMbInfo   *ref_mbs;  ///< ptr to the macroblock descriptors of the reference tile
} IVITile;


/**
 *  information for Indeo wavelet band
 */
typedef struct {
    int             plane;          ///< plane number this band belongs to
    int             band_num;       ///< band number
    int             width;
    int             height;
    const uint8_t   *data_ptr;      ///< ptr to the first byte of the band data
    int             data_size;      ///< size of the band data
    int16_t         *buf;           ///< pointer to the output buffer for this band
    int16_t         *ref_buf;       ///< pointer to the reference frame buffer (for motion compensation)
    int16_t         *bufs[3];       ///< array of pointers to the band buffers
    int             pitch;          ///< pitch associated with the buffers above
    int             is_empty;       ///< = 1 if this band doesn't contain any data
    int             mb_size;        ///< macroblock size
    int             blk_size;       ///< block size
    int             is_halfpel;     ///< precision of the motion compensation: 0 - fullpel, 1 - halfpel
    int             inherit_mv;     ///< tells if motion vector is inherited from reference macroblock
    int             inherit_qdelta; ///< tells if quantiser delta is inherited from reference macroblock
    int             qdelta_present; ///< tells if Qdelta signal is present in the bitstream (Indeo5 only)
    int             quant_mat;      ///< dequant matrix index
    int             glob_quant;     ///< quant base for this band
    const uint8_t   *scan;          ///< ptr to the scan pattern

    IVIHuffTab      blk_vlc;        ///< vlc table for decoding block data

    uint16_t        *dequant_intra; ///< ptr to dequant tables for intra blocks
    uint16_t        *dequant_inter; ///< ptr dequant tables for inter blocks
    int             num_corr;       ///< number of correction entries
    uint8_t         corr[61*2];     ///< rvmap correction pairs
    int             rvmap_sel;      ///< rvmap table selector
    RVMapDesc       *rv_map;        ///< ptr to the RLE table for this band
    int             num_tiles;      ///< number of tiles in this band
    IVITile         *tiles;         ///< array of tile descriptors
    void (*inv_transform)(const int32_t *in, int16_t *out, uint32_t pitch, const uint8_t *flags); ///< inverse transform function pointer
    void (*dc_transform) (const int32_t *in, int16_t *out, uint32_t pitch, int blk_size);   ///< dc transform function pointer, it may be NULL
    int             is_2d_trans;    ///< 1 indicates that the two-dimensional inverse transform is used
    int32_t         checksum;       ///< for debug purposes
    int             checksum_present;
    int             bufsize;        ///< band buffer size in bytes
    const uint8_t   *intra_base;    ///< quantization matrix for intra blocks
    const uint8_t   *inter_base;    ///< quantization matrix for inter blocks
    const uint8_t   *intra_scale;   ///< quantization coefficient for intra blocks
    const uint8_t   *inter_scale;   ///< quantization coefficient for inter blocks
} IVIBandDesc;


/**
 *  color plane (luma or chroma) information
 */
typedef struct {
    uint16_t    width;
    uint16_t    height;
    uint8_t     num_bands;  ///< number of bands this plane subdivided into
    IVIBandDesc *bands;     ///< array of band descriptors
} IVIPlaneDesc;


typedef struct {
    uint16_t    pic_width;
    uint16_t    pic_height;
    uint16_t    chroma_width;
    uint16_t    chroma_height;
    uint16_t    tile_width;
    uint16_t    tile_height;
    uint8_t     luma_bands;
    uint8_t     chroma_bands;
} IVIPicConfig;

/** compares some properties of two pictures */
static inline int ivi_pic_config_cmp(IVIPicConfig *str1, IVIPicConfig *str2)
{
    return (str1->pic_width    != str2->pic_width    || str1->pic_height    != str2->pic_height    ||
            str1->chroma_width != str2->chroma_width || str1->chroma_height != str2->chroma_height ||
            str1->tile_width   != str2->tile_width   || str1->tile_height   != str2->tile_height   ||
            str1->luma_bands   != str2->luma_bands   || str1->chroma_bands  != str2->chroma_bands);
}

/** calculate number of tiles in a stride */
#define IVI_NUM_TILES(stride, tile_size) (((stride) + (tile_size) - 1) / (tile_size))

/** calculate number of macroblocks in a tile */
#define IVI_MBs_PER_TILE(tile_width, tile_height, mb_size) \
    ((((tile_width) + (mb_size) - 1) / (mb_size)) * (((tile_height) + (mb_size) - 1) / (mb_size)))

/** convert unsigned values into signed ones (the sign is in the LSB) */
#define IVI_TOSIGNED(val) (-(((val) >> 1) ^ -((val) & 1)))

/** scales motion vector */
static inline int ivi_scale_mv(int mv, int mv_scale)
{
    return (mv + (mv > 0) + (mv_scale - 1)) >> mv_scale;
}

/**
 *  Generates a huffman codebook from the given descriptor
 *  and converts it into the FFmpeg VLC table.
 *
 *  @param cb   [in]  pointer to codebook descriptor
 *  @param vlc  [out] where to place the generated VLC table
 *  @param flag [in]  flag: 1 - for static or 0 for dynamic tables
 *  @return     result code: 0 - OK, -1 = error (invalid codebook descriptor)
 */
int  ff_ivi_create_huff_from_desc(const IVIHuffDesc *cb, VLC *vlc, int flag);

/**
 * Initializes static codes used for macroblock and block decoding.
 */
void ff_ivi_init_static_vlc(void);

/**
 *  Decodes a huffman codebook descriptor from the bitstream
 *  and selects specified huffman table.
 *
 *  @param gb           [in,out] the GetBit context
 *  @param desc_coded   [in] flag signalling if table descriptor was coded
 *  @param which_tab    [in] codebook purpose (IVI_MB_HUFF or IVI_BLK_HUFF)
 *  @param huff_tab     [out] pointer to the descriptor of the selected table
 *  @param avctx        [in] AVCodecContext pointer
 *  @return             zero on success, negative value otherwise
 */
int  ff_ivi_dec_huff_desc(GetBitContext *gb, int desc_coded, int which_tab,
                          IVIHuffTab *huff_tab, AVCodecContext *avctx);

/**
 *  Compares two huffman codebook descriptors.
 *
 *  @param desc1    [in] ptr to the 1st descriptor to compare
 *  @param desc2    [in] ptr to the 2nd descriptor to compare
 *  @return         comparison result: 0 - equal, 1 - not equal
 */
int  ff_ivi_huff_desc_cmp(const IVIHuffDesc *desc1, const IVIHuffDesc *desc2);

/**
 *  Copies huffman codebook descriptors.
 *
 *  @param dst  [out] ptr to the destination descriptor
 *  @param src  [in]  ptr to the source descriptor
 */
void ff_ivi_huff_desc_copy(IVIHuffDesc *dst, const IVIHuffDesc *src);

/**
 *  Initializes planes (prepares descriptors, allocates buffers etc).
 *
 *  @param planes       [in,out] pointer to the array of the plane descriptors
 *  @param cfg          [in] pointer to the ivi_pic_config structure describing picture layout
 *  @return             result code: 0 - OK
 */
int  ff_ivi_init_planes(IVIPlaneDesc *planes, const IVIPicConfig *cfg);

/**
 *  Frees planes, bands and macroblocks buffers.
 *
 *  @param planes       [in] pointer to the array of the plane descriptors
 */
void ff_ivi_free_buffers(IVIPlaneDesc *planes);

/**
 *  Initializes tile and macroblock descriptors.
 *
 *  @param planes       [in,out] pointer to the array of the plane descriptors
 *  @param tile_width   [in]     tile width
 *  @param tile_height  [in]     tile height
 *  @return             result code: 0 - OK
 */
int  ff_ivi_init_tiles(IVIPlaneDesc *planes, int tile_width, int tile_height);

/**
 *  Decodes size of the tile data.
 *  The size is stored as a variable-length field having the following format:
 *  if (tile_data_size < 255) than this field is only one byte long
 *  if (tile_data_size >= 255) than this field four is byte long: 0xFF X1 X2 X3
 *  where X1-X3 is size of the tile data
 *
 *  @param gb   [in,out] the GetBit context
 *  @return     size of the tile data in bytes
 */
int  ff_ivi_dec_tile_data_size(GetBitContext *gb);

/**
 *  Decodes block data:
 *  extracts huffman-coded transform coefficients from the bitstream,
 *  dequantizes them, applies inverse transform and motion compensation
 *  in order to reconstruct the picture.
 *
 *  @param gb   [in,out] the GetBit context
 *  @param band [in]     pointer to the band descriptor
 *  @param tile [in]     pointer to the tile descriptor
 *  @return     result code: 0 - OK, -1 = error (corrupted blocks data)
 */
int  ff_ivi_decode_blocks(GetBitContext *gb, IVIBandDesc *band, IVITile *tile);

/**
 *  Handles empty tiles by performing data copying and motion
 *  compensation respectively.
 *
 *  @param avctx    [in] ptr to the AVCodecContext
 *  @param band     [in] pointer to the band descriptor
 *  @param tile     [in] pointer to the tile descriptor
 *  @param mv_scale [in] scaling factor for motion vectors
 */
void ff_ivi_process_empty_tile(AVCodecContext *avctx, IVIBandDesc *band,
                               IVITile *tile, int32_t mv_scale);

/**
 *  Converts and outputs the current plane.
 *  This conversion is done by adding back the bias value of 128
 *  (subtracted in the encoder) and clipping the result.
 *
 *  @param plane        [in]  pointer to the descriptor of the plane being processed
 *  @param dst          [out] pointer to the buffer receiving converted pixels
 *  @param dst_pitch    [in]  pitch for moving to the next y line
 */
void ff_ivi_output_plane(IVIPlaneDesc *plane, uint8_t *dst, int dst_pitch);

#if IVI_DEBUG
/**
 *  Calculates band checksum from band data.
 */
uint16_t ivi_calc_band_checksum (IVIBandDesc *band);

/**
 *  Verifies that band data lies in range.
 */
int ivi_check_band (IVIBandDesc *band, const uint8_t *ref, int pitch);
#endif

#endif /* AVCODEC_IVI_COMMON_H */
