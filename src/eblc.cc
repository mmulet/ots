// Copyright (c) 2011-2021 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eblc.h"
#include "ebdt.h"

#include <limits>
#include <vector>

// EBLC - Embedded Bitmap Location Table
// http://www.microsoft.com/typography/otspec/eblc.htm

#define TABLE_NAME "EBLC"

namespace
{

    bool ParseIndexSubTable(const ots::Font *font,
                            ots::OpenTypeEBDT *ebdt,
                            uint8_t bit_depth,
                            uint16_t first_glyph_index,
                            uint16_t last_glyph_index,
                            const uint8_t *data, size_t length)
    {
        ots::Buffer table(data, length);
        uint16_t index_format = 0;
        uint16_t image_format = 0;
        uint32_t ebdt_table_image_data_offset = 0;
        if (!table.ReadU16(&index_format) ||
            !table.ReadU16(&image_format) ||
            !table.ReadU32(&ebdt_table_image_data_offset))
        {
            return OTS_FAILURE_MSG("Failed to read IndexSubTable");
        }

        // @TODO index_format and image_format can be contradictory
        // ie, a constatnt metric index format can have a variable metric image format
        //  (They can both provide metrics) is this allowed?
        switch (index_format)
        {
        /**
         * @brief IndexSubTable1: variable-metrics glyphs with 4-byte offsets
         *
         */
        case 1:
        {
            /**
             * From spec:
             * sbitOffsets[glyphIndex] + imageDataOffset = glyphData
             * sizeOfArray = (lastGlyph - firstGlyph + 1) + 1 + 1 pad if needed
             *
             */
            // uint32_t sizeOfArray = last_glyph_index - first_glyph_index + 1 + 1 + ((last_glyph_index - first_glyph_index + 1) % 2 ? 0 : 1);
            uint32_t number_of_glyphs = last_glyph_index - first_glyph_index + 1;
            uint32_t this_glyph_sbit_offset = 0;
            uint32_t next_glyph_sbit_offset = 0;

            if (!table.ReadU32(&this_glyph_sbit_offset))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }

            for (uint32_t glyphIndex = 0; glyphIndex < number_of_glyphs; glyphIndex++)
            {
                if (!table.ReadU32(&next_glyph_sbit_offset))
                {
                    return OTS_FAILURE_MSG("Failed to read IndexSubTable");
                }
                int32_t image_size = next_glyph_sbit_offset - this_glyph_sbit_offset;
                this_glyph_sbit_offset = next_glyph_sbit_offset;
                if (image_size < 0)
                {
                    return OTS_FAILURE_MSG("Offsets not in orderInvalid image size %d", image_size);
                }
                if (image_size == 0)
                {
                    /**
                     * @brief The spec says that image-size 0 is used
                     * to skip glyphs
                     *
                     */
                    continue;
                }
                uint32_t unsigned_image_size = static_cast<uint32_t>(image_size);
                uint32_t out_image_size = 0;

                auto glyphDataOffset = this_glyph_sbit_offset + ebdt_table_image_data_offset;

                if (!ebdt->ParseGlyphBitmapDataWithVariableMetrics(image_format,
                                                                   glyphDataOffset,
                                                                   bit_depth,
                                                                   &out_image_size))
                {
                    return OTS_FAILURE_MSG("Failed to parse glyph bitmap data");
                }
                if (out_image_size != unsigned_image_size)
                {
                    return OTS_FAILURE_MSG("Image size %d does not match expected size %d", out_image_size, unsigned_image_size);
                }
            }
            break;
        }
        break;
            /**
             *  IndexSubTable2: all glyphs have identical metrics
             *
             */
        case 2:
        {
            uint32_t image_size;
            if (!table.ReadU32(&image_size))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable, image size");
            }
            // Parsing BigGlyphMetrics
            ots::ebdt::BigGlyphMetrics metrics;

            if (!ots::ebdt::ParseBigGlyphMetrics(table, &metrics))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable, image size");
            }
            uint32_t num_glyphs = last_glyph_index - first_glyph_index + 1;
            /**
             * @brief @TODO does out_image size have to match image_size?
             *
             */
            uint32_t __unused_out_image_size = 0;
            for (uint32_t i = 0; i < num_glyphs; i++)
            {
                uint32_t glyphDataOffset = ebdt_table_image_data_offset + image_size * i;
                if (!ebdt->ParseGlyphBitmapDataWithConstantMetrics(image_format,
                                                                   glyphDataOffset,
                                                                   bit_depth,
                                                                   metrics.width,
                                                                   metrics.height,
                                                                   &__unused_out_image_size))
                {
                    return OTS_FAILURE_MSG("Failed to parse glyph bitmap data");
                }
            }

