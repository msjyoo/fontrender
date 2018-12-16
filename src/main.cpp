#include <iostream>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <algorithm>

#include <SDL2/SDL.h>
#include <cassert>
#include <cstdlib>

// TODO: RenderBoundingBox
// TODO: render_glyph
// TODO: render_line
// TODO: render_glyph and render_line will both have bounding boxes
// TODO: For examples, upwards combining characters will increase the bounding box.

std::optional<std::vector<uint8_t>> unifont_lookup(uint32_t codepoint);

std::optional<std::vector<uint8_t>> unifont_lookup(uint32_t codepoint) {
    std::ifstream unifont_istream("../fonts/unifont-11.0.02.hex");

    if (codepoint > 0xFFFD) {
        std::cerr << "W unifont_lookup: Asked to lookup out-of-range codepoint" << std::endl;
        return std::nullopt;
    }

    while (unifont_istream.is_open()) {
        char codepoint_in_hexstr[5] = {0};
        unifont_istream >> codepoint_in_hexstr[0] >> codepoint_in_hexstr[1] >> codepoint_in_hexstr[2] >> codepoint_in_hexstr[3];

        if (strcmp("", codepoint_in_hexstr) == 0) break; // Must be end of file
        uint32_t unifont_hexcodepoint = (uint32_t) std::stoul(codepoint_in_hexstr, nullptr, 16);

        if (unifont_hexcodepoint == codepoint) {
            std::vector<uint8_t> bytes{};

            char c; // consume ':'
            unifont_istream >> c;

            char byteworth[3] = {0};

            while (unifont_istream.peek() != '\n') {
                unifont_istream.read(byteworth, 2);
                assert(byteworth[0] != '\0' && byteworth[0] != '\n' && byteworth[0] != '\r');
                assert(byteworth[1] != '\0' && byteworth[1] != '\n' && byteworth[1] != '\r');
                bytes.push_back((uint8_t) std::stoul(byteworth, nullptr, 16));
            }

            assert(bytes.size() == 16 || bytes.size() == 32);

            return std::make_optional<std::vector<uint8_t>>(bytes);
        }

        // Start from next line
        unifont_istream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    return std::nullopt;
}

/*
 * Assumptions:
 * 1. Line height is always 16px.
 * 2. Each glyph is separated by 1px.
 * 3. Each glyph has two widths: 16px and 32px.
 * 4. Each line is limited to 256px.
 *
 * 5 (temp). Each UTF-8 byte represents 1 codepoint, without surrogates.
 */
// TODO: Transpose the vector, such that line height is specified and text length is infinite.
// For example,
std::array<uint32_t,16*256> render_utf8str(const char *utf8str) {
    if (utf8str == nullptr) {
        std::cerr << "W render_utf8str: Asked to render nullptr." << std::endl;
        return {};
    }
    if (*utf8str == '\0') {
        std::cerr << "W render_utf8str: Asked to render empty string." << std::endl;
        return {};
    }

    std::array<uint32_t,16*256> render = {};
    render.fill(0x00FFFFFF); // Transparent
    int render_col = 0;

    for (const char *c = utf8str; *c != '\0'; c++) {
        uint32_t c_codepoint = 0;

        // 0b1100 0000:  -64
        // 0b1101 1111:  -33
        // 0b1110 0000:  -32
        // 0b1110 1111:  -17
        // 0b1111 0000:  -16
        // 0b1111 0111:   -9
        // 0b1000 0000: -128
        // 0b1011 1111:  -65

        if (*c > 0) { // 1 byte code point
            c_codepoint = (uint32_t) *c; // Safe to cast: Value is always positive.
            assert(c_codepoint > 0);
        } else if (*c >= -64 && *c <= -33) { // 2 byte code point
            assert((*c - (-64)) >= 0);
            if ((*c - (-64)) == 0) {
                std::cerr << "W render_utf8str: Insignificant UTF-8 leading byte; halting." << std::endl;
                return render;
            }
            c_codepoint = (uint32_t) (*c - (-64)) << 6;
            assert(c_codepoint >= 0b1000000 && c_codepoint <= 0b11111000000);

            if (*++c > -65) {
                std::cerr << "W render_utf8str: Invalid UTF-8 surrogate found; halting." << std::endl;
                return render;
            }
            assert((*c - (-128)) >= 0);
            c_codepoint += (uint32_t) (*c - (-128));
            assert(c_codepoint >= 0b1000000 && c_codepoint <= 0b11111111111);
        } else if (*c >= -32 && *c <= -17) { // 3 byte code point
            assert((*c - (-32)) >= 0);
            if ((*c - (-32)) == 0) {
                std::cerr << "W render_utf8str: Insignificant UTF-8 leading byte; halting." << std::endl;
                return render;
            }
            c_codepoint = (uint32_t) (*c - (-32)) << 12;
            assert(c_codepoint >= 0b1000000000000 && c_codepoint <= 0b1111000000000000);

            if (*++c > -65) {
                std::cerr << "W render_utf8str: Invalid UTF-8 surrogate found; halting." << std::endl;
                return render;
            }
            assert((*c - (-128)) >= 0);
            c_codepoint += (uint32_t) (*c - (-128)) << 6;
            assert(c_codepoint >= 0b1000000000000 && c_codepoint <= 0b1111111111000000);

            if (*++c > -65) {
                std::cerr << "W render_utf8str: Invalid UTF-8 surrogate found; halting." << std::endl;
                return render;
            }
            assert((*c - (-128)) >= 0);
            c_codepoint += (uint32_t) (*c - (-128));
            assert(c_codepoint >= 0b1000000000000 && c_codepoint <= 0b1111111111111111);
        } else if (*c >= -16 && *c <= -9) { // 4 byte code point
            assert((*c - (-16)) >= 0);
            if ((*c - (-16)) == 0) {
                std::cerr << "W render_utf8str: Insignificant UTF-8 leading byte; halting." << std::endl;
                return render;
            }
            c_codepoint = (uint32_t) (*c - (-16)) << 18;
            assert(c_codepoint >= 0b1000000000000000000 && c_codepoint <= 0b111000000000000000000);

            if (*++c > -65) {
                std::cerr << "W render_utf8str: Invalid UTF-8 surrogate found; halting." << std::endl;
                return render;
            }
            assert((*c - (-128)) >= 0);
            c_codepoint += (uint32_t) (*c - (-128)) << 12;
            assert(c_codepoint >= 0b1000000000000000000 && c_codepoint <= 0b111111111000000000000);

            if (*++c > -65) {
                std::cerr << "W render_utf8str: Invalid UTF-8 surrogate found; halting." << std::endl;
                return render;
            }
            assert((*c - (-128)) >= 0);
            c_codepoint += (uint32_t) (*c - (-128)) << 6;
            assert(c_codepoint >= 0b1000000000000000000 && c_codepoint <= 0b111111111111111000000);

            if (*++c > -65) {
                std::cerr << "W render_utf8str: Invalid UTF-8 surrogate found; halting." << std::endl;
                return render;
            }
            assert((*c - (-128)) >= 0);
            c_codepoint += (uint32_t) (*c - (-128));
            assert(c_codepoint >= 0b1000000000000000000 && c_codepoint <= 0b111111111111111111111);
        } else {
            std::cerr << "W render_utf8str: Invalid UTF-8 byte (" << *c << ") found; halting." << std::endl;
            return render;
        }

        // Represents either 16 or 32 bytes glyph.
        std::optional<std::vector<uint8_t>> glyph_o = unifont_lookup(c_codepoint);
        assert(glyph_o.has_value());
        std::vector<uint8_t> glyph = glyph_o.value();
        assert(glyph.size() == 16 || glyph.size() == 32);

        if (glyph.size() == 16) { // Each byte represents a row
            for (int i = 0; i < 16; i++) { // For each pixel line = i'th row
                assert(((i * 256) + render_col + 7) < render.size());

                render[(i * 256) + render_col + 0] = (glyph[i] & (1u << 7)) ? 0xFF000000 : 0x00FFFFFF; // Black or Transparent
                render[(i * 256) + render_col + 1] = (glyph[i] & (1u << 6)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 2] = (glyph[i] & (1u << 5)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 3] = (glyph[i] & (1u << 4)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 4] = (glyph[i] & (1u << 3)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 5] = (glyph[i] & (1u << 2)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 6] = (glyph[i] & (1u << 1)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 7] = (glyph[i] & (1u << 0)) ? 0xFF000000 : 0x00FFFFFF;
            }
            render_col = render_col + 8 + 0; // TODO: Unifont kerning necessary? 0 or 1?
        } else if (glyph.size() == 32) { // Every two bytes represent a row
            for (int i = 0; i < 16; i++) { // For each pixel line = i'th row
                assert(((i * 256) + render_col + 15) < render.size());

                render[(i * 256) + render_col +  0] = (glyph[i*2] & (1u << 7)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  1] = (glyph[i*2] & (1u << 6)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  2] = (glyph[i*2] & (1u << 5)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  3] = (glyph[i*2] & (1u << 4)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  4] = (glyph[i*2] & (1u << 3)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  5] = (glyph[i*2] & (1u << 2)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  6] = (glyph[i*2] & (1u << 1)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  7] = (glyph[i*2] & (1u << 0)) ? 0xFF000000 : 0x00FFFFFF;

                render[(i * 256) + render_col +  8] = (glyph[i*2 + 1] & (1u << 7)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col +  9] = (glyph[i*2 + 1] & (1u << 6)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 10] = (glyph[i*2 + 1] & (1u << 5)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 11] = (glyph[i*2 + 1] & (1u << 4)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 12] = (glyph[i*2 + 1] & (1u << 3)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 13] = (glyph[i*2 + 1] & (1u << 2)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 14] = (glyph[i*2 + 1] & (1u << 1)) ? 0xFF000000 : 0x00FFFFFF;
                render[(i * 256) + render_col + 15] = (glyph[i*2 + 1] & (1u << 0)) ? 0xFF000000 : 0x00FFFFFF;
            }
            render_col = render_col + 16 + 0;
        } else {
            std::cerr << "W render_utf8str: Asked to render illogical glyph width." << std::endl;
        }
    }

    return render;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // https://github.com/mosra/magnum/issues/184
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    SDL_Window *window = SDL_CreateWindow("fontrender", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 480, 0);
    if (window == nullptr) {
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 256, 480);

    auto *pixels = new Uint32[256 * 480];
    memset(pixels, 255, 256 * 480 * sizeof(Uint32));

    // 'accent': 0300:300C0000000000000000000000000000
    // 'a': 0061:0000000000003C42023E4242463A0000
    // combine: 30 0C 00 00 00 00 3C 42 02 3E 42 42 46 3A 00 00
    // latin small a with grave: 00E0:0000300C00003C42023E4242463A0000

    std::array<uint32_t,16*256> render = render_utf8str("abcd ¢ a 가나");

    memcpy(pixels, render.data(), 16 * 256 * sizeof(Uint32));

    SDL_UpdateTexture(texture, nullptr, pixels, 256 * sizeof(Uint32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    SDL_Event event;
    do {
        SDL_WaitEvent(&event);
    } while (event.type != SDL_QUIT);

    delete[] pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}