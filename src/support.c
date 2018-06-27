#include "support.h"

SDL_Surface *screen;
SDL_Surface *display;
int keyboard[SDLK_LAST];
int debug=0;
int use_joystick=1;
IMGLIST *img_list=NULL;
KEYCONFIG keyconfig;
//Uint32 videoflags=SDL_FULLSCREEN | SDL_DOUBLEBUF |SDL_HWSURFACE | SDL_HWPALETTE| SDL_HWACCEL;
#ifdef GP2X
Uint32 videoflags=SDL_FULLSCREEN | SDL_SWSURFACE;
#elif defined(RS97)
Uint32 videoflags=SDL_HWSURFACE | SDL_HWPALETTE| SDL_HWACCEL;
#else
Uint32 videoflags=SDL_DOUBLEBUF |SDL_HWSURFACE | SDL_HWPALETTE| SDL_HWACCEL;
#endif
int WIDTH=320;
int scaling=1;
int depth=16;
int screennum=0;
char *moddir = "./data";
char screenbuf[20];
extern GAMESTATE state;
extern GAMESTATE laststate;

void game_init(int argc, char *argv[])
{
	Uint32 initflags=0;
	int i;
	SDL_Joystick *joy;

	use_joystick = 0;
	/*initflags=SDL_INIT_VIDEO;
	if(use_joystick)
		initflags|=SDL_INIT_JOYSTICK;*/

	SDL_Init(SDL_INIT_VIDEO);
	//atexit(SDL_Quit);

	//if((screen=SDL_SetVideoMode(WIDTH,HEIGHT,depth,videoflags))==NULL) {
	if((screen=SDL_SetVideoMode(320,240,16,SDL_HWSURFACE))==NULL) {
		CHECKPOINT;
		error(ERR_FATAL,"cant open screen: %s",SDL_GetError());
	}
	SDL_ShowCursor(0);

	if(use_joystick) {
		if(debug) {
			error(ERR_DEBUG,"%i joysticks found",SDL_NumJoysticks());
			for(i=0;i<SDL_NumJoysticks();i++) {
				error(ERR_DEBUG,"stick %d: %s",i,SDL_JoystickName(i));
			}
		}
		if(SDL_NumJoysticks()>0) {
			joy=SDL_JoystickOpen(0);
			if(joy) {
				error(ERR_DEBUG,"Joystick 0:");
				error(ERR_DEBUG,"Name: %s",SDL_JoystickName(0));
				error(ERR_DEBUG,"Axes: %d",SDL_JoystickNumAxes(joy));
				error(ERR_DEBUG,"Buttons: %d",SDL_JoystickNumButtons(joy));
				error(ERR_DEBUG,"Balls: %d",SDL_JoystickNumBalls(joy));
			} else {
				error(ERR_WARN,"could not open joystick #0");
			}
		}
	}

	/* Tastenbelegung initialisieren */
	keyconfig.u=SDLK_UP;
	keyconfig.d=SDLK_DOWN;
	keyconfig.l=SDLK_LEFT;
	keyconfig.r=SDLK_RIGHT;
	keyconfig.f=SDLK_LCTRL;
	keyconfig.e=SDLK_LALT;
	keyconfig.g=SDLK_ESCAPE;
	
	keyboard_clear();
	preload_gfx();
	font_init();
	menusystem_init();
	hsc_init();
	hsc_load();
	fps_init();
	newstate(ST_START_INTRO,0,1);
	initSound();

}

void error(int errorlevel, char *msg, ...)
{
	char msgbuf[128];
	va_list argptr;

	va_start(argptr, msg);
	vsprintf(msgbuf, msg, argptr);
	va_end(argptr);

	switch(errorlevel) {
		case ERR_DEBUG: if(debug) { fprintf(stdout,"DEBUG: %s\n",msgbuf); } break;
		case ERR_INFO: fprintf(stdout,"INFO: %s\n",msgbuf); break;
		//case ERR_WARN: fprintf(stdout,"WARNING: %s\n",msgbuf); break;
		case ERR_FATAL: fprintf(stdout,"FATAL: %s\n",msgbuf); break;
	}

	if(errorlevel==ERR_FATAL) exit(1);
}