            break;
        }

        // IndexSubTable3: variable-metrics glyphs with 2-byte offsets
        case 3:
        {
            /**
             * @brief @TODO check table for 32-bit alignment
             *
             */
            uint16_t sbitOffsets_arrray = 0;
            if (!table.ReadU16(&sbitOffsets_arrray))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }
            /**
             * @brief From spec:
             * sbitOffets[glyphIndex] + imageDataOffset = glyphData
             * sizeOfArray = (lastGlyph - firstGlyph + 1) + 1 + 1 pad if needed
             */

            // uint16_t sizeOfArray = last_glyph_index - first_glyph_index + 1 + 1 + ((last_glyph_index - first_glyph_index + 1) % 2 ? 0 : 1);
            uint32_t number_of_glyphs = last_glyph_index - first_glyph_index + 1;

            uint32_t this_glyph_sbit_offset = 0;
            uint32_t next_glyph_sbit_offset = 0;
            uint16_t read_sbit_offset = 0;

            if (!table.ReadU16(&read_sbit_offset))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }
            this_glyph_sbit_offset = read_sbit_offset;

            for (uint16_t glyphIndex = 0; glyphIndex < number_of_glyphs; glyphIndex++)
            {
                if (!table.ReadU16(&read_sbit_offset))
                {
                    return OTS_FAILURE_MSG("Failed to read IndexSubTable");
                }
                next_glyph_sbit_offset = read_sbit_offset;

                int32_t image_size = next_glyph_sbit_offset - this_glyph_sbit_offset;
                this_glyph_sbit_offset = next_glyph_sbit_offset;

                if (image_size < 0)
                {
                    return OTS_FAILURE_MSG("Offsets not in orderInvalid image size %d", image_size);
                }
                if (image_size == 0)
                {
                    /**
                     * @brief The spec says that image-size 0 is used
                     * to skip glyphs
                     *
                     */

                    continue;
                }
                uint32_t unsigned_image_size = static_cast<uint32_t>(image_size);
                uint32_t out_image_size = 0;

                uint32_t glyphDataOffset = this_glyph_sbit_offset + ebdt_table_image_data_offset;
                if (!ebdt->ParseGlyphBitmapDataWithVariableMetrics(image_format,
                                                                   glyphDataOffset,
                                                                   bit_depth,
                                                                   &out_image_size))
                {
                    return OTS_FAILURE_MSG("Failed to parse glyph bitmap data");
                }
                if (out_image_size != unsigned_image_size)
                {
                    return OTS_FAILURE_MSG("Image size %d does not match expected size %d", out_image_size, unsigned_image_size);
                }
            }
        }
        break;
        // IndexSubTable4: variable-metrics glyphs with sparse glyph codes
        case 4:
        {
            uint16_t num_glyphs = 0;
            if (!table.ReadU16(&num_glyphs))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }
            uint16_t this_glyph_id = 0;
            uint16_t this_sbix_offset = 0;

            if (!table.ReadU16(&this_glyph_id) ||
                !table.ReadU16(&this_sbix_offset))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }

            uint16_t next_glyph_id = 0;
            uint16_t next_sbix_offset = 0;
            for (uint16_t glyphIndex = 0; glyphIndex < num_glyphs; glyphIndex++)
            {
                if (!table.ReadU16(&next_glyph_id) ||
                    !table.ReadU16(&next_sbix_offset))
                {
                    return OTS_FAILURE_MSG("Failed to read IndexSubTable");
                }
                if (glyphIndex < num_glyphs - 1 && next_glyph_id < this_glyph_id)
                {
                    return OTS_FAILURE_MSG("Invalid glyph id %d, last glyph id %d, they must be sorted by glyph id", next_glyph_id, this_glyph_id);
                }
                this_glyph_id = next_glyph_id;

                uint32_t image_size = next_sbix_offset - this_sbix_offset;
                this_sbix_offset = next_sbix_offset;
                if (image_size < 0)
                {
                    return OTS_FAILURE_MSG("Offsets not in orderInvalid image size %d", image_size);
                }
                if (image_size == 0)
                {
                    /**
                     * @brief The spec says that image-size 0 is used
                     * to skip glyphs
                     *
                     */
                    continue;
                }
                uint32_t unsigned_image_size = static_cast<uint32_t>(image_size);
                uint32_t out_image_size = 0;

                /**
                 * @TODO is this an absolute sbit offset or is it relative to the
                 * ebdt_table_image_data_offset, ie should
                 * glyphDataOffset = sbitOffset
                 * or
                 * glyphDataOffset = sbitOffset + ebdt_table_image_data_offset
                 *
                 */
                uint32_t glyphDataOffset = this_sbix_offset + ebdt_table_image_data_offset;
                if (!ebdt->ParseGlyphBitmapDataWithVariableMetrics(image_format,
                                                                   glyphDataOffset,
                                                                   bit_depth,
                                                                   &out_image_size))
                {
                    return OTS_FAILURE_MSG("Failed to parse glyph bitmap data");
                }
                if (out_image_size != unsigned_image_size)
                {
                    return OTS_FAILURE_MSG("Image size %d does not match expected size %d", out_image_size, unsigned_image_size);
                }
            }
        }
        break;
        // IndexSubTable5: constant-metrics glyphs with sparse glyph codes
        case 5:
        {
            /**
             * @brief @TODO check table for 32-bit alignment
             *
             */
            uint32_t image_size = 0;
            if (!table.ReadU32(&image_size))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }
            ots::ebdt::BigGlyphMetrics metrics;
            if (!ots::ebdt::ParseBigGlyphMetrics(table, &metrics))
            {

                return OTS_FAILURE_MSG("Failed to read IndexSubTable, image size");
            }
            uint32_t num_glyphs = 0;
            if (!table.ReadU32(&num_glyphs))
            {
                return OTS_FAILURE_MSG("Failed to read IndexSubTable");
            }
            uint16_t glyphId = 0;
            uint16_t last_glyph_id = 0;

            for (uint32_t i = 0; i < num_glyphs; i++)
            {
                if (!table.ReadU16(&glyphId))
                {
                    return OTS_FAILURE_MSG("Failed to read IndexSubTable");
                }
                if (last_glyph_id != 0)
                {
                    /**
                     * @TODO what does the design doc say about things not being sorted?
                     */
                    if (glyphId <= last_glyph_id)
                    {
                        return OTS_FAILURE_MSG("Invalid glyph id %d, last glyph id %d, they must be sorted by glyph id", glyphId, last_glyph_id);
                    }
                }
                last_glyph_id = glyphId;
                uint32_t glyphDataOffset = image_size * i + ebdt_table_image_data_offset;
                /**
                 * @brief @TODO does out_image size have to match image_size?
                 *
                 */
                uint32_t _unused_out_image_size = 0;
                if (!ebdt->ParseGlyphBitmapDataWithConstantMetrics(image_format,
                                                                   glyphDataOffset,
                                                                   bit_depth,
                                                                   metrics.width,
                                                                   metrics.height,
                                                                   &_unused_out_image_size))
                {
                    return OTS_FAILURE_MSG("Failed to parse glyph bitmap data");
                }
            }
            break;
        }
        default:
            return OTS_FAILURE_MSG("Invalid format %d", index_format);
        }

        return true;
    };

    bool ParseIndexSubTableArray(
        const ots::Font *font,
        ots::OpenTypeEBDT *ebdt,
        const uint8_t *eblc_data,
        size_t eblc_length,
        uint8_t bit_depth,
        uint32_t index_sub_table_array_offset,
        const uint8_t *data,
        size_t length)
    {

        ots::Buffer table(data, length);
        uint16_t first_glyph_index = 0;
        uint16_t last_glyph_index = 0;
        uint32_t additional_offset_to_index_subtable = 0;
        if (
            // The firstGlyphIndex and the lastglyphIndex
            !table.ReadU16(&first_glyph_index) ||
            !table.ReadU16(&last_glyph_index) ||
            !table.ReadU32(&additional_offset_to_index_subtable))
        {
            return OTS_FAILURE_MSG("Failed to read IndexSubTableArray");
        }
        if (last_glyph_index < first_glyph_index)
        {
            return OTS_FAILURE_MSG("Invalid glyph indicies, first index %d > than last index %d", first_glyph_index, last_glyph_index);
        }
        uint32_t offset = index_sub_table_array_offset + additional_offset_to_index_subtable;
        if (offset >= eblc_length)
        {
            // Don't need to check the lower bound because we already checked
            // index_sub_table_array_offset
            return OTS_FAILURE_MSG("Bad index sub table offset %d", offset);
        }
        if (!ParseIndexSubTable(font,
                                ebdt,
                                bit_depth,
                                first_glyph_index,
                                last_glyph_index,
                                eblc_data + offset,
                                eblc_length - offset))
        {
            return OTS_FAILURE_MSG("Bad index sub table ");
        }
        return true;
    }
} // namespace

