/*
  game.c

  For TuxMath
  The main game loop!

  by Bill Kendrick
  bill@newbreedsoftware.com
  http://www.newbreedsoftware.com/


  Part of "Tux4Kids" Project
  http://www.tux4kids.org/
      
  August 26, 2001 - February 18, 2004
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#ifndef NOSOUND
#include <SDL_mixer.h>
#endif
#include <SDL_image.h>
#include "game.h"
#include "images.h"
#include "setup.h"
#include "sounds.h"
#include "playsound.h"
#include "tuxmath.h"
#include "mathcards.h"  

#define FPS (1000 / 15)   /* 15 fps max */

#define CITY_EXPL_START 3 * 5  /* Must be mult. of 5 (number of expl frames) */
#define ANIM_FRAME_START 4 * 2 /* Must be mult. of 2 (number of tux frames) */
#define GAMEOVER_COUNTER_START 750
#define LEVEL_START_WAIT_START 20
#define LASER_START 5

char operchars[NUM_OPERS] = {
  "+-*/"
};

char * oper_opts[NUM_OPERS] = {
  "add", "subtract", "multiply", "divide"
};

char * oper_alt_opts[NUM_OPERS] = {
  "addition", "subtraction", "multiplication", "division"
};

range_type ranges[NUM_Q_RANGES] = {
  {0, 5},
  {6, 12},
  {13, 20}
};


#define ANSWER_LEN 5
#define FORMULA_LEN 8
typedef struct comet_type {
  int alive;
  int expl;
  int city;
  float x, y;
  int eq1, oper, eq2;
  char formula[FORMULA_LEN];
  int answer;
  char answer_str[ANSWER_LEN];
  MC_FlashCard flashcard;
} comet_type;

/* Local (to game.c) 'globals': */

static int quit;
static int done;
static int gameover;
static int game_status;
static int SDL_quit_received;
static int escape_received;
static int paused;
static int wave;
static int score;
static int pre_wave_score;
static int num_attackers;
static int demo_countdown;
static int tux_anim;
static int tux_anim_frame;
static int num_cities_alive;
static int num_comets_alive;
static int tux_img;
static int old_tux_img;
static int frame;
static int neg_answer_picked;
static int tux_pressing;
static int doing_answer;
static int level_start_wait;
static int last_bkgd;

static  Uint32 last_time, now_time;

static int digits[3];
static comet_type comets[MAX_COMETS];
static city_type cities[NUM_CITIES];
static laser_type laser;
static SDL_Surface* bkgd;

/* Local function prototypes: */
static int  game_initialize(void);
static void game_handle_user_events(void);
static void game_handle_demo(void);
static void game_handle_answer(void);
static void game_countdown(void);
static void game_handle_tux(void);
static void game_handle_comets(void);
static void game_handle_cities(void);
static void game_draw(void);
static int check_exit_conditions(void);
static void print_exit_conditions(void);

static void reset_level(void);
static int add_comet(void);
static void draw_numbers(char* str, int x);
static int  pause_game(void);
static void draw_line(int x1, int y1, int x2, int y2, int r, int g, int b);
static void putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel);
static void draw_console_image(int i);
static void add_score(int inc);
static int  pick_operand(int min);
static int  in_range(int n);
static void reset_comets(void);

static void draw_led_nums(void);
static void game_mouse_event(SDL_Event event);
static void game_key_event(SDLKey key);

/* --- MAIN GAME FUNCTION!!! --- */


int game(void)
{

   /* most code moved into smaller functions (game_*()): */
  if (!game_initialize())
  {
    printf("\ngame_initialize() failed!");
    fprintf(stderr, "\ngame_initialze() failed!");
    done = 1;
    return 0;
    /*exit(1);*/    
  } 

  /* --- MAIN GAME LOOP: --- */ 
  do
  {
    /* reset or increment various things with each loop: */
    frame++;
    last_time = SDL_GetTicks();
    old_tux_img = tux_img;
    tux_pressing = 0;

    game_handle_user_events();
     
    if (doing_answer)
    {
      game_handle_answer();
    }

    if (laser.alive > 0)
    {
      laser.alive--;
    }      

    if (level_start_wait > 0)
    {
      game_countdown();
    }
     
    game_handle_tux();
    game_handle_comets();
    game_handle_cities();


    if (game_options->demo_mode)
    {
      game_handle_demo(); 
    }     
    /* drawing code moved into own function: */
    game_draw();
    
   
    /* If we're in "PAUSE" mode, pause! */
    if (paused)
    {
      /*quit = */pause_game();
      paused = 0;
    }
      
      /* Keep playing music: */
      
#ifndef NOSOUND
    if (game_options->use_sound)
    {
      if (!Mix_PlayingMusic())
      {
	    Mix_PlayMusic(musics[MUS_GAME + (rand() % 3)], 0);
      }  
    }
#endif
    /* figure out if we should leave loop: */
    game_status = check_exit_conditions(); 
 
    /* Pause (keep frame-rate event) */
    now_time = SDL_GetTicks();
    if (now_time < last_time + FPS)
    {
      SDL_Delay(last_time + FPS - now_time);
    }
  }
  while(GAME_IN_PROGRESS == game_status);
  /* END OF MAIN GAME LOOP! */

 #ifdef TUXMATH_DEBUG
   print_exit_conditions();
  #endif

  /* TODO: handle various end of game scenarios based on value of game_status */
  switch (game_status)
  {
    SDL_Rect dest;
    SDL_Event event;

    case GAME_OVER_WON:
    {
      break;
    }

    case GAME_OVER_ERROR:
    {
      #ifdef TUXMATH_DEBUG
      printf("\ngame() exiting with error");
      #endif
    }
    case GAME_OVER_LOST: 
    case GAME_OVER_OTHER:
    {
      gameover = 1;
      do
      {

        while (SDL_PollEvent(&event) > 0)
        {
          if  (event.type == SDL_QUIT
            || event.type == SDL_KEYDOWN
            || event.type == SDL_MOUSEBUTTONDOWN)
          {
            gameover = 0;
          }   
        }

        dest.x = (screen->w - images[IMG_GAMEOVER]->w) / 2;
        dest.y = (screen->h - images[IMG_GAMEOVER]->h) / 2;
        dest.w = images[IMG_GAMEOVER]->w;
        dest.h = images[IMG_GAMEOVER]->h;
	
        SDL_BlitSurface(images[IMG_GAMEOVER], NULL, screen, &dest);
        SDL_Flip(screen);

        now_time = SDL_GetTicks();

        if (now_time < last_time + FPS)
	  SDL_Delay(last_time + FPS - now_time);     
      }
      while (gameover);

      break;
    }

    case GAME_OVER_ESCAPE:
    {
      break;
    }

    case GAME_OVER_WINDOW_CLOSE:
    {
      break;
    }

  } 

  /* Free background: */
  if (bkgd != NULL)
  {
    SDL_FreeSurface(bkgd);
  }

  /* Stop music: */
#ifndef NOSOUND
  if (game_options->use_sound)
  {
    if (Mix_PlayingMusic())
    {
      Mix_HaltMusic();
    }
  }
#endif
  
  /* Return the chosen command: */
  return quit;
}



