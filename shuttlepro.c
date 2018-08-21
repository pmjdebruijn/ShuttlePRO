
/*

 Contour ShuttlePro v2 interface

 Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

 Copyright 2018 Albert Graef <aggraef@gmail.com>, various improvements

 Based on a version (c) 2006 Trammell Hudson <hudson@osresearch.net>

 which was in turn

 Based heavily on code by Arendt David <admin@prnet.org>

*/

#include "shuttle.h"
#if HAVE_JACK
#include "jackdriver.h"
#endif

typedef struct input_event EV;

extern int debug_regex;
extern translation *default_translation;

unsigned short jogvalue = 0xffff;
int shuttlevalue = 0xffff;
struct timeval last_shuttle;
int need_synthetic_shuttle;
Display *display;

#if HAVE_JACK
JACK_SEQ seq;
#endif
int enable_jack = 0, debug_jack = 0;

void
initdisplay(void)
{
  int event, error, major, minor;

  display = XOpenDisplay(0);
  if (!display) {
    fprintf(stderr, "unable to open X display\n");
    exit(1);
  }
  if (!XTestQueryExtension(display, &event, &error, &major, &minor)) {
    fprintf(stderr, "Xtest extensions not supported\n");
    XCloseDisplay(display);
    exit(1);
  }
}

void
send_button(unsigned int button, int press)
{
  XTestFakeButtonEvent(display, button, press ? True : False, DELAY);
}

void
send_key(KeySym key, int press)
{
  KeyCode keycode;

  if (key >= XK_Button_1 && key <= XK_Scroll_Down) {
    send_button((unsigned int)key - XK_Button_0, press);
    return;
  }
  keycode = XKeysymToKeycode(display, key);
  XTestFakeKeyEvent(display, keycode, press ? True : False, DELAY);
}

#if HAVE_JACK
// cached controller and pitch bend values
static int ccvalue[16][128];
static int pbvalue[16] =
  {8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192,
   8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192};

void
send_midi(int status, int data, int incr, int kjs, int index)
{
  if (!enable_jack) return; // MIDI support not enabled
  uint8_t msg[3];
  int chan = status & 0x0f;
  msg[0] = status;
  msg[1] = data;
  switch (status & 0xf0) {
  case 0x90:
    // send a note on (with maximum velocity) for KJS_KEY_DOWN, note off for
    // KJS_KEY_UP, or both for non-key input
    if (kjs == KJS_KEY_DOWN)
      msg[2] = 127;
    else if (kjs == KJS_KEY_UP)
      msg[2] = 0;
    else {
      msg[2] = 127;
      queue_midi(&seq, msg);
      msg[2] = 0;
    }
    break;
  case 0xb0:
    switch (kjs) {
      // for key events, send a controller value of 127 (KJS_KEY_DOWN) or 0
      // (KJS_KEY_UP), similar to how note events are handled
    case KJS_KEY_DOWN:
      msg[2] = 127;
      break;
    case KJS_KEY_UP:
      msg[2] = 0;
      break;
      // for the jog wheel, increment (index==1) or decrement (index==0) the
      // current value, clamping it to the 0..127 data byte range
    case KJS_JOG:
      if (incr) {
	// incremental controller, simply spit out a relative sign bit value
	msg[2] = index?1:65;
      } else if (index) {
	if (ccvalue[chan][data] >= 127) return;
	msg[2] = ++ccvalue[chan][data];
      } else {
	if (ccvalue[chan][data] == 0) return;
	msg[2] = --ccvalue[chan][data];
      }
      break;
      // the shuttle is handled similarly in relative mode, but scaled from
      // 0..14 to the 0..127 data byte range (approximately)
    case KJS_SHUTTLE_INCR:
      if (index) {
	if (ccvalue[chan][data] >= 127) return;
	ccvalue[chan][data] += 9;
	if (ccvalue[chan][data] > 127) ccvalue[chan][data] = 127;
      } else {
	if (ccvalue[chan][data] == 0) return;
	ccvalue[chan][data] -= 9;
	if (ccvalue[chan][data] < 0) ccvalue[chan][data] = 0;
      }
      msg[2] = ccvalue[chan][data];
      break;
      // absolute shuttle (index gives value), scaled in the same fashion
    case KJS_SHUTTLE:
      msg[2] = index * 9;
      break;
    default:
      // this can't happen
      return;
    }
    break;
  case 0xe0: {
    // pitch bends are treated similarly to a controller, but with a 14 bit
    // range (0..16383, with 8192 being the center value)
    int pbval = 0;
    switch (kjs) {
    case KJS_KEY_DOWN:
      pbval = 16383;
      break;
    case KJS_KEY_UP:
      // we use 8192 (center) as the "home" (a.k.a. "off") value, so the pitch
      // will only bend up, never down below the center value
      pbval = 8192;
      break;
    case KJS_JOG:
      if (index) {
	if (pbvalue[chan] >= 16383) return;
	pbvalue[chan] += 128;
	if (pbvalue[chan] > 16383) pbvalue[chan] = 16383;
      } else {
	if (pbvalue[chan] == 0) return;
	pbvalue[chan] -= 128;
	if (pbvalue[chan] < 0) pbvalue[chan] = 0;
      }
      pbval = pbvalue[chan];
      break;
    case KJS_SHUTTLE_INCR:
      if (index) {
	if (pbvalue[chan] >= 16383) return;
	pbvalue[chan] += 1170;
	if (pbvalue[chan] > 16383) pbvalue[chan] = 16383;
      } else {
	if (pbvalue[chan] == 0) return;
	pbvalue[chan] -= 1170;
	if (pbvalue[chan] < 0) pbvalue[chan] = 0;
      }
      pbval = pbvalue[chan];
      break;
    case KJS_SHUTTLE:
      // make sure that we get a proper center value of 8192
      pbval = (index-7)*1170;
      pbval += 8192;
      break;
    default:
      return;
    }
    // the result is a 14 bit value which gets encoded as a combination of two
    // 7 bit values which become the data bytes of the message
    msg[1] = pbval & 0x7f; // LSB (lower 7 bits)
    msg[2] = pbval >> 7;   // MSB (upper 7 bits)
    break;
  }
  case 0xc0:
    // just send the message (once for key presses)
    if (kjs == KJS_KEY_UP) return;
    break;
  default:
    return;
  }
  queue_midi(&seq, msg);
}
#endif

