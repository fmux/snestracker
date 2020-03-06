#include "Tracker.h"
#include <getopt.h>
#include "utility.h"
#include "Screen.h"
#include "sdl_userevents.h"
#include "apuram.h"

#define L_FLAG 0
#define R_FLAG 1

std::unordered_set<DrawRenderer *> Tracker::prerenders, Tracker::postrenders;

Tracker::Tracker(int &argc, char **argv) :
bpm(120),
spd(6),
main_window(argc,argv, this)
{
	/* eventually I want to make sub-windows just an overlay in the one main
	 * window, rather than having separate windows. That's my choice. But
	 * since I have already impl'd some separate window functionality, might
	 * as well keep it and make it an optional thing?? */
	// MARK START of subwindow code
  ::options_window = &options_window;
  ::spc_export_window = &spc_export_window;
  cur_exp = &main_window;

	// Make subwindow location deviate slightly from mainwindow position to
	// make it easier to toggle and position windows.
  int x,y;
  SDL_GetWindowPosition(::render->sdlWindow, &x, &y);
  fprintf(stderr, "x: %d, y: %d\n", x, y);
  x += 20;
  y += 20;
  SDL_SetWindowPosition(::options_window->sdlWindow, x, y);

  int i=0;
  window_map[i++] = ::options_window;
  window_map[i++] = ::spc_export_window;
  window_map[i] = NULL;
	// MARK END of subwindow code

  int should_be_zero = SDL_GetCurrentDisplayMode(0, &monitor_display_mode);

    if(should_be_zero != 0)
      // In case of error...
      SDL_Log("Could not get display mode for video display #%d: %s", i, SDL_GetError());

    else
      // On success, print the current display mode.
      SDL_Log("Display #%d: current display mode is %dx%dpx @ %dhz. \n", i, 
        monitor_display_mode.w, monitor_display_mode.h, monitor_display_mode.refresh_rate);

	/* Init mouse cursors. This could probably be on the stack, but during
	 * my testing it became dynamic and I just kept it */
  mousecursors = new MouseCursors;
  ::mousecursors = mousecursors;
  mousecursors->set_cursor(CURSOR_MPAINT_WHITE_HAND);

	// We need to load the APU emulator at some point. Why not here, right
	// now?
	/* APU EMU LOAD CODE */
	char tb[260];
	int len;
	assert(::file_system);

	strcpy(tb, ::file_system->data_path);
	strcat(tb, SPCDRIVER_FILENAME);
	assert(::player);
	handle_error( ::player->load_file(tb) );

	::IAPURAM = player->spc_emu()->ram();
	apuram = (TrackerApuRam *)::IAPURAM;
#ifndef NDEBUG
	::IAPURAM[6] = 5;
	assert(apuram->ticks == 5);
#endif
	/* END APU EMU LOAD CODE */

  update_fps(30);
}

Tracker::~Tracker()
{
  DEBUGLOG("~Tracker");
  delete mousecursors;
  mousecursors = NULL;
}

void Tracker::update_fps(int fps)
{
  this->fps = fps;
  // calc from framerate. could put this into dynamic function later
  frame_tick_duration = 1000 / fps; // how many ms per frame
}

