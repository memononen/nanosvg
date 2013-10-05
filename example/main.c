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
#include <GLFW/glfw3.h>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

struct NSVGPath* g_plist = NULL;

static unsigned char bgColor[4] = {205,202,200,255};
static unsigned char lineColor[4] = {0,160,192,255};

static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }

static float distPtSeg(float x, float y, float px, float py, float qx, float qy)
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

static void cubicBez(float x1, float y1, float x2, float y2,
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

	d = distPtSeg(x1234, y1234, x1,y1, x4,y4);
	if (d > tol*tol) {
		cubicBez(x1,y1, x12,y12, x123,y123, x1234,y1234, tol, level+1); 
		cubicBez(x1234,y1234, x234,y234, x34,y34, x4,y4, tol, level+1); 
	} else {
		glVertex2f(x4, y4);
	}
}

static void calcBounds(struct NSVGPath* plist, float* bounds)
{
	struct NSVGPath* it;
	int i;
	bounds[0] = FLT_MAX;
	bounds[1] = FLT_MAX;
	bounds[2] = -FLT_MAX;
	bounds[3] = -FLT_MAX;
	for (it = plist; it; it = it->next) {
		for (i = 0; i < it->npts; i++) {
			float* p = &it->pts[i*2];
			bounds[0] = minf(bounds[0], p[0]);
			bounds[1] = minf(bounds[1], p[1]);
			bounds[2] = maxf(bounds[2], p[0]);
			bounds[3] = maxf(bounds[3], p[1]);
		}
	}
}

void drawPath(float* pts, int npts, char closed, float tol)
{
	int i;
	glBegin(GL_LINE_STRIP);
	glColor4ubv(lineColor);
	glVertex2f(pts[0], pts[1]);
	for (i = 0; i < npts-1; i += 3) {
		float* p = &pts[i*2];
		cubicBez(p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], tol, 0);
	}
	if (closed) {
		glVertex2f(pts[0], pts[1]);
	}
	glEnd();
}

void drawControlPts(float* pts, int npts, char closed)
{
	int i;

	// Control lines
	glColor4ubv(lineColor);
	glBegin(GL_LINES);
	for (i = 0; i < npts-1; i += 3) {
		float* p = &pts[i*2];
		glVertex2f(p[0],p[1]);
		glVertex2f(p[2],p[3]);
		glVertex2f(p[4],p[5]);
		glVertex2f(p[6],p[7]);
	}
	glEnd();

	// Points
	glPointSize(6.0f);
	glColor4ubv(lineColor);

	glVertex2f(pts[0],pts[1]);
	glBegin(GL_POINTS);
	for (i = 0; i < npts-1; i += 3) {
		float* p = &pts[i*2];
		glVertex2f(p[6],p[7]);
	}
	glEnd();

	// Points
	glPointSize(3.0f);

	glColor4ubv(bgColor);
	glVertex2f(pts[0],pts[1]);
	glBegin(GL_POINTS);
	for (i = 0; i < npts-1; i += 3) {
		float* p = &pts[i*2];
		glColor4ubv(lineColor);
		glVertex2f(p[2],p[3]);
		glVertex2f(p[4],p[5]);
		glColor4ubv(bgColor);
		glVertex2f(p[6],p[7]);
	}
	glEnd();
}

void drawframe(GLFWwindow* window)
{
	int width = 0, height = 0;
	float bounds[4], view[4], cx, cy, w, h, aspect, px;
	struct NSVGPath* it;

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwGetFramebufferSize(window, &width, &height);

	glViewport(0, 0, width, height);
	glClearColor(220.0f/255.0f, 220.0f/255.0f, 220.0f/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Fit view to bounds
	calcBounds(g_plist, bounds);
	cx = (bounds[0]+bounds[2])/2;
	cy = (bounds[3]+bounds[1])/2;
	w = (bounds[2]-bounds[0])/2;
	h = (bounds[3]-bounds[1])/2;

	if (width/w < height/h) {
		aspect = (float)height / (float)width;
		view[0] = cx - w * 1.2f;
		view[2] = cx + w * 1.2f;
		view[1] = cy - w * 1.2f * aspect;
		view[3] = cy + w * 1.2f * aspect;
	} else {
		aspect = (float)width / (float)height;
		view[0] = cx - h * 1.2f * aspect;
		view[2] = cx + h * 1.2f * aspect;
		view[1] = cy - h * 1.2f;
		view[3] = cy + h * 1.2f;
	}
	// Size of one pixel.
	px = (view[2] - view[1]) / (float)width;

	glOrtho(view[0], view[2], view[3], view[1], -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glColor4ub(255,255,255,255);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	for (it = g_plist; it; it = it->next) {
		drawPath(it->pts, it->npts, it->closed, px * 1.5f);
		drawControlPts(it->pts, it->npts, it->closed);
	}

	glfwSwapBuffers(window);
}

void resizecb(GLFWwindow* window, int width, int height)
{
	// Update and render
	drawframe(window);
}

int main()
{
	GLFWwindow* window;
	const GLFWvidmode* mode;

	if (!glfwInit())
		return -1;

	mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    window = glfwCreateWindow(mode->width - 40, mode->height - 80, "Nano SVG", NULL, NULL);
	if (!window)
	{
		printf("Could not open window\n");
		glfwTerminate();
		return -1;
	}

	glfwSetFramebufferSizeCallback(window, resizecb);
	glfwMakeContextCurrent(window);
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);


	g_plist = nsvgParseFromFile("../example/nano.svg");
	if (g_plist == NULL) {
		printf("Could not open test.svg\n");
		glfwTerminate();
		return -1;
	}

	while (!glfwWindowShouldClose(window))
	{
		drawframe(window);
		glfwPollEvents();
	}

	nsvgDelete(g_plist);

	glfwTerminate();
	return 0;
}
