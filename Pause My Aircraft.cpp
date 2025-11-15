// BASED ON SAMPLE CODE FROM https://developer.x-plane.com/sdk/plugin-sdk-sample-code/

// IMPORTING
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMPlugin.h"
#include <string.h>
#include <iostream>

#include "utilities.h"

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
void draw_waypoint_mode(int l, int t, int r, int b, int char_height);


// Callbacks we will register when we create our window
void				draw(XPLMWindowID in_window_id, void * in_refcon);
int					handle_mouse(XPLMWindowID in_window_id, int x, int y, int is_down, void* in_refcon);
int					dummy_mouse_handler(XPLMWindowID in_window_id, int x, int y, int is_down, void * in_refcon) { return 0; }
XPLMCursorStatus	dummy_cursor_status_handler(XPLMWindowID in_window_id, int x, int y, void * in_refcon) { return xplm_CursorDefault; }
int					dummy_wheel_handler(XPLMWindowID in_window_id, int x, int y, int wheel, int clicks, void * in_refcon) { return 0; }
void				dummy_key_handler(XPLMWindowID in_window_id, char key, XPLMKeyFlags flags, char virtual_key, void* in_refcon, int losing_focus) {};

// Other Callbacks
float flight_loop_callback(float, float, int, void*);
int myKeySniffer(char inChar, XPLMKeyFlags inFlags, char inVirtualKey, void* inRefcon);

// CALLBACK VARIABLES
bool g_flight_loop_active;

// PLUGIN MODE
typedef enum {
	ZULU_TIME = 0,
	WAYPOINT = 1,
	TOD = 2,
	MANUAL = 3,
	ABOUT = 4
} PauseMode;
static PauseMode current_mode;

// PAUSE BUTTON
static const char*	g_pause_label = "PAUSE";
static float		g_pause_button_lbrt[4];

// PLAYER
float g_player_pos[2];

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

// WAYPOINT MODE
struct WaypointTarget {
	char name[10];
	int distance_nm;
	bool is_set;
	float coords[2];
};

WaypointTarget g_waypoint_target;

// Button coordinate arrays
float g_nm_up_btn[4];
float g_nm_down_btn[4];
float g_nm_up_btn5[4];
float g_nm_down_btn5[4];
float g_confirm_btn_wp[4];
float g_cancel_btn_wp[4];
float g_click_here_box[4];

// Text input
bool g_typing_waypoint = false;    // Whether we’re currently typing
char g_waypoint_input[10] = "";    // Temp buffer for typing