stroke *
fetch_stroke(translation *tr, int kjs, int index)
{
  if (tr != NULL) {
    switch (kjs) {
    case KJS_SHUTTLE:
      return tr->shuttle[index];
    case KJS_SHUTTLE_INCR:
      return tr->shuttle_incr[index];
    case KJS_JOG:
      return tr->jog[index];
    case KJS_KEY_UP:
      return tr->key_up[index];
    case KJS_KEY_DOWN:
    default:
      return tr->key_down[index];
    }
  }
  return NULL;
}

void
send_stroke_sequence(translation *tr, int kjs, int index)
{
  stroke *s;
  char key_name[100];
  int nkeys = 0;

  s = fetch_stroke(tr, kjs, index);
  if (s == NULL) {
    s = fetch_stroke(default_translation, kjs, index);
  }
  if (debug_keys && s) {
    switch (kjs) {
    case KJS_SHUTTLE:
      sprintf(key_name, "S%d", index-7);
      print_stroke_sequence(key_name, "", s);
      break;
    case KJS_SHUTTLE_INCR:
      sprintf(key_name, "I%s", (index>0)?"R":"L");
      print_stroke_sequence(key_name, "", s);
      break;
    case KJS_JOG:
      sprintf(key_name, "J%s", (index>0)?"R":"L");
      print_stroke_sequence(key_name, "", s);
      break;
    case KJS_KEY_UP:
      sprintf(key_name, "K%d", index);
      print_stroke_sequence(key_name, "U", s);
      break;
    case KJS_KEY_DOWN:
    default:
      sprintf(key_name, "K%d", index);
      print_stroke_sequence(key_name, "D", s);
      break;
    }
  }
  while (s) {
    if (s->keysym) {
      send_key(s->keysym, s->press);
      nkeys++;
#if HAVE_JACK
    } else {
      send_midi(s->status, s->data, s->incr, kjs, index);
#endif
    }
    s = s->next;
  }
  // no need to flush the display if we didn't send any keys
  if (nkeys) {
    XFlush(display);
  }
}

