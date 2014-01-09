#ifndef NANOSVGRAST_H
#define NANOSVGRAST_H

#ifdef __cplusplus
extern "C" {
#endif


struct NSVGrasterizer* nsvgCreateRasterizer();

void nsvgRasterize(struct NSVGrasterizer* r,
				   struct NSVGimage* image, float tx, float ty, float scale,
				   unsigned char* dst, int w, int h, int stride);

void nsvgDeleteRasterizer(struct NSVGrasterizer*);


#ifdef __cplusplus
};
#endif

#endif // NANOSVGRAST_H

#ifdef NANOSVGRAST_IMPLEMENTATION

#include <math.h>

// The polygon rasterization is heavily based on stb_truetype rasterizer by Sean Barrett - http://nothings.org/

#define NSVG__SUBSAMPLES 5

#define NSVG__FIXSHIFT   10
#define NSVG__FIX        (1 << NSVG__FIXSHIFT)
#define NSVG__FIXMASK    (NSVG__FIX-1)


struct NSVGedge {
   float x0,y0, x1,y1;
   int dir;
//   struct NSVGedge* next;
};

struct NSVGactedge {
	int x,dx;
	float ey;
	int dir;
	struct NSVGactedge *next;
};


struct NSVGrasterizer
{
	float px, py;

	struct NSVGedge* edges;
	int nedges;
	int cedges;

	struct NSVGactedge* actedges;
	int nactedges;
	int cactedges;
	struct NSVGactedge* freelist;

	unsigned char* scanline;
	int cscanline;

	unsigned char* bitmap;
	int width, height, stride;
};

struct NSVGrasterizer* nsvgCreateRasterizer()
{
	struct NSVGrasterizer* r = (struct NSVGrasterizer*)malloc(sizeof(struct NSVGrasterizer));
	if (r == NULL) goto error;
	memset(r, 0, sizeof(struct NSVGrasterizer));
	return r;

error:
	nsvgDeleteRasterizer(r);
	return NULL;
}

void nsvgDeleteRasterizer(struct NSVGrasterizer* r)
{
	if (r == NULL) return;
	if (r->edges) free(r->edges);
	if (r->actedges) free(r->actedges);
	if (r->scanline) free(r->scanline);
	free(r);
}


static void nsvg__addEdge(struct NSVGrasterizer* r, float x0, float y0, float x1, float y1)
{
	struct NSVGedge* e;

	// Skip horizontal edges
	if (y0 == y1)
		return;

	if (r->nedges+1 > r->cedges) {
		r->cedges = r->cedges > 0 ? r->cedges * 2 : 64;
		r->edges = (struct NSVGedge*)realloc(r->edges, sizeof(struct NSVGedge) * r->cedges);
		if (r->edges == NULL) return;
	}

	e = &r->edges[r->nedges];
	r->nedges++;

	if (y0 < y1) {
		e->x0 = x0;
		e->y0 = y0;
		e->x1 = x1;
		e->y1 = y1;
		e->dir = 1;
	} else {
		e->x0 = x1;
		e->y0 = y1;
		e->x1 = x0;
		e->y1 = y0;
		e->dir = -1;
	}
}

static float nsvg__distPtSeg(float x, float y, float px, float py, float qx, float qy)
{
	float pqx, pqy, dx, dy, d, t;
	pqx = qx-px;
	pqy = qy-py;
	dx = x-px;
	dy = y-py;
	d = pqx*pqx + pqy*pqy;
	t = pqx*dx + pqy*dy;
	if (d > 0) t /= d;
	if (t < 0) t = 0;
	else if (t > 1) t = 1;
	dx = px + t*pqx - x;
	dy = py + t*pqy - y;
	return dx*dx + dy*dy;
}