int game_initialize(void)
{
  int i;
  /* Clear window: */
  
  SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
  SDL_Flip(screen);

  /* FIXME done and quit will be superseded by game_status */
  done = 0;
  quit = 0;
  
  game_status = GAME_IN_PROGRESS;  
  SDL_quit_received = 0;
  escape_received = 0;

  /* Start MathCards backend: */
  /* FIXME may need to move this into tuxmath.c to accomodate option */
  /* to us MC_StartUsingWrongs() */
  if (!MC_StartGame())
  {
    printf("\nMC_StartGame() failed!");
    fprintf(stderr, "\nMC_StartGame() failed!");
    done = 1;
    return 0;
    /*exit(1);*/
  }  
  
  /* Prepare to start the game: */
  
  wave = 1;
  num_attackers = 2;
  score = 0;
  gameover = 0;
  demo_countdown = 1000;
  level_start_wait = LEVEL_START_WAIT_START;
  neg_answer_picked = 0;
 
  /* (Create and position cities) */
  
  for (i = 0; i < NUM_CITIES; i++)
    {
      cities[i].alive = 1;
      cities[i].expl = 0;
      cities[i].shields = 1;
     

      /* Left vs. Right - makes room for Tux and the console */

      if (i < NUM_CITIES / 2)
	{
	  cities[i].x = (((screen->w / (NUM_CITIES + 1)) * i) +
			 ((images[IMG_CITY_BLUE] -> w) / 2));
	}
      else
	{
	  cities[i].x = (screen->w -
			 ((((screen->w / (NUM_CITIES + 1)) *
			    (i - (NUM_CITIES / 2)) +
			    ((images[IMG_CITY_BLUE] -> w) / 2)))));
	}
    }

  num_cities_alive = NUM_CITIES;
  num_comets_alive = 0;


  /* (Clear laser) */
  laser.alive = 0;
  
  /* Reset remaining stuff: */
 
  bkgd = NULL;
  last_bkgd = -1;
  reset_level();
  reset_comets();  

  frame = 0;
  paused = 0;
  doing_answer = 0;
  tux_pressing = 0;
  tux_img = IMG_TUX_RELAX1;
  tux_anim = -1;
  tux_anim_frame = 0;

  return 1;
}

void game_handle_user_events(void)
{
  SDL_Event event;
  SDLKey key;

  while (SDL_PollEvent(&event) > 0)
  {
    if (event.type == SDL_QUIT)
    {
      SDL_quit_received = 1;
      /* FIXME quit and done flags will be superseded */
      /* Window close event - quit! */
      quit = 1;
      done = 1;
    }
    else if (event.type == SDL_KEYDOWN)
    {
      key = event.key.keysym.sym;
      game_key_event(key);
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN)
    {
      game_mouse_event(event);
    }
  }
}

void game_handle_demo(void)
{
  /* Demo mode! */
  int demo_answer, answer_digit;
  static int picked_comet;
  if (picked_comet == -1 && (rand() % 10) < 3)
  {
    /* Demo mode!  Randomly pick a comet to destroy: */
    picked_comet = (rand() % MAX_COMETS);

    if (!comets[picked_comet].alive || comets[picked_comet].y < 80)
    {
      picked_comet = -1;
    }
    else
    {
      /* found a comet to blow up! */
      demo_answer = comets[picked_comet].answer;
      /* handle negative answer: */
      if (demo_answer < 0)
      {
        demo_answer = -demo_answer;
        neg_answer_picked = 1;
      }
      if (demo_answer >= 100)
        answer_digit = 0;
      else if (demo_answer >= 10)
        answer_digit = 1;
      else
        answer_digit = 2;
    }
  }
  
  /* Add a digit: */
  if (picked_comet != -1 && (frame % 5) == 0 && (rand() % 10) < 8)
  {
    tux_pressing = 1;

    if (answer_digit < 3)
    {
      digits[0] = digits[1];
      digits[1] = digits[2];

      if (answer_digit == 0)
      {
        digits[2] = demo_answer / 100;
      }
      else if (answer_digit == 1)
      {
        digits[2] = (demo_answer % 100) / 10;
      }
      else if (answer_digit == 2)
      {
        digits[2] = (demo_answer % 10);
      }
	    
      answer_digit++;
    }
    else
    {
      /* "Press Return" */
      doing_answer = 1;
      picked_comet = -1;
    }
  }

  /* Count down counter: */
  demo_countdown--;
  if (demo_countdown <= 0 || num_cities_alive == 0)
    done = 1;
}

