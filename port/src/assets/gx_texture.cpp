#include "assets/gx_texture.hpp"

#include "assets/hsd_archive.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace melee::assets {
namespace {

constexpr std::uint32_t kGxI4 = 0;
constexpr std::uint32_t kGxI8 = 1;
constexpr std::uint32_t kGxIA4 = 2;
constexpr std::uint32_t kGxIA8 = 3;
constexpr std::uint32_t kGxRgb565 = 4;
constexpr std::uint32_t kGxRgb5A3 = 5;
constexpr std::uint32_t kGxRgba8 = 6;
constexpr std::uint32_t kGxC4 = 8;
constexpr std::uint32_t kGxC8 = 9;
constexpr std::uint32_t kGxC14X2 = 10;
constexpr std::uint32_t kGxCmpr = 14;
constexpr std::uint32_t kGxZ8 = 0x11;
constexpr std::uint32_t kGxZ16 = 0x13;
constexpr std::uint32_t kGxZ24X8 = 0x16;
constexpr std::uint32_t kGxTlutIa8 = 0;
constexpr std::uint32_t kGxTlutRgb565 = 1;
constexpr std::uint32_t kGxTlutRgb5A3 = 2;

std::size_t ceil_div(std::size_t value, std::size_t divisor)
{
    return (value + divisor - 1) / divisor;
}

/* An intensity texel.  GX hands TEV an I4 or I8 texture as the same value in
 * every channel, alpha included, so such a texture masks its own shape. */
void write_pixel(DecodedTexture* output, std::size_t x, std::size_t y,
                 std::uint8_t intensity)
{
    if (x >= output->width || y >= output->height) {
        return;
    }
    const std::size_t offset = (y * output->width + x) * 4;
    output->rgba[offset] = intensity;
    output->rgba[offset + 1] = intensity;
    output->rgba[offset + 2] = intensity;
    output->rgba[offset + 3] = intensity;
}

void write_rgba(DecodedTexture* output, std::size_t x, std::size_t y,
                std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                std::uint8_t alpha)
{
    if (x >= output->width || y >= output->height) {
        return;
    }
    const std::size_t offset = (y * output->width + x) * 4;
    output->rgba[offset] = red;
    output->rgba[offset + 1] = green;
    output->rgba[offset + 2] = blue;
    output->rgba[offset + 3] = alpha;
}

std::uint16_t read_be16(std::span<const std::byte> data, std::size_t offset)
{
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(data[offset]) << 8U) |
        std::to_integer<std::uint16_t>(data[offset + 1]));
}

std::uint8_t expand(std::uint32_t value, unsigned bits)
{
    return static_cast<std::uint8_t>(
        (value * 255U + ((1U << bits) - 1U) / 2U) / ((1U << bits) - 1U));
}

std::array<std::uint8_t, 4> rgb565(std::uint16_t value)
{
    return { expand((value >> 11U) & 0x1FU, 5),
             expand((value >> 5U) & 0x3FU, 6), expand(value & 0x1FU, 5), 255 };
}

std::array<std::uint8_t, 4> tlut_color(std::span<const std::byte> tlut,
                                       std::size_t index,
                                       std::uint32_t format)
{
    const std::size_t offset = index * 2;
    if (offset + 2 > tlut.size()) {
        throw HsdArchiveError("GX TLUT index " + std::to_string(index) +
                              " exceeds a palette of " +
                              std::to_string(tlut.size() / 2) + " entries");
    }
    const std::uint16_t value = read_be16(tlut, offset);
    switch (format) {
    case kGxTlutIa8: {
        const auto intensity = static_cast<std::uint8_t>(value & 0xFFU);
        return { intensity, intensity, intensity,
                 static_cast<std::uint8_t>(value >> 8U) };
    }
    case kGxTlutRgb565:
        return rgb565(value);
    case kGxTlutRgb5A3:
        if ((value & 0x8000U) != 0) {
            return { expand((value >> 10U) & 0x1FU, 5),
                     expand((value >> 5U) & 0x1FU, 5),
                     expand(value & 0x1FU, 5), 255 };
        }
        return { expand((value >> 8U) & 0xFU, 4),
                 expand((value >> 4U) & 0xFU, 4),
                 expand(value & 0xFU, 4),
                 expand((value >> 12U) & 0x7U, 3) };
    default:
        throw HsdArchiveError("unsupported GX TLUT format");
    }
}

} // namespace