SDL_Surface *loadbmp(char *filename)
{
	char fn[255];
	strcpy(fn,moddir);
	strcat(fn,"/");
	strcat(fn,filename);

	SDL_Surface *s1,*s2;

	if((s1=imglist_search(fn))!=NULL) {
		return s1;
	}

	//if((s1=SDL_LoadBMP(fn))==NULL) {
	if((s1=IMG_Load(fn))==NULL) {
		CHECKPOINT;
		error(ERR_FATAL,"cant load image %s: %s",fn,SDL_GetError());
	}
	if((s2=SDL_DisplayFormat(s1))==NULL) {
		CHECKPOINT;
		error(ERR_FATAL,"cant convert image %s to display format: %s",fn,SDL_GetError());
	}
	SDL_FreeSurface(s1);
	s1=NULL;
	imglist_add(s2,fn);
	return(s2);
}

void unloadbmp_by_surface(SDL_Surface *s)
{
	IMGLIST *i=img_list;

	while(i!=NULL) {
		if(s==i->img) {
			if(!i->refcount) {
				CHECKPOINT;
				error(ERR_WARN,"unloadbmp_by_surface: refcount for object %s is already zero",i->name);
			} else {
				i->refcount--;
			}
			return;
		}
		i=i->next;
	}
	CHECKPOINT;
	error(ERR_WARN,"unloadbmp_by_surface: object not found");
}

void unloadbmp_by_name(char *name)
{
	char fn[255];
	strcpy(fn,moddir);
	strcat(fn,"/");
	strcat(fn,name);
	IMGLIST *i=img_list;

	while(i!=NULL) {
		if(!strcmp(i->name,fn)) {
			if(!i->refcount) {
				CHECKPOINT;
				error(ERR_WARN,"unloadbmp_by_name: refcount for object %s is already zero",i->name);
			} else {
				i->refcount--;
			}
			return;
		}
		i=i->next;
	}
	CHECKPOINT;
	error(ERR_WARN,"unloadbmp_by_name: object not found");
}

void imglist_add(SDL_Surface *s, char *name)
{
	IMGLIST *new;

	new=mmalloc(sizeof(IMGLIST));
	strcpy(new->name,name);
	new->refcount=1;
	new->img=s;

	if(img_list==NULL) {
		img_list=new;
		new->next=NULL;
	} else {
		new->next=img_list;
		img_list=new;
	}
}

SDL_Surface *imglist_search(char *name)
{
	IMGLIST *i=img_list;

	while(i!=NULL) {
		if(!strcmp(name,i->name)) {
			i->refcount++;
			return (i->img);
		}
		i=i->next;
	}
	return(NULL);
}

void imglist_garbagecollect()
{
	IMGLIST *s=img_list,*p=NULL,*n=NULL;

	while(s!=NULL) {
		n=s->next;

		if(s->refcount==0) {
			if(p==NULL) {
				img_list=n;
			} else {
				p->next=n;
			}
			//SDL_FreeSurface(s->img);
			free(s);
		} else {
			p=s;
		}
		s=n;
	}
}

