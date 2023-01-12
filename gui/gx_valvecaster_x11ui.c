
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include <stdio.h>
#include <stdlib.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include "./gx_valvecaster.h"

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
				define controller numbers
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

#define CONTROLS 5

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
				define min/max if not defined already
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef max
#define max(x, y) ((x) < (y) ? (y) : (x))
#endif


/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
					define debug print
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

#ifndef DEBUG
#define DEBUG 0
#endif
#define debug_print(...) \
            ((void)((DEBUG) ? fprintf(stderr, __VA_ARGS__) : 0))


/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
		define some MACROS to read png data from binary stream 
		png's been converted to object files with
		ld -r -b binary name.png -o name.o
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

#ifdef __APPLE__
#include <mach-o/getsect.h>

#define EXTLD(NAME) \
  extern const unsigned char _section$__DATA__ ## NAME [];
#define LDVAR(NAME) _section$__DATA__ ## NAME
#define LDLEN(NAME) (getsectbyname("__DATA", "__" #NAME)->size)

#elif (defined __WIN32__)  /* mingw */

#define EXTLD(NAME) \
  extern const unsigned char binary_ ## NAME ## _start[]; \
  extern const unsigned char binary_ ## NAME ## _end[];
#define LDVAR(NAME) \
  binary_ ## NAME ## _start
#define LDLEN(NAME) \
  ((binary_ ## NAME ## _end) - (binary_ ## NAME ## _start))

#else /* gnu/linux ld */

#define EXTLD(NAME) \
  extern const unsigned char _binary_ ## NAME ## _start[]; \
  extern const unsigned char _binary_ ## NAME ## _end[];
#define LDVAR(NAME) \
  _binary_ ## NAME ## _start
#define LDLEN(NAME) \
  ((_binary_ ## NAME ## _end) - (_binary_ ## NAME ## _start))
#endif

// png's linked in as binarys
EXTLD(pedal_png)
EXTLD(pswitch_png)

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
					define needed structs
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// struct definition to read binary data into cairo surface 
typedef struct  {
	const unsigned char * data;
	long int position;
} binary_stream;

// define controller type
typedef enum {
	KNOB,
	SWITCH,
	ENUM,
	BSWITCH,
} ctype;

// define controller position in window
typedef struct {
	int x;
	int y;
	int width;
	int height;
} gx_alinment;

// define controller adjustment
typedef struct {
	float std_value;
	float value;
	float min_value;
	float max_value;
	float step;
} gx_adjustment;

// controller struct
typedef struct {
	gx_adjustment adj;
	gx_alinment al;
	bool is_active;
	const char* label;
	ctype type;
	PortIndex port;
} gx_controller;

// resize window
typedef struct {
	double x;
	double y;
	double x1;
	double y1;
	double x2;
	double y2;
	double c;
	double xc;
} gx_scale;

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
				the main LV2 handle->XWindow
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// main window struct
typedef struct {
	Display *dpy;
	Window win;
	void *parentXwindow;
	Visual *visual;
	long event_mask;
	Atom DrawController;
	bool first_resize;
	bool blocked;

	int width;
	int height;
	int init_width;
	int init_height;
	int pos_x;
	int pos_y;

	binary_stream png_stream;
	cairo_surface_t *surface;
	cairo_surface_t *pedal;
	cairo_surface_t *pswitch;
	cairo_surface_t *frame;
	cairo_t *crf;
	cairo_t *cr;

	gx_controller controls[CONTROLS];
	int block_event;
	double start_value;
	gx_scale rescale;
	gx_controller *sc;
	int set_sc;

	void *controller;
	LV2UI_Write_Function write_function;
	LV2UI_Resize* resize;
} gx_valvecasterUI;

// forward declaration to resize window and cairo surface
static void resize_event(gx_valvecasterUI * const ui);

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
		load png data from binary blob into cairo surface
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// read png data from binary blob
cairo_status_t png_stream_reader (void *_stream, unsigned char *data, unsigned int length) {
	binary_stream * stream = (binary_stream *) _stream;
	memcpy(data, &stream->data[stream->position],length);
	stream->position += length;
	return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *cairo_image_surface_create_from_stream (gx_valvecasterUI * const ui, const unsigned char* name) {
	ui->png_stream.data = name;
	ui->png_stream.position = 0;
	return cairo_image_surface_create_from_png_stream(&png_stream_reader, (void *)&ui->png_stream);
}

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
				XWindow init the LV2 handle
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// init the xwindow and return the LV2UI handle
static LV2UI_Handle instantiate(const LV2UI_Descriptor * descriptor,
			const char * plugin_uri, const char * bundle_path,
			LV2UI_Write_Function write_function,
			LV2UI_Controller controller, LV2UI_Widget * widget,
			const LV2_Feature * const * features) {

	gx_valvecasterUI* ui = (gx_valvecasterUI*)malloc(sizeof(gx_valvecasterUI));

	if (!ui) {
		fprintf(stderr,"ERROR: failed to instantiate plugin with URI %s\n", plugin_uri);
		return NULL;
	}

	ui->parentXwindow = 0;
	LV2UI_Resize* resize = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_UI__parent)) {
			ui->parentXwindow = features[i]->data;
		} else if (!strcmp(features[i]->URI, LV2_UI__resize)) {
			resize = (LV2UI_Resize*)features[i]->data;
		}
	}

	if (ui->parentXwindow == NULL)  {
		fprintf(stderr, "ERROR: Failed to open parentXwindow for %s\n", plugin_uri);
		free(ui);
		return NULL;
	}

	ui->dpy = XOpenDisplay(0);

	if (ui->dpy == NULL)  {
		fprintf(stderr, "ERROR: Failed to open display for %s\n", plugin_uri);
		free(ui);
		return NULL;
	}

	ui->controls[0] = (gx_controller) {{1.0, 1.0, 0.0, 1.0, 1.0}, {40, 70, 81, 81}, false,"POWER", BSWITCH, BYPASS};
	ui->controls[1] = (gx_controller) {{0.5, 0.5, 0.0, 1.0, 0.01}, {350, 70, 81, 81}, false,"GAIN", KNOB, GAIN};
	ui->controls[2] = (gx_controller) {{0.5, 0.5, 0.0, 1.0, 0.01}, {470, 70, 81, 81}, false,"TONE", KNOB, TONE};
	ui->controls[3] = (gx_controller) {{0.5, 0.5, 0.0, 1.0, 0.01}, {590, 70, 81, 81}, false,"VOLUME", KNOB, VOLUME};
	ui->controls[4] = (gx_controller) {{0.0, 0.0, 0.0, 1.0, 1.0}, {680, 70, 81, 81}, false,"BOOST", SWITCH, BOOST};
	ui->block_event = -1;
	ui->start_value = 0.0;
	ui->sc = NULL;
	ui->set_sc = 0;

	ui->pedal = cairo_image_surface_create_from_stream(ui, LDVAR(pedal_png));
	ui->init_width = cairo_image_surface_get_width(ui->pedal);
	ui->height = ui->init_height = cairo_image_surface_get_height(ui->pedal);
	ui->width = ui->init_width;

	ui->win = XCreateWindow(ui->dpy, (Window)ui->parentXwindow, 0, 0,
								ui->width, ui->height, 0,
								CopyFromParent, InputOutput,
								CopyFromParent, CopyFromParent, 0);

	ui->event_mask = StructureNotifyMask|ExposureMask|KeyPressMask
					|EnterWindowMask|LeaveWindowMask|ButtonReleaseMask
					|ButtonPressMask|Button1MotionMask;


	XSelectInput(ui->dpy, ui->win, ui->event_mask);

	XSizeHints* win_size_hints;
	win_size_hints = XAllocSizeHints();
	win_size_hints->flags = PBaseSize | PMinSize;
	win_size_hints->min_width = ui->width/1.4;
	win_size_hints->min_height = ui->height/1.4;
	win_size_hints->base_width = ui->width;
	win_size_hints->base_height = ui->height;
	XSetWMNormalHints(ui->dpy, ui->win, win_size_hints);
	XFree(win_size_hints);

	XMapWindow(ui->dpy, ui->win);
	XClearWindow(ui->dpy, ui->win);

	ui->visual = DefaultVisual(ui->dpy, DefaultScreen (ui->dpy));
	ui->surface = cairo_xlib_surface_create (ui->dpy, ui->win, ui->visual,
										ui->width, ui->height);
	ui->cr = cairo_create(ui->surface);

	ui->frame = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 81, 101);
	ui->crf = cairo_create (ui->frame);

	ui->pswitch = cairo_image_surface_create_from_stream(ui, LDVAR(pswitch_png));

	*widget = (void*)ui->win;

	ui->first_resize = false;
	ui->blocked = false;
	if (resize){
		ui->resize = resize;
		resize->ui_resize(resize->handle, ui->width, ui->height);
		ui->first_resize = true;
	}

	ui->rescale.x  = (double)ui->width/ui->init_width;
	ui->rescale.y  = (double)ui->height/ui->init_height;
	ui->rescale.x1 = (double)ui->init_width/ui->width;
	ui->rescale.y1 = (double)ui->init_height/ui->height;
	ui->rescale.xc = (double)ui->width/ui->init_width;
	ui->rescale.c = (ui->rescale.xc < ui->rescale.y) ? ui->rescale.xc : ui->rescale.y;
	ui->rescale.x2 =  ui->rescale.xc / ui->rescale.c;
	ui->rescale.y2 = ui->rescale.y / ui->rescale.c;

	ui->DrawController = XInternAtom(ui->dpy, "ControllerMessage", False);

	ui->controller = controller;
	ui->write_function = write_function;
	//resize_event(ui);

	return (LV2UI_Handle)ui;
}

// cleanup after usage
static void cleanup(LV2UI_Handle handle) {
	gx_valvecasterUI * const ui = (gx_valvecasterUI*)handle;
	cairo_destroy(ui->cr);
	cairo_destroy(ui->crf);
	cairo_surface_destroy(ui->pedal);
	cairo_surface_destroy(ui->pswitch);
	cairo_surface_destroy(ui->surface);
	cairo_surface_destroy(ui->frame);
	XDestroyWindow(ui->dpy, ui->win);
	XCloseDisplay(ui->dpy);
	free(ui);
}

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
				XWindow drawing expose handling
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// draw knobs and simple switches
static void knob_expose(const gx_valvecasterUI * const ui, const gx_controller * const knob) {
	cairo_set_operator(ui->crf,CAIRO_OPERATOR_CLEAR);
	cairo_paint(ui->crf);
	cairo_set_operator(ui->crf,CAIRO_OPERATOR_OVER);
	const double scale_zero = 20 * (M_PI/180); // defines "dead zone" for knobs
	int arc_offset = 0;
	int knob_x = 0;
	int knob_y = 0;
	int w = cairo_image_surface_get_width(ui->frame);
	int h = cairo_image_surface_get_height(ui->frame)-20;
	int grow = (w > h) ? h:w;
	if (knob->type == SWITCH) {
		knob_x = grow-45;
		knob_y = grow-45; 
	} else if (knob->type == ENUM) {
		knob_x = grow-25;
		knob_y = grow-25; 
	} else {
		knob_x = grow-1;
		knob_y = grow-1;
	}
	/** get values for the knob **/

	int knobx = (w - knob_x) * 0.5;
	int knobx1 = w* 0.5;

	int knoby = (h - knob_y) * 0.5;
	int knoby1 = h * 0.5;

	double knobstate = (knob->adj.value - knob->adj.min_value) / (knob->adj.max_value - knob->adj.min_value);
	double angle = scale_zero + knobstate * 2 * (M_PI - scale_zero);

	double pointer_off =knob_x/6;
	double radius = min(knob_x-pointer_off, knob_y-pointer_off) / 2;
	double lengh_x = (knobx+radius+pointer_off/2) - radius/1.4 * sin(angle);
	double lengh_y = (knoby+radius+pointer_off/2) + radius/1.4 * cos(angle);
	double radius_x = (knobx+radius+pointer_off/2) - radius/ 1.6 * sin(angle);
	double radius_y = (knoby+radius+pointer_off/2) + radius/ 1.6 * cos(angle);

	cairo_pattern_t* pat;
	cairo_new_path (ui->crf);

	pat = cairo_pattern_create_linear (0, 0, 0, knob_y);
	cairo_pattern_add_color_stop_rgba (pat, 0,  0.0, 0.0, 0.0, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 0.75,  0.15, 0.15, 0.15, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 0.5,  0.2, 0.2, 0.2, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 0.25,  0.15, 0.15, 0.15, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 1,  0.0, 0.0, 0.0, 1.0);

	cairo_arc(ui->crf,knobx1+arc_offset/2, knoby1+arc_offset/2, knob_x/2.1, 0, 2 * M_PI );
	cairo_set_source (ui->crf, pat);
	cairo_fill_preserve (ui->crf);
 	cairo_set_source_rgb (ui->crf, 0.1, 0.1, 0.1); 
	cairo_set_line_width(ui->crf,1);
	cairo_stroke(ui->crf);
	cairo_new_path (ui->crf);

	cairo_arc(ui->crf,knobx1+arc_offset/2, knoby1+arc_offset/2, knob_x/2.6, 0, 2 * M_PI );
	cairo_set_source (ui->crf, pat);
	cairo_fill_preserve (ui->crf);
 	cairo_set_source_rgb (ui->crf, 0.15, 0.15, 0.15); 
	cairo_set_line_width(ui->crf,1);
	cairo_stroke(ui->crf);
	cairo_new_path (ui->crf);

	pat = cairo_pattern_create_radial (knobx1+arc_offset/2-10, knoby1+arc_offset/2-20,
											  1,knobx1+arc_offset,knoby1+arc_offset,knob_x/2.4 );
	pat = cairo_pattern_create_linear (0, 0, 0, knob_y);
	cairo_pattern_add_color_stop_rgba (pat, 1,  0.0, 0.0, 0.0, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 0.5,  0.15, 0.15, 0.15, 1.0);
	cairo_pattern_add_color_stop_rgba (pat, 0.,  0.2, 0.2, 0.2, 1.0);

	cairo_arc(ui->crf,knobx1+arc_offset/2, knoby1+arc_offset/2, knob_x/2.8, 0, 2 * M_PI );
	cairo_set_source (ui->crf, pat);
	cairo_fill_preserve (ui->crf);
 	cairo_set_source_rgb (ui->crf, 0.15, 0.15, 0.15); 
	cairo_set_line_width(ui->crf,1);
	cairo_stroke(ui->crf);
	cairo_new_path (ui->crf);

	/** create a rotating pointer on the kob**/
	cairo_set_line_cap(ui->crf, CAIRO_LINE_CAP_ROUND); 
	cairo_set_line_join(ui->crf, CAIRO_LINE_JOIN_BEVEL);
	cairo_move_to(ui->crf, radius_x, radius_y);
	cairo_line_to(ui->crf,lengh_x,lengh_y);
	cairo_set_line_width(ui->crf,4);
	cairo_set_source_rgb (ui->crf,0.63,0.63,0.63);
	cairo_stroke(ui->crf);
	cairo_new_path (ui->crf);

	cairo_text_extents_t extents;
	/** show value on the kob**/
	if (knob->type == KNOB && knob->is_active == true) {
		char s[64];
		const char* format[] = {"%.1f", "%.2f", "%.3f"};
		if (fabs(knob->adj.value)>99.99) {
			snprintf(s, 63,"%d",  (int) knob->adj.value);
		} else if (fabs(knob->adj.value)>9.99) {
			snprintf(s, 63, format[1-1], knob->adj.value);
		} else {
			snprintf(s, 63, format[2-1], knob->adj.value);
		}
		cairo_set_source_rgba (ui->crf, 0.6, 0.6, 0.6, 0.6);
		cairo_set_font_size (ui->crf, 11.0);
		cairo_select_font_face (ui->crf, "Sans", CAIRO_FONT_SLANT_NORMAL,
								   CAIRO_FONT_WEIGHT_BOLD);
		cairo_text_extents(ui->crf, "0.00", &extents);
		cairo_move_to (ui->crf, knobx1-extents.width/2, knoby1+extents.height/2);
		cairo_show_text(ui->crf, s);
		cairo_new_path (ui->crf);
	} else if (knob->type == SWITCH) {
		if(knob->adj.value)
			cairo_set_source_rgba (ui->crf, 0.6, 0.6, 0.6, 0.6);
		else
			cairo_set_source_rgba (ui->crf, 0.8, 0.8, 0.8, 0.8);
		cairo_text_extents(ui->crf,"Off", &extents);
		cairo_move_to (ui->crf, knobx1-knob_x/2.4-extents.width/1.6, knoby1+knob_y/1.4+extents.height/1.4);
		cairo_show_text(ui->crf, "Off");
		cairo_new_path (ui->crf);

		if(!knob->adj.value)
			cairo_set_source_rgba (ui->crf, 0.6, 0.6, 0.6, 0.6);
		else
			cairo_set_source_rgba (ui->crf, 0.8, 0.8, 0.8, 0.8);
		cairo_text_extents(ui->crf,"On", &extents);
		cairo_move_to (ui->crf, knobx1+knob_x/2.6-extents.width/2.3, knoby1+knob_y/1.4+extents.height/1.4);
		cairo_show_text(ui->crf, "On");
		cairo_new_path (ui->crf);
	} else if (knob->type == ENUM) {
		cairo_set_source_rgba (ui->crf, 0.0, 0.0, 0.0, 1.0);
		cairo_text_extents(ui->crf,"1", &extents);
		cairo_move_to (ui->crf, knobx1-knob_x/2.4-extents.width/1.6, knoby1+knob_y/2+extents.height/1.4);
		cairo_show_text(ui->crf, "1");
		cairo_new_path (ui->crf);

		cairo_text_extents(ui->crf,"2", &extents);
		cairo_move_to (ui->crf, knobx1-extents.width/2, knoby1-knob_y/2-extents.height/2);
		cairo_show_text(ui->crf, "2");
		cairo_new_path (ui->crf);

		cairo_text_extents(ui->crf,"3", &extents);
		cairo_move_to (ui->crf, knobx1+knob_x/2.6-extents.width/2.3, knoby1+knob_y/2+extents.height/1.4);
		cairo_show_text(ui->crf, "3");
		cairo_new_path (ui->crf);
	}
	cairo_pattern_destroy (pat);

	/** show label below the knob**/
	if (knob->is_active) {
		cairo_set_source_rgba (ui->crf, 0.8, 0.8, 0.8, 0.8);
	} else {
		cairo_set_source_rgba (ui->crf, 0.6, 0.6, 0.6, 0.6);
	}
	cairo_set_font_size (ui->crf, 12.0);
	cairo_select_font_face (ui->crf, "Sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_BOLD);
	cairo_text_extents(ui->crf,knob->label , &extents);

	cairo_move_to (ui->crf, knobx1-extents.width/2, grow+6+extents.height);
	cairo_show_text(ui->crf, knob->label);
	cairo_new_path (ui->crf);
}

static void draw_grid(const gx_valvecasterUI * const ui) {

	const double x0 = 160.0;
	const double y0 = 58.0;
	const double x1 = 320.0;
	const double y1 = 140.0;

	cairo_pattern_t* pat = cairo_pattern_create_radial (300, 140,
											  1,300,140,140 );
	if (ui->controls[0].adj.value > 0.9) {
		cairo_pattern_add_color_stop_rgba (pat, 1,  0.1*ui->controls[0].adj.value, 0.0, 0.0, 1.0);
		cairo_pattern_add_color_stop_rgba (pat, 0.5,  0.15, 0.15,0.15+ (0.1*ui->controls[0].adj.value), 1.0);
		cairo_pattern_add_color_stop_rgba (pat, 0,  0.0, 0.0, 0.0 + (0.3*ui->controls[0].adj.value), 1.0);
	} else {
		cairo_pattern_add_color_stop_rgba (pat, 1,  0.0, 0.0, 0.0, 1.0);
	}

	cairo_set_line_cap(ui->cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(ui->cr, CAIRO_LINE_JOIN_BEVEL);
	//cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.4);
	cairo_set_source (ui->cr, pat);
	cairo_set_line_width (ui->cr, 6.0);
	for (int i=x0+5; i<=x1; i+=20) {
		cairo_move_to (ui->cr, i, y0);
		cairo_line_to (ui->cr, i, y1);
	}
	cairo_stroke (ui->cr);
	cairo_pattern_destroy (pat);

}

// draw the power switch (bypass)
static void bypass_expose(const gx_valvecasterUI * const ui,  const gx_controller * const switch_) {
	cairo_set_operator(ui->crf,CAIRO_OPERATOR_CLEAR);
	cairo_paint(ui->crf);
	cairo_set_operator(ui->crf,CAIRO_OPERATOR_OVER);

	cairo_set_source_surface (ui->crf, ui->pswitch, -80 * switch_->adj.value, 0);

	cairo_rectangle(ui->crf,0, 0, 81, 81);
	cairo_fill(ui->crf);
	/** show label below the switch**/
	cairo_text_extents_t extents;

	if (switch_->is_active) {
		cairo_set_source_rgba (ui->crf, 0.8, 0.8, 0.8,0.8);
	} else {
		cairo_set_source_rgba (ui->crf, 0.6, 0.6, 0.6,0.6);
	}
	cairo_set_font_size (ui->crf, 12.0);
	cairo_select_font_face (ui->crf, "Sans", CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_BOLD);
	cairo_text_extents(ui->crf,switch_->label , &extents);

	cairo_move_to (ui->crf, 40.0-extents.width/2, 87.0+extents.height);
	cairo_show_text(ui->crf, switch_->label);
	cairo_new_path (ui->crf);

	cairo_scale (ui->cr, 1.0/ui->rescale.c, 1.0/ui->rescale.c);
	cairo_scale (ui->cr, ui->rescale.x, ui->rescale.y);
	draw_grid(ui);
	cairo_scale (ui->cr, ui->rescale.x1, ui->rescale.y1);
	cairo_scale (ui->cr, ui->rescale.c, ui->rescale.c);

}

// select draw methode by controller type
static void draw_controller(const gx_valvecasterUI * const ui,  const gx_controller * const controller) {
	if (controller->type < BSWITCH) knob_expose(ui, controller);
	else bypass_expose(ui, controller);
}

// general XWindow expose callback, 
static void _expose(const gx_valvecasterUI * const ui) {
	cairo_push_group (ui->cr);

	cairo_scale (ui->cr, ui->rescale.x, ui->rescale.y);
	cairo_set_source_surface (ui->cr, ui->pedal, 0, 0);
	cairo_paint (ui->cr);
	cairo_scale (ui->cr, ui->rescale.x1, ui->rescale.y1);

	cairo_scale (ui->cr, ui->rescale.c, ui->rescale.c);
	for (int i=0;i<CONTROLS;i++) {
		draw_controller(ui, &ui->controls[i]);
		cairo_set_source_surface (ui->cr, ui->frame, 
		  (double)ui->controls[i].al.x * ui->rescale.x2,
		  (double)ui->controls[i].al.y * ui->rescale.y2);
		cairo_paint (ui->cr);
	}

	cairo_pop_group_to_source (ui->cr);
	cairo_paint (ui->cr);
}

// redraw a single controller
static void controller_expose(const gx_valvecasterUI * const ui,  const gx_controller * const control) {
	cairo_push_group (ui->cr);

	cairo_scale (ui->cr, ui->rescale.x, ui->rescale.y);
	cairo_set_source_surface (ui->cr, ui->pedal, 0, 0);
	cairo_scale (ui->cr, ui->rescale.x1, ui->rescale.y1);

	cairo_scale (ui->cr, ui->rescale.c, ui->rescale.c);
	cairo_rectangle (ui->cr,(double)control->al.x * ui->rescale.x2,
	  (double)control->al.y * ui->rescale.y2,
	  (double)control->al.width, (double)control->al.height+20.0);
	cairo_fill(ui->cr);
	cairo_stroke(ui->cr);

	draw_controller(ui, control);
	cairo_set_source_surface (ui->cr, ui->frame, 
	  (double)control->al.x * ui->rescale.x2,
	  (double)control->al.y * ui->rescale.y2);
	cairo_paint (ui->cr);

	cairo_pop_group_to_source (ui->cr);
	cairo_paint (ui->cr);
}

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
				XWindow event handling
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// resize the xwindow and the cairo xlib surface
static void resize_event(gx_valvecasterUI * const ui) {
	XWindowAttributes attrs;
	XGetWindowAttributes(ui->dpy, (Window)ui->parentXwindow, &attrs);
	ui->width = attrs.width;
	ui->height = attrs.height;
	XResizeWindow (ui->dpy,ui->win ,ui->width, ui->height);
	cairo_xlib_surface_set_size( ui->surface, ui->width, ui->height);
	ui->rescale.x  = (double)ui->width/ui->init_width;
	ui->rescale.y  = (double)ui->height/ui->init_height;
	ui->rescale.x1 = (double)ui->init_width/ui->width;
	ui->rescale.y1 = (double)ui->init_height/ui->height;
	ui->rescale.xc = (double)ui->width/ui->init_width;
	ui->rescale.c = (ui->rescale.xc < ui->rescale.y) ? ui->rescale.xc : ui->rescale.y;
	ui->rescale.x2 =  ui->rescale.xc / ui->rescale.c;
	ui->rescale.y2 = ui->rescale.y / ui->rescale.c;
}

// send event when active controller changed
static void send_controller_event(gx_valvecasterUI * const ui, int controller) {
	XClientMessageEvent xevent;
	xevent.type = ClientMessage;
	xevent.message_type = ui->DrawController;
	xevent.display = ui->dpy;
	xevent.window = ui->win;
	xevent.format = 16;
	xevent.data.l[0] = controller;
	XSendEvent(ui->dpy, ui->win, 0, 0, (XEvent *)&xevent);
}

/*------------- check and set state of controllers ---------------*/

// check if controller value changed, if so, redraw
static void check_value_changed(gx_valvecasterUI * const ui, int i, const float * const value) {
	if(fabs(*(value) - ui->controls[i].adj.value)>=0.00001) {
		ui->controls[i].adj.value = *(value);
		if (ui->block_event != ui->controls[i].port)
			ui->write_function(ui->controller,ui->controls[i].port,sizeof(float),0,value);
		debug_print("send_controller_event for %i value %f\n",i,*(value));
		send_controller_event(ui, i);
	}
}

// check if controller activation state changed, if so, redraw
static void check_is_active(gx_valvecasterUI * const ui, int i, bool set) {
	if (ui->controls[i].is_active != set) {
		ui->controls[i].is_active = set;
		send_controller_event(ui, i);
	}
}

// check if controller is under mouse pointer
static bool aligned(int x, int y, const gx_controller * const control, const gx_valvecasterUI * const ui) {
	double ax = (control->al.x * ui->rescale.x2)* ui->rescale.c;
	double ay = (control->al.y  * ui->rescale.y2)* ui->rescale.c;
	double aw = ax + (control->al.width * ui->rescale.c);
	double ah = ay + (control->al.height * ui->rescale.c);
	return ((x >= ax ) && (x <= aw)
	  && (y >= ay ) && (y <= ah)) ? true : false;
}

// get controller number under mouse pointer and make it active, or return false
static bool get_active_ctl_num(gx_valvecasterUI * const ui, int *num) {
	bool ret = false;
	for (int i=0;i<CONTROLS;i++) {
		if (aligned(ui->pos_x, ui->pos_y, &ui->controls[i], ui)) {
			*(num) = i;
			check_is_active(ui, i, true);
			debug_print("get_active_ctl_num %i \n",i);
			ret = true;
		} else {
			check_is_active(ui, i, false);
		}
	}
	return ret;
}

// get current active controller number, or return false
static bool get_active_controller_num( const gx_valvecasterUI * const ui, int *num) {
	for (int i=0;i<CONTROLS;i++) {
		if (ui->controls[i].is_active) {
			*(num) = i;
			debug_print("get_active_controller_num %i \n",i);
			return true;
		}
	}
	return false;
}

/*------------- mouse event handlings ---------------*/

// mouse wheel scroll event
static void scroll_event(gx_valvecasterUI * const ui, int direction) {
	float value;
	int num;
	if (get_active_ctl_num(ui, &num)) {
		value = min(ui->controls[num].adj.max_value,max(ui->controls[num].adj.min_value, 
		  ui->controls[num].adj.value + (ui->controls[num].adj.step * direction)));
		check_value_changed(ui, num, &value);
	}
}

// control is a enum, so switch value
static void enum_event(gx_valvecasterUI * const ui, int i) {
	float value;
	if (ui->controls[i].adj.value != ui->controls[i].adj.max_value) {
		value = min(ui->controls[i].adj.max_value,max(ui->controls[i].adj.min_value, 
		  ui->controls[i].adj.value + ui->controls[i].adj.step));
	} else {
		value = ui->controls[i].adj.min_value;
	}
	check_value_changed(ui, i, &value);
}

// control is a switch, so switch value
static void switch_event(gx_valvecasterUI * const ui, int i) {
	float value = ui->controls[i].adj.value ? 0.0 : 1.0;
	check_value_changed(ui, i, &value);
}

// left mouse button is pressed, generate a switch event, or set controller active
static void button1_event(gx_valvecasterUI * const ui, double* start_value) {
	int num;
	debug_print("Button1_event \n");
	if (get_active_ctl_num(ui, &num)) {
		if (ui->controls[num].type == BSWITCH ||ui->controls[num].type == SWITCH) {
			switch_event(ui, num);
		} else if (ui->controls[num].type == ENUM) {
			enum_event(ui, num);
		} else {
			*(start_value) = ui->controls[num].adj.value;
			debug_print("Button1_event set start value %f \n", *(start_value));
		}
	}
}

// mouse move while left button is pressed
static void motion_event(gx_valvecasterUI * const ui, double start_value, int m_y) {
	const double scaling = 0.5;
	float value = 0.0;
	int num;
	debug_print("motion_event \n");
	if (get_active_controller_num(ui, &num)) {
		if (ui->controls[num].type != BSWITCH && ui->controls[num].type != SWITCH && ui->controls[num].type != ENUM) {
			double knobstate = (start_value - ui->controls[num].adj.min_value) /
							   (ui->controls[num].adj.max_value - ui->controls[num].adj.min_value);
			double nsteps = ui->controls[num].adj.step / (ui->controls[num].adj.max_value-ui->controls[num].adj.min_value);
			double nvalue = min(1.0,max(0.0,knobstate + ((double)(ui->pos_y - m_y)*scaling *nsteps)));
			value = nvalue * (ui->controls[num].adj.max_value-ui->controls[num].adj.min_value) + ui->controls[num].adj.min_value;
			debug_print("motion_event check value changed from %f to %f \n",ui->controls[num].adj.value, value);
			check_value_changed(ui, num, &value);
		}
	}
}

/*------------- keyboard event handlings ---------------*/

// set min std or max value, depending on which key is pressed
static void set_key_value(gx_valvecasterUI * const ui, int set_value) {
	float value = 0.0;
	int num;
	if (get_active_controller_num(ui, &num)) {
		if (set_value == 1) value = ui->controls[num].adj.min_value;
		else if (set_value == 2) value = ui->controls[num].adj.std_value;
		else if (set_value == 3) value = ui->controls[num].adj.max_value;
		check_value_changed(ui, num, &value);
	}
}

// scroll up/down on key's up/right down/left
static void key_event(gx_valvecasterUI * const ui, int direction) {
	float value;
	int num;
	if (get_active_controller_num(ui, &num)) {
		value = min(ui->controls[num].adj.max_value,max(ui->controls[num].adj.min_value, 
		  ui->controls[num].adj.value + (ui->controls[num].adj.step * direction)));
		check_value_changed(ui, num, &value);
	}
}

// set previous controller active on shift+tab key's
static void set_previous_controller_active(gx_valvecasterUI * const ui) {
	int num;
	if (get_active_controller_num(ui, &num)) {
		ui->controls[num].is_active = false;
		send_controller_event(ui, num);
		if(num>0) {
			if (ui->controls[num-1].is_active != true) {
				ui->controls[num-1].is_active = true;
				send_controller_event(ui, num-1);
			}
			return;
		} else {
			if (ui->controls[CONTROLS-1].is_active != true) {
				ui->controls[CONTROLS-1].is_active = true;
				send_controller_event(ui, CONTROLS-1);
			}
			return;
		}
	}

	check_is_active(ui, CONTROLS-1, true);
}

// set next controller active on tab key
static void set_next_controller_active(gx_valvecasterUI * const ui) {
	int num;
	if (get_active_controller_num(ui, &num)) {
		ui->controls[num].is_active = false;
		send_controller_event(ui, num);
		if(num<CONTROLS-1) {
			if (ui->controls[num+1].is_active != true) {
				ui->controls[num+1].is_active = true;
				send_controller_event(ui, num+1);
			}
			return;
		} else {
			if (ui->controls[0].is_active != true) {
				ui->controls[0].is_active = true;
				send_controller_event(ui, 0);
			}
			return;
		}
	}
	check_is_active(ui, 0, true);
}

// get/set active controller on enter and leave notify
static void get_last_active_controller(gx_valvecasterUI * const ui, bool set) {
	int num;
	if (get_active_controller_num(ui, &num)) {
		ui->sc = &ui->controls[num];
		ui->set_sc = num;
		ui->controls[num].is_active = set;
		send_controller_event(ui, num);
		return;
	} else if (!set) {
		ui->sc =  NULL;
	}
	if (ui->sc != NULL) {
		ui->sc->is_active = true;
		send_controller_event(ui, ui->set_sc);
	}
}

// map supported key's to integers or return zerro
static int key_mapping(Display *dpy, const XKeyEvent * const xkey) {
	if (xkey->keycode == XKeysymToKeycode(dpy,XK_Tab))
		return (xkey->state == ShiftMask) ? 1 : 2;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_Up))
		return 3;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_Right))
		return 3;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_Down))
		return 4;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_Left))
		return 4;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_Home))
		return 5;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_Insert))
		return 6;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_End))
		return 7;
	// keypad
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Subtract))
		return 1;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Add))
		return 2;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Up))
		return 3;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Right))
		return 3;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Down))
		return 4;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Left))
		return 4;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Home))
		return 5;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_Insert))
		return 6;
	else if (xkey->keycode == XKeysymToKeycode(dpy,XK_KP_End))
		return 7;
	else return 0;
}