void
key(unsigned short code, unsigned int value, translation *tr)
{
  code -= EVENT_CODE_KEY1;

  if (code <= NUM_KEYS) {
    send_stroke_sequence(tr, value ? KJS_KEY_DOWN : KJS_KEY_UP, code);
  } else {
    fprintf(stderr, "key(%d, %d) out of range\n", code + EVENT_CODE_KEY1, value);
  }
}


void
shuttle(int value, translation *tr)
{
  if (value < -7 || value > 7) {
    fprintf(stderr, "shuttle(%d) out of range\n", value);
  } else {
    gettimeofday(&last_shuttle, 0);
    need_synthetic_shuttle = value != 0;
    if( value != shuttlevalue ) {
      if (shuttlevalue < -7 || shuttlevalue > 7) {
	// not yet initialized, assume 0
	shuttlevalue = 0;
      }
      int direction = (value < shuttlevalue) ? -1 : 1;
      int index = direction > 0 ? 1 : 0, val = value+7;
      if (fetch_stroke(tr, KJS_SHUTTLE_INCR, index)) {
	while (shuttlevalue != value) {
	  send_stroke_sequence(tr, KJS_SHUTTLE_INCR, index);
	  shuttlevalue += direction;
	}
      } else
	shuttlevalue = value;
      send_stroke_sequence(tr, KJS_SHUTTLE, val);
    }
  }
}

// Due to a bug (?) in the way Linux HID handles the ShuttlePro, the
// center position is not reported for the shuttle wheel.  Instead,
// a jog event is generated immediately when it returns.  We check to
// see if the time since the last shuttle was more than a few ms ago
// and generate a shuttle of 0 if so.
//
// Note, this fails if jogvalue happens to be 0, as we don't see that
// event either!
void
jog(unsigned int value, translation *tr)
{
  int direction;
  struct timeval now;
  struct timeval delta;

  // We should generate a synthetic event for the shuttle going
  // to the home position if we have not seen one recently
  if (need_synthetic_shuttle) {
    gettimeofday( &now, 0 );
    timersub( &now, &last_shuttle, &delta );

    if (delta.tv_sec >= 1 || delta.tv_usec >= 5000) {
      shuttle(0, tr);
      need_synthetic_shuttle = 0;
    }
  }

  if (jogvalue != 0xffff) {
    value = value & 0xff;
    direction = ((value - jogvalue) & 0x80) ? -1 : 1;
    while (jogvalue != value) {
      // driver fails to send an event when jogvalue == 0
      if (jogvalue != 0) {
	send_stroke_sequence(tr, KJS_JOG, direction > 0 ? 1 : 0);
      }
      jogvalue = (jogvalue + direction) & 0xff;
    }
  }
  jogvalue = value;
}

void
jogshuttle(unsigned short code, unsigned int value, translation *tr)
{
  switch (code) {
  case EVENT_CODE_JOG:
    jog(value, tr);
    break;
  case EVENT_CODE_SHUTTLE:
    shuttle(value, tr);
    break;
  default:
    fprintf(stderr, "jogshuttle(%d, %d) invalid code\n", code, value);
    break;
  }
}

char *
get_window_name(Window win)
{
  Atom prop = XInternAtom(display, "WM_NAME", False);
  Atom type;
  int form;
  unsigned long remain, len;
  unsigned char *list;

  if (XGetWindowProperty(display, win, prop, 0, 1024, False,
			 AnyPropertyType, &type, &form, &len, &remain,
			 &list) != Success) {
    fprintf(stderr, "XGetWindowProperty failed for window 0x%x\n", (int)win);
    return NULL;
  }

  return (char*)list;
}

char *
get_window_class(Window win)
{
  Atom prop = XInternAtom(display, "WM_CLASS", False);
  Atom type;
  int form;
  unsigned long remain, len;
  unsigned char *list;

  if (XGetWindowProperty(display, win, prop, 0, 1024, False,
			 AnyPropertyType, &type, &form, &len, &remain,
			 &list) != Success) {
    fprintf(stderr, "XGetWindowProperty failed for window 0x%x\n", (int)win);
    return NULL;
  }

  return (char*)list;
}

char *
walk_window_tree(Window win, char **window_class)
{
  char *window_name;
  Window root = 0;
  Window parent;
  Window *children;
  unsigned int nchildren;

  while (win != root) {
    window_name = get_window_name(win);
    if (window_name != NULL) {
      *window_class = get_window_class(win);
      return window_name;
    }
    if (XQueryTree(display, win, &root, &parent, &children, &nchildren)) {
      win = parent;
      XFree(children);
    } else {
      fprintf(stderr, "XQueryTree failed for window 0x%x\n", (int)win);
      return NULL;
    }
  }
  return NULL;
}

