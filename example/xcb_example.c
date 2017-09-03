#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

int main(int argc, char **argv){
   xcb_connection_t *c = xcb_connect(0, 0);
   xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
   int w, h, n,
      depth = s->root_depth,
      win_class = XCB_WINDOW_CLASS_INPUT_OUTPUT,
      format = XCB_IMAGE_FORMAT_Z_PIXMAP;
   xcb_drawable_t win = xcb_generate_id(c);
   xcb_gcontext_t gc = xcb_generate_id(c);
   xcb_pixmap_t pixmap = xcb_generate_id(c);
   xcb_generic_event_t *ev;
   xcb_image_t *image;
   NSVGimage *shapes = NULL;
   NSVGrasterizer *rast = NULL;
   char *file = NULL;
   unsigned char *data = NULL;
   unsigned *dp;
   size_t i, len;
   uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
      value_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS,
      values[] = { s->black_pixel, value_mask };

   if (argc<2) file = "../example/23.svg";
   else file = argv[1];

   if ((data = stbi_load(file, &w, &h, &n, 4)))
      ;
   else if ((shapes = nsvgParseFromFile(file, "px", 96.0f))) {
      w = (int)shapes->width;
      h = (int)shapes->height;
      rast = nsvgCreateRasterizer();
      data = malloc(w*h*4);
      nsvgRasterize(rast, shapes, 0,0,1, data, w, h, w*4);
   }else return -1;
   for(i=0,len=w*h,dp=(unsigned *)data;i<len;i++) //rgba to bgra
      dp[i]=dp[i]&0xff00ff00|((dp[i]>>16)&0xFF)|((dp[i]<<16)&0xFF0000);
   xcb_create_window(c,depth,win,s->root,0,0,w,h,1,win_class,s->root_visual,mask,values);
   xcb_create_pixmap(c,depth,pixmap,win,w,h);
   xcb_create_gc(c,gc,pixmap,0,NULL);
   image = xcb_image_create_native(c,w,h,format,depth,data,w*h*4,data);
   xcb_image_put(c, pixmap, gc, image, 0, 0, 0);
   xcb_image_destroy(image);
   xcb_map_window(c, win);
   xcb_flush(c);
   while ((ev = xcb_wait_for_event(c))) {
      switch (ev->response_type & ~0x80){
      case XCB_EXPOSE: {
         xcb_expose_event_t *x = (xcb_expose_event_t *)ev;
         xcb_copy_area(c,pixmap,win,gc,x->x,x->y,x->x,x->y,x->width,x->height);
         xcb_flush(c);
      }break;
      case XCB_BUTTON_PRESS: goto end;
      default: break;
      }
   }
end:
   xcb_free_pixmap(c, pixmap);
   xcb_disconnect(c);
   return 0;
}
