
/*

  Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

  Copyright 2018 Albert Graef <aggraef@gmail.com>, various improvements

  Read and process the configuration file ~/.shuttlepro

  Lines starting with # are comments.

  Sequence of sections defining translation classes, each section is:

  [name] regex
  K<1..15> output
  S<-7..7> output
  I<LR> output
  J<LR> output

  When focus is on a window whose class or title matches regex, the
  following translation class is in effect.  An empty regex for the last
  class will always match, allowing default translations.  Any output
  sequences not bound in a matched section will be loaded from the
  default section if they are bound there.

  Each "[name] regex" line introduces the list of key and shuttle
  translations for the named translation class.  The name is only used
  for debugging output, and needn't be unique.  The following lines
  with K, S, I and J labels indicate what output should be produced for
  the given keypress, shuttle position, shuttle direction, or jog direction.

  output is a sequence of one or more key codes with optional up/down
  indicators, or strings of printable characters enclosed in double
  quotes, separated by whitespace.  Sequences bound to keys may have
  separate press and release sequences, separated by the word RELEASE.

  Examples:

  K1 "qwer"
  K2 XK_Right
  K3 XK_Alt_L/D XK_Right
  K4 "V" XK_Left XK_Page_Up "v"
  K5 XK_Alt_L/D "v" XK_Alt_L/U "x" RELEASE "q"

  Any keycode can be followed by an optional /D, /U, or /H, indicating
  that the key is just going down (without being released), going up,
  or going down and being held until the shuttlepro key is released.

  So, in general, modifier key codes will be followed by /D, and
  precede the keycodes they are intended to modify.  If a sequence
  requires different sets of modifiers for different keycodes, /U can
  be used to release a modifier that was previously pressed with /D.

  At the end of shuttle and jog sequences, all down keys will be
  released.

  Keypresses translate to separate press and release sequences.

  At the end of the press sequence for key sequences, all down keys
  marked by /D will be released, and the last key not marked by /D,
  /U, or /H will remain pressed.  The release sequence will begin by
  releasing the last held key.  If keys are to be pressed as part of
  the release sequence, then any keys marked with /D will be repressed
  before continuing the sequence.  Keycodes marked with /H remain held
  between the press and release sequences.

  JACK MIDI support (added by aggraef@gmail.com Fri Aug 3 11:01:32 CEST 2018):

  The MIDI output option is useful if you want to be able to send control
  messages from a Shuttle device to any kind of MIDI-capable program, such as
  a synthesizer or a DAW. Also, if you put the MIDI translations into a
  special "MIDI" default section of the shuttlerc file, then the target
  application will be able to receive data from the device no matter which
  window has the keyboard focus. (This special "MIDI" section will only be
  active if Jack MIDI support is actually enabled with the -o option, see
  below. This allows you to have another default section after the MIDI
  section for non-MIDI operation.)

  To enable MIDI output, add the -o option when invoking the program (also,
  you can set the Jack client name with the -j option, and the -dj option can
  be used to get verbose output from Jack if needed). This causes a Jack
  client (named "shuttlepro" by default) with a single MIDI output port to be
  created, and will also start up Jack if it is not already running. Any MIDI
  messages in the translations will be sent on that port. You can then use any
  Jack patchbay such as qjackctl to connect the output to any other Jack MIDI
  client (use the a2jmidid program to connect to non-Jack ALSA MIDI
  applications).

 */

#include "shuttle.h"

int default_debug_regex = 0;
int default_debug_strokes = 0;
int default_debug_keys = 0;

int debug_regex = 0;
int debug_strokes = 0;
int debug_keys = 0;

int midi_octave = 0;

char *
allocate(size_t len)
{
  char *ret = (char *)malloc(len);
  if (ret == NULL) {
    fprintf(stderr, "Out of memory!\n");
    exit(1);
  }
  return ret;
}

char *
alloc_strcat(char *a, char *b)
{
  size_t len = 0;
  char *result;

  if (a != NULL) {
    len += strlen(a);
  }
  if (b != NULL) {
    len += strlen(b);
  }
  result = allocate(len+1);
  result[0] = '\0';
  if (a != NULL) {
    strcpy(result, a);
  }
  if (b != NULL) {
    strcat(result, b);
  }
  return result;
}

static char *read_line_buffer = NULL;
static int read_line_buffer_length = 0;