namespace ots
{

    bool OpenTypeEBLC::Parse(const uint8_t *data, size_t length)
    {
        Font *font = GetFont();
        Buffer table(data, length);

        this->m_data = data;
        this->m_length = length;

        uint16_t version_major = 0, version_minor = 0;
        uint32_t num_sizes = 0;
        if (!table.ReadU16(&version_major) ||
            !table.ReadU16(&version_minor) ||
            !table.ReadU32(&num_sizes))
        {
            return Error("Incomplete table");
        }
        if (version_major != 2 || version_minor != 0)
        {
            return Error("Bad version");
        }
        std::vector<uint32_t> index_subtable_offsets_arrays;
        index_subtable_offsets_arrays.reserve(num_sizes);
        std::vector<uint8_t> bit_depths(num_sizes);
        const unsigned bitmap_size_end = 48 * static_cast<unsigned>(num_sizes) + 8;

        OpenTypeEBDT *ebdt = static_cast<OpenTypeEBDT *>(
            font->GetTypedTable(OTS_TAG_EBDT));

        if (ebdt == NULL)
        {
            return OTS_FAILURE_MSG("Missing required table EBDT");
        }
        for (uint32_t i = 0; i < num_sizes; i++)
        {
            uint32_t index_sub_table_array_offset = 0;
            uint32_t index_table_size = 0;
            uint32_t number_of_index_sub_tables = 0;
            uint32_t color_ref = 0;
            uint16_t start_glyph_index = 0;
            uint16_t end_glyph_index = 0;
            uint8_t bit_depth = 0;
            uint8_t flags = 0;
            // BitmapSize Record
            if (!table.ReadU32(&index_sub_table_array_offset) ||
                !table.ReadU32(&index_table_size) ||
                !table.ReadU32(&number_of_index_sub_tables) ||
                !table.ReadU32(&color_ref) ||
                // Skip Horizontal and vertical SbitLineMetrics
                !table.Skip(24) ||
                !table.ReadU16(&start_glyph_index) ||
                !table.ReadU16(&end_glyph_index) ||
                // Skip ppemX and ppemY
                !table.Skip(2) ||
                !table.ReadU8(&bit_depth) ||
                !table.ReadU8(&flags))
            {
                return Error("Incomplete table");
            }

            if (color_ref != 0)
            {
                return Error("Color ref should be 0");
            }

            if (end_glyph_index < start_glyph_index)
            {
                return Error("start glyph is greater than end glyph");
            }
            if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8)
            {
                return Error("Invalid bit depth %d", bit_depth);
            }
            bit_depths[i] = bit_depth;

            if (flags & 0xFC)
            {
                return Error("bitmap flags 0xFX reserved for future use");
            }

            if (index_sub_table_array_offset < bitmap_size_end ||
                index_sub_table_array_offset >= length)
            {
                return OTS_FAILURE_MSG("Bad index sub table array offset %d for BitmapSize %d", index_sub_table_array_offset, i);
            }
            index_subtable_offsets_arrays.push_back(index_sub_table_array_offset);
        }
        if (index_subtable_offsets_arrays.size() != num_sizes)
        {
            return OTS_FAILURE_MSG("Bad subtable size %ld", index_subtable_offsets_arrays.size());
        }

        // uint16_t lowest_glyph = UINT16_MAX;
        // uint16_t highest_glyph = 0;

        for (unsigned i = 0; i < num_sizes; ++i)
        {
            if (!ParseIndexSubTableArray(
                    font,
                    ebdt,
                    /* eblc_data */ data,
                    /* eblc_length */ length,
                    /* bit_depth*/ bit_depths[i],
                    /*index_sub_table_array_offset*/ index_subtable_offsets_arrays[i],
                    data + index_subtable_offsets_arrays[i],
                    length - index_subtable_offsets_arrays[i]))
            {
                return OTS_FAILURE_MSG("Failed to parse IndexSubTableArray %d", i);
            }
        }
        // if (lowest_glyph)

        return true;
    }

    bool OpenTypeEBLC::Serialize(OTSStream *out)
    {
        if (!out->Write(this->m_data, this->m_length))
        {
            return Error("Failed to write EBLC table");
        }

        return true;
    }

} // namespace ots
#undef TABLE_NAME