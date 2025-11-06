// BASED ON SAMPLE CODE FROM https://developer.x-plane.com/sdk/plugin-sdk-sample-code/

// IMPORTING
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMMenus.h"
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

// MENU
int			g_menu_container_idx; // The index of our menu item in the Plugins menu
XPLMMenuID	g_menu_id; // The menu container we'll append all our menu items to
void		menu_handler(void*, void*);

// WINDOWS
void draw_manual_mode(int l, int t, int r, int b, int char_height);
void draw_zulu_time_mode(int l, int t, int r, int b, int char_height);


// Callbacks we will register when we create our window
void				draw(XPLMWindowID in_window_id, void * in_refcon);
int					handle_mouse(XPLMWindowID in_window_id, int x, int y, int is_down, void* in_refcon);
int					dummy_mouse_handler(XPLMWindowID in_window_id, int x, int y, int is_down, void * in_refcon) { return 0; }
XPLMCursorStatus	dummy_cursor_status_handler(XPLMWindowID in_window_id, int x, int y, void * in_refcon) { return xplm_CursorDefault; }
int					dummy_wheel_handler(XPLMWindowID in_window_id, int x, int y, int wheel, int clicks, void * in_refcon) { return 0; }
void				dummy_key_handler(XPLMWindowID in_window_id, char key, XPLMKeyFlags flags, char virtual_key, void * in_refcon, int losing_focus) { }

// PLUGIN MODE
typedef enum {
	ZULU_TIME = 0,
	WAYPOINT = 1,
	TOD = 2,
	MANUAL = 3
} PauseMode;
static PauseMode current_mode;

// PAUSE BUTTON
static const char*	g_pause_label = "PAUSE";
static float		g_pause_button_lbrt[4];

// ZULU TIME
// --- Zulu Time Mode Data ---
struct ZuluTimeTarget {
	int hours;
	int minutes;
	bool is_set;
};

static ZuluTimeTarget g_zulu_target;

// Button bounds for click detection
static float g_hour_up_btn[4],	g_hour_down_btn[4];
static float g_min_up_btn[4],	g_min_down_btn[4];
static float g_confirm_btn[4],	g_cancel_btn[4];


static int	coord_in_rect(float x, float y, float* bounds_lbrt) { return ((x >= bounds_lbrt[0]) && (x < bounds_lbrt[2]) && (y < bounds_lbrt[3]) && (y >= bounds_lbrt[1])); }

// PLUGIN START FUNCTION
void pause_sim();

// XPLUGIN_START
PLUGIN_API int XPluginStart(
							char *		outName,
							char *		outSig,
							char *		outDesc)
{
	strcpy(outName, "Pause My Aircraft Plugin");
	strcpy(outSig, "joaquimmanjama.pausemyaircraft");
	strcpy(outDesc, "A plugin that pauses your aircraft for you!");

	// MENU
	g_menu_container_idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Pause My Aircraft", 0, 0);
	g_menu_id = XPLMCreateMenu("Pause My Aircraft", XPLMFindPluginsMenu(), g_menu_container_idx, menu_handler, NULL);
	XPLMAppendMenuItem(g_menu_id, "Zulu Time", (void*)"Menu Item 1", 1);
	XPLMAppendMenuItem(g_menu_id, "Waypoint", (void*)"Menu Item 2", 1);
	XPLMAppendMenuItem(g_menu_id, "TOD", (void*)"Menu Item 3", 1);
	XPLMAppendMenuItem(g_menu_id, "Manual", (void*)"Menu Item 4", 1);

	XPLMCreateWindow_t params;
	params.structSize = sizeof(params);
	params.visible = 1;
	params.drawWindowFunc = draw;
	// Note on "dummy" handlers:
	// Even if we don't want to handle these events, we have to register a "do-nothing" callback for them
	params.handleMouseClickFunc = handle_mouse;
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
	params.left = left + 500;
	params.bottom = bottom + 500;
	params.right = params.left + 500;
	params.top = params.bottom + 300;
	
	g_window = XPLMCreateWindowEx(&params);
	
	// Position the window as a "free" floating window, which the user can drag around
	XPLMSetWindowPositioningMode(g_window, xplm_WindowPositionFree, -1);
	// Limit resizing our window: maintain a minimum width/height of 100 boxels and a max width/height of 300 boxels
	XPLMSetWindowResizingLimits(g_window, 500, 300,  500, 300);
	XPLMSetWindowTitle(g_window, "Pause My Aircraft");
	XPLMSetWindowIsVisible(g_window, 0);

	// PLUGIN MODE
	current_mode = MANUAL;

	// ZULU TIME 
	g_zulu_target = { 0, 0, false };

	return g_window != NULL;
}