// FILE PATHS
char plugin_path[512];
std::string waypoint_file;


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
	XPLMAppendMenuItem(g_menu_id, "About", (void*)"Menu Item 5", 1);

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
	params.right = params.left + 400;
	params.top = params.bottom + 250;
	
	g_window = XPLMCreateWindowEx(&params);
	
	// Position the window as a "free" floating window, which the user can drag around
	XPLMSetWindowPositioningMode(g_window, xplm_WindowPositionFree, -1);
	// Limit resizing our window: maintain a minimum width/height of 100 boxels and a max width/height of 300 boxels
	XPLMSetWindowResizingLimits(g_window, 400, 250,  400, 250);
	XPLMSetWindowTitle(g_window, "Pause My Aircraft");
	XPLMSetWindowIsVisible(g_window, 0);

	// PLUGIN MODE
	current_mode = MANUAL;
	g_flight_loop_active = false;

	// ZULU TIME 
	g_zulu_target = { 0, 0, false };

	// WAYPOINT MODE
	g_waypoint_target = { "", 0, false, {} };
	XPLMRegisterKeySniffer(myKeySniffer, 1, NULL); // 1 = before X-Plane gets keys

	// FILE PATHS
	XPLMGetPluginInfo(XPLMGetMyID(), NULL, plugin_path, NULL, NULL);
	waypoint_file = clean_path(plugin_path, "Waypoints.txt");

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

	// OTHER CALLBACKS
	XPLMUnregisterFlightLoopCallback(flight_loop_callback, NULL);
	XPLMUnregisterKeySniffer(myKeySniffer, 1, NULL);
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
						 0 /* do alpha blend */,
						 0 /* do depth testing */,
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

	// WAYPOINT SCREEN
	if (current_mode == WAYPOINT)
	{
		draw_waypoint_mode(l, t, r, b, char_height);
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
				if (coord_in_rect(x, y, g_hour_up_btn)) {
					g_zulu_target.hours = (g_zulu_target.hours + 1) % 24;
				}
				else if (coord_in_rect(x, y, g_hour_down_btn)) {
					g_zulu_target.hours = (g_zulu_target.hours + 23) % 24;
				}
				else if (coord_in_rect(x, y, g_min_up_btn)) {
					g_zulu_target.minutes = (g_zulu_target.minutes + 1) % 60;
				}
				else if (coord_in_rect(x, y, g_min_down_btn)) {
					g_zulu_target.minutes = (g_zulu_target.minutes + 59) % 60;
				}
				else if (coord_in_rect(x, y, g_confirm_btn)) {
					if (!g_flight_loop_active) 
					{
						XPLMRegisterFlightLoopCallback(flight_loop_callback, 1.0f, NULL);
						g_flight_loop_active = true;
					}
					
					g_zulu_target.is_set = true;
					g_flight_loop_active = true;
					
				}
				else if (coord_in_rect(x, y, g_cancel_btn)) {
					g_zulu_target.is_set = false;
					g_flight_loop_active = false;
				}
			}
			else if (current_mode == WAYPOINT)
			{
				if (coord_in_rect(x, y, g_click_here_box)) {
					if (g_typing_waypoint)
					{
						g_typing_waypoint = false;
					}
					else
					{
						g_typing_waypoint = true;
						g_waypoint_input[0] = '\0';
					}
				}
				else if (coord_in_rect(x, y, g_nm_up_btn))
				{
					g_waypoint_target.distance_nm += 1;
				}
				else if (coord_in_rect(x, y, g_nm_down_btn))
				{
					if (g_waypoint_target.distance_nm > 0)
					{
						g_waypoint_target.distance_nm -= 1;
					}
				}
				else if (coord_in_rect(x, y, g_nm_up_btn5))
				{
					g_waypoint_target.distance_nm += 5;
				}
				else if (coord_in_rect(x, y, g_nm_down_btn5))
				{
					if (g_waypoint_target.distance_nm > 4)
					{
						g_waypoint_target.distance_nm -= 5;
					}
				}
				else if (coord_in_rect(x, y, g_confirm_btn_wp))
				{
					if (is_valid_waypoint(std::string(g_waypoint_target.name), waypoint_file))
					{
						g_waypoint_target.is_set = true;
						get_coordinates(std::string(g_waypoint_target.name), g_waypoint_target.coords, waypoint_file);
						g_player_pos[0] = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/latitude"));
						g_player_pos[1] = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/longitude"));

						if (!g_flight_loop_active)
						{
							XPLMRegisterFlightLoopCallback(flight_loop_callback, 1.0f, NULL);
							g_flight_loop_active = true;
						}
					}
				}
				else if (coord_in_rect(x, y, g_cancel_btn_wp))
				{
					g_waypoint_target.is_set = false;
					g_flight_loop_active = false;
				}
				else
				{
					strncpy(g_waypoint_target.name, g_waypoint_input, sizeof(g_waypoint_target.name) - 1);
					g_waypoint_target.name[sizeof(g_waypoint_target.name) - 1] = '\0';
					g_typing_waypoint = false;
				}
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
	else if (!strcmp((const char*)in_item_ref, "Menu Item 5"))
	{
		current_mode = ABOUT;
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
	float white[] = { 1.0f, 1.0f, 1.0f };
	float green[] = { 0.0f, 1.0f, 0.0f, 1.0f };
	float blue[] = { 0.0f, 1.0f, 1.0f, 1.0f };
	float red[] = { 1.0f, 0.0f, 0.0f, 1.0f };
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
	std::snprintf(time_str, sizeof(time_str), "%02d : %02d", g_zulu_target.hours, g_zulu_target.minutes);

	// Draw time string
	XPLMDrawString(white, center_x - 25, mid_y, time_str, NULL, xplmFont_Proportional);

	// Draw hour up/down arrows
	g_hour_up_btn[0] = center_x - 60; g_hour_up_btn[1] = mid_y + 25;
	g_hour_up_btn[2] = center_x - 45; g_hour_up_btn[3] = mid_y + 40;

	g_hour_down_btn[0] = center_x - 60; g_hour_down_btn[1] = mid_y - 40;
	g_hour_down_btn[2] = center_x - 45; g_hour_down_btn[3] = mid_y - 25;

	// Draw minute up/down arrows
	g_min_up_btn[0] = center_x + 45; g_min_up_btn[1] = mid_y + 25;
	g_min_up_btn[2] = center_x + 60; g_min_up_btn[3] = mid_y + 40;

	g_min_down_btn[0] = center_x + 45; g_min_down_btn[1] = mid_y - 40;
	g_min_down_btn[2] = center_x + 60; g_min_down_btn[3] = mid_y - 25;

	glColor4fv(green);
	glDisable(GL_CULL_FACE);
	glBegin(GL_TRIANGLES);
	{
		// Up triangle (hour)
		glVertex2i((g_hour_up_btn[0] + g_hour_up_btn[2]) / 2, g_hour_up_btn[3]);
		glVertex2i(g_hour_up_btn[0], g_hour_up_btn[1]);
		glVertex2i(g_hour_up_btn[2], g_hour_up_btn[1]);

		// Up triangle (minute)
		glVertex2i((g_min_up_btn[0] + g_min_up_btn[2]) / 2, g_min_up_btn[3]);
		glVertex2i(g_min_up_btn[0], g_min_up_btn[1]);
		glVertex2i(g_min_up_btn[2], g_min_up_btn[1]);
	}
	glEnd();

	glColor4fv(red);
	glBegin(GL_TRIANGLES);
	{
		// Down triangle (hour)
		glVertex2i((g_hour_down_btn[0] + g_hour_down_btn[2]) / 2, g_hour_down_btn[1]);
		glVertex2i(g_hour_down_btn[0], g_hour_down_btn[3]);
		glVertex2i(g_hour_down_btn[2], g_hour_down_btn[3]);

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
		char remaining[50];
		int curr_hours;
		int curr_minutes;
		get_current_utc_time(curr_hours, curr_minutes);
		curr_hours = (g_zulu_target.hours - curr_hours + 24) % 24;
		curr_minutes = (g_zulu_target.minutes - curr_minutes + 60) % 60;

		std::snprintf(info, sizeof(info), "Target set to %02d:%02d Z", g_zulu_target.hours, g_zulu_target.minutes);
		std::snprintf(remaining, sizeof(remaining), "Time Left: %02d:%02d", curr_hours, curr_minutes);
		
		XPLMDrawString(green, l + 15, b + 65, info, NULL, xplmFont_Proportional);
		XPLMDrawString(blue, l + 15, b + char_height, remaining, NULL, xplmFont_Proportional);
	}
}

// WAYPOINT MODE
void draw_waypoint_mode(int l, int t, int r, int b, int char_height)
{
	float white[] = { 1.0, 1.0, 1.0, 1.0 };
	float green[] = { 0.0, 1.0, 0.0, 1.0 };
	float blue[] = { 0.0, 1.0, 1.0, 1.0 };
	float red[] = { 1.0, 0.0, 0.0, 1.0 };
	float gray[] = { 0.6f, 0.6f, 0.6f, 1.0f };
	float yellow[] = { 1.0f, 0.8f, 0.2f, 1.0f };

	int center_x = (l + r) / 2;
	int mid_y = (t + b) / 2;

	// Title
	XPLMDrawString(yellow, l + 20, t - 30, (char*)"WAYPOINT MODE", NULL, xplmFont_Proportional);
	XPLMDrawString(white, l + 20, t - 50, (char*)"Set distance and waypoint:", NULL, xplmFont_Proportional);

	// Distance input display
	char dist_str[20];
	std::snprintf(dist_str, sizeof(dist_str), "%d NM", g_waypoint_target.distance_nm);
	XPLMDrawString(white, center_x - 25, mid_y, dist_str, NULL, xplmFont_Proportional);

	// Waypoint label
	XPLMDrawString(white, l + 20, mid_y - 40, (char*)"Waypoint:", NULL, xplmFont_Proportional);

	g_click_here_box[0] = l + 110 - 5;
	g_click_here_box[1] = mid_y - 40 - 5;
	g_click_here_box[2] = g_click_here_box[0] + 70;
	g_click_here_box[3] = g_click_here_box[1] + 25;

	// Waypoint text box (typing support)
	if (g_typing_waypoint)
	{
		char buffer[20];
		std::snprintf(buffer, sizeof(buffer), "[%s_]", g_waypoint_input);
		XPLMDrawString(yellow, l + 110, mid_y - 40, buffer, NULL, xplmFont_Proportional);
		XPLMDrawString(white, g_click_here_box[0] - 5, g_click_here_box[1] - 12,
			(char*)"*Press Enter to Confirm*", NULL, xplmFont_Proportional);
	}
	else
	{
		XPLMDrawString(green, g_click_here_box[0] + 5, g_click_here_box[1] + 5,
			g_waypoint_target.name[0] ? g_waypoint_target.name : (char*)"<Click to enter>", NULL, xplmFont_Proportional);
	}

	// Up/Down arrows for distance
	g_nm_up_btn[0] = center_x + 45; g_nm_up_btn[1] = mid_y + 25;
	g_nm_up_btn[2] = center_x + 60; g_nm_up_btn[3] = mid_y + 40;

	g_nm_down_btn[0] = center_x + 45; g_nm_down_btn[1] = mid_y - 40;
	g_nm_down_btn[2] = center_x + 60; g_nm_down_btn[3] = mid_y - 25;

	g_nm_up_btn5[0] = center_x + 75; g_nm_up_btn5[1] = mid_y + 25;
	g_nm_up_btn5[2] = center_x + 90; g_nm_up_btn5[3] = mid_y + 40;

	g_nm_down_btn5[0] = center_x + 75; g_nm_down_btn5[1] = mid_y - 40;
	g_nm_down_btn5[2] = center_x + 90; g_nm_down_btn5[3] = mid_y - 25;

	// Draw triangles
	glColor4fv(green);
	glDisable(GL_CULL_FACE);
	glBegin(GL_TRIANGLES);
	{
		glVertex2i((g_nm_up_btn[0] + g_nm_up_btn[2]) / 2, g_nm_up_btn[3]);
		glVertex2i(g_nm_up_btn[0], g_nm_up_btn[1]);
		glVertex2i(g_nm_up_btn[2], g_nm_up_btn[1]);

		glVertex2i((g_nm_up_btn5[0] + g_nm_up_btn5[2]) / 2, g_nm_up_btn5[3]);
		glVertex2i(g_nm_up_btn5[0], g_nm_up_btn5[1]);
		glVertex2i(g_nm_up_btn5[2], g_nm_up_btn5[1]);
	}
	glEnd();

	glColor4fv(red);
	glBegin(GL_TRIANGLES);
	{
		glVertex2i((g_nm_down_btn[0] + g_nm_down_btn[2]) / 2, g_nm_down_btn[1]);
		glVertex2i(g_nm_down_btn[0], g_nm_down_btn[3]);
		glVertex2i(g_nm_down_btn[2], g_nm_down_btn[3]);

		glVertex2i((g_nm_down_btn5[0] + g_nm_down_btn5[2]) / 2, g_nm_down_btn5[1]);
		glVertex2i(g_nm_down_btn5[0], g_nm_down_btn5[3]);
		glVertex2i(g_nm_down_btn5[2], g_nm_down_btn5[3]);
	}
	glEnd();

	// Confirm & Cancel buttons
	const char* confirm_label = "CONFIRM";
	const char* cancel_label = "CANCEL";

	float confirm_w = XPLMMeasureString(xplmFont_Proportional, confirm_label, strlen(confirm_label));
	float cancel_w = XPLMMeasureString(xplmFont_Proportional, cancel_label, strlen(cancel_label));

	g_confirm_btn_wp[0] = center_x - confirm_w - 20; g_confirm_btn_wp[1] = b + 20;
	g_confirm_btn_wp[2] = g_confirm_btn_wp[0] + confirm_w + 20; g_confirm_btn_wp[3] = g_confirm_btn_wp[1] + 25;

	g_cancel_btn_wp[0] = center_x + 20; g_cancel_btn_wp[1] = b + 20;
	g_cancel_btn_wp[2] = g_cancel_btn_wp[0] + cancel_w + 20; g_cancel_btn_wp[3] = g_cancel_btn_wp[1] + 25;

	// Draw button boxes
	glColor4fv(gray);
	glBegin(GL_LINE_LOOP);
	glVertex2i(g_confirm_btn_wp[0], g_confirm_btn_wp[1]);
	glVertex2i(g_confirm_btn_wp[2], g_confirm_btn_wp[1]);
	glVertex2i(g_confirm_btn_wp[2], g_confirm_btn_wp[3]);
	glVertex2i(g_confirm_btn_wp[0], g_confirm_btn_wp[3]);
	glEnd();

	glBegin(GL_LINE_LOOP);
	glVertex2i(g_cancel_btn_wp[0], g_cancel_btn_wp[1]);
	glVertex2i(g_cancel_btn_wp[2], g_cancel_btn_wp[1]);
	glVertex2i(g_cancel_btn_wp[2], g_cancel_btn_wp[3]);
	glVertex2i(g_cancel_btn_wp[0], g_cancel_btn_wp[3]);
	glEnd();

	XPLMDrawString(white, g_confirm_btn_wp[0] + 10, g_confirm_btn_wp[1] + 7, (char*)confirm_label, NULL, xplmFont_Proportional);
	XPLMDrawString(white, g_cancel_btn_wp[0] + 10, g_cancel_btn_wp[1] + 7, (char*)cancel_label, NULL, xplmFont_Proportional);

	XPLMDrawString(white, g_nm_up_btn[0], g_nm_up_btn[3] + 7, "+1", NULL, xplmFont_Proportional);
	XPLMDrawString(white, g_nm_up_btn5[0], g_nm_up_btn5[3] + 7, "+5", NULL, xplmFont_Proportional);
	XPLMDrawString(white, g_nm_down_btn[0], g_nm_down_btn[1] - 7 - char_height, "-1", NULL, xplmFont_Proportional);
	XPLMDrawString(white, g_nm_down_btn5[0], g_nm_down_btn5[1] - 7 - char_height, "-5", NULL, xplmFont_Proportional);

	// If confirmed, show target info
	if (g_waypoint_target.is_set)
	{
		char info[60];
		char remaining[60];
		char lat[60];
		char longi[60];

		if (g_waypoint_target.distance_nm == 0) 
		{
			std::snprintf(info, sizeof(info), "Target: %s", g_waypoint_target.name);
		}
		else
		{
			std::snprintf(info, sizeof(info), "Target: %d NM from %s", g_waypoint_target.distance_nm, g_waypoint_target.name);
		}

		std::snprintf(remaining, sizeof(remaining), "Distance Left: %d NM", (int) km_to_nm(get_distance_km(g_player_pos, g_waypoint_target.coords)) - g_waypoint_target.distance_nm);
		
		XPLMDrawString(green, l + 15, b + 55, info, NULL, xplmFont_Proportional);
		XPLMDrawString(blue, l + 15, b + char_height, remaining, NULL, xplmFont_Proportional);
	}
}

// CALLBACKS
float flight_loop_callback(float elapsedMe, float elapsedSim, int counter, void* refcon)
{

	if (g_zulu_target.is_set)
	{
		// check every time this function runs
		if (check_time_to_pause(g_zulu_target.hours, g_zulu_target.minutes))
		{
			g_zulu_target.is_set = false;
			pause_sim(); // pause when time matches
		}
	}

	if (g_waypoint_target.is_set)
	{
		g_player_pos[0] = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/latitude"));
		g_player_pos[1] = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/longitude"));
		// check every time this function runs
		if (detect_collision(g_waypoint_target.distance_nm, g_player_pos, g_waypoint_target.coords))
		{
			pause_sim(); // pause when within distance
			g_waypoint_target.distance_nm = 0; // reset distance
			g_waypoint_target.name[0] = '\0'; // clear name
			g_waypoint_input[0] = '\0'; // clear input
			g_waypoint_target.coords[0] = 0.0; // reset coords
			g_waypoint_target.coords[1] = 0.0; // reset coords
			g_waypoint_target.is_set = false;

		}

		if (!g_zulu_target.is_set && !g_waypoint_target.is_set)
		{
			if (g_flight_loop_active)
			{
				XPLMUnregisterFlightLoopCallback(flight_loop_callback, NULL);
			}
		}
	}

	return 10.0f;  // run again in 10 seconds
}

int myKeySniffer(char inChar, XPLMKeyFlags inFlags, char inVirtualKey, void* inRefcon)
{
	if (!g_typing_waypoint)
		return 1; // let X-Plane handle it normally

	// Only process on key press, ignore repeats and releases
	if ((inFlags & xplm_DownFlag) == 0)
		return 0; // block from going to X-Plane but don’t process

	// Handle Enter key
	if (inChar == '\r' || inChar == '\n')
	{
		strncpy(g_waypoint_target.name, g_waypoint_input, sizeof(g_waypoint_target.name) - 1);
		g_waypoint_target.name[sizeof(g_waypoint_target.name) - 1] = '\0';
		g_typing_waypoint = false;
		return 0; // stop X-Plane from seeing it
	}

	// Handle backspace
	if (inChar == 8 && strlen(g_waypoint_input) > 0)
	{
		g_waypoint_input[strlen(g_waypoint_input) - 1] = '\0';
		return 0;
	}

	// Handle printable characters (ignore control chars)
	if (inChar >= 32 && inChar <= 126)
	{
		size_t len = strlen(g_waypoint_input);
		if (len < sizeof(g_waypoint_input) - 1)
		{
			g_waypoint_input[len] = (char) toupper(inChar);
			g_waypoint_input[len + 1] = '\0';
		}
		return 0;
	}

	return 0; // block everything else
}

// RESET AFTER PAUSING ZULU TIME