static Window last_focused_window = 0;
static translation *last_window_translation = NULL;

translation *
get_focused_window_translation()
{
  Window focus;
  int revert_to;
  char *window_name = NULL, *window_class = NULL;
  char *name;

  XGetInputFocus(display, &focus, &revert_to);
  if (focus != last_focused_window) {
    last_focused_window = focus;
    window_name = walk_window_tree(focus, &window_class);
    if (window_name == NULL) {
      name = "-- Unlabeled Window --";
    } else {
      name = window_name;
    }
    last_window_translation = get_translation(name, window_class);
    if (debug_regex) {
      if (last_window_translation != NULL) {
	printf("translation: %s for %s (class %s)\n",
	       last_window_translation->name, name, window_class);
      } else {
	printf("no translation found for %s (class %s)\n", name, window_class);
      }
    }
    if (window_name != NULL) {
      XFree(window_name);
    }
    if (window_class != NULL) {
      XFree(window_class);
    }
  }
  return last_window_translation;
}

void
handle_event(EV ev)
{
  translation *tr = get_focused_window_translation();
  
  //fprintf(stderr, "event: (%d, %d, 0x%x)\n", ev.type, ev.code, ev.value);
  if (tr != NULL) {
    switch (ev.type) {
    case EVENT_TYPE_DONE:
    case EVENT_TYPE_ACTIVE_KEY:
      break;
    case EVENT_TYPE_KEY:
      key(ev.code, ev.value, tr);
      break;
    case EVENT_TYPE_JOGSHUTTLE:
      jogshuttle(ev.code, ev.value, tr);
      break;
    default:
      fprintf(stderr, "handle_event() invalid type code\n");
      break;
    }
  }
}

void help(char *progname)
{
  fprintf(stderr, "Usage: %s [-h] [-o] [-j name] [-r rcfile] [-d[rskj]] [device]\n", progname);
  fprintf(stderr, "-h print this message\n");
  fprintf(stderr, "-o enable MIDI output\n");
  fprintf(stderr, "-j jack client name (default: shuttlepro)\n");
  fprintf(stderr, "-r config file name (default: SHUTTLE_CONFIG_FILE variable or ~/.shuttlerc)\n");
  fprintf(stderr, "-d debug (r = regex, s = strokes, k = keys, j = jack; default: all)\n");
  fprintf(stderr, "device, if specified, is the name of the shuttle device to open.\n");
  fprintf(stderr, "Otherwise the program will try to find a suitable device on its own.\n");
}

void jack_warning(char *progname)
{
  fprintf(stderr, "%s: Warning: this version was compiled without Jack support\n", progname);
  fprintf(stderr, "Try recompiling the program with Jack installed to enable this option.\n");
}

#include <glob.h>

// Helper functions to process the command line, so that we can pass it to
// Jack session management.

static char *command_line;
static size_t len;

static void add_command(char *arg)
{
  char *a = arg;
  // Do some simplistic quoting if the argument contains blanks. This won't do
  // the right thing if the argument also contains quotes. Oh well.
  if ((strchr(a, ' ') || strchr(a, '\t')) && !strchr(a, '"')) {
    a = malloc(strlen(arg)+3);
    sprintf(a, "\"%s\"", arg);
  }
  if (!command_line) {
    len = strlen(a);
    command_line = malloc(len+1);
    strcpy(command_line, a);
  } else {
    size_t l = strlen(a)+1;
    command_line = realloc(command_line, len+l+1);
    command_line[len] = ' ';
    strcpy(command_line+len+1, a);
    len += l;
  }
  if (a != arg) free(a);
}

static char *absolute_path(char *name)
{
  if (*name == '/') {
    return name;
  } else {
    // This is a relative pathname, we turn it into a canonicalized absolute
    // path.  NOTE: This requires glibc. We should probably rewrite this code
    // to be more portable.
    char *pwd = getcwd(NULL, 0);
    if (!pwd) {
      perror("getcwd");
      return name;
    } else {
      char *path = malloc(strlen(pwd)+strlen(name)+2);
      static char abspath[PATH_MAX];
      sprintf(path, "%s/%s", pwd, name);
      realpath(path, abspath);
      free(path); free(pwd);
      return abspath;
    }
  }
}