/* dont forget to lock surface when using get/putpixel! */
Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
	int bpp=surface->format->BytesPerPixel;
	Uint8 *p=(Uint8 *)surface->pixels+y*surface->pitch+x*bpp;

	if(x>=clip_xmin(surface) && x<=clip_xmax(surface) && y>=clip_ymin(surface) && y<=clip_ymax(surface)){
		/*switch(bpp) {
			case 1:
				return *p;
			case 2:
				return *(Uint16 *)p;
			case 3:
				if(SDL_BYTEORDER==SDL_BIG_ENDIAN)
					return p[0]<<16|p[1]<<8|p[2];
				else
					return p[0]|p[1]<<8|p[2]<<16;
			case 4:
				return *(Uint32 *)p;
			default:
				return 0;
		}*/
		return *(Uint16 *)p;
	} else return 0;
}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	int bpp=surface->format->BytesPerPixel;
	Uint8 *p=(Uint8 *)surface->pixels+y*surface->pitch+x*bpp;
	if(x>=clip_xmin(surface) && x<=clip_xmax(surface) && y>=clip_ymin(surface) && y<=clip_ymax(surface)){
		/*switch(bpp) {
			case 1:
				*p=pixel;
				break;
			case 2:
				*(Uint16 *)p=pixel;
				break;
			case 3:
				if(SDL_BYTEORDER==SDL_BIG_ENDIAN) {
					p[0]=(pixel>>16)&0xff;
					p[1]=(pixel>>8)&0xff;
					p[2]=pixel&0xff;
				} else {
					p[2]=(pixel>>16)&0xff;
					p[1]=(pixel>>8)&0xff;
					p[0]=pixel&0xff;
				}
				break;
			case 4:
				*(Uint32 *)p=pixel;
		}*/
		*(Uint16 *)p=pixel;
	}
}