static void nsvg__flattenCubicBez(struct NSVGrasterizer* r, 
								  float x1, float y1, float x2, float y2,
								  float x3, float y3, float x4, float y4,
								  float tol, int level)
{
	float x12,y12,x23,y23,x34,y34,x123,y123,x234,y234,x1234,y1234;
	float d;
	
	if (level > 12) return;

	x12 = (x1+x2)*0.5f;
	y12 = (y1+y2)*0.5f;
	x23 = (x2+x3)*0.5f;
	y23 = (y2+y3)*0.5f;
	x34 = (x3+x4)*0.5f;
	y34 = (y3+y4)*0.5f;
	x123 = (x12+x23)*0.5f;
	y123 = (y12+y23)*0.5f;
	x234 = (x23+x34)*0.5f;
	y234 = (y23+y34)*0.5f;
	x1234 = (x123+x234)*0.5f;
	y1234 = (y123+y234)*0.5f;

	d = nsvg__distPtSeg(x1234, y1234, x1,y1, x4,y4);
	if (d > tol*tol) {
		nsvg__flattenCubicBez(r, x1,y1, x12,y12, x123,y123, x1234,y1234, tol, level+1); 
		nsvg__flattenCubicBez(r, x1234,y1234, x234,y234, x34,y34, x4,y4, tol, level+1); 
	} else {
		nsvg__addEdge(r, r->px, r->py, x4, y4);
		r->px = x4;
		r->py = y4;
	}
}

static int nsvg__cmpEdge(const void *p, const void *q)
{
	struct NSVGedge* a = (struct NSVGedge*)p;
	struct NSVGedge* b = (struct NSVGedge*)q;

	if (a->y0 < b->y0) return -1;
	if (a->y0 > b->y0) return  1;
	return 0;
}


static struct NSVGactedge* nsvg__addActive(struct NSVGrasterizer* r, struct NSVGedge* e, float startPoint)
{
	struct NSVGactedge* z;

	if (r->freelist != NULL) {
		// Restore from freelist.
		z = r->freelist;
		r->freelist = z->next;
	} else {
		// Alloc new edge.
		if (r->nactedges+1 > r->cactedges) {
			r->cactedges = r->cactedges > 0 ? r->cactedges * 2 : 64;
			r->actedges = (struct NSVGactedge*)realloc(r->actedges, sizeof(struct NSVGactedge) * r->cactedges);
			if (r->actedges == NULL) return NULL;
		}
		z = &r->actedges[r->nactedges];
		r->nactedges++;
	}

	float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
//	STBTT_assert(e->y0 <= start_point);
	// round dx down to avoid going too far
	if (dxdy < 0)
		z->dx = -floorf(NSVG__FIX * -dxdy);
	else
		z->dx = floorf(NSVG__FIX * dxdy);
	z->x = floorf(NSVG__FIX * (e->x0 + dxdy * (startPoint - e->y0)));
//	z->x -= off_x * FIX;
	z->ey = e->y1;
	z->next = 0;
	z->dir = e->dir;

	return z;
}

static void nsvg__freeActive(struct NSVGrasterizer* r, struct NSVGactedge* z)
{
	z->next = r->freelist;
	r->freelist = z;
}

// note: this routine clips fills that extend off the edges... ideally this
// wouldn't happen, but it could happen if the truetype glyph bounding boxes
// are wrong, or if the user supplies a too-small bitmap
static void nsvg__fillActiveEdges(unsigned char* scanline, int len, struct NSVGactedge* e, int maxWeight, int* xmin, int* xmax)
{
	// non-zero winding fill
	int x0 = 0, w = 0;

	while (e != NULL) {
		if (w == 0) {
			// if we're currently at zero, we need to record the edge start point
			x0 = e->x; w += e->dir;
		} else {
			int x1 = e->x; w += e->dir;
			// if we went to zero, we need to draw
			if (w == 0) {
				int i = x0 >> NSVG__FIXSHIFT;
				int j = x1 >> NSVG__FIXSHIFT;
				if (i < *xmin) *xmin = i;
				if (j > *xmax) *xmax = j;
				if (i < len && j >= 0) {
					if (i == j) {
						// x0,x1 are the same pixel, so compute combined coverage
						scanline[i] += (unsigned char)((x1 - x0) * maxWeight >> NSVG__FIXSHIFT);
					} else {
						if (i >= 0) // add antialiasing for x0
							scanline[i] += (unsigned char)(((NSVG__FIX - (x0 & NSVG__FIXMASK)) * maxWeight) >> NSVG__FIXSHIFT);
						else
							i = -1; // clip

						if (j < len) // add antialiasing for x1
							scanline[j] += (unsigned char)(((x1 & NSVG__FIXMASK) * maxWeight) >> NSVG__FIXSHIFT);
						else
							j = len; // clip

						for (++i; i < j; ++i) // fill pixels between x0 and x1
							scanline[i] += (unsigned char)maxWeight;
					}
				}
			}
		}
		e = e->next;
	}
}