#define BUF_GROWTH_STEP 1024


// read a line of text from the given file into a managed buffer.
// returns a partial line at EOF if the file does not end with \n.
// exits with error message on read error.
char *
read_line(FILE *f, char *name)
{
  int pos = 0;
  char *new_buffer;
  int new_buffer_length;

  if (read_line_buffer == NULL) {
    read_line_buffer_length = BUF_GROWTH_STEP;
    read_line_buffer = allocate(read_line_buffer_length);
    read_line_buffer[0] = '\0';
  }

  while (1) {
    read_line_buffer[read_line_buffer_length-1] = '\377';
    if (fgets(read_line_buffer+pos, read_line_buffer_length-pos, f) == NULL) {
      if (feof(f)) {
	if (pos > 0) {
	  // partial line at EOF
	  return read_line_buffer;
	} else {
	  return NULL;
	}
      }
      perror(name);
      exit(1);
    }
    if (read_line_buffer[read_line_buffer_length-1] != '\0') {
      return read_line_buffer;
    }
    if (read_line_buffer[read_line_buffer_length-2] == '\n') {
      return read_line_buffer;
    }
    new_buffer_length = read_line_buffer_length + BUF_GROWTH_STEP;
    new_buffer = allocate(new_buffer_length);
    memcpy(new_buffer, read_line_buffer, read_line_buffer_length);
    free(read_line_buffer);
    pos = read_line_buffer_length-1;
    read_line_buffer = new_buffer;
    read_line_buffer_length = new_buffer_length;
  }
}

static translation *first_translation_section = NULL;
static translation *last_translation_section = NULL;

translation *default_translation;

translation *
new_translation_section(char *name, char *regex)
{
  translation *ret = (translation *)allocate(sizeof(translation));
  int err;
  int i;

  if (debug_strokes) {
    printf("------------------------\n[%s] %s\n\n", name, regex);
  }
  ret->next = NULL;
  ret->name = alloc_strcat(name, NULL);
  if (regex == NULL || *regex == '\0') {
    ret->is_default = 1;
    default_translation = ret;
  } else {
    ret->is_default = 0;
    err = regcomp(&ret->regex, regex, REG_NOSUB);
    if (err != 0) {
      regerror(err, &ret->regex, read_line_buffer, read_line_buffer_length);
      fprintf(stderr, "error compiling regex for [%s]: %s\n", name, read_line_buffer);
      regfree(&ret->regex);
      free(ret->name);
      free(ret);
      return NULL;
    }
  }
  for (i=0; i<NUM_KEYS; i++) {
    ret->key_down[i] = NULL;
    ret->key_up[i] = NULL;
  }
  for (i=0; i<NUM_SHUTTLES; i++) {
    ret->shuttle[i] = NULL;
  }
  for (i=0; i<NUM_SHUTTLE_INCRS; i++) {
    ret->shuttle_incr[i] = NULL;
  }
  for (i=0; i<NUM_JOGS; i++) {
    ret->jog[i] = NULL;
  }
  if (first_translation_section == NULL) {
    first_translation_section = ret;
    last_translation_section = ret;
  } else {
    last_translation_section->next = ret;
    last_translation_section = ret;
  }
  return ret;
}

void
free_strokes(stroke *s)
{
  stroke *next;
  while (s != NULL) {
    next = s->next;
    free(s);
    s = next;
  }
}

void
free_translation_section(translation *tr)
{
  int i;

  if (tr != NULL) {
    free(tr->name);
    if (!tr->is_default) {
      regfree(&tr->regex);
    }
    for (i=0; i<NUM_KEYS; i++) {
      free_strokes(tr->key_down[i]);
      free_strokes(tr->key_up[i]);
    }
    for (i=0; i<NUM_SHUTTLES; i++) {
      free_strokes(tr->shuttle[i]);
    }
    for (i=0; i<NUM_SHUTTLE_INCRS; i++) {
      free_strokes(tr->shuttle_incr[i]);
    }
    for (i=0; i<NUM_JOGS; i++) {
      free_strokes(tr->jog[i]);
    }
    free(tr);
  }
}

void
free_all_translations(void)
{
  translation *tr = first_translation_section;
  translation *next;

  while (tr != NULL) {
    next = tr->next;
    free_translation_section(tr);
    tr = next;
  }
  first_translation_section = NULL;
  last_translation_section = NULL;
}