int
main(int argc, char **argv)
{
  EV ev;
  int nread;
  char *dev_name;
#if HAVE_JACK
  char *client_name = "shuttlepro";
#endif
  int fd;
  int first_time = 1;
  int opt;

  // Start recording the command line to be passed to Jack session management.
  add_command(argv[0]);

  while ((opt = getopt(argc, argv, "hod::j:r:")) != -1) {
    switch (opt) {
    case 'h':
      help(argv[0]);
      exit(0);
    case 'o':
#if HAVE_JACK
      enable_jack = 1;
      add_command("-o");
#else
      jack_warning(argv[0]);
#endif
      break;
    case 'd':
      if (optarg && *optarg) {
	const char *a = optarg; char buf[100];
	snprintf(buf, 100, "-d%s", optarg);
	add_command(buf);
	while (*a) {
	  switch (*a) {
	  case 'r':
	    default_debug_regex = 1;
	    break;
	  case 's':
	    default_debug_strokes = 1;
	    break;
	  case 'k':
	    default_debug_keys = 1;
	    break;
	  case 'j':
	    debug_jack = 1;
	    break;
	  default:
	    fprintf(stderr, "%s: unknown debugging option (-d), must be r, s, k or j\n", argv[0]);
	    fprintf(stderr, "Try -h for help.\n");
	    exit(1);
	  }
	  ++a;
	}
      } else {
	default_debug_regex = default_debug_strokes = default_debug_keys = 1;
	debug_jack = 1;
	add_command("-d");
      }
      break;
    case 'j':
#if HAVE_JACK
      client_name = optarg;
      add_command("-j");
      add_command(optarg);
#else
      jack_warning(argv[0]);
#endif
      break;
    case 'r':
      config_file_name = optarg;
      add_command("-r");
      // We need to convert this to an absolute pathname for Jack session
      // management.
      add_command(absolute_path(optarg));
      break;
    default:
      fprintf(stderr, "Try -h for help.\n");
      exit(1);
    }
  }

  if (optind+1 < argc) {
    help(argv[0]);
    exit(1);
  }

  if (optind >= argc) {
    // Try to find a suitable device.
    glob_t globbuf;
    if (glob("/dev/input/by-id/usb-Contour_Design_Shuttle*-event-if*",
	     0, NULL, &globbuf)) {
      fprintf(stderr, "%s: found no suitable shuttle device\n", argv[0]);
      fprintf(stderr, "Please make sure that your shuttle device is connected.\n");
      fprintf(stderr, "You can also specify the device name on the command line.\n");
      fprintf(stderr, "Try -h for help.\n");
      exit(1);
    } else {
      dev_name = globbuf.gl_pathv[0];
      fprintf(stderr, "%s: found shuttle device:\n%s\n", argv[0], dev_name);
    }
  } else {
    dev_name = argv[optind];
  }

  initdisplay();

#if HAVE_JACK
  if (command_line) jack_command_line = command_line;
  if (enable_jack) {
    seq.client_name = client_name;
    if (!init_jack(&seq, debug_jack)) enable_jack = 0;
  }
#endif

  while (1) {
    fd = open(dev_name, O_RDONLY);
    if (fd < 0) {
      perror(dev_name);
      if (first_time) {
	exit(1);
      }
    } else {
      // Flag it as exclusive access
      if(ioctl( fd, EVIOCGRAB, 1 ) < 0) {
	perror( "evgrab ioctl" );
      } else {
	first_time = 0;
	while (1) {
	  // Bail out if Jack asked us to quit.
	  if (jack_quit) exit(0);
	  nread = read(fd, &ev, sizeof(ev));
	  if (nread == sizeof(ev)) {
	    handle_event(ev);
	    // Make sure that debugging output gets flushed every once in a
	    // while (may be buffered when we're running inside a QjackCtl
	    // session).
	    if (debug_regex || debug_strokes || debug_keys)
	      fflush(stdout);
	  } else {
	    if (nread < 0) {
	      perror("read event");
	      break;
	    } else {
	      fprintf(stderr, "short read: %d\n", nread);
	      break;
	    }
	  }
	}
      }
    }
    close(fd);
    sleep(1);
  }
}