// XPLUGIN_STOP
PLUGIN_API void	XPluginStop(void)
{
	// Since we created the window, we'll be good citizens and clean it up
	XPLMDestroyWindow(g_window);
	g_window = NULL;

	// MENU
	XPLMDestroyMenu(g_menu_id);
}

PLUGIN_API void XPluginDisable(void) { } // XPLUGIN_DISABLE
PLUGIN_API int  XPluginEnable(void)  { return 1; } // XPLUGIN_ENABLE
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) { } // XPLUGIN_RECEIVE_MESSAGE

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
	
	int char_height;
	XPLMGetFontDimensions(xplmFont_Proportional, NULL, &char_height, NULL);

	int l, t, r, b;
	XPLMGetWindowGeometry(in_window_id, &l, &t, &r, &b);

	// ZULU TIME SCREEN
	if (current_mode == ZULU_TIME)
	{
		draw_zulu_time_mode(l, t, r, b, char_height);
	}

	// MANUAL SCREEN
	if (current_mode == MANUAL)
	{
		draw_manual_mode(l, t, r, b, char_height);
	}
}

int handle_mouse(XPLMWindowID in_window_id, int x, int y, int is_down, void* in_refcon)
{
	if (is_down == xplm_MouseDown)
	{
		const int is_popped_out = XPLMWindowIsPoppedOut(in_window_id);
		if (!XPLMIsWindowInFront(in_window_id))
		{
			XPLMBringWindowToFront(in_window_id);
		}
		else
		{
			if (current_mode == ZULU_TIME)
			{
			
			}
			else if (current_mode == WAYPOINT)
			{

			}
			else if (current_mode == TOD)
			{

			}
			else if (current_mode == MANUAL)
			{
				if (coord_in_rect(x, y, g_pause_button_lbrt))
				{
					pause_sim();
				}
			}
		}
	}

	return 1;
}

void pause_sim()
{
	if (!XPLMGetDatai(XPLMFindDataRef("sim/time/paused")))
	{
		XPLMCommandOnce(XPLMFindCommand("sim/operation/pause_toggle"));
	}
}

// MENU
void menu_handler(void* in_menu_ref, void* in_item_ref)
{
	const char* item_name = (const char*)in_item_ref;

	XPLMSetWindowIsVisible(g_window, 1);
	XPLMBringWindowToFront(g_window);

	if (!strcmp((const char*)in_item_ref, "Menu Item 1"))
	{
		current_mode = ZULU_TIME;
	} 
	else if (!strcmp((const char*)in_item_ref, "Menu Item 2"))
	{
		current_mode = WAYPOINT;
	}
	else if (!strcmp((const char*)in_item_ref, "Menu Item 3"))
	{
		current_mode = TOD;
	}
	else if (!strcmp((const char*)in_item_ref, "Menu Item 4"))
	{
		current_mode = MANUAL;
	}
}

// DRAWING