void Tracker::run()
{
  // gotta call this once to initialize important stuffz
  main_window.one_time_draw();

  cur_exp->draw();
  SDL_ShowWindow(::render->sdlWindow);

  // exp is changed from BaseD
  while (!::quitting)
  {
    frame_tick_timeout = SDL_GetTicks() + frame_tick_duration;

    cur_exp->run();
    cur_exp->draw();

    if (sub_window_experience)
    {
      sub_window_experience->run();
      sub_window_experience->draw();
    }

    menu_bar.draw(::render->screen);

    SDL_UpdateTexture(::render->sdlTexture, NULL, ::render->screen->pixels, ::render->screen->pitch);
    SDL_SetRenderDrawColor(::render->sdlRenderer, 0, 0, 0, 0);
    SDL_RenderClear(::render->sdlRenderer);

    // Certain things want to draw directly to renderer before the screen
    // texture is copied
    for (const auto& elem : prerenders)
      elem->draw_renderer();

    SDL_RenderCopy(::render->sdlRenderer, ::render->sdlTexture, NULL, NULL);

    // Other things may want to draw directly to renderer after the screen
    // texture is copied
    for (const auto& elem : postrenders)
      elem->draw_renderer();

    // Optionally, let's draw on top of EVERYTHING ELSE, any auxiliary
    // mouse FX
    mousecursors->draw_aux();

    SDL_RenderPresent(::render->sdlRenderer);

    /* The reason I handle events at the end of the loop rather than
     * before display code is so I can easily poll events for the rest of
     * the frame time, otherwise I would have to calculate how much frame
     * time to allocate for event polling, which wouldn't be so bad I
     * imagine. but this works.. */
    handle_events();

    // If we finished the frame early, sleep until the next frame start
    Uint32 curticks = SDL_GetTicks();
    if (!SDL_TICKS_PASSED(curticks, frame_tick_timeout))
    {
      SDL_Delay(frame_tick_timeout - curticks);
      //DEBUGLOG("duration: %d, remaining: %d\n", frame_tick_duration,
                 //frame_tick_timeout - curticks);
    }
    //else DEBUGLOG("no time to wait :(\n");
  }

  sub_window_experience = NULL;

  if (!player->is_paused() && player->track_started)
  {
    player->fade_out(false);
    player->pause(1, false, false);
  }
}