void blit_scaled(SDL_Surface *src, SDL_Rect *src_rct, SDL_Surface *dst, SDL_Rect *dst_rct)
{
	Sint32 x, y;
	Sint32 u, v;
	Uint32 col, key;

	key=src->format->colorkey;

	if(SDL_MUSTLOCK(src))
		SDL_LockSurface(src);
	if(SDL_MUSTLOCK(dst))
		SDL_LockSurface(dst);

	for(y = 0; y<dst_rct->h; y++) {
		for(x = 0; x<dst_rct->w; x++) {
			u = src_rct->w * x / dst_rct->w;
			v = src_rct->h * y / dst_rct->h;
			col=getpixel(src, u + src_rct->x, v + src_rct->y);
			if(col!=key)
				putpixel(dst, x + dst_rct->x, y + dst_rct->y, col);
		}
	}

	if(SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
	if(SDL_MUSTLOCK(dst))
		SDL_UnlockSurface(dst);
}

/* just a quick hack - dont know if i will use it in the final game...
 * blits only every 2nd pixel, to archive a cheap 50%-alpha effect.
 */
void blit_calpha(SDL_Surface *src, SDL_Rect *src_rct, SDL_Surface *dst, SDL_Rect *dst_rct)
{
	Sint32 x, y;
	Uint32 col, key;

	Uint16 *ps,*pd;
	Uint16 *sps,*spd;

	if(src->format->BitsPerPixel!=dst->format->BitsPerPixel) {
		CHECKPOINT;
		error(ERR_FATAL,"cant handle different pixelformats (yet?)");
	}
	if(src->format->BitsPerPixel!=16) {
		CHECKPOINT;
		error(ERR_FATAL,"can only handle 16bit-pixelformat");
	}
	key=src->format->colorkey;

	if(SDL_MUSTLOCK(src))
		SDL_LockSurface(src);
	if(SDL_MUSTLOCK(dst))
		SDL_LockSurface(dst);

	ps=(Uint16 *)src->pixels+src_rct->y*src->pitch/2+src_rct->x;
	pd=(Uint16 *)dst->pixels+dst_rct->y*dst->pitch/2+dst_rct->x;

	for(y=0;y<src_rct->h;y++) {
		sps=ps;
		spd=pd;

		if(y%2) {
			ps++;
			pd++;
		}

		for(x=0;x<src_rct->w;x+=2) {

			if((x+dst_rct->x>=0) &&
			   (x+dst_rct->x<dst->w) &&
			   (y+dst_rct->y>=0) &&
			   (y+dst_rct->y<dst->h)) {

				col=*(ps);
				if(col!=key)
					*(pd)=col;
			}
			pd+=2;
			ps+=2;
		}
		ps=sps+src->pitch/2;
		pd=spd+dst->pitch/2;
	}

	if(SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
	if(SDL_MUSTLOCK(dst))
		SDL_UnlockSurface(dst);
}

void keyboard_clear()
{
	int i;

	for(i=0;i<SDLK_LAST;i++) {
		keyboard[i]=0;
	}
}

void keyboard_poll()
{
	SDL_Event event;

	if(SDL_PollEvent(&event)) {
		switch(event.type) {
		
			case SDL_KEYDOWN:
				keyboard[event.key.keysym.sym]=1;
				break;
			case SDL_KEYUP:
				keyboard[event.key.keysym.sym]=0;
				break;
		
			if(use_joystick) {
				case SDL_JOYAXISMOTION:
					if(event.jaxis.axis==0) {
						if(event.jaxis.value <= -16384) {
							keyboard[keyconfig.l]=1;
						} else {
							keyboard[keyconfig.l]=0;
						}
						if(event.jaxis.value >= 16384) {
							keyboard[keyconfig.r]=1;
						} else {
							keyboard[keyconfig.r]=0;
						}
					}
					if(event.jaxis.axis==1) {
						if(event.jaxis.value <= -16384) {
							keyboard[keyconfig.u]=1;
						} else {
							keyboard[keyconfig.u]=0;
						}
						if(event.jaxis.value >= 16384) {
							keyboard[keyconfig.d]=1;
						} else {
							keyboard[keyconfig.d]=0;
						}
					}
					break;
					break;
                #ifdef GP2X
                case SDL_JOYBUTTONDOWN:
					if(event.jbutton.button==12) { //button A
						keyboard[keyconfig.f]=1;
					}
					if(event.jbutton.button==13) { // button B
						keyboard[keyconfig.f]=1;
					}
					if(event.jbutton.button==4) { // button DOWN
						keyboard[keyconfig.d]=1;
					}
					if(event.jbutton.button==2) { // button LEFT
						keyboard[keyconfig.l]=1;
					}
					if(event.jbutton.button==0) { // button UP
						keyboard[keyconfig.u]=1;
					}
					if(event.jbutton.button==6) { // button RIGHT
						keyboard[keyconfig.r]=1;
					}
					if(event.jbutton.button==9) { // button SELECT
						keyboard[keyconfig.e]=1;
						//newstate(ST_GAME_QUIT,0,1);
					}
					if(event.jbutton.button==10) { // button L
						//save screenshot
						sprintf(screenbuf,"KETM%d.bmp",screennum++);
						 SDL_SaveBMP(screen,screenbuf);
					}
					if(event.jbutton.button==16) { // button VOL_UP
						GlobalVolumeUp();
					}
					if(event.jbutton.button==17) { // button VOL_DOWN
						GlobalVolumeDown();
					}
					break;

				case SDL_JOYBUTTONUP:
					if(event.jbutton.button==12) {
						keyboard[keyconfig.f]=0;
					}
					if(event.jbutton.button==13) {
						keyboard[keyconfig.f]=0;
					}
					if(event.jbutton.button==4) {
						keyboard[keyconfig.d]=0;
					}
					if(event.jbutton.button==2) {
						keyboard[keyconfig.l]=0;
					}
					if(event.jbutton.button==0) {
						keyboard[keyconfig.u]=0;
					}
					if(event.jbutton.button==6) {
						keyboard[keyconfig.r]=0;
					}
					if(event.jbutton.button==9) {
						keyboard[keyconfig.e]=0;
					}
					break;

                #else

				case SDL_JOYBUTTONDOWN:
					if(event.jbutton.button==2) {
						keyboard[keyconfig.f]=1;
					}
					if(event.jbutton.button==6) {
						keyboard[keyconfig.d]=1;
					}
					if(event.jbutton.button==7) {
						keyboard[keyconfig.l]=1;
					}
					if(event.jbutton.button==8) {
						keyboard[keyconfig.u]=1;
					}
					if(event.jbutton.button==9) {
						keyboard[keyconfig.r]=1;
					}
					if(event.jbutton.button==10) {
						keyboard[keyconfig.e]=1;
						//newstate(ST_GAME_QUIT,0,1);
					}
					if(event.jbutton.button==5) {
						//save screenshot
						sprintf(screenbuf,"KETM%d.bmp",screennum++);
						 SDL_SaveBMP(screen,screenbuf);
					}
					break;

				case SDL_JOYBUTTONUP:
					if(event.jbutton.button==2) {
						keyboard[keyconfig.f]=0;
					}
					if(event.jbutton.button==6) {
						keyboard[keyconfig.d]=0;
					}
					if(event.jbutton.button==7) {
						keyboard[keyconfig.l]=0;
					}
					if(event.jbutton.button==8) {
						keyboard[keyconfig.u]=0;
					}
					if(event.jbutton.button==9) {
						keyboard[keyconfig.r]=0;
					}
					if(event.jbutton.button==10) {
						keyboard[keyconfig.e]=0;
					}
					break;
                #endif
			}

			case SDL_QUIT:
				newstate(ST_GAME_QUIT,0,1);
				break;
		}
	}
}

int keyboard_keypressed()
{
	int i;
	for(i=0;i<SDLK_LAST;i++) {
		if(keyboard[i]) return 1;
	}
	return 0;
}

void newstate(int m, int s, int n)
{
	laststate=state;
	if(m>=0) state.mainstate=m;
	if(s>=0) state.substate=s;
	if(n>=0) state.newstate=n;
}

void *mmalloc(size_t size)
{
	void *ptr;

	ptr=malloc(size);
	if(ptr==NULL) {
		error(ERR_WARN,"can't alloc %d bytes, trying garbage collection",size);
		imglist_garbagecollect();
		ptr=malloc(size);
		if(ptr==NULL) {
			error(ERR_FATAL,"I'm sorry, but you're out of memory!");
		}
	}

	return ptr;
}

void preload_gfx()
{
	SDL_Surface *tmp;

	tmp=loadbmp("12side.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("badblocks.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("badguy.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bgpanel.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bgpanel2.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bonus_f.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bonus_p.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bonus_s.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bonus_h.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bonus_x.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss01-lo.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss01-lu.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss01-mo.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss01-mu.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss01-ro.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss01-ru.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss02_v2.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss02_v2x.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss03-lo.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss03-lu.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss03-mo.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss03-mu.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss03-ro.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("boss03-ru.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("bshoot.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("coin.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("crusher.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("cshoot.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("cube.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("ex.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("eyefo.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("fireball.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("fireball1.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("firebomb.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("font01.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("font02.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("font04.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("font05.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("font07.bmp"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("grounder.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("iris.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("homing.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("ketm.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("killray-b.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("killray-r.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("kugel.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("moon.jpg"); unloadbmp_by_surface(tmp);
	#ifdef PACC
	////////////////////////////////////////////////////////////////////
	tmp=loadbmp("pacc.png"); unloadbmp_by_surface(tmp);//added for pacc
	////////////////////////////////////////////////////////////////////
	#endif
	tmp=loadbmp("plasma.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("plasmaball.png"); unloadbmp_by_surface(tmp);
	//tmp=loadbmp("plate.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("plus1000.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("rotating_rocket.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("rwingx.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("ship-med.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("speed.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("tr_blue.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("tr_red.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("tr_green.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("target.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("weapon.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke01_1.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke02_1.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke03_1.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke01_2.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke02_2.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke03_2.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke01_3.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke02_3.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke03_3.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke01_4.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke02_4.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("wolke03_4.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("back1.jpg"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("back2.jpg"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("back3.jpg"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("back4.jpg"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("ming.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("tshoot.png"); unloadbmp_by_surface(tmp);
	tmp=loadbmp("protectball.png"); unloadbmp_by_surface(tmp);
	/* alle benoetigten Bilder in den Cache laden */
}