std::size_t gx_texture_data_size(std::uint16_t width, std::uint16_t height,
                                 std::uint32_t format)
{
    if (width == 0 || height == 0) {
        throw HsdArchiveError("GX texture has zero dimensions");
    }
    switch (format) {
    case kGxI4:
    case kGxC4:
        return ceil_div(width, 8) * ceil_div(height, 8) * 32;
    case kGxI8:
    case kGxIA4:
    case kGxC8:
    case kGxZ8:
        return ceil_div(width, 8) * ceil_div(height, 4) * 32;
    case kGxIA8:
    case kGxC14X2:
    case kGxZ16:
        return ceil_div(width, 4) * ceil_div(height, 4) * 32;
    case kGxRgb5A3:
    case kGxRgb565:
        return ceil_div(width, 4) * ceil_div(height, 4) * 32;
    case kGxRgba8:
    case kGxZ24X8:
        return ceil_div(width, 4) * ceil_div(height, 4) * 64;
    case kGxCmpr:
        return ceil_div(width, 8) * ceil_div(height, 8) * 32;
    default:
        throw HsdArchiveError("unsupported GX texture format");
    }
}

DecodedTexture decode_gx_texture_with_tlut(
    std::span<const std::byte> data, std::uint16_t width,
    std::uint16_t height, std::uint32_t format,
    std::span<const std::byte> tlut, std::uint32_t tlut_format)
{
    if (format != kGxC4 && format != kGxC8 && format != kGxC14X2) {
        throw HsdArchiveError("GX texture format does not use a TLUT");
    }
    const std::size_t expected = gx_texture_data_size(width, height, format);
    if (data.size() < expected) {
        throw HsdArchiveError("GX texture data is truncated");
    }
    if ((tlut.size() & 1U) != 0) {
        throw HsdArchiveError("GX TLUT has an odd byte count");
    }

    DecodedTexture output{ width, height,
                           std::vector<std::uint8_t>(
                               static_cast<std::size_t>(width) * height * 4) };
    const std::size_t tile_width = format == kGxC14X2 ? 4U : 8U;
    const std::size_t tile_height = format == kGxC4 ? 8U : 4U;
    std::size_t cursor = 0;
    for (std::size_t tile_y = 0; tile_y < height; tile_y += tile_height) {
        for (std::size_t tile_x = 0; tile_x < width; tile_x += tile_width) {
            for (std::size_t y = 0; y < tile_height; ++y) {
                for (std::size_t x = 0; x < tile_width; ++x) {
                    /* A tile past the image's edge still holds whole texels,
                     * and the padding can name any index; only the texels
                     * inside the image go through the palette. */
                    if (tile_x + x >= width || tile_y + y >= height) {
                        continue;
                    }
                    std::size_t index;
                    if (format == kGxC4) {
                        const auto packed = std::to_integer<std::uint8_t>(
                            data[cursor + (y * 8 + x) / 2]);
                        index = (x & 1U) == 0 ? packed >> 4U : packed & 0xFU;
                    } else if (format == kGxC8) {
                        index = std::to_integer<std::uint8_t>(
                            data[cursor + y * 8 + x]);
                    } else {
                        index = read_be16(data, cursor + (y * 4 + x) * 2) &
                                0x3FFFU;
                    }
                    const auto color = tlut_color(tlut, index, tlut_format);
                    write_rgba(&output, tile_x + x, tile_y + y, color[0],
                               color[1], color[2], color[3]);
                }
            }
            cursor += 32;
        }
    }
    return output;
}