// MANUAL MODE
void draw_manual_mode(int l, int t, int r, int b, int char_height)
{
	float white[] = { 1.0f, 1.0f, 1.0f };
	float green[] = { 0.0f, 1.0f, 0.0f, 1.0f };
	float darkGreen[] = { 0.0f, 0.4f, 0.0f, 1.0f };
	float shadow[] = { 0.0f, 0.0f, 0.0f, 0.4f };

	// Measure text width for centering
	float pause_string_length = XPLMMeasureString(xplmFont_Proportional, g_pause_label, strlen(g_pause_label));

	// Define button bounds
	g_pause_button_lbrt[0] = l + ((r - l) - pause_string_length - 20) / 2; // LEFT (20px padding)
	g_pause_button_lbrt[3] = t - (((t - b) - char_height) / 2);            // TOP
	g_pause_button_lbrt[2] = g_pause_button_lbrt[0] + pause_string_length + 20; // RIGHT
	g_pause_button_lbrt[1] = g_pause_button_lbrt[3] - (1.8f * char_height);     // BOTTOM

	// --- Draw subtle shadow behind the button ---
	glColor4fv(shadow);
	glBegin(GL_QUADS);
	{
		glVertex2i(g_pause_button_lbrt[0] + 2, g_pause_button_lbrt[3] - 2);
		glVertex2i(g_pause_button_lbrt[2] + 2, g_pause_button_lbrt[3] - 2);
		glVertex2i(g_pause_button_lbrt[2] + 2, g_pause_button_lbrt[1] - 2);
		glVertex2i(g_pause_button_lbrt[0] + 2, g_pause_button_lbrt[1] - 2);
	}
	glEnd();

	// --- Draw filled button background ---
	glColor4f(0.0f, 0.3f, 0.0f, 0.85f); // semi-transparent dark green fill
	glBegin(GL_QUADS);
	{
		glVertex2i(g_pause_button_lbrt[0], g_pause_button_lbrt[3]);
		glVertex2i(g_pause_button_lbrt[2], g_pause_button_lbrt[3]);
		glVertex2i(g_pause_button_lbrt[2], g_pause_button_lbrt[1]);
		glVertex2i(g_pause_button_lbrt[0], g_pause_button_lbrt[1]);
	}
	glEnd();

	// --- Draw bright outline for clarity ---
	glLineWidth(2.0f);
	glColor4fv(green);
	glBegin(GL_LINE_LOOP);
	{
		glVertex2i(g_pause_button_lbrt[0], g_pause_button_lbrt[3]);
		glVertex2i(g_pause_button_lbrt[2], g_pause_button_lbrt[3]);
		glVertex2i(g_pause_button_lbrt[2], g_pause_button_lbrt[1]);
		glVertex2i(g_pause_button_lbrt[0], g_pause_button_lbrt[1]);
	}
	glEnd();
	glLineWidth(1.0f);

	// --- Draw the pause label centered vertically and horizontally ---
	int text_x = g_pause_button_lbrt[0] + ((g_pause_button_lbrt[2] - g_pause_button_lbrt[0]) - pause_string_length) / 2;
	int text_y = g_pause_button_lbrt[1] + (char_height * 0.5f);

	XPLMDrawString(white, text_x, text_y, (char*)g_pause_label, NULL, xplmFont_Proportional);
}