/* FIXME need to end game if no more questions left */
void game_handle_answer(void)
{
  int i, num, lowest, lowest_y;

  doing_answer = 0;

  num = (digits[0] * 100 +
         digits[1] * 10 +
         digits[2]);
  /* negative answer support DSB */
  if (neg_answer_picked)
  {
    num = -num;
  }	
	
  /*  Pick the lowest comet which has the right answer: */
  lowest_y = 0;
  lowest = -1;

  for (i = 0; i < MAX_COMETS; i++)
  {
    if (comets[i].alive &&
        comets[i].expl < COMET_EXPL_END && 
        comets[i].answer == num &&
        comets[i].y > lowest_y)
    {
      lowest = i;
      lowest_y = comets[i].y;
    }
  }
	
  /* If there was an comet with this answer, destroy it! */
  if (lowest != -1)  /* -1 means no comet had this answer */
  {
    MC_AnsweredCorrectly(&(comets[lowest].flashcard));

    /* Destroy comet: */
    comets[lowest].expl = COMET_EXPL_START;
    /* Fire laser: */
    laser.alive = LASER_START;
    laser.x1 = screen->w / 2;
    laser.y1 = screen->h;
    laser.x2 = comets[lowest].x;
    laser.y2 = comets[lowest].y;
    playsound(SND_LASER);
    playsound(SND_SIZZLE);
	    
    /* FIXME maybe should move this into game_handle_tux() */
    /* 50% of the time.. */
    if ((rand() % 10) < 5)
    {
      /* ... pick an animation to play: */
      if ((rand() % 10) < 5)
        tux_anim = IMG_TUX_YES1;
      else
        tux_anim = IMG_TUX_YAY1;
      tux_anim_frame = ANIM_FRAME_START;
    }

    /* Increment score: */

    /* [ add = 25, sub = 50, mul = 75, div = 100 ] */
    /* [ the higher the better ] */
    add_score(((25 * (comets[lowest].oper + 1)) *
              (screen->h - comets[lowest].y + 1)) /
	       screen->h);
  }
  else
  {
    /* Didn't hit anything! */
    laser.alive = LASER_START;
    laser.x1 = screen->w / 2;
    laser.y1 = screen->h;
    laser.x2 = laser.x1;
    laser.y2 = 0;
    playsound(SND_LASER);
    playsound(SND_BUZZ);
	    
    if ((rand() % 10) < 5)
      tux_img = IMG_TUX_DRAT;
    else
      tux_img = IMG_TUX_YIPE;
  }
	
  /* Clear digits: */
  digits[0] = 0;
  digits[1] = 0;
  digits[2] = 0;
  neg_answer_picked = 0;
}

void game_countdown(void)
{
  level_start_wait--;
  if (level_start_wait > LEVEL_START_WAIT_START / 4)
    tux_img = IMG_TUX_RELAX1;
  else if (level_start_wait > 0)
    tux_img = IMG_TUX_RELAX2;
  else
    tux_img = IMG_TUX_SIT;
	  
  if (level_start_wait == LEVEL_START_WAIT_START / 4)
  {
    playsound(SND_ALARM);
  }
}

void game_handle_tux(void)
{
  static int tux_same_counter;
  /* If Tux pressed a button, pick a new (different!) stance: */
  if (tux_pressing)
  {

    do
    {
      tux_img = IMG_TUX_CONSOLE1 + (rand() % 4);
    }
    while (tux_img == old_tux_img);

    playsound(SND_CLICK);
  }
 
  /* If Tux is being animated, show the animation: */
  if (tux_anim != -1)
  {
    tux_anim_frame--;
    if (tux_anim_frame < 0)
      tux_anim = -1;
    else
      tux_img = tux_anim + 1 - (tux_anim_frame / (ANIM_FRAME_START / 2));
  }

  /* Reset Tux to sitting if he's been doing nothing for a while: */
  if (old_tux_img == tux_img)
  {
    tux_same_counter++;
    if (tux_same_counter >= 20)
    {
      tux_img = IMG_TUX_SIT;
    }
  }
  else
    tux_same_counter = 0;
}

void game_handle_comets(void)
{
  int i;
  /* Handle comets: */
  num_comets_alive = 0;
      
  for (i = 0; i < MAX_COMETS; i++)
  {
    if (comets[i].alive)
    {
      num_comets_alive++;
      /* update comet position */
      comets[i].x = comets[i].x + 0; /* no lateral motion for now! */
      comets[i].y = comets[i].y + (game_options->speed);
	      
      if (comets[i].y >= (screen->h - images[IMG_CITY_BLUE]->h) &&
	  comets[i].expl < COMET_EXPL_END)
      {
        printf("\nAbout to disable shields or destroy city!\n");
        /* Disable shields or destroy city: */
        MC_AnsweredIncorrectly(&(comets[i].flashcard));
        if (cities[comets[i].city].shields)
	{
          printf("\nDisabling shields!\n");
	  cities[comets[i].city].shields = 0;
	  playsound(SND_SHIELDSDOWN);
	}
        else
        {
          printf("\nDestroying city!\n");
          cities[comets[i].city].expl = CITY_EXPL_START;
          playsound(SND_EXPLOSION);
        }

        tux_anim = IMG_TUX_FIST1;
        tux_anim_frame = ANIM_FRAME_START;

        /* Destroy comet: */
        comets[i].expl = COMET_EXPL_START;
      }

      /* Handle comet explosion animation: */
      if (comets[i].expl >= COMET_EXPL_END)
      {
        comets[i].expl--;
	if (comets[i].expl < COMET_EXPL_END)
	  comets[i].alive = 0;
      }
    }
  }

  /* add more comets if needed: */
  if (level_start_wait == 0 &&
      (frame % 20) == 0 &&
      gameover == 0)
  {
    /* num_attackers is how many comets are left in wave */
    if (num_attackers > 0)
    {
      if ((rand() % 2) == 0 || num_comets_alive == 0)
      {
        if (add_comet())
        {
	  num_attackers--;
        }
      }
    }
    else
    {
      if (num_comets_alive == 0)
      {
        /* Time for the next wave! */

	/* FIXME: End of level stuff goes here */
        /* FIXME this belongs in game_handle_cities(), I think */
        if (num_cities_alive > 0)
	{
          /* Go on to the next wave: */
	  wave++;
	  reset_level();
	}
	else
	{
          /* No more cities!  Game over! */
          gameover = GAMEOVER_COUNTER_START;
	}
      }
    }
  }
}