DecodedTexture decode_gx_texture(std::span<const std::byte> data,
                                 std::uint16_t width, std::uint16_t height,
                                 std::uint32_t format)
{
    const std::size_t expected = gx_texture_data_size(width, height, format);
    if (data.size() < expected) {
        throw HsdArchiveError("GX texture data is truncated");
    }
    DecodedTexture output{ width, height,
                           std::vector<std::uint8_t>(
                               static_cast<std::size_t>(width) * height * 4) };
    /* Depth textures use the regular GX tile geometry.  Keep their depth
     * bytes in RGB: the presenter uses those bytes to reconstruct a 24-bit
     * fragment depth, while TEV sees their high byte as intensity. */
    if (format == kGxZ8 || format == kGxZ16 || format == kGxZ24X8) {
        const std::size_t tile_width = format == kGxZ8 ? 8U : 4U;
        const std::size_t tile_height = 4U;
        std::size_t cursor = 0;
        for (std::size_t tile_y = 0; tile_y < height; tile_y += tile_height) {
            for (std::size_t tile_x = 0; tile_x < width;
                 tile_x += tile_width)
            {
                for (std::size_t pixel = 0;
                     pixel < tile_width * tile_height; ++pixel)
                {
                    const std::size_t x = tile_x + pixel % tile_width;
                    const std::size_t y = tile_y + pixel / tile_width;
                    if (format == kGxZ8) {
                        const auto value = std::to_integer<std::uint8_t>(
                            data[cursor + pixel]);
                        write_rgba(&output, x, y, value, value, value, value);
                    } else if (format == kGxZ16) {
                        const std::size_t offset = cursor + pixel * 2;
                        const auto hi = std::to_integer<std::uint8_t>(
                            data[offset]);
                        const auto lo = std::to_integer<std::uint8_t>(
                            data[offset + 1]);
                        write_rgba(&output, x, y, hi, lo, hi, hi);
                    } else {
                        /* Z24X8 has the same two 32-byte planes as RGBA8:
                         * X/Z23..16 first, then Z15..8/Z7..0. */
                        const std::size_t ar = cursor + pixel * 2;
                        const std::size_t gb = cursor + 32 + pixel * 2;
                        write_rgba(&output, x, y,
                                   std::to_integer<std::uint8_t>(data[ar + 1]),
                                   std::to_integer<std::uint8_t>(data[gb]),
                                   std::to_integer<std::uint8_t>(data[gb + 1]),
                                   std::to_integer<std::uint8_t>(data[ar]));
                    }
                }
                cursor += format == kGxZ24X8 ? 64U : 32U;
            }
        }
        return output;
    }
    std::size_t cursor = 0;
    const std::size_t tile_width =
        (format == kGxRgb565 || format == kGxRgb5A3 || format == kGxRgba8)
            ? 4U
            : (format == kGxIA8 ? 4U : 8U);
    const std::size_t tile_height =
        (format == kGxI4 || format == kGxCmpr) ? 8U : 4U;
    for (std::size_t tile_y = 0; tile_y < height; tile_y += tile_height) {
        for (std::size_t tile_x = 0; tile_x < width; tile_x += tile_width) {
            if (format == kGxCmpr) {
                for (std::size_t subblock = 0; subblock < 4; ++subblock) {
                    const std::uint16_t first = read_be16(data, cursor);
                    const std::uint16_t second = read_be16(data, cursor + 2);
                    std::array<std::array<std::uint8_t, 4>, 4> palette{};
                    palette[0] = rgb565(first);
                    palette[1] = rgb565(second);
                    if (first > second) {
                        for (std::size_t channel = 0; channel < 3; ++channel) {
                            palette[2][channel] = static_cast<std::uint8_t>(
                                (2U * palette[0][channel] + palette[1][channel]) / 3U);
                            palette[3][channel] = static_cast<std::uint8_t>(
                                (palette[0][channel] + 2U * palette[1][channel]) / 3U);
                        }
                        palette[2][3] = 255;
                        palette[3][3] = 255;
                    } else {
                        for (std::size_t channel = 0; channel < 3; ++channel) {
                            palette[2][channel] = static_cast<std::uint8_t>(
                                (palette[0][channel] + palette[1][channel]) / 2U);
                        }
                        palette[2][3] = 255;
                        palette[3] = { 0, 0, 0, 0 };
                    }
                    const std::uint32_t selectors =
                        (static_cast<std::uint32_t>(read_be16(data, cursor + 4)) << 16U) |
                        read_be16(data, cursor + 6);
                    const std::size_t block_x = tile_x + (subblock % 2) * 4;
                    const std::size_t block_y = tile_y + (subblock / 2) * 4;
                    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
                        const std::size_t shift = 30U - pixel * 2U;
                        const auto& color = palette[(selectors >> shift) & 3U];
                        write_rgba(&output, block_x + pixel % 4, block_y + pixel / 4,
                                   color[0], color[1], color[2], color[3]);
                    }
                    cursor += 8;
                }
                continue;
            }
            if (format == kGxRgba8) {
                for (std::size_t pixel = 0; pixel < 16; ++pixel) {
                    const std::size_t ar = cursor + pixel * 2;
                    const std::size_t gb = cursor + 32 + pixel * 2;
                    write_rgba(&output, tile_x + pixel % 4, tile_y + pixel / 4,
                               std::to_integer<std::uint8_t>(data[ar + 1]),
                               std::to_integer<std::uint8_t>(data[gb]),
                               std::to_integer<std::uint8_t>(data[gb + 1]),
                               std::to_integer<std::uint8_t>(data[ar]));
                }
                cursor += 64;
                continue;
            }
            for (std::size_t y = 0; y < tile_height; ++y) {
                for (std::size_t x = 0; x < tile_width; ++x) {
                    std::uint8_t intensity = 0;
                    if (format == kGxI4) {
                        const auto packed = std::to_integer<std::uint8_t>(
                            data[cursor + (y * 8 + x) / 2]);
                        const auto nibble = static_cast<std::uint8_t>(
                            (x & 1U) == 0 ? packed >> 4U : packed & 0x0FU);
                        intensity = static_cast<std::uint8_t>(nibble * 17U);
                    } else if (format == kGxI8) {
                        intensity = std::to_integer<std::uint8_t>(
                            data[cursor + y * 8 + x]);
                    } else if (format == kGxIA4) {
                        const auto value = std::to_integer<std::uint8_t>(
                            data[cursor + y * 8 + x]);
                        write_rgba(&output, tile_x + x, tile_y + y,
                                   static_cast<std::uint8_t>((value & 0xFU) * 17U),
                                   static_cast<std::uint8_t>((value & 0xFU) * 17U),
                                   static_cast<std::uint8_t>((value & 0xFU) * 17U),
                                   static_cast<std::uint8_t>((value >> 4U) * 17U));
                        continue;
                    } else if (format == kGxIA8) {
                        const auto alpha = std::to_integer<std::uint8_t>(
                            data[cursor + (y * 4 + x) * 2]);
                        const auto ia8_intensity = std::to_integer<std::uint8_t>(
                            data[cursor + (y * 4 + x) * 2 + 1]);
                        write_rgba(&output, tile_x + x, tile_y + y, ia8_intensity,
                                   ia8_intensity, ia8_intensity, alpha);
                        continue;
                    } else if (format == kGxRgb5A3) {
                        const std::uint16_t value = read_be16(data, cursor +
                                                               (y * 4 + x) * 2);
                        if ((value & 0x8000U) != 0) {
                            write_rgba(&output, tile_x + x, tile_y + y,
                                       expand((value >> 10U) & 0x1FU, 5),
                                       expand((value >> 5U) & 0x1FU, 5),
                                       expand(value & 0x1FU, 5), 255);
                        } else {
                            write_rgba(&output, tile_x + x, tile_y + y,
                                       expand((value >> 8U) & 0xFU, 4),
                                       expand((value >> 4U) & 0xFU, 4),
                                       expand(value & 0xFU, 4),
                                       expand((value >> 12U) & 0x7U, 3));
                        }
                        continue;
                    } else {
                        const auto color = rgb565(read_be16(
                            data, cursor + (y * 4 + x) * 2));
                        write_rgba(&output, tile_x + x, tile_y + y, color[0],
                                   color[1], color[2], color[3]);
                        continue;
                    }
                    write_pixel(&output, tile_x + x, tile_y + y, intensity);
                }
            }
            cursor += 32;
        }
    }
    return output;
}

} // namespace melee::assets