// ZULU TIME MODE
void draw_zulu_time_mode(int l, int t, int r, int b, int char_height)
{
	float white[] = { 1.0, 1.0, 1.0 };
	float green[] = { 0.0, 1.0, 0.0, 1.0 };
	float gray[] = { 0.6f, 0.6f, 0.6f, 1.0f };
	float yellow[] = { 1.0f, 0.8f, 0.2f, 1.0f };

	// Title
	XPLMDrawString(yellow, l + 20, t - 30, (char*)"ZULU TIME MODE", NULL, xplmFont_Proportional);

	// Instruction
	XPLMDrawString(white, l + 20, t - 50, (char*)"Set target Zulu time (HH:MM):", NULL, xplmFont_Proportional);

	// Center region for the time input
	int center_x = (l + r) / 2;
	int mid_y = (t + b) / 2;

	char time_str[10];

	// Draw time string
	XPLMDrawString(white, center_x - 25, mid_y, time_str, NULL, xplmFont_Proportional);

	// Draw hour up/down arrows
	g_hour_up_btn[0] = center_x - 60; g_hour_up_btn[1] = mid_y + 25;
	g_hour_up_btn[2] = center_x - 45; g_hour_up_btn[3] = mid_y + 40;

	g_hour_down_btn[0] = center_x - 60; g_hour_down_btn[1] = mid_y - 40;
	g_hour_down_btn[2] = center_x - 45; g_hour_down_btn[3] = mid_y - 25;

	glColor4fv(green);
	glBegin(GL_TRIANGLES);
	{
		// Up triangle (hour)
		glVertex2i((g_hour_up_btn[0] + g_hour_up_btn[2]) / 2, g_hour_up_btn[3]);
		glVertex2i(g_hour_up_btn[0], g_hour_up_btn[1]);
		glVertex2i(g_hour_up_btn[2], g_hour_up_btn[1]);

		// Down triangle (hour)
		glVertex2i((g_hour_down_btn[0] + g_hour_down_btn[2]) / 2, g_hour_down_btn[1]);
		glVertex2i(g_hour_down_btn[0], g_hour_down_btn[3]);
		glVertex2i(g_hour_down_btn[2], g_hour_down_btn[3]);
	}
	glEnd();

	// Draw minute up/down arrows
	g_min_up_btn[0] = center_x + 45; g_min_up_btn[1] = mid_y + 25;
	g_min_up_btn[2] = center_x + 60; g_min_up_btn[3] = mid_y + 40;

	g_min_down_btn[0] = center_x + 45; g_min_down_btn[1] = mid_y - 40;
	g_min_down_btn[2] = center_x + 60; g_min_down_btn[3] = mid_y - 25;

	glBegin(GL_TRIANGLES);
	{
		// Up triangle (minute)
		glVertex2i((g_min_up_btn[0] + g_min_up_btn[2]) / 2, g_min_up_btn[3]);
		glVertex2i(g_min_up_btn[0], g_min_up_btn[1]);
		glVertex2i(g_min_up_btn[2], g_min_up_btn[1]);

		// Down triangle (minute)
		glVertex2i((g_min_down_btn[0] + g_min_down_btn[2]) / 2, g_min_down_btn[1]);
		glVertex2i(g_min_down_btn[0], g_min_down_btn[3]);
		glVertex2i(g_min_down_btn[2], g_min_down_btn[3]);
	}
	glEnd();

	// Confirm and Cancel buttons
	const char* confirm_label = "CONFIRM";
	const char* cancel_label = "CANCEL";

	float confirm_w = XPLMMeasureString(xplmFont_Proportional, confirm_label, strlen(confirm_label));
	float cancel_w = XPLMMeasureString(xplmFont_Proportional, cancel_label, strlen(cancel_label));

	g_confirm_btn[0] = center_x - confirm_w - 20; g_confirm_btn[1] = b + 20;
	g_confirm_btn[2] = g_confirm_btn[0] + confirm_w + 20; g_confirm_btn[3] = g_confirm_btn[1] + 25;

	g_cancel_btn[0] = center_x + 20; g_cancel_btn[1] = b + 20;
	g_cancel_btn[2] = g_cancel_btn[0] + cancel_w + 20; g_cancel_btn[3] = g_cancel_btn[1] + 25;

	// Draw button boxes
	glColor4fv(gray);
	glBegin(GL_LINE_LOOP);
	glVertex2i(g_confirm_btn[0], g_confirm_btn[1]);
	glVertex2i(g_confirm_btn[2], g_confirm_btn[1]);
	glVertex2i(g_confirm_btn[2], g_confirm_btn[3]);
	glVertex2i(g_confirm_btn[0], g_confirm_btn[3]);
	glEnd();

	glBegin(GL_LINE_LOOP);
	glVertex2i(g_cancel_btn[0], g_cancel_btn[1]);
	glVertex2i(g_cancel_btn[2], g_cancel_btn[1]);
	glVertex2i(g_cancel_btn[2], g_cancel_btn[3]);
	glVertex2i(g_cancel_btn[0], g_cancel_btn[3]);
	glEnd();

	XPLMDrawString(white, g_confirm_btn[0] + 10, g_confirm_btn[1] + 7, (char*)confirm_label, NULL, xplmFont_Proportional);
	XPLMDrawString(white, g_cancel_btn[0] + 10, g_cancel_btn[1] + 7, (char*)cancel_label, NULL, xplmFont_Proportional);

	// If confirmed, show the target info
	if (g_zulu_target.is_set) {
		char info[50];
		XPLMDrawString(green, l + 20, b + 40, info, NULL, xplmFont_Proportional);
	}
}