void game_handle_cities(void)
{
  int i;
  /* FIXME does the following variable do anything? */
  num_cities_alive = 0;

  for (i = 0; i < NUM_CITIES; i++)
  {
    if (cities[i].alive)
    {
      num_cities_alive++;
      /* Handle animated explosion: */
      if (cities[i].expl)
      {
        cities[i].expl--;
	if (cities[i].expl == 0)
	   cities[i].alive = 0;
      }
    }
  }
}

void game_draw(void)
{
  int i,j, img;
  SDL_Rect src, dest;
  char str[64];
  char* comet_str;

  /* Clear screen: */
  if (bkgd == NULL)
  {
    dest.x = 0;
    dest.y = 0;
    dest.w = screen->w;
    dest.h = ((screen->h) / 4) * 3;

    SDL_FillRect(screen, &dest, 
                 SDL_MapRGB(screen->format,
                                    64,
		                    64 + ((wave * 32) % 192),
		                    128 - ((wave * 16) % 128)));

    dest.x = 0;
    dest.y = ((screen->h) / 4) * 3;
    dest.w = screen->w;
    dest.h = (screen->h) / 4;

    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 64, 96, 64));
  }
  else
  {
    SDL_BlitSurface(bkgd, NULL, screen, NULL);
  }

  /* Draw "Demo" */
  if (game_options->demo_mode)
  {
    dest.x = (screen->w - images[IMG_DEMO]->w) / 2;
    dest.y = (screen->h - images[IMG_DEMO]->h) / 2;
    dest.w = images[IMG_DEMO]->w;
    dest.h = images[IMG_DEMO]->h;

    SDL_BlitSurface(images[IMG_DEMO], NULL, screen, &dest);
  }

  /* Draw wave: */
  dest.x = 0;
  dest.y = 0;
  dest.w = images[IMG_WAVE]->w;
  dest.h = images[IMG_WAVE]->h;

  SDL_BlitSurface(images[IMG_WAVE], NULL, screen, &dest);

  sprintf(str, "%d", wave);
  draw_numbers(str, images[IMG_WAVE]->w + (images[IMG_NUMBERS]->w / 10));

  /* Draw score: */
  dest.x = (screen->w - ((images[IMG_NUMBERS]->w / 10) * 7) -
	        images[IMG_SCORE]->w);
  dest.y = 0;
  dest.w = images[IMG_SCORE]->w;
  dest.h = images[IMG_SCORE]->h;

  SDL_BlitSurface(images[IMG_SCORE], NULL, screen, &dest);
      
  sprintf(str, "%.6d", score);
  draw_numbers(str, screen->w - ((images[IMG_NUMBERS]->w / 10) * 6));
      
  /* Draw cities: */
  for (i = 0; i < NUM_CITIES; i++)
  {
    /* Decide which image to display: */
    if (cities[i].alive)
    {
      if (cities[i].expl == 0)
        img = IMG_CITY_BLUE;
      else
        img = (IMG_CITY_BLUE_EXPL5 - (cities[i].expl / (CITY_EXPL_START / 5)));
    }
    else
    {
      img = IMG_CITY_BLUE_DEAD;
    }
    /* Change image to appropriate color: */
    img = img + ((wave % MAX_CITY_COLORS) *
	       (IMG_CITY_GREEN - IMG_CITY_BLUE));
	  
    /* Draw it! */
    dest.x = cities[i].x - (images[img]->w / 2);
    dest.y = (screen->h) - (images[img]->h);
    dest.w = (images[img]->w);
    dest.h = (images[img]->h);
  
    SDL_BlitSurface(images[img], NULL, screen, &dest);

    /* Draw sheilds: */
    if (cities[i].shields)
    {
      for (j = (frame % 3); j < images[IMG_SHIELDS]->h; j = j + 3)
      {
	      src.x = 0;
	      src.y = j;
	      src.w = images[IMG_SHIELDS]->w;
	      src.h = 1;

	      dest.x = cities[i].x - (images[IMG_SHIELDS]->w / 2);
	      dest.y = (screen->h) - (images[IMG_SHIELDS]->h) + j;
	      dest.w = src.w;
	      dest.h = src.h;

	      SDL_BlitSurface(images[IMG_SHIELDS], &src, screen, &dest);
      }
    }
  }


  /* Draw comets: */
 for (i = 0; i < MAX_COMETS; i++)
 {
   if (comets[i].alive)
   { 
     if (comets[i].expl < COMET_EXPL_END)
     {
       /* Decide which image to display: */
       img = IMG_COMET1 + ((frame + i) % 3);
       /* Display the formula (flashing, in the bottom half
		   of the screen) */
       if (comets[i].y < screen->h / 2 || frame % 8 < 6)
       {
         comet_str = comets[i].formula;
       }
       else 
       {
         comet_str = NULL;
       }
      }
      else
      {
        img = comets[i].expl;
        comet_str = comets[i].answer_str;
      }
	      
      /* Draw it! */
      dest.x = comets[i].x - (images[img]->w / 2);
      dest.y = comets[i].y - images[img]->h;
      dest.w = images[img]->w;
      dest.h = images[img]->h;
	      
      SDL_BlitSurface(images[img], NULL, screen, &dest);
      if (comet_str != NULL)
      {
        draw_nums(comet_str, comets[i].x, comets[i].y);
      }
    }
  }

  /* Draw laser: */
  if (laser.alive)
  {
    draw_line(laser.x1, laser.y1, laser.x2, laser.y2,
		  255 / (LASER_START - laser.alive),
		  192 / (LASER_START - laser.alive),
		  64);
  }

  /* Draw numeric keypad: */
  if (game_options->use_keypad)
  {
    /* pick image to draw: */
    int keypad_image;
    if (MC_AllowNegAnswer())
    {
      /* draw regular keypad */
      keypad_image = IMG_KEYPAD;
    }
    else
    {
      /* draw keypad with with grayed-out '+' and '-' */
      keypad_image = IMG_KEYPAD_NO_NEG;
    }

    /* now draw it: */ 
    dest.x = (screen->w - images[keypad_image]->w) / 2;
    dest.y = (screen->h - images[keypad_image]->h) / 2;
    dest.w = images[keypad_image]->w;
    dest.h = images[keypad_image]->h;
    SDL_BlitSurface(images[keypad_image], NULL, screen, &dest);
  }

  /* Draw console & tux: */
  /* FIXME add counter for remaining questions */
  draw_console_image(IMG_CONSOLE);

  if (gameover > 0)
  {
    tux_img = IMG_TUX_FIST1 + ((frame / 2) % 2);
  }
  draw_console_image(tux_img);

  /* LED code moved into separate function by DSB */
  draw_led_nums();

  /* Draw "Game Over" */
  if (gameover > 0)
  {
    dest.x = (screen->w - images[IMG_GAMEOVER]->w) / 2;
    dest.y = (screen->h - images[IMG_GAMEOVER]->h) / 2;
    dest.w = images[IMG_GAMEOVER]->w;
    dest.h = images[IMG_GAMEOVER]->h;
	
    SDL_BlitSurface(images[IMG_GAMEOVER], NULL, screen, &dest);
   }
 
  /* Swap buffers: */
  SDL_Flip(screen);
}