char *config_file_name = NULL;
static time_t config_file_modification_time;

static char *token_src = NULL;

// similar to strtok, but it tells us what delimiter was found at the
// end of the token, handles double quoted strings specially, and
// hardcodes the delimiter set.
char *
token(char *src, char *delim_found)
{
  char *delims = " \t\n/\"";
  char *d;
  char *token_start;

  if (src == NULL) {
    src = token_src;
  }
  if (src == NULL) {
    *delim_found = '\0';
    return NULL;
  }
  token_start = src;
  while (*src) {
    d = delims;
    while (*d && *src != *d) {
      d++;
    }
    if (*d) {
      if (src == token_start) {
	src++;
	token_start = src;
	if (*d == '"') {
	  while (*src && *src != '"' && *src != '\n') {
	    src++;
	  }
	} else {
	  continue;
	}
      }
      *delim_found = *d;
      if (*src) {
	*src = '\0';
	token_src = src+1;
      } else {
	token_src = NULL;
      }
      return token_start;
    }
    src++;
  }
  token_src = NULL;
  *delim_found = '\0';
  if (src == token_start) {
    return NULL;
  }
  return token_start;
}

typedef struct _keysymmapping {
  char *str;
  KeySym sym;
} keysymmapping;

static keysymmapping key_sym_mapping[] = {
#include "keys.h"
  { "XK_Button_1", XK_Button_1 },
  { "XK_Button_2", XK_Button_2 },
  { "XK_Button_3", XK_Button_3 },
  { "XK_Scroll_Up", XK_Scroll_Up },
  { "XK_Scroll_Down", XK_Scroll_Down },
  { NULL, 0 }
};

KeySym
string_to_KeySym(char *str)
{
  size_t len = strlen(str) + 1;
  int i = 0;

  while (key_sym_mapping[i].str != NULL) {
    if (!strncmp(str, key_sym_mapping[i].str, len)) {
      return key_sym_mapping[i].sym;
    }
    i++;
  }
  return 0;
}

char *
KeySym_to_string(KeySym ks)
{
  int i = 0;

  while (key_sym_mapping[i].sym != 0) {
    if (key_sym_mapping[i].sym == ks) {
      return key_sym_mapping[i].str;
    }
    i++;
  }
  return NULL;
}

static char *note_names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

void
print_stroke(stroke *s)
{
  char *str;

  if (s != NULL) {
    if (s->keysym) {
      str = KeySym_to_string(s->keysym);
      if (str == NULL) {
	printf("0x%x", (int)s->keysym);
	str = "???";
      }
      printf("%s/%c ", str, s->press ? 'D' : 'U');
    } else {
      int status = s->status & 0xf0;
      int channel = (s->status & 0x0f) + 1;
      switch (status) {
      case 0x90:
	printf("%s%d-%d ", note_names[s->data % 12],
	       s->data / 12 + midi_octave, channel);
	break;
      case 0xb0:
	printf("CC%d-%d%s ", s->data, channel, s->incr?"~":"");
	break;
      case 0xc0:
	printf("PC%d-%d ", s->data, channel);
	break;
      case 0xe0:
	printf("PB-%d ", channel);
	break;
      default: // this can't happen
	break;
      }
    }
  }
}

void
print_stroke_sequence(char *name, char *up_or_down, stroke *s)
{
  if (up_or_down && *up_or_down)
    printf("%s[%s]: ", name, up_or_down);
  else
    printf("%s: ", name);
  while (s) {
    print_stroke(s);
    s = s->next;
  }
  printf("\n");
}

stroke **first_stroke;
stroke *last_stroke;
stroke **press_first_stroke;
stroke **release_first_stroke;
int is_keystroke;
int is_midi;
char *current_translation;
char *key_name;
int first_release_stroke; // is this the first stroke of a release?
KeySym regular_key_down;

#define NUM_MODIFIERS 64

stroke modifiers_down[NUM_MODIFIERS];
int modifier_count;

int midi_channel;

void
append_stroke(KeySym sym, int press)
{
  stroke *s = (stroke *)allocate(sizeof(stroke));

  s->next = NULL;
  s->keysym = sym;
  s->press = press;
  s->status = s->data = s->incr = s->dirty = 0;
  if (*first_stroke) {
    last_stroke->next = s;
  } else {
    *first_stroke = s;
  }
  last_stroke = s;
}

