// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include <algorithm>
#include <stdint.h>

// yuy2 aka uyvy
// based on https://stackoverflow.com/questions/4491649/how-to-convert-yuy2-to-a-bitmap-in-c
void yuy2_to_rgb(const uint8_t *const in, const int width, const int height, uint8_t **out)
{
	const uint8_t *in_work = in;
	const int n_pixels = width * height;
	const int n_bytes_out = n_pixels * 3;

	uint8_t *out_work = *out = (uint8_t *)malloc(n_bytes_out);

	for(int y=0; y<height; y++) {
		for(int i = 0; i < width/2; i++) {
			int y0 = in_work[0];
			int u0 = in_work[1];
			int y1 = in_work[2];
			int v0 = in_work[3];
			in_work += 4;

			int c = y0 - 16;
			int d = u0 - 128;
			int e = v0 - 128;
			out_work[0] = std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255); // red
			out_work[1] = std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255); // green
			out_work[2] = std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255); // blue

			c = y1 - 16;
			out_work[3] = std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255); // red
			out_work[4] = std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255); // green
			out_work[5] = std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255); // blue

			out_work += 6;
		}
	}
}

// RGB -> YUV
// from https://stackoverflow.com/questions/1737726/how-to-perform-rgb-yuv-conversion-in-c-c
// and https://stackoverflow.com/questions/39040944/convert-yuv444-to-yuv422-images/39048445#39048445
#define RGB2Y(R, G, B) std::clamp(( (  66 * (R) + 129 * (G) +  25 * (B) + 128) >> 8) +  16, 0, 255)
#define RGB2U(R, G, B) std::clamp(( ( -38 * (R) -  74 * (G) + 112 * (B) + 128) >> 8) + 128, 0, 255)
#define RGB2V(R, G, B) std::clamp(( ( 112 * (R) -  94 * (G) -  18 * (B) + 128) >> 8) + 128, 0, 255)
// uyvy
void rgb_to_yuy2(const uint8_t *const in, const int width, const int height, uint8_t **const out)
{
	*out = (uint8_t *)malloc(width * 2 * height);
	int outo = 0;

	for(int y=0; y<height; y++) {
		for(int x=0; x<width; x += 2) {
			int o = y * width * 3 + x * 3;

			int r1 = in[o + 0];
			int g1 = in[o + 1];
			int b1 = in[o + 2];

			int r2 = in[o + 3];
			int g2 = in[o + 4];
			int b2 = in[o + 5];

			uint8_t Y1 = RGB2Y(r1, g1, b1);
			uint8_t U1 = RGB2U(r1, g1, b1);
			uint8_t V1 = RGB2V(r1, g1, b1);

			uint8_t Y2 = RGB2Y(r2, g2, b2);
			uint8_t U2 = RGB2U(r2, g2, b2);
			uint8_t V2 = RGB2V(r2, g2, b2);

			uint8_t u12 = (U1 + U2 + 1) / 2;
			uint8_t v12 = (V1 + V2 + 1) / 2;

			(*out)[outo++] = Y1;
			(*out)[outo++] = u12;
			(*out)[outo++] = Y2;
			(*out)[outo++] = v12;
		}
	}
}
