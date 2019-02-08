//
// Copyright (c) 2017  Michael Tesch tesch1@gmail.com
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

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

NSVGimage* g_image = NULL;

int main(int argc, char *argv[])
{
  for (int arg = 1; arg < argc; arg++) {
    const char* filename = argv[arg];

    g_image = nsvgParseFromFile(filename, "px", 96.0f);
    if (g_image == NULL) {
      printf("Could not open SVG image '%s'.\n", filename);
      return -1;
    }

    printf("%s:\n", filename);
    printf("size: %f x %f.\n", g_image->width, g_image->height);

    for (NSVGgroup* group = g_image->groups; group != NULL; group = group->next) {
      printf("group: %s parent:%s\n", group->id, group->parent ? group->parent->id : "-");
    }

    for (NSVGshape* shape = g_image->shapes; shape != NULL; shape = shape->next) {
      printf("shape: '%s' visible:%d\n", shape->id, 0 != (shape->flags & NSVG_FLAGS_VISIBLE));
      if (shape->group)
        printf("     : group '%s'\n", shape->group->id);
      for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
        //drawPath(path->pts, path->npts, path->closed, px * 1.5f);
        printf(" npts: %d  [%f %f %f %f]\n", path->npts,
               path->bounds[0], path->bounds[1], path->bounds[2], path->bounds[3]);
      }
    }

    nsvgDelete(g_image);
  }

  return 0;
}