void
append_midi(int status, int data, int incr)
{
  stroke *s = (stroke *)allocate(sizeof(stroke));

  s->next = NULL;
  s->keysym = 0;
  s->press = 0;
  s->status = status;
  s->data = data;
  s->incr = incr;
  // if this is a keystroke event, for all messages but program change (which
  // has no "on" and "off" states), mark the event as "dirty" so that the
  // corresponding "off" event gets added later to the "release" strokes
  s->dirty = is_keystroke && ((status&0xf0) != 0xc0);
  if (*first_stroke) {
    last_stroke->next = s;
  } else {
    *first_stroke = s;
  }
  last_stroke = s;
  is_midi = 1;
}

// s->press values in modifiers_down:
// PRESS -> down
// HOLD -> held
// PRESS_RELEASE -> released, but to be re-pressed if necessary
// RELEASE -> up

void
mark_as_down(KeySym sym, int hold)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].keysym == sym) {
      modifiers_down[i].press = hold ? HOLD : PRESS;
      return;
    }
  }
  if (modifier_count > NUM_MODIFIERS) {
    fprintf(stderr, "too many modifiers down in [%s]%s\n", current_translation, key_name);
    return;
  }
  modifiers_down[modifier_count].keysym = sym;
  modifiers_down[modifier_count].press = hold ? HOLD : PRESS;
  modifier_count++;
}

void
mark_as_up(KeySym sym)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].keysym == sym) {
      modifiers_down[i].press = RELEASE;
      return;
    }
  }
}

void
release_modifiers(int allkeys)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].press == PRESS) {
      append_stroke(modifiers_down[i].keysym, 0);
      modifiers_down[i].press = PRESS_RELEASE;
    } else if (allkeys && modifiers_down[i].press == HOLD) {
      append_stroke(modifiers_down[i].keysym, 0);
      modifiers_down[i].press = RELEASE;
    }
  }
}

void
re_press_temp_modifiers(void)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].press == PRESS_RELEASE) {
      append_stroke(modifiers_down[i].keysym, 1);
      modifiers_down[i].press = PRESS;
    }
  }
}

int
start_translation(translation *tr, char *which_key)
{
  char c;
  int k;
  int n;

  //printf("start_translation(%s)\n", which_key);

  if (tr == NULL) {
    fprintf(stderr, "need to start translation section before defining key: %s\n", which_key);
    return 1;
  }
  current_translation = tr->name;
  key_name = which_key;
  is_keystroke = is_midi = 0;
  first_release_stroke = 0;
  regular_key_down = 0;
  modifier_count = 0;
  midi_channel = 0;
  if (tolower(which_key[0]) == 'j' &&
      (tolower(which_key[1]) == 'l' || tolower(which_key[1]) == 'r') &&
      which_key[2] == '\0') {
    // JL, JR
    k = tolower(which_key[1]) == 'l' ? 0 : 1;
    first_stroke = &(tr->jog[k]);
  } else if (tolower(which_key[0]) == 'i' &&
      (tolower(which_key[1]) == 'l' || tolower(which_key[1]) == 'r') &&
      which_key[2] == '\0') {
    // IL, IR
    k = tolower(which_key[1]) == 'l' ? 0 : 1;
    first_stroke = &(tr->shuttle_incr[k]);
  } else {
    n = 0;
    sscanf(which_key, "%c%d%n", &c, &k, &n);
    if (n != (int)strlen(which_key)) {
      fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
      return 1;
    }
    switch (c) {
    case 'k':
    case 'K':
      // K1 .. K15
      k = k - 1;
      if (k < 0 || k >= NUM_KEYS) {
	fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
	return 1;
      }
      first_stroke = &(tr->key_down[k]);
      release_first_stroke = &(tr->key_up[k]);
      is_keystroke = 1;
      break;
    case 's':
    case 'S':
      // S-7 .. S7
      if (k < -7 || k > 7) {
	fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
	return 1;
      }
      first_stroke = &(tr->shuttle[k+7]);
      break;
    default:
      fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
      return 1;
    }
  }
  if (*first_stroke != NULL) {
    fprintf(stderr, "can't redefine key: [%s]%s\n", current_translation, which_key);
    return 1;
  }
  press_first_stroke = first_stroke;
  return 0;
}

