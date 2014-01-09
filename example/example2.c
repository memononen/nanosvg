//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <string.h>
#include <float.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }

static void calcBounds(struct NSVGimage* image, float* bounds)
{
	struct NSVGshape* shape;
	struct NSVGpath* path;
	int i;
	bounds[0] = FLT_MAX;
	bounds[1] = FLT_MAX;
	bounds[2] = -FLT_MAX;
	bounds[3] = -FLT_MAX;
	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		for (path = shape->paths; path != NULL; path = path->next) {
			for (i = 0; i < path->npts; i++) {
				float* p = &path->pts[i*2];
				bounds[0] = minf(bounds[0], p[0]);
				bounds[1] = minf(bounds[1], p[1]);
				bounds[2] = maxf(bounds[2], p[0]);
				bounds[3] = maxf(bounds[3], p[1]);
			}
		}
	}
}

static void getImageSize(struct NSVGimage *image, int* w, int* h)
{
	float bounds[4];
	if (image->width < 1 || image->height < 1)
		calcBounds(image, bounds);
	*w = image->width < 1 ? (bounds[2]+1) : image->width;
	*h = image->height < 1 ? (bounds[3]+1) : image->height;
}

int main()
{
	struct NSVGimage *image = NULL;
	struct NSVGrasterizer *rast = NULL;
	unsigned char* img = NULL;
	int w, h;

	image = nsvgParseFromFile("../example/23.svg");
	if (image == NULL) {
		printf("Could not open SVG image.\n");
		goto error;
	}
	getImageSize(image, &w, &h);
	if (w < 1 || h < 1) {
		printf("Size of SVG not specified.\n");
		goto error;
	}

	rast = nsvgCreateRasterizer();
	if (rast == NULL) {
		printf("Could not init rasterizer.\n");
		goto error;
	}

	img = malloc(w*h*4);
	if (img == NULL) {
		printf("Could not alloc image buffer.\n");
		goto error;
	}

	nsvgRasterize(rast, image, 0,0,1, img, w, h, w*4);

 	stbi_write_png("svg.png", w, h, 4, img, w*4);

error:
	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);

	return 0;
}