static void nsvg__scanlineSolid(unsigned char* dst, int count, unsigned char* cover, unsigned int color)
{
	int x, cr, cg, cb, ca;

	cr = color & 0xff;
	cg = (color >> 8) & 0xff;
	cb = (color >> 16) & 0xff;
	ca = (color >> 24) & 0xff;

	for (x = 0; x < count; x++) {
		int r,g,b;
		int a = ((int)cover[0] * ca) >> 8;
		int ia = 255 - a;
		// Premultiply
		r = (cr * a) >> 8;
		g = (cg * a) >> 8;
		b = (cb * a) >> 8;

		// Blend over
		r += ((ia * (int)dst[0]) >> 8);
		g += ((ia * (int)dst[1]) >> 8);
		b += ((ia * (int)dst[2]) >> 8);
		a += ((ia * (int)dst[3]) >> 8);

		dst[0] = (unsigned char)r;
		dst[1] = (unsigned char)g;
		dst[2] = (unsigned char)b;
		dst[3] = (unsigned char)a;

		cover++;
		dst += 4;
	}
}

static void nsvg__rasterizeSortedEdges(struct NSVGrasterizer *r, unsigned int color)
{
	struct NSVGactedge *active = NULL;
	int y, s;
	int e = 0;
	int maxWeight = (255 / NSVG__SUBSAMPLES);  // weight per vertical scanline
	int xmin, xmax;

	for (y = 0; y < r->height; y++) {
		memset(r->scanline, 0, r->width);
		xmin = r->width;
		xmax = 0;
		for (s = 0; s < NSVG__SUBSAMPLES; ++s) {
			// find center of pixel for this scanline
			float scany = y*NSVG__SUBSAMPLES + s + 0.5f;
			struct NSVGactedge **step = &active;

			// update all active edges;
			// remove all active edges that terminate before the center of this scanline
			while (*step) {
				struct NSVGactedge *z = *step;
				if (z->ey <= scany) {
					*step = z->next; // delete from list
//					NSVG__assert(z->valid);
					nsvg__freeActive(r, z);
				} else {
					z->x += z->dx; // advance to position for current scanline
					step = &((*step)->next); // advance through list
				}
			}

			// resort the list if needed
			for (;;) {
				int changed = 0;
				step = &active;
				while (*step && (*step)->next) {
					if ((*step)->x > (*step)->next->x) {
						struct NSVGactedge* t = *step;
						struct NSVGactedge* q = t->next;
						t->next = q->next;
						q->next = t;
						*step = q;
						changed = 1;
					}
					step = &(*step)->next;
				}
				if (!changed) break;
			}

			// insert all edges that start before the center of this scanline -- omit ones that also end on this scanline
			while (e < r->nedges && r->edges[e].y0 <= scany) {
				if (r->edges[e].y1 > scany) {
					struct NSVGactedge* z = nsvg__addActive(r, &r->edges[e], scany);
					if (z == NULL) break;
					// find insertion point
					if (active == NULL) {
						active = z;
					} else if (z->x < active->x) {
						// insert at front
						z->next = active;
						active = z;
					} else {
						// find thing to insert AFTER
						struct NSVGactedge* p = active;
						while (p->next && p->next->x < z->x)
							p = p->next;
						// at this point, p->next->x is NOT < z->x
						z->next = p->next;
						p->next = z;
					}
				}
				e++;
			}

			// now process all active edges in non-zero fashion
			if (active != NULL)
				nsvg__fillActiveEdges(r->scanline, r->width, active, maxWeight, &xmin, &xmax);
		}
		// Blit
		if (xmin <= xmax) {
			nsvg__scanlineSolid(&r->bitmap[y * r->stride] + xmin*4, xmax-xmin+1, &r->scanline[xmin], color);
		}
	}

}