void
add_keysym(KeySym sym, int press_release)
{
  //printf("add_keysym(0x%x, %d)\n", (int)sym, press_release);
  switch (press_release) {
  case PRESS:
    append_stroke(sym, 1);
    mark_as_down(sym, 0);
    break;
  case RELEASE:
    append_stroke(sym, 0);
    mark_as_up(sym);
    break;
  case HOLD:
    append_stroke(sym, 1);
    mark_as_down(sym, 1);
    break;
  case PRESS_RELEASE:
  default:
    if (first_release_stroke) {
      re_press_temp_modifiers();
    }
    if (regular_key_down != 0) {
      append_stroke(regular_key_down, 0);
    }
    append_stroke(sym, 1);
    regular_key_down = sym;
    first_release_stroke = 0;
    break;
  }
}

void
add_release(int all_keys)
{
  //printf("add_release(%d)\n", all_keys);
  release_modifiers(all_keys);
  if (!all_keys) {
    first_stroke = release_first_stroke;
    if (is_midi) {
      // walk the list of "press" strokes, find all "dirty" (as yet unhandled)
      // MIDI events in there and add them to the "release" strokes
      stroke *s = *press_first_stroke;
      while (s) {
	if (!s->keysym && s->dirty) {
	  append_midi(s->status, s->data, s->incr);
	  s->dirty = 0;
	}
	s = s->next;
      }
    }
  }
  if (regular_key_down != 0) {
    append_stroke(regular_key_down, 0);
  }
  regular_key_down = 0;
  first_release_stroke = 1;
}

void
add_keystroke(char *keySymName, int press_release)
{
  KeySym sym;

  if (is_keystroke && !strncmp(keySymName, "RELEASE", 8)) {
    add_release(0);
    return;
  }
  sym = string_to_KeySym(keySymName);
  if (sym != 0) {
    add_keysym(sym, press_release);
  } else {
    fprintf(stderr, "unrecognized keysym: %s\n", keySymName);
  }
}

void
add_string(char *str)
{
  while (str && *str) {
    if (*str >= ' ' && *str <= '~') {
      add_keysym((KeySym)(*str), PRESS_RELEASE);
    }
    str++;
  }
}

/* Parser for the MIDI message syntax. The syntax we actually parse here is:

   tok  ::= ( note | msg ) [number] [ "-" number] [ "~" ]
   note ::= ( "a" | ... | "g" ) [ "#" | "b" ]
   msg  ::= "ch" | "pb" | "pc" | "cc"

   Numbers are always in decimal. The meaning of the first number depends on
   the context (octave number for notes, the actual data byte for other
   messages). If present, the suffix with the second number (after the dash)
   denotes the MIDI channel, otherwise the default MIDI channel is used. Also,
   for "cc" messages the "~" suffix denotes a special kind of "incremental"
   (endless rotary) control whose values are encoded as relative changes in
   "sign bit format". Note that not all combinations are possible -- "pb" is
   *not* followed by a data byte, only "cc" may have the "~" suffix on it, and
   "ch" may *not* have a channel number suffix on it. (In fact, "ch" is no
   real MIDI message at all; it just sets the default MIDI channel for
   subsequent messages.) */

static int note_number(char c, char b, int k)
{
  c = tolower(c); b = tolower(b);
  if (c < 'a' || c > 'g' || (b && b != '#' && b != 'b'))
    return -1; // either wrong note name or invalid accidental
  else {
    static int note_numbers[] = { -3, -1, 0, 2, 4, 5, 7 };
    int m = note_numbers[c-'a'], a = (b=='#')?1:(b=='b')?-1:0;
    if (m<0) k++;
    return m + a + 12*k;
  }
}