/*------------- the event loop ---------------*/

// general xevent handler
static void event_handler(gx_valvecasterUI * const ui) {
	XEvent xev;

	while (XPending(ui->dpy) > 0) {
		XNextEvent(ui->dpy, &xev);

		switch(xev.type) {
			case ConfigureNotify:
				// configure event, we only check for resize events here
				resize_event(ui);
			break;
			case Expose:
				// only fetch the last expose event
				if (xev.xexpose.count == 0) {
					_expose(ui);
				}
			break;

			case ButtonPress:
				ui->pos_x = xev.xbutton.x;
				ui->pos_y = xev.xbutton.y;
				debug_print("Button %i pressed \n", xev.xbutton.button);

				switch(xev.xbutton.button) {
					case Button1:
						if (xev.xbutton.type == ButtonPress) {
							ui->blocked = true;
						}
						// left mouse button click
						button1_event(ui, &ui->start_value);
					break;
					case Button2:
						// click on the mouse wheel
						//debug_print("Button2 \n");
					break;
					case Button3:
						// right mouse button click
						//debug_print("Button3 \n");
					break;
					case  Button4:
						// mouse wheel scroll up
						scroll_event(ui, 1);
					break;
					case Button5:
						// mouse wheel scroll down
						scroll_event(ui, -1);
					break;
					default:
					break;
				}
			break;
			case ButtonRelease:
				if (xev.xbutton.type == ButtonRelease) {
					ui->blocked = false;
				}
			break;

			case KeyPress:
				debug_print("KeyPress %i state %i \n",xev.xkey.keycode,xev.xkey.state);
				switch (key_mapping(ui->dpy, &xev.xkey)) {
					case 1: set_previous_controller_active(ui);
					break;
					case 2: set_next_controller_active(ui);
					break;
					case 3: key_event(ui, 1);
					break;
					case 4: key_event(ui, -1);
					break;
					case 5: set_key_value(ui, 1);
					break;
					case 6: set_key_value(ui, 2);
					break;
					case 7: set_key_value(ui, 3);
					break;
					default:
					break;
				}
			break;

			case EnterNotify:
				if (!ui->blocked) get_last_active_controller(ui, true);
			break;
			case LeaveNotify:
				if (!ui->blocked) get_last_active_controller(ui, false);
			break;
			case MotionNotify:
				// mouse move while button1 is pressed
				debug_print("mouse move from %i to %i  \n", ui->pos_y, xev.xmotion.y);
				if(xev.xmotion.state & Button1Mask) {
					motion_event(ui, ui->start_value, xev.xmotion.y);
				}
			break;
			case ClientMessage:
				if (xev.xclient.message_type == ui->DrawController) {
					int i = (int)xev.xclient.data.l[0];
					controller_expose(ui, &ui->controls[i]);
				}
			break;

			default:
			break;
		}
	}
}