static void nsvg__flattenShape(struct NSVGrasterizer* r,
							   struct NSVGshape* shape, float tx, float ty, float scale)
{
	struct NSVGpath* path;
	float tol = 0.5f * scale;
	int i;

	for (path = shape->paths; path != NULL; path = path->next) {
		// Flatten path
		r->px = path->pts[0];
		r->py = path->pts[1];
		for (i = 0; i < path->npts-1; i += 3) {
			float* p = &path->pts[i*2];
			nsvg__flattenCubicBez(r, p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], tol, 0);
		}
		// Close path
		nsvg__addEdge(r, r->px,r->py, path->pts[0],path->pts[1]);
	}
}

static void nsvg__unpremultiplyAlpha(unsigned char* image, int w, int h, int stride)
{
	int x,y;

	// Unpremultiply
	for (y = 0; y < h; y++) {
		unsigned char *row = &image[y*stride];
		for (x = 0; x < w; x++) {
			int r = row[0], g = row[1], b = row[2], a = row[3];
			if (a != 0) {
				r = (r*255/a);
				g = (g*255/a);
				b = (b*255/a);
			}
			row += 4;
		}
	}

	// Defringe
	for (y = 0; y < h; y++) {
		unsigned char *row = &image[y*stride];
		for (x = 0; x < w; x++) {
			int r = 0, g = 0, b = 0, a = row[3], n = 0;
			if (a == 0) {
				if (x-1 > 0 && row[-1] != 0) {
					r += row[-4];
					g += row[-3];
					b += row[-2];
					n++;
				}
				if (x+1 < w && row[7] != 0) {
					r += row[4];
					g += row[5];
					b += row[6];
					n++;
				}
				if (y-1 > 0 && row[-stride+3] != 0) {
					r += row[-stride];
					g += row[-stride+1];
					b += row[-stride+2];
					n++;
				}
				if (y+1 < h && row[stride+3] != 0) {
					r += row[stride];
					g += row[stride+1];
					b += row[stride+2];
					n++;
				}
				if (n > 0) {
					row[0] = r/n;
					row[1] = g/n;
					row[2] = b/n;
				}
			}
			row += 4;
		}
	}
}

void nsvgRasterize(struct NSVGrasterizer* r,
				   struct NSVGimage* image, float tx, float ty, float scale,
				   unsigned char* dst, int w, int h, int stride)
{
	struct NSVGshape *shape = NULL;
	struct NSVGedge *e = NULL;
	int i;
	
	r->bitmap = dst;
	r->width = w;
	r->height = h;
	r->stride = stride;

	if (w > r->cscanline) {
		r->cscanline = w;
		r->scanline = (unsigned char*)realloc(r->scanline, w);
		if (r->scanline == NULL) return;
	}

	for (i = 0; i < h; i++)
		memset(&dst[i*stride], 0, w*4);

	for (shape = image->shapes; shape != NULL; shape = shape->next) {

		if (!shape->hasFill)
			continue;

		r->nedges = 0;
		r->nactedges = 0;
		r->freelist = NULL;

		nsvg__flattenShape(r, shape, tx,ty,scale);

		// Scale and translate edges
		for (i = 0; i < r->nedges; i++) {
			e = &r->edges[i];
			e->x0 = tx + e->x0 * scale;
			e->y0 = (ty + e->y0 * scale) * NSVG__SUBSAMPLES;
			e->x1 = tx + e->x1 * scale;
			e->y1 = (ty + e->y1 * scale) * NSVG__SUBSAMPLES;
		}

		// Rasterize edges
		qsort(r->edges, r->nedges, sizeof(struct NSVGedge), nsvg__cmpEdge);

		// now, traverse the scanlines and find the intersections on each scanline, use non-zero rule
		nsvg__rasterizeSortedEdges(r, shape->fillColor);
	}

	nsvg__unpremultiplyAlpha(dst, w, h, stride);

	r->bitmap = NULL;
	r->width = 0;
	r->height = 0;
	r->stride = 0;
}

#endif