int
parse_midi(char *tok, char *s, int *status, int *data, int *incr)
{
  char *p = tok, *t;
  int n, m = -1, k = midi_channel;
  s[0] = 0;
  while (*p && !isdigit(*p)) p++;
  if (p == tok || p-tok > 10) return 0; // no valid token
  // the token by itself
  strncpy(s, tok, p-tok); s[p-tok] = 0;
  // normalize to lowercase
  for (t = s; *t; t++) *t = tolower(*t);
  // octave number or data byte (not permitted with 'pb', otherwise required)
  if (isdigit(*p) && sscanf(p, "%d%n", &m, &n) == 1) {
    if (strcmp(s, "pb") == 0) return 0;
    p += n;
  } else if (strcmp(s, "pb")) {
    return 0;
  }
  if (*p == '-') {
    // suffix with MIDI channel (not permitted with 'ch')
    if (strcmp(s, "ch") == 0) return 0;
    if (sscanf(++p, "%d%n", &k, &n) == 1) {
      // check that it is a valid channel number
      if (k < 1 || k > 16) return 0;
      k--; // actual MIDI channel in the range 0..15
      p += n;
    } else {
      return 0;
    }
  }
  if (*p == '~') {
    // incremental bit (only valid for 'cc')
    if (strcmp(s, "cc")) return 0;
    *incr = 1;
    p++;
  } else {
    *incr = 0;
  }
  // check for trailing garbage
  if (*p) return 0;
  if (strcmp(s, "ch") == 0) {
    // we return a bogus status of 0 here, along with the MIDI channel in the
    // data byte; also check that the MIDI channel is in the proper range
    if (m < 1 || m > 16) return 0;
    *status = 0; *data = m-1;
    return 1;
  } else if (strcmp(s, "pb") == 0) {
    // pitch bend, no data byte
    *status = 0xe0 | k; *data = 0;
    return 1;
  } else if (strcmp(s, "pc") == 0) {
    // program change
    if (m < 0 || m > 127) return 0;
    *status = 0xc0 | k; *data = m;
    return 1;
  } else if (strcmp(s, "cc") == 0) {
    // control change
    if (m < 0 || m > 127) return 0;
    *status = 0xb0 | k; *data = m;
    return 1;
  } else {
    // we must be looking at a MIDI note here, with m denoting the octave
    // number; first character is the note name (must be a..g); optionally,
    // the second character may denote an accidental (# or b)
    n = note_number(s[0], s[1], m - midi_octave);
    if (n < 0 || n > 127) return 0;
    *status = 0x90 | k; *data = n;
    return 1;
  }
}

void
add_midi(char *tok)
{
  int status, data, incr;
  char buf[100];
  if (parse_midi(tok, buf, &status, &data, &incr)) {
    if (status == 0) {
      // 'ch' token; this doesn't actually generate any output, it just sets
      // the default MIDI channel
      midi_channel = data;
    } else {
      append_midi(status, data, incr);
    }
  } else {
    // inspect the token that was actually recognized (if any) to give some
    // useful error message here
    if (strcmp(buf, "ch"))
      fprintf(stderr, "unrecognized keysym: %s\n", tok);
    else
      fprintf(stderr, "invalid MIDI channel: %s\n", tok);
  }
}

void
finish_translation(void)
{
  //printf("finish_translation()\n");
  if (is_keystroke) {
    add_release(0);
  }
  add_release(1);
  if (debug_strokes) {
    if (is_keystroke) {
      print_stroke_sequence(key_name, "D", *press_first_stroke);
      print_stroke_sequence(key_name, "U", *release_first_stroke);
    } else {
      print_stroke_sequence(key_name, "", *first_stroke);
    }
    printf("\n");
  }
}