int check_exit_conditions(void)
{
  if (SDL_quit_received)
  {
    return GAME_OVER_WINDOW_CLOSE;
  }

  if (escape_received)
  {
    return GAME_OVER_ESCAPE;
  }

  /* determine if game lost (i.e. all cities blown up): */
  {
    int i;
    int surviving_cities = 0;

    for (i = 0; i < NUM_CITIES; i++)
    {
      if (cities[i].alive)
      {
        surviving_cities++;
      }
    }

    if (!surviving_cities)
    {
      return GAME_OVER_LOST;
    }
  }

  /* determine if game won (i.e. all questions in mission answered correctly): */
  if (MC_MissionAccomplished())
  {
    return GAME_OVER_WON;
  }

  /* Could have situation where mathcards doesn't have more questions */
  /* even though not all questions answered correctly:                */
  if (MC_GameOver())
  {
    return GAME_OVER_OTHER;
  }

  /* Need to get out if no comets alive and MathCards out of unanswered questions, */
  /* even though MathCards thinks there are still questions "in play".  */
  /* This SHOULD NOT HAPPEN and means we have a bug somewhere. */

  {
//     int i;
//     int living_comets = 0;
// 
//     for (i = 0; i < MAX_COMETS; i++)
//     {
//       living_comets += comets[i].alive;
//     }

    if (MC_NoQuestionsLeft() && !num_comets_alive)
    {
      #ifdef TUXMATH_DEBUG
      printf("\nNoQuestionsLeft() = %d", MC_NoQuestionsLeft());
      printf("\nnum_comets_alive = %d", num_comets_alive);
      #endif 
      return GAME_OVER_ERROR;
    }
  }

  /* if we made it to here, the game goes on! */
  return GAME_IN_PROGRESS;
}

void print_exit_conditions(void)
{
  printf("\ngame_status:\t");
  switch (game_status)
  {
    case GAME_IN_PROGRESS:
    {
      printf("GAME_IN_PROGRESS\n");
      break;
    }

    case GAME_OVER_WON:
    {
      printf("GAME_OVER_WON\n");
      break;
    }
    case GAME_OVER_LOST:
    {
      printf("GAME_OVER_LOST\n");
      break;
    }
    case GAME_OVER_OTHER:
    {
      printf("GAME_OVER_OTHER\n");
      break;
    }
    case GAME_OVER_ESCAPE:
    {
      printf("GAME_OVER_ESCAPE\n");
      break;
    }
    case GAME_OVER_WINDOW_CLOSE:
    {
      printf("GAME_OVER_WINDOW_CLOSE\n");
      break;
    }
    case GAME_OVER_ERROR:
    {
      printf("GAME_OVER_ERROR\n");
      break;
    }
    default:
    {
      printf("Unrecognized value\n");
      break;
    }
  }
}

/* Reset stuff for the next level! */
void reset_level(void)
{
  char fname[1024];
  int i;
  
  
  /* Clear all comets: */
  
  for (i = 0; i < MAX_COMETS; i++)
  {
    comets[i].alive = 0;
  }
  num_comets_alive = 0;

  /* Clear LED digits: */
  
  digits[0] = 0;
  digits[1] = 0;
  digits[2] = 0;
  neg_answer_picked = 0;

  /* Load random background image: */

  do
  {
    /* Don't pick the same one as last time... */

    i = rand() % NUM_BKGDS;
  }
  while (i == last_bkgd);

  last_bkgd = i;

  sprintf(fname, "%s/images/backgrounds/%d.jpg", DATA_PREFIX, i);

  if (bkgd != NULL)
    SDL_FreeSurface(bkgd);

  
  if (game_options->use_bkgd)
  {
    bkgd = IMG_Load(fname);
    if (bkgd == NULL)
    {
      fprintf(stderr,
	      "\nWarning: Could not load background image:\n"
	      "%s\n"
	      "The Simple DirectMedia error that ocurred was: %s\n",
	      fname, SDL_GetError());
      game_options->use_bkgd = 0;
    }
  }


  /* Record score before this wave: */

  pre_wave_score = score;


  /* Set number of attackers for this wave: */
  /* num_attackers = 2 * wave; */ 

  /* increase number of comets and speed if desired */
  if (game_options->allow_speedup)
  {
    num_attackers = wave * game_options->extra_comets;
    game_options->speed = (game_options->speed) * game_options->speedup_factor;

    if (game_options->speed > game_options->max_speed)
    {
      game_options->speed = game_options->max_speed;
    } 
  }
  else
  {
    num_attackers = 2;
  }
}



/* Add a comet to the game (if there's room): */

