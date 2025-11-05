#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include <string.h>
#if IBM
	#include <windows.h>
#endif
#if LIN
	#include <GL/gl.h>
#elif __GNUC__
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

#ifndef XPLM300
	#error This is made to be compiled against the XPLM300 SDK
#endif

// An opaque handle to the window we will create
static XPLMWindowID	g_window;

// Callbacks we will register when we create our window
void				draw(XPLMWindowID in_window_id, void * in_refcon);
int					dummy_mouse_handler(XPLMWindowID in_window_id, int x, int y, int is_down, void * in_refcon) { return 0; }
XPLMCursorStatus	dummy_cursor_status_handler(XPLMWindowID in_window_id, int x, int y, void * in_refcon) { return xplm_CursorDefault; }
int					dummy_wheel_handler(XPLMWindowID in_window_id, int x, int y, int wheel, int clicks, void * in_refcon) { return 0; }
void				dummy_key_handler(XPLMWindowID in_window_id, char key, XPLMKeyFlags flags, char virtual_key, void * in_refcon, int losing_focus) { }

static const char * g_pause_label = "PAUSE";
static float g_pause_button_lbrt[4];


PLUGIN_API int XPluginStart(
							char *		outName,
							char *		outSig,
							char *		outDesc)
{
	strcpy(outName, "Pause My Aircraft Plugin");
	strcpy(outSig, "joaquimmanjama.pausemyaircraft");
	strcpy(outDesc, "A plugin that pauses your aircraft for you!");
	
	XPLMCreateWindow_t params;
	params.structSize = sizeof(params);
	params.visible = 1;
	params.drawWindowFunc = draw;
	// Note on "dummy" handlers:
	// Even if we don't want to handle these events, we have to register a "do-nothing" callback for them
	params.handleMouseClickFunc = dummy_mouse_handler;
	params.handleRightClickFunc = dummy_mouse_handler;
	params.handleMouseWheelFunc = dummy_wheel_handler;
	params.handleKeyFunc = dummy_key_handler;
	params.handleCursorFunc = dummy_cursor_status_handler;
	params.refcon = NULL;
	params.layer = xplm_WindowLayerFloatingWindows;
	// Opt-in to styling our window like an X-Plane 11 native window
	// If you're on XPLM300, not XPLM301, swap this enum for the literal value 1.
	params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
	
	// Set the window's initial bounds
	// Note that we're not guaranteed that the main monitor's lower left is at (0, 0)...
	// We'll need to query for the global desktop bounds!
	int left, bottom, right, top;
	XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);
	params.left = left + 50;
	params.bottom = bottom + 150;
	params.right = params.left + 200;
	params.top = params.bottom + 200;
	
	g_window = XPLMCreateWindowEx(&params);
	
	// Position the window as a "free" floating window, which the user can drag around
	XPLMSetWindowPositioningMode(g_window, xplm_WindowPositionFree, -1);
	// Limit resizing our window: maintain a minimum width/height of 100 boxels and a max width/height of 300 boxels
	XPLMSetWindowResizingLimits(g_window, 200, 200, 300, 300);
	XPLMSetWindowTitle(g_window, "Pause My Aircraft");
	
	return g_window != NULL;
}

PLUGIN_API void	XPluginStop(void)
{
	// Since we created the window, we'll be good citizens and clean it up
	XPLMDestroyWindow(g_window);
	g_window = NULL;
}

PLUGIN_API void XPluginDisable(void) { }
PLUGIN_API int  XPluginEnable(void)  { return 1; }
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) { }

void	draw(XPLMWindowID in_window_id, void * in_refcon)
{
	// Mandatory: We *must* set the OpenGL state before drawing
	// (we can't make any assumptions about it)
	XPLMSetGraphicsState(
						 0 /* no fog */,
						 0 /* 0 texture units */,
						 0 /* no lighting */,
						 0 /* no alpha testing */,
						 1 /* do alpha blend */,
						 1 /* do depth testing */,
						 0 /* no depth writing */
						 );
	
	float white[] = {1.0, 1.0, 1.0}; // red, green, blue
	float green[] = { 0.0, 1.0, 0.0, 1.0 };
	
	int char_height;
	XPLMGetFontDimensions(xplmFont_Proportional, NULL, &char_height, NULL);

	int l, t, r, b;
	XPLMGetWindowGeometry(in_window_id, &l, &t, &r, &b);

	// Draw Button
	{
		float pause_string_length = XPLMMeasureString(xplmFont_Proportional, g_pause_label, strlen(g_pause_label));

		g_pause_button_lbrt[0] = l + ((r - l) - pause_string_length) / 2; // LEFT
		g_pause_button_lbrt[3] = t - (((t - b) - char_height) / 2); // TOP
		g_pause_button_lbrt[2] = g_pause_button_lbrt[0] + pause_string_length; // RIGHT
		g_pause_button_lbrt[1] = g_pause_button_lbrt[3] - (1.25f * char_height); // BOTTOM

		glColor4fv(green);
		glBegin(GL_LINE_LOOP);
		{
			glVertex2i(g_pause_button_lbrt[0], g_pause_button_lbrt[3]);
			glVertex2i(g_pause_button_lbrt[2], g_pause_button_lbrt[3]);
			glVertex2i(g_pause_button_lbrt[2], g_pause_button_lbrt[1]);
			glVertex2i(g_pause_button_lbrt[0], g_pause_button_lbrt[1]); 
		}
		glEnd();


		XPLMDrawString(white, g_pause_button_lbrt[0], g_pause_button_lbrt[1] + 4, (char*)g_pause_label, NULL, xplmFont_Proportional);
	}
}