int
read_config_file(void)
{
  struct stat buf;
  char *home;
  char *line;
  char *s;
  char *name = NULL;
  char *regex;
  char *tok;
  char *which_key;
  char *updown;
  char delim;
  translation *tr = NULL;
  FILE *f;
  int config_file_default = 0;

  if (config_file_name == NULL) {
    config_file_name = getenv("SHUTTLE_CONFIG_FILE");
    if (config_file_name == NULL) {
      home = getenv("HOME");
      config_file_name = alloc_strcat(home, "/.shuttlerc");
      config_file_default = 1;
    } else {
      config_file_name = alloc_strcat(config_file_name, NULL);
    }
    config_file_modification_time = 0;
  }
  if (stat(config_file_name, &buf) < 0) {
    // AG: Fall back to the system-wide configuration file.
    if (!config_file_default) perror(config_file_name);
    config_file_name = "/etc/shuttlerc";
    config_file_modification_time = 0;
  }
  if (stat(config_file_name, &buf) < 0) {
    perror(config_file_name);
    return 0;
  }
  if (buf.st_mtime == 0) {
    buf.st_mtime = 1;
  }
  if (buf.st_mtime > config_file_modification_time) {
    config_file_modification_time = buf.st_mtime;
    if (default_debug_regex || default_debug_strokes || default_debug_keys) {
      printf("Loading configuration: %s\n", config_file_name);
    }

    f = fopen(config_file_name, "r");
    if (f == NULL) {
      perror(config_file_name);
      return 0;
    }

    free_all_translations();
    debug_regex = default_debug_regex;
    debug_strokes = default_debug_strokes;
    debug_keys = default_debug_keys;
    midi_octave = 0;

    while ((line=read_line(f, config_file_name)) != NULL) {
      //printf("line: %s", line);
      
      s = line;
      while (*s && isspace(*s)) {
	s++;
      }
      if (*s == '#') {
	continue;
      }
      if (*s == '[') {
	//  [name] regex\n
	name = ++s;
	while (*s && *s != ']') {
	  s++;
	}
	regex = NULL;
	if (*s) {
	  *s = '\0';
	  s++;
	  while (*s && isspace(*s)) {
	    s++;
	  }
	  regex = s;
	  while (*s) {
	    s++;
	  }
	  s--;
	  while (s > regex && isspace(*s)) {
	    s--;
	  }
	  s[1] = '\0';
	}
	tr = new_translation_section(name, regex);
	continue;
      }

      tok = token(s, &delim);
      if (tok == NULL) {
	continue;
      }
      if (!strcmp(tok, "DEBUG_REGEX")) {
	debug_regex = 1;
	continue;
      }
      if (!strcmp(tok, "DEBUG_STROKES")) {
	debug_strokes = 1;
	continue;
      }
      if (!strcmp(tok, "DEBUG_KEYS")) {
	debug_keys = 1;
	continue;
      }
      if (!strncmp(tok, "MIDI_OCTAVE", 11)) {
	char *a = tok+11;
	int k, n;
	if (!*a)
	  // look for the offset in the next token
	  a = token(NULL, &delim);
	if (sscanf(a, "%d%n", &k, &n) == 1 && !a[n]) {
	  midi_octave = k;
	} else {
	  fprintf(stderr, "invalid octave offset: %s\n", a);
	}
	continue;
      }
      which_key = tok;
      if (start_translation(tr, which_key)) {
	continue;
      }
      tok = token(NULL, &delim);
      while (tok != NULL) {
	if (delim != '"' && tok[0] == '#') {
	  break; // skip rest as comment
	}
	//printf("token: [%s] delim [%d]\n", tok, delim);
	switch (delim) {
	case ' ':
	case '\t':
	case '\n':
	  if (strncmp(tok, "XK", 2) && strncmp(tok, "RELEASE", 8))
	    add_midi(tok);
	  else
	    add_keystroke(tok, PRESS_RELEASE);
	  break;
	case '"':
	  add_string(tok);
	  break;
	default: // should be slash
	  updown = token(NULL, &delim);
	  if (updown != NULL) {
	    switch (updown[0]) {
	    case 'U':
	      add_keystroke(tok, RELEASE);
	      break;
	    case 'D':
	      add_keystroke(tok, PRESS);
	      break;
	    case 'H':
	      add_keystroke(tok, HOLD);
	      break;
	    default:
	      fprintf(stderr, "invalid up/down modifier [%s]%s: %s\n", name, which_key, updown);
	      add_keystroke(tok, PRESS);
	      break;
	    }
	  }
	}
	tok = token(NULL, &delim);
      }
      finish_translation();
    }

    fclose(f);
    return 1;

  }
  return 0;
}

translation *
get_translation(char *win_title, char *win_class)
{
  translation *tr;

  read_config_file();
  tr = first_translation_section;
  while (tr != NULL) {
    extern int enable_jack;
    if (tr->is_default &&
	(strcmp(tr->name, "MIDI") || enable_jack)) {
      return tr;
    } else if (!tr->is_default) {
      // AG: We first try to match the class name, since it usually provides
      // better identification clues.
      if (win_class && *win_class &&
	  regexec(&tr->regex, win_class, 0, NULL, 0) == 0) {
	return tr;
      }
      if (regexec(&tr->regex, win_title, 0, NULL, 0) == 0) {
	return tr;
      }
    }
    tr = tr->next;
  }
  return NULL;
}