int add_comet(void)
{
  static int prev_city = -1;
  int i, found;
  

  /* Look for a free comet slot: */
  
  found = -1;
  
  for (i = 0; i < MAX_COMETS && found == -1; i++)
  {
    if (comets[i].alive == 0)
    {
      found = i;
    }
  }
  
  
  if (found != -1)
  {
    /* Get math question for new comet: */
    if (!MC_NextQuestion(&(comets[found].flashcard)))
    /* no more questions - cannot create comet.  */ 
    {
      return 0;
    }

    /* FIXME streamline comet struct - for now assign to old variables */
    /* FIXME also need to handle question formats */
    comets[found].eq1 = comets[found].flashcard.num1;
    comets[found].oper = comets[found].flashcard.operation;
    comets[found].eq2 = comets[found].flashcard.num2;
    comets[found].answer = comets[found].flashcard.num3;

    comets[found].alive = 1;
    num_comets_alive++;      

    /* Pick a city to attack that was not attacked last time */
    /* (so formulas are less likely to overlap). */
    do 
    {
      i = rand() % NUM_CITIES;
    }
    while (i == prev_city);

    prev_city = i;
     
    /* Set in to attack that city: */
    comets[found].city = i; 
    /* Start at the top, above the city in question: */
    comets[found].x = cities[i].x;
    comets[found].y = 0;

    snprintf(comets[found].formula, FORMULA_LEN,"%d%c%d",
	     comets[found].eq1,
	     operchars[comets[found].oper],
	     comets[found].eq2);
    snprintf(comets[found].answer_str, ANSWER_LEN, "%d",
	     comets[found].answer);

    /* comet slot found and question found so return successfully: */
    return 1;
  }
  /* free comet slot not found - no comet added: */
  return 0;
}


/* Draw numbers/symbols over the attacker: */
/* This draw the numbers related to the comets */
void draw_nums(char * str, int x, int y)
{
  int i, j, cur_x, c;
  SDL_Rect src, dest;


  /* Center around the shape */
  
  cur_x = x - ((strlen(str) * (images[IMG_NUMS]->w / (10 + NUM_OPERS)))) / 2;

  if (cur_x < 0)
    cur_x = 0;

  if (cur_x + (strlen(str) * (images[IMG_NUMS]->w / (10 + NUM_OPERS))) >=
      screen->w)
    cur_x = ((screen->w) -
             (strlen(str) * (images[IMG_NUMS]->w / (10 + NUM_OPERS))));


  /* Draw each character: */
  
  for (i = 0; i < strlen(str); i++)
    {
      c = -1;


      /* Determine which character to display: */
      
      if (str[i] >= '0' && str[i] <= '9')
	c = str[i] - '0';
      else
	{
	  /* [ THIS COULD CAUSE SLOWNESS... ] */
		
	  for (j = 0; j < NUM_OPERS; j++)
	    {
	      if (str[i] == operchars[j])
		{
		  c = 10 + j;
		}
	    }
	}
      

      /* Display this character! */
      
      if (c != -1)
	{
	  src.x = c * (images[IMG_NUMS]->w / (10 + NUM_OPERS));
	  src.y = 0;
	  src.w = (images[IMG_NUMS]->w / (10 + NUM_OPERS));
	  src.h = images[IMG_NUMS]->h;
	  
	  dest.x = cur_x;
	  dest.y = y - images[IMG_NUMS]->h;
	  dest.w = src.w;
	  dest.h = src.h;
	  
	  SDL_BlitSurface(images[IMG_NUMS], &src,
			  screen, &dest);


          /* Move the 'cursor' one character width: */

	  cur_x = cur_x + (images[IMG_NUMS]->w / (10 + NUM_OPERS));
	}
    }
}


/* Draw status numbers: */

void draw_numbers(char * str, int x)
{
  int i, cur_x, c;
  SDL_Rect src, dest;


  cur_x = x;


  /* Draw each character: */
  
  for (i = 0; i < strlen(str); i++)
    {
      c = -1;


      /* Determine which character to display: */
      
      if (str[i] >= '0' && str[i] <= '9')
	c = str[i] - '0';
      

      /* Display this character! */
      
      if (c != -1)
	{
	  src.x = c * (images[IMG_NUMBERS]->w / 10);
	  src.y = 0;
	  src.w = (images[IMG_NUMBERS]->w / 10);
	  src.h = images[IMG_NUMBERS]->h;
	  
	  dest.x = cur_x;
	  dest.y = 0;
	  dest.w = src.w;
	  dest.h = src.h;
	  
	  SDL_BlitSurface(images[IMG_NUMBERS], &src,
			  screen, &dest);


          /* Move the 'cursor' one character width: */

	  cur_x = cur_x + (images[IMG_NUMBERS]->w / 10);
	}
    }
}


/* Pause loop: */

int pause_game(void)
{
  /* NOTE - done and quit changed to pause_done and pause_quit */
  /* due to potentially confusing name collision */
  int pause_done, pause_quit;
  SDL_Event event;
  SDL_Rect dest;
 
  pause_done = 0;
  pause_quit = 0;

  dest.x = (screen->w - images[IMG_PAUSED]->w) / 2;
  dest.y = (screen->h - images[IMG_PAUSED]->h) / 2;
  dest.w = images[IMG_PAUSED]->w;
  dest.h = images[IMG_PAUSED]->h;
    
  SDL_BlitSurface(images[IMG_PAUSED], NULL, screen, &dest);
  SDL_Flip(screen);


#ifndef NOSOUND
  if (game_options->use_sound)
    Mix_PauseMusic();
#endif
  
  
  do
  {
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_KEYDOWN)
	pause_done = 1;
      else if (event.type == SDL_QUIT)
      {
        SDL_quit_received = 1;
        /* FIXME quit is being superseded */
 	pause_quit = 1;
      }
    }

    SDL_Delay(100);
  }
  while (!pause_done && !pause_quit);


#ifndef NOSOUND
  if (game_options->use_sound)
    Mix_ResumeMusic();
#endif

  return (pause_quit);
}  



/* Draw a line: */

void draw_line(int x1, int y1, int x2, int y2, int red, int grn, int blu)
{
  int dx, dy, tmp;
  float m, b;
  Uint32 pixel;
  SDL_Rect dest;
 
  pixel = SDL_MapRGB(screen->format, red, grn, blu);

  dx = x2 - x1;
  dy = y2 - y1;

  putpixel(screen, x1, y1, pixel);
  
  if (dx != 0)
  {
    m = ((float) dy) / ((float) dx);
    b = y1 - m * x1;

    if (x2 > x1)
      dx = 1;
    else
      dx = -1;

    while (x1 != x2)
    {
      x1 = x1 + dx;
      y1 = m * x1 + b;
      
      putpixel(screen, x1, y1, pixel);
    }
  }
  else
  {
    if (y1 > y2)
    {
      tmp = y1;
      y1 = y2;
      y2 = tmp;
    }
    
    dest.x = x1;
    dest.y = y1;
    dest.w = 3;
    dest.h = y2 - y1;

    SDL_FillRect(screen, &dest, pixel);
  }
}