void Tracker::handle_events()
{
  SDL_Event ev;
  int ev_frame = 0; /* track how many events are processed. Created primarily
  for the soft click frame support detailed below */
  int mbd_frame = -2; /* track the event frame that an SDL_MOUSEBUTTONDOWN
  event occurred in. This is used to detect when event pair of SDL_MOUSEBUTTONDOWN
  is followed immediately by an SDL_MOUSEBUTTONUP in the same graphical frame,
  an otherwise perhaps impossible circumstance that only occurs for touchpad
  "Soft" clicks. These variables help detect when such an event pair occurs and
  pushes the MOUSEBUTTONUP event to be processed in the next graphical frame's event handler,
  so that we can graphically have time to display reactions to the MOUSEBUTTONDOWN
  event, and then later to the MOUSEBUTTONUP event. This at least gives us
  1 frame of graphical change for display entities that update their visual
  based on when the mouse button is down and then up. If I didn't do this,
  there would be no graphical change for event pairs of this nature (once again,
  that is touchpad "soft" clicks, when you simply lightly tap the touchpad rather
  than a hard push down and release).*/
  /* The -2 default value for mbd_frame is merely to disassociate the
   * starting value from the code that will check this value between
   * ev_frame */
  SDL_Event mbu_postpone_ev; /* When the aforementioned event pair anomaly occurs,
  a copy of the mousebuttonup (mbu) event is placed here for later pushing onto
  the event queue after exiting the event poller loop*/

  //static int scrolled_this_gframe = 0; // if we scrolled in this graphical frame

  mbu_postpone_ev.type = SDL_QUIT; /* set the default to anything that isnt mousebuttonup,
  that way we can reliably check later if the type has changed to SDL_MOUSEBUTTONUP
  we know that we must then push this event copy to the queue for next frame's
  processing */

  // Poll events for the remainder of this graphical frame time while the queue
  // has events.
  while (!SDL_TICKS_PASSED(SDL_GetTicks(), frame_tick_timeout)
          && SDL_PollEvent(&ev))
  {
    switch(ev.type)
    {
      case SDL_QUIT:
        ::quitting = true;
        break;
      case SDL_WINDOWEVENT:
      {
        switch(ev.window.event)
        {
          case SDL_WINDOWEVENT_RESIZED:
          {
            break;
            int w = ev.window.data1;
            int h = ev.window.data2;

            DEBUGLOG("window resize: w = %d, h = %d\n", w, h);

            int wd, hd;
            wd = w - ::render->w;
            hd = h - ::render->h;

            if (abs(wd) < abs(hd)) wd = hd;
            else if (abs(wd) > abs(hd)) hd = wd;

            ::render->w += wd;
            ::render->h += hd;

            DEBUGLOG("\twindow resize: w = %d, h = %d\n", ::render->w, ::render->h);

            if (::render->w > monitor_display_mode.w)
            {
              int tmp_wd = ::render->w - monitor_display_mode.w;
              ::render->w -= tmp_wd;
              ::render->h -= tmp_wd;
              DEBUGLOG("\t\twindow resize: tmp_wd = %d, w = %d, h = %d\n", tmp_wd, ::render->w, ::render->h);
            }
            if (::render->h > monitor_display_mode.h)
            {
              int tmp_hd = ::render->h - monitor_display_mode.h;
              ::render->w -= tmp_hd;
              ::render->h -= tmp_hd;
              DEBUGLOG("\t\twindow resize: tmp_hd = %d, w = %d, h = %d\n", tmp_hd, ::render->w, ::render->h);

            }

            SDL_SetWindowSize(::render->sdlWindow, ::render->w, ::render->h);
            DEBUGLOG("\t\t\twindow resize: w = %d, h = %d\n", ::render->w, ::render->h);
          } break;
          case SDL_WINDOWEVENT_FOCUS_LOST:
          {
            DEBUGLOG("Window %d Lost keyboard focus\n", ev.window.windowID);
            if (ev.window.windowID == ::render->windowID)
            {
              // OFF context menus
              menu_bar.context_menus.deactivate_all();
            }
          }
          break;
          case SDL_WINDOWEVENT_FOCUS_GAINED:
          {
            DEBUGLOG("Window %d Gained keyboard focus\n", ev.window.windowID);
            sub_window_experience = NULL;
            for (int i=0; i < NUM_WINDOWS; i++)
            {
              if (ev.window.windowID == window_map[i]->windowID)
              {
                if (window_map[i]->oktoshow)
                {
                  DEBUGLOG("Window_map %d gained experience. :D\n", i);
                  sub_window_experience = (Experience *)window_map[i];
                }
                break;
              }
            }
          }
          break;
          case SDL_WINDOWEVENT_SHOWN:
          {
            SDL_Log("Window %d shown", ev.window.windowID);
            for (int i=0; i < NUM_WINDOWS; i++)
            {
              if (ev.window.windowID == window_map[i]->windowID)
              {
                if (!window_map[i]->oktoshow)
                {
                  window_map[i]->hide();
                  /* maybe right here, instead of raising the main window, we should
                    have a history of displayed windows.. and the last one should be raised.
                  */
                  sub_window_experience = NULL;
                }
                else
                {
                  sub_window_experience = NULL;
                  DEBUGLOG("Window_map %d gained experience. :D\n", i);
                  sub_window_experience = (Experience *)window_map[i];
                  window_map[i]->raise();
                }
                break;
              }
            }
          }
          break;
          case SDL_WINDOWEVENT_CLOSE:
          {
            SDL_Log("Window %d closed", ev.window.windowID);
            if (ev.window.windowID == ::render->windowID)
            {
              // quit app

              SDL_Event quit_ev;
              quit_ev.type = SDL_QUIT;
              SDL_PushEvent(&quit_ev);
            }
            else
            {
              for (int i=0; i < NUM_WINDOWS; i++)
              {
                if (ev.window.windowID == window_map[i]->windowID)
                {
                  window_map[i]->hide();
                  // maybe right here, instead of raising the main window, we should
                  //  have a history of displayed windows.. and the last one should be raised.

                  sub_window_experience = NULL;
                  //SDL_RaiseWindow(::render->::render->sdlWindow);
                  break;
                }
              }
            }
          }
          break;

          default:break;
        }
      } break;
      case SDL_DROPFILE:
      {
        char *dropped_filedir = ev.drop.file;

        ::nfd.free_pathset();
        SDL_free(dropped_filedir);    // Free dropped_filedir memory
        SDL_RaiseWindow(::render->sdlWindow);
      } break;
      case SDL_USEREVENT:
      {
        switch (ev.user.code)
        {
          case UserEvents::sound_stop:
            sound_stop();
            break;
          case UserEvents::callback:
            void (*p) (void*) = ev.user.data1;
            p(ev.user.data2);
          break;
        }
      } break;
      case SDL_MOUSEMOTION:
      {
        mouse::x = ev.motion.x;
        mouse::y = ev.motion.y;
      } break;
      case SDL_MOUSEBUTTONDOWN:
      {
        /* In the case of touchpad soft clicking, it is expected that the
         * MOUSEBUTTONUP is sent as the immediate next event after the
         * mousebuttondown. In this case, it does not allow for the
         * graphical updates between down and up mouse presses to take
         * place, because they are all processed within the same frame.
         * For this reason, I will check if the next event is
         * MOUSEBUTTONUP, and if so, I will postpone it to the following
         * graphical frame */
        mbd_frame = ev_frame; // save the frame number the mbd occurred
      } break;
      case SDL_MOUSEBUTTONUP:
      {
        /* If the last frame was the mbd, we know we have found the soft
         * click event pair. copy this mbu event to be postponed after
         * this event loop exits. Also don't process this event */
        if (mbd_frame == (ev_frame - 1))
        {
          mbu_postpone_ev = ev;
          goto next_event;
        }
      } break;
      case SDL_MOUSEWHEEL:
      {
        //DEBUGLOG("gframe: %d, ev_frame: %d\n", gframe, ev_frame);
        /*if (scrolled_this_gframe)
        {
          goto next_event;
        }
        scrolled_this_gframe = 300;*/
      } break;
      case SDL_KEYDOWN:
      {
        int scancode = ev.key.keysym.sym;
        int mod = ev.key.keysym.mod;
        switch (scancode)
        {
          case SDLK_LEFT:
            if ((mod & KMOD_SHIFT) && (mod & KMOD_CTRL))
              mousecursors->prev();
          break;
          case SDLK_RIGHT:
            if ((mod & KMOD_SHIFT) && (mod & KMOD_CTRL))
              mousecursors->next();
          break;
        }
      } break;
      default:break;
    }

    mousecursors->handle_event(ev);

    if (menu_bar.receive_event(ev))
      continue;
 
    if (sub_window_experience)
    {
      sub_window_experience->receive_event(ev);
    }
    else cur_exp->receive_event(ev);
next_event:
    ev_frame++;
  }

  /* If we have copied an mbu event to mbu_postpone_ev, then push it to
   * the event queue for next graphical frame to pick up */
  if (mbu_postpone_ev.type == SDL_MOUSEBUTTONUP)
    SDL_PushEvent(&mbu_postpone_ev);
}

