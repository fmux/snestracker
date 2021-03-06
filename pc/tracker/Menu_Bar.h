#pragma once

#include "gui/Clickable_Text.h"
#include "gui/Expanding_List.h"
#include "Main_Window.h"
#include "Options_Window.h"
#include "gui/Spc_Export_Window.h"

struct Menu_Bar
{
	Menu_Bar();
	~Menu_Bar();

  bool is_first_run=true;
  void draw(SDL_Surface *screen);
  int receive_event(SDL_Event &ev);
  
  struct File_Context
  {
    File_Context() : menu(menu_items, true)
    {
      memset(filepath, 0, 500);
    }

    static int new_song(void *data);
    static int open_song(void *data);
    static int save_song(void *data);
		static int save_as_song(void *data);
		static int open_spc(void *data);
    static int export_spc(void *data);
    static int export_wav(void *data);
    static int quit(void *data) { ::quitting = true; return 0; }

		nfdchar_t filepath[500];

    Expanding_List menu;
    Context_Menu_Item menu_items[10] =
    {
      {"File",         true,  NULL,       NULL},
////////////////////////////////////////////////////////
      {"New Song",     true,  new_song,   this},
      {"Open ST-Song", true,  open_song,  this},
			{"Open ST-SPC",  true,  open_spc,   NULL},
			{"Save",         true,  save_song,  this},
			{"Save as...",   true,  save_as_song,  this},
      {"Export SPC",   true,  export_spc, NULL},
      {"Export WAV",   true,  export_wav, NULL},
      {"Quit",         true,  quit,       NULL},
////////////////////////////////////////////////////////
      {"",             false, NULL,      NULL}
    };
  };

  struct Edit_Context
  {
    Edit_Context() : menu(menu_items, true)
    {
    }

    static int open_options_window(void *data);
    static int copy(void *null);
    static int paste(void *null);
    
    Expanding_List menu;
    Context_Menu_Item menu_items[6] = 
    {
      {"Edit",        true,  NULL,                 NULL},
      {"------",      true,  NULL,                 NULL},
      {"Copy",        true,  copy,                 NULL},
      {"Paste",       true,  paste,                NULL},
      {"Options",     true,  open_options_window,  NULL},
      {"",            false, NULL,                 NULL}
    };
  };

  struct Track_Context
  {
    Track_Context() : menu(menu_items, true)
    {
    }

    void draw(SDL_Surface *screen);

    static int toggle_pause (void *data);         
    static int restart_current_track (void *data);

    Expanding_List menu;
    Context_Menu_Item menu_items[8] = 
    {
      {"track",           true,   NULL,                   NULL},
      {"pause",           true,   toggle_pause,           &menu_items[1].clickable_text},
      {"restart",         true,   restart_current_track,  NULL},
      {"",                false,  NULL,                   NULL}
    };
  };

  struct Window_Context
  {
    Window_Context() : menu(menu_items, true)
    {
      
    }

    static int restore_window_size(void *nada);

    Expanding_List menu;
    Context_Menu_Item menu_items[7] = 
    {
      {"Window",               true,  NULL,  NULL},
      {"-------------",        true,  NULL,  NULL},
      {"Nothing here yet :9",  true,  NULL,  NULL},
      {"",                     false, NULL,  NULL}
    };
  };

  enum
  {
    EVENT_INACTIVE=0,
    EVENT_ACTIVE=1,
    EVENT_FILE,
    EVENT_TRACK,
    EVENT_WINDOW
  };

  struct Context_Menus
  {
    int x = 10, y = 10;
    int h = CHAR_HEIGHT;
    Context_Menus() {}

    bool is_anything_active();

    bool check_left_click_activate(int &x, int &y, const Uint8 &button=0, const SDL_Event *ev=NULL);
    int receive_event(SDL_Event &ev);
    void draw(SDL_Surface *screen);
    void preload(int x, int y);
    void update(Uint8 adsr, Uint8 adsr2);
    void deactivate_all() {
      file_context.menu.deactivate();
      edit_context.menu.deactivate();
      track_context.menu.deactivate();
      window_context.menu.deactivate();
    }

    File_Context          file_context;
    Edit_Context          edit_context;
    Track_Context         track_context;
    Window_Context        window_context;
  } context_menus;
};