/* Draw a single pixel into the surface: */

void putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel)
{
#ifdef PUTPIXEL_RAW
  int bpp;
  Uint8 * p;
  
  /* Determine bytes-per-pixel for the surface in question: */
  
  bpp = surface->format->BytesPerPixel;
  
  
  /* Set a pointer to the exact location in memory of the pixel
     in question: */
  
  p = (Uint8 *) (surface->pixels +       /* Start at beginning of RAM */
                 (y * surface->pitch) +  /* Go down Y lines */
                 (x * bpp));             /* Go in X pixels */
  
  
  /* Assuming the X/Y values are within the bounds of this surface... */
  
  if (x >= 0 && y >= 0 && x < surface -> w && y < surface -> h)
    {
      /* Set the (correctly-sized) piece of data in the surface's RAM
         to the pixel value sent in: */
      
      if (bpp == 1)
        *p = pixel;
      else if (bpp == 2)
        *(Uint16 *)p = pixel;
      else if (bpp == 3)
        {
          if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
            {
              p[0] = (pixel >> 16) & 0xff;
              p[1] = (pixel >> 8) & 0xff;
              p[2] = pixel & 0xff;
            }
          else
            {
              p[0] = pixel & 0xff;
              p[1] = (pixel >> 8) & 0xff;
              p[2] = (pixel >> 16) & 0xff;
            }
        }
      else if (bpp == 4)
        {
          *(Uint32 *)p = pixel;
        }
    }
#else
  SDL_Rect dest;

  dest.x = x;
  dest.y = y;
  dest.w = 3;
  dest.h = 4;

  SDL_FillRect(surface, &dest, pixel);
#endif
}


/* Draw image at lower center of screen: */

void draw_console_image(int i)
{
  SDL_Rect dest;

  dest.x = (screen->w - images[i]->w) / 2;
  dest.y = (screen->h - images[i]->h);
  dest.w = images[i]->w;
  dest.h = images[i]->h;

  SDL_BlitSurface(images[i], NULL, screen, &dest);
}
/* Draw LED digits at the top of the screen: */
/* Modified by DSB to display minus sign */

void draw_led_nums(void)
{
  int i;
  SDL_Rect src, dest;

  /* begin drawing so as to center display depending on whether minus */
  /* sign needed (4 digit slots) or not (3 digit slots) DSB */
  if (MC_AllowNegAnswer())
    dest.x = ((screen->w - ((images[IMG_LEDNUMS]->w) / 10) * 4) / 2);
  else
    dest.x = ((screen->w - ((images[IMG_LEDNUMS]->w) / 10) * 3) / 2);

  for (i = -1; i < 3; i++)  /* -1 is special case to allow minus sign */
                              /* with minimal modification of existing code DSB */
  { 
    if (-1 == i)
    {
      if (MC_AllowNegAnswer())
      {
        if (neg_answer_picked)
          src.x =  (images[IMG_LED_NEG_SIGN]->w) / 2;
        else
          src.x = 0;

        src.y = 0;
        src.w = (images[IMG_LED_NEG_SIGN]->w) / 2;
        src.h = images[IMG_LED_NEG_SIGN]->h;
	  
        dest.y = 4;
        dest.w = src.w;
        dest.h = src.h;
	  
        SDL_BlitSurface(images[IMG_LED_NEG_SIGN], &src, screen, &dest);
        /* move "cursor" */
        dest.x += src.w;
      }
    }
    else
    {
      src.x = digits[i] * ((images[IMG_LEDNUMS]->w) / 10);
      src.y = 0;
      src.w = (images[IMG_LEDNUMS]->w) / 10;
      src.h = images[IMG_LEDNUMS]->h;
	  
      /* dest.x already set */
      dest.y = 4;
      dest.w = src.w;
      dest.h = src.h;
	  
      SDL_BlitSurface(images[IMG_LEDNUMS], &src, screen, &dest);
      /* move "cursor" */
      dest.x += src.w;
    }
  }
}