void Tracker::inc_bpm()
{
  if (bpm >= 300)
    bpm = 300;
  else bpm++;
}

void Tracker::dec_bpm()
{
  if (bpm <= 32)
    bpm = 32;
  else bpm--;
}

void Tracker::inc_spd()
{
  if (spd >= 31)
    spd = 31;
  else spd++;
}

void Tracker::dec_spd()
{
  if (spd <= 1)
    spd = 1;
  else spd--;
}

// SNES APU timer 0 and 1 frequency rate in seconds
#define TIMER01_FREQS 0.000125
// compress bits for patterns
#define CBIT 0x80
#define CBIT_NOTE (1<<0)
#define CBIT_INSTR (1<<1)
#define CBIT_VOL (1<<2)
#define CBIT_FX (1<<3)
#define CBIT_FXPARAM (1<<4)
#define CBIT_RLE (1<<5) // if this bit present, the next byte specifies how many rows to skip before checking the next byte
#define CBIT_RLE_ONLY1 (1<<6)
//#define CBIT_TPEND (1<<6)

void Tracker::render_to_apu()
{
	/* BPM AND SPD */
  /* Quick thoughts on Timer : We could add a checkmark to use the high
   * frequency timer. Also could have a mode where you specify ticks and
   * see the actual BPM */
  /* Convert BPM to ticks */
  // Ticks = 60 / ( BPM * 4 * Spd * freqS )
  double ticks = 60.0 / ( (double)bpm * 4.0 * (double)spd * TIMER01_FREQS );
  int ticksi = (int) floor(ticks + 0.5);
	if (ticksi == 256)
		ticksi = 0; // max timer setting is 0
	else if (ticksi > 256)
	{
		DEBUGLOG("Ticks value too high\n");
		ticksi = 255;
	}
	// Forcing using timer0 for ticks, but that's fine for now (or forever)
	apuram->ticks = ticksi;
	apuram->spd = spd;
	/* END BPM AND SPD */

	uint16_t freeram_i = SPCDRIVER_CODESTART + SPCDRIVER_CODESIZE;

	// what's next? How about the instrument import? (export to spc ram)
	// INSTRUMENTS (and inadvertantly samples)
	/* Need a way to check that an instrument is used. How about checking if
	 * the first sample slot's BRR ptr is valid */
	/* I will probably remove the ability for multiple samples to be
	 * specified to one instrument */
	uint8_t numsamples = 0;
	uint16_t sampletable_i;
	// as much as I hate to iterate through the instruments twice, we need
	// to know how many samples are used to know how big to allocate for DIR
	for (int i=0; i < NUM_INSTR; i++)
	{
		Sample *firstsample = &instruments[i].samples[0];
		if (firstsample->brr == NULL)
			continue;
		numsamples++;
	}
	sampletable_i = freeram_i + (numsamples * 0x4); // put sample_table directly after the amount of DIR entries required

	uint16_t dir_i, dspdir_i;
	dir_i = freeram_i + (freeram_i % 0x100) ? (0x100 - (freeram_i % 0x100)) : 0;
	dspdir_i = dir_i / 0x100;

	/* We have got to load these samples in first, so the DIR table knows
	 * where the samples are */
	/* DIR is specified in multiples of 0x100. So if we're shy of that, we
	 * need to move it up. I think a smarter program would mark that unused
	 * area as free for something */

	/* DIR can be at max 0x400 bytes in size, but any unused space in DIR
	 * can be used for other data */
	// Write the sample and loop information to the DIR. Then write the DSP
	// DIR value to DSP
	uint16_t cursample_i = sampletable_i;
	for (int i=0; i < NUM_INSTR; i++)
	{
		Sample *firstsample = &instruments[i].samples[0];
		if (firstsample->brr == NULL)
			continue;

		// has a sample. this instrument is valid for export. However, perhaps
		// an even better exporter would also check if this instrument has
		// been used in any (in)active pattern data.
		uint16_t *dir = (uint16_t *) &::IAPURAM[dir_i + (i * 4)];
		*dir = cursample_i;
		*(dir+1) = cursample_i + firstsample->rel_loop;
		int s=0;
		for (; s < firstsample->brrsize; s++)
		{
			uint8_t *bytes = (uint8_t *)firstsample->brr;
			::IAPURAM[cursample_i + s] = bytes[s];
		}
		cursample_i += s;
		/* Could add a (SHA1) signature to Sample struct so that we can
		 * identify repeat usage of the same sample and only load it once to
		 * SPC RAM. For now, don't do this!! We're trying to get to first
		 * working tracker status here! Plus, it's possible the user wants the
		 * 2 identical samples to be treated individually (maybe their doing
		 * something complicated) */
	}
	::player->spc_write_dsp(dsp_reg::dir, dspdir_i);
	// INSTRUMENTS END

	// PATTERNS
	// First calculate the number of used patterns in the song. This is not
	// sequence length, but the number of unique patterns. With that length
	// calculated, we can allocate the amount of RAM necessary for the
	// PatternLUT (detailed below comments).
	uint8_t num_usedpatterns = 0;
	uint16_t patternlut_i = cursample_i, patternlut_size;
	for (int p=0; p < MAX_PATTERNS; p++)
	{
		PatternMeta *pm = &patseq.patterns[p];
		if (pm->used == 0)
			continue;
		num_usedpatterns++;
	}
	patternlut_size = num_usedpatterns * 2; // WORD sized address table
	/* OK I'm thinking about how aside from the pattern data itself, how
	 * that pattern data will be accessed. We can store a Pattern lookup
	 * table that has the 16-bit addresses of each pattern in ascending
	 * order. The pattern sequencer table can simply store the pattern
	 * number, and that number is converted to an index into the lookup
	 * table to get the address of the pattern. That address will be stored
	 * into a DP address for indirect access to pattern data. There may be
	 * another DP pointer for accessing Row data row by row.*/
	uint16_t *patlut = (uint16_t *) &::IAPURAM[patternlut_i];
	uint16_t pat_i = patternlut_i + patternlut_size; // index into RAM for actual pattern data
	for (int p=0; p < MAX_PATTERNS; p++)
	{
		PatternMeta *pm = &patseq.patterns[p];
		if (pm->used == 0)
			continue;

		/* Here we need to iterate through the Pattern to form the compressed
		 * version. For now, let's try to follow the XM version without RLE
		 * compression */
		*(patlut++) = pat_i; // put the address of this pattern into the patlut
		Pattern *pattern = &pm->p;
		/* I just thought of another problem here. How do we access the
		 * individual tracks of a pattern if they are going to be in a
		 * compressed form (non-fixed length), other than storing the offset
		 * of each track? Since they will be stored sequentially, We could iterate to the end of each pattern when the pattern is first loaded to be played by the driver (in snes_driver code), and store those offsets into RAM. I
		 * suppose? that would save (8*2) bytes Per pattern! that's a lot!
		 */
		// BEFORE THIS LOOP IS WHERE WE CAN WRITE A PATTERN HEADER FOR SNES
		// When we do that, make sure to update pat_i or use a new variable
		// for the next codeblock
		for (int t=0; t < MAX_TRACKS; t++)
		{
			for (int tr=0; tr < pattern->len; tr++)
			{
				int ttrr;
				PatternRow *pr = &pattern->trackrows[t][tr];
				uint8_t cbyte = 0, rlebyte;
				/* Lookahead: how many empty rows from this one until the next
				 * filled row? If there's only 1 empty row, use RLE_ONLY1 */
#define PATROW_EMPTY(pr) ( pr->note == 0 && pr->instr == 0 && pr->vol == 0 && pr->fx == 0 && pr->fxparam == 0 )
				for (ttrr=tr+1; ttrr < pattern->len; ttrr++)
				{
					PatternRow *row = &pattern->trackrows[t][ttrr];
					if (!PATROW_EMPTY(row))
					{
						// we found a filled row.
						ttrr--;
						int num_empty = ttrr - tr;
						if (num_empty == 0)
							break;
						else if (num_empty == 1)
							cbyte |= CBIT_RLE_ONLY1;
						else if (num_empty == 2)
							cbyte |= CBIT_RLE | CBIT_RLE_ONLY1;
						else
						{
							cbyte |= CBIT_RLE;
							rlebyte = num_empty;
						}
					}
				}
				// Only if every element is filled are we NOT going to use a
				// special compression byte. so let's check if every element is
				// filled first.
				if (! (
				       pr->note != NOTE_NONE &&
				       pr->instr != 0 &&
				       pr->vol != 0 && pr->fx != 0 && pr->fxparam != 0) )
				{
					cbyte |=
					  pr->note ? 0 : CBIT_NOTE |
					  pr->instr ? 0 : CBIT_INSTR |
					  pr->vol ? 0 : CBIT_VOL |
					  pr->fx ? 0 : CBIT_FX |
					  pr->fxparam ? 0 : CBIT_FXPARAM;
				}

				if (cbyte)
					::IAPURAM[pat_i++] = CBIT | cbyte;
				/* we should now write the actual byte for any data that is
				 * present */
				if (pr->note)
					::IAPURAM[pat_i++] = pr->note;
				if (pr->instr)
					::IAPURAM[pat_i++] = pr->instr;
				if (pr->vol)
					::IAPURAM[pat_i++] = pr->vol;
				if (pr->fx)
					::IAPURAM[pat_i++] = pr->fx;
				if (pr->fxparam)
					::IAPURAM[pat_i++] = pr->fxparam;
				if ( (cbyte & (CBIT_RLE | CBIT_RLE_ONLY1)) == CBIT_RLE)
					::IAPURAM[pat_i++] = rlebyte;

				tr += ttrr; // skip over empty rows
			}
		}
	}
	// PATTERNS END
}

/* Define the Packed Pattern Format.
 * For this, I am electing to use the XM packing scheme, with the addition
 * of an RLE for absences of row data > 2 rows.
 *
 *
 * Import a Note LUT for APU driver
 * Write the Song player in APU driver
 * finish render_to_apu() function above
 * Add a "rows" updater to SpcReport gme_m library
 * Add a song/pattern playback keyboard shortcut that writes the
 * appopriate (may need to be added to apu driver) command that signals
 * playback.
 *
 * Add the same keyboard shortcut as a toggle(?) to stop playback or a
 * different keyboard shortcut altogether*/