/*---------------------------------------------------------------------
-----------------------------------------------------------------------	
						LV2 interface
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// port value change message from host
static void port_event(LV2UI_Handle handle, uint32_t port_index,
						uint32_t buffer_size, uint32_t format,
						const void * buffer) {
	gx_valvecasterUI * const ui = (gx_valvecasterUI*)handle;
	float value = *(float*)buffer;
	for (int i=0;i<CONTROLS;i++) {
		if (port_index == ui->controls[i].port) {
			ui->block_event = (int)port_index;
			check_value_changed(ui, i, &value);
			ui->block_event = -1;
		}
	}
}

// LV2 idle interface to host
static int ui_idle(LV2UI_Handle handle) {
	gx_valvecasterUI * const ui = (gx_valvecasterUI*)handle;
	event_handler(ui);
	return 0;
}

// LV2 resize interface to host
static int ui_resize(LV2UI_Feature_Handle handle, int w, int h) {
	gx_valvecasterUI * const ui = (gx_valvecasterUI*)handle;
	if (ui) resize_event(ui);
	return 0;
}

// connect idle and resize functions to host
static const void* extension_data(const char* uri) {
	static const LV2UI_Idle_Interface idle = { ui_idle };
	static const LV2UI_Resize resize = { 0 ,ui_resize };
	if (!strcmp(uri, LV2_UI__idleInterface)) {
		return &idle;
	}
	if (!strcmp(uri, LV2_UI__resize)) {
		return &resize;
	}
	return NULL;
}

static const LV2UI_Descriptor descriptor = {
	GXPLUGIN_UI_URI,
	instantiate,
	cleanup,
	port_event,
	extension_data
};


LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
	switch (index) {
		case 0:
			return &descriptor;
		default:
		return NULL;
	}
}