/* Translates mouse events into keyboard events when on-screen keypad used */
void game_mouse_event(SDL_Event event)
{
  int keypad_w, keypad_h, x, y, row, column;
  SDLKey key = SDLK_UNKNOWN;

  keypad_w = 0;
  keypad_h = 0;

  /* get out unless we really are using keypad */
  if ( level_start_wait 
    || game_options->demo_mode
    || !game_options->use_keypad)
  {
    return;
  }
  /* make sure keypad image is valid and has non-zero dimensions: */
  /* FIXME maybe this checking should be done once at the start */
  /* of game() rather than with every mouse click */
  if (MC_AllowNegAnswer())
  {
    if (!images[IMG_KEYPAD])
      return;
    else
    {
      keypad_w = images[IMG_KEYPAD]->w;
      keypad_h = images[IMG_KEYPAD]->h;
    }
  }
  else
  {
    if (!images[IMG_KEYPAD_NO_NEG])
      return;
    else
    {
      keypad_w = images[IMG_KEYPAD]->w;
      keypad_h = images[IMG_KEYPAD]->h;
    }
  }

 
  if (!keypad_w || !keypad_h)
  {
    return;
  }

  /* only proceed if click falls within keypad: */
  if (!((event.button.x >=
        (screen->w / 2) - (keypad_w / 2) &&
        event.button.x <=
        (screen->w / 2) + (keypad_w / 2) &&
        event.button.y >= 
        (screen->h / 2) - (keypad_h / 2) &&
        event.button.y <=
        (screen->h / 2) + (keypad_h / 2))))
  /* click outside of keypad - do nothing */
  {
    return;
  }
  
  else /* click was within keypad */ 
  {
    x = (event.button.x - ((screen->w / 2) - (keypad_w / 2)));
    y = (event.button.y - ((screen->h / 2) - (keypad_h / 2)));
 
  /* Now determine what onscreen key was pressed */
  /*                                             */
  /* The on-screen keypad has a 4 x 4 layout:    */
  /*                                             */
  /*    *********************************        */
  /*    *       *       *       *       *        */
  /*    *   7   *   8   *   9   *   -   *        */
  /*    *       *       *       *       *        */
  /*    *********************************        */
  /*    *       *       *       *       *        */
  /*    *   4   *   5   *   6   *       *        */
  /*    *       *       *       *       *        */
  /*    *************************   +   *        */
  /*    *       *       *       *       *        */
  /*    *   1   *   2   *   3   *       *        */
  /*    *       *       *       *       *        */
  /*    *********************************        */
  /*    *       *                       *        */
  /*    *   0   *         Enter         *        */
  /*    *       *                       *        */
  /*    *********************************        */
  /*                                             */
  /*  The following code simply figures out the  */
  /*  row and column based on x and y and looks  */
  /*  up the SDlKey accordingly.                 */

    column = x/((keypad_w)/4);
    row    = y/((keypad_h)/4);

    /* make sure row and column are sane */
    if (column < 0
     || column > 3
     || row    < 0
     || row    > 3)
    {
      printf("\nIllegal row or column value!\n");
      return;
    }

    /* simple but tedious - I am sure this could be done more elegantly */

    if (0 == row)
    {
      if (0 == column)
        key = SDLK_7;
      if (1 == column)
        key = SDLK_8;
      if (2 == column)
        key = SDLK_9;
      if (3 == column)
        key = SDLK_MINUS;
    } 
    if (1 == row)
    {
      if (0 == column)
        key = SDLK_4;
      if (1 == column)
        key = SDLK_5;
      if (2 == column)
        key = SDLK_6;
      if (3 == column)
        key = SDLK_PLUS;
    }     
    if (2 == row)
    {
      if (0 == column)
        key = SDLK_1;
      if (1 == column)
        key = SDLK_2;
      if (2 == column)
        key = SDLK_3;
      if (3 == column)
        key = SDLK_PLUS;
    } 
    if (3 == row)
    {
      if (0 == column)
        key = SDLK_0;
      if (1 == column)
        key = SDLK_RETURN;
      if (2 == column)
        key = SDLK_RETURN;
      if (3 == column)
        key = SDLK_RETURN;
    }     

    if (key == SDLK_UNKNOWN)
    {
      return;
    }

    /* now can proceed as if keyboard was used */
    game_key_event(key);
  }
}

/* called by either key presses or mouse clicks on */
/* on-screen keypad */
void game_key_event(SDLKey key)
{
  if (key == SDLK_ESCAPE)
  {
    /* Escape key - quit! */
    escape_received = 1;
    /* FIXME done will be superseded */
    done = 1;
  }
  else if (key == SDLK_TAB
        || key == SDLK_p)
  {
  /* [TAB] or [P]: Pause! */
    paused = 1;
  }
      
  if (level_start_wait > 0 || game_options->demo_mode)
  {
    /* Eat other keys until level start wait has passed,
    or if game is in demo mode: */
    key = SDLK_UNKNOWN;
  }
	      
  if (key >= SDLK_0 && key <= SDLK_9)
  {
    /* [0]-[9]: Add a new digit: */
    digits[0] = digits[1];
    digits[1] = digits[2];
    digits[2] = key - SDLK_0;
    tux_pressing = 1;
  }
  else if (key >= SDLK_KP0 && key <= SDLK_KP9)
  {
    /* Keypad [0]-[9]: Add a new digit: */
    digits[0] = digits[1];
    digits[1] = digits[2];
    digits[2] = key - SDLK_KP0;
    tux_pressing = 1;
  }
  /* support for negative answer input DSB */
  else if ((key == SDLK_MINUS || key == SDLK_KP_MINUS)
        && MC_AllowNegAnswer())  /* do nothing unless neg answers allowed */
  {
    /* allow player to make answer negative: */
    neg_answer_picked = 1;
    tux_pressing = 1;
  }
  else if ((key == SDLK_PLUS || key == SDLK_KP_PLUS)
         && MC_AllowNegAnswer())  /* do nothing unless neg answers allowed */
  {
    /* allow player to make answer positive: */
    neg_answer_picked = 0;
    tux_pressing = 1;
  }
  else if (key == SDLK_BACKSPACE ||
           key == SDLK_CLEAR ||
	   key == SDLK_DELETE)
  {
    /* [BKSP]: Clear digits! */
    digits[0] = 0;
    digits[1] = 0;
    digits[2] = 0;
    tux_pressing = 1;
  }
  else if (key == SDLK_RETURN ||
           key == SDLK_KP_ENTER ||
	   key == SDLK_SPACE)
  {
    /* [ENTER]: Accept digits! */
    doing_answer = 1;
  }
}

/* Increment score: */

void add_score(int inc)
{
  score += inc;
}


/* Pick a suitable operand: */

int pick_operand(int min)
{
  int i;

  do
  {
    i = (rand() % 50) + min;
  }
  while (!in_range(i));

  return i;
}


/* Is the value with available operand ranges? */

int in_range(int n)
{
  int ok, i;
  
  ok = 0;
  
  for (i = 0; i < NUM_Q_RANGES && ok == 0; i++)
  {
    if (range_enabled[i])
    {
      if (n >= ranges[i].min && n <= ranges[i].max)
	ok = 1;
    }
  }
  
  return ok;
}

void reset_comets(void)
{
  int i =0;
  for (i = 0; i < MAX_COMETS; i++)
  {
    comets[i].alive = 0;
    comets[i].expl = 0;
    comets[i].city = 0;
    comets[i].x = 0;
    comets[i].y = 0;
    comets[i].eq1 = 0;
    comets[i].oper = 0;
    comets[i].eq2 = 0;
    comets[i].formula[FORMULA_LEN] = "";
    comets[i].answer = 0;
    comets[i].answer_str[ANSWER_LEN] = "";
  }
}
