% shuttlepro(1)

# Name

shuttlepro -- translate input from the Contour Design Shuttle devices

# Synopsis

shuttlepro [-h] [-o] [-j *name*] [-r *rcfile*] [-d[rskj]]

# Options

-h
:   Print a short help message.

-o
:   Enable MIDI output.

-j *name*
:   Set the Jack client name. Default: "shuttlepro".

-r *rcfile*
:   Set the configuration file name. Default: taken from the SHUTTLE_CONFIG_FILE environment variable if it exists, or ~/.shuttlerc if it exists, /etc/shuttlerc otherwise.

-d[rskj]
:   Enable various debugging options: r = regex (print matched translation sections), s = strokes (print the parsed configuration file in a human-readable format), k = keys (print executed translations), j = jack (additional Jack debugging output). Just `-d` enables all debugging options.

# Description

The Contour Design Shuttle devices, such as the ShuttlePRO v2 and the Shuttle Xpress, are specialized input devices mainly used with audio and video editing software. They offer three different kinds of controls: a jog wheel (an endless rotary encoder), a shuttle wheel surrounding the jog wheel which automatically snaps back into its center position, and a collection of buttons arranged around the jog and shuttle wheels.

The shuttlepro program enables you to use these devices with your X11 applications. It will work with any application taking keyboard, mouse or MIDI input, without requiring any special support from the application. The program translates input events (button presses, jog and shuttle wheel movements) into X keystrokes, mouse button presses, scroll wheel events, or, as an option, MIDI output. It does this by matching the `WM_CLASS` and `WM_NAME` properties of the window that has the keyboard focus against the regular expressions for each application section in its configuration (shuttlerc) file. If a regex matches, the corresponding set of translations is used. Otherwise the program falls back to a set of translations in a default section at the end of the file, if available.

The shuttlerc file is just an ordinary text file which you can edit to configure the program for your applications. A sample shuttlerc file containing configurations for some applications is included, see example.shuttlerc in the sources.

## Installation

First, make sure that you have the required dependencies installed. The program needs a few X11 libraries and [Jack][]; the latter is only required if you plan to utilize the MIDI support. And of course you need GNU make and gcc (the GNU C compiler). On Ubuntu and other Debian-based systems you should be able to get everything that's needed by running this command:

    sudo apt install build-essential libx11-dev libxtst-dev libjack-dev

Then just run `make` and `sudo make install`. This installs the example.shuttlerc file as /etc/shuttlerc, and the shuttlepro program and the manual page in the default install location. Usually this will under /usr/local, but the installation prefix can be changed with the `prefix` variable in the Makefile. Also, package maintainers can use the `DESTDIR` variable as usual to install into a staging directory for packaging purposes.

The program will automatically be built with Jack MIDI support if the Jack development files are detected at compile time. (If you do have Jack installed, but you still want to build a Jack-less version of the program for some reason, you can do that by running `make JACK=` instead of just `make`.)

[Jack]: http://jackaudio.org/

# Configuration File

After installation the system-wide default configuration file will be in /etc/shuttlerc, where the program will be able to find it. We recommend copying this file to your home directory, renaming it to .shuttlerc:

    cp /etc/shuttlerc ~/.shuttlerc

The ~/.shuttlerc file, if it exists, takes priority over /etc/shuttlerc, so it becomes your personal shuttlepro configuration. You can edit this file as you see fit, in order to customize the configuration for the applications that you use.

**NOTE:** The program re-reads its configuration file whenever it notices that the file has been changed. However, in the current implementation this check is only done when an input event arrives after the keyboard focus changed to a new window. Thus you can edit the file while the program keeps running, but you'll have to switch windows *and* operate the device to have the changes take effect.

# Usage

The shuttlepro program is a command line application, so you typically run it from the terminal. However, it is also possible to launch it from your Jack session manager (see *MIDI Output* below) or from your desktop environment's startup files once you've set up everything to your liking.

Before you can use the program, you have to make sure that you can access the device. On modern Linux systems, becoming a member of the `input` group should be all that is needed:

    sudo useradd -G input username

Log out and in again, and you should be set. Now make sure that your Shuttle device is connected, and try running `shuttlepro` from the command line (without any arguments). The program should hopefully detect your device and print something like:

    shuttlepro: found shuttle device:
	/dev/input/by-id/usb-Contour_Design_ShuttleXpress-event-if00

(The precise name of the device will differ, depending on the type of device that you have. E.g., the output above indicates that a Shuttle Xpress was found.)

If the program fails to find your device, you'll have to locate it yourself and specify the absolute pathname to it on the command line. Usually there should be an entry under /dev/input/by-id for it, which is simply a symbolic link to some device node under /dev/input. Naming the device on the command line will also be necessary if you have multiple Shuttle devices. In this case you may want to run a separate instance of shuttlepro for each of them (possibly with different configurations, using the `-r` option).

If your device was found, you should be able to operate it now and have, e.g., the terminal window in which you launched the program scroll and execute mouse clicks if you move the jog wheel and press the three center buttons on the device. When you're finished, terminate the program by typing Ctrl+C in the terminal window where you launched it.

This default "mouse emulation mode" is actually configured in the `[Default]` section near the end of the distributed shuttlerc file, which reads as follows:

~~~
[Default]
 K6 XK_Button_1
 K7 XK_Button_2
 K8 XK_Button_3
 JL XK_Scroll_Up
 JR XK_Scroll_Down
~~~

As you can see, the buttons denoted `K6`, `K7` and `K8` (which are the three buttons right above the jog wheel, see the comments at the beginning of the shuttlerc file for a description of the button layout) are mapped to the corresponding mouse buttons, and rotating the jog wheel to the left (`JL`) and right (`JR`) emulates the scroll wheel, scrolling up and down, respectively. (Besides these mouse actions, you can also bind input events to arbitrary sequences of key strokes, so operating the functions of any application that is well-equipped with keyboard shortcuts should in most cases be a piece of cake. Have a look at the other configuration entries to see how this is done.)

One useful feature is that you can invoke the program with various debugging options to get more verbose output as the program recognizes events from the device and translates them to corresponding mouse actions or key presses. E.g., try running `shuttlepro -drk` to have the program print the recognized configuration sections and translations as they are executed. For instance, here is what the program may print in the terminal if you move the jog wheel one tick to the right (`JR`), then left (`JL`), and finally press the leftmost of the three buttons (`K6`):

~~~
$ shuttlepro -drk
shuttlepro: found shuttle device:
/dev/input/by-id/usb-Contour_Design_ShuttleXpress-event-if00
Loading configuration: /home/foo/.shuttlerc
translation: Default for ShuttlePRO : bash (class konsole)
JR: XK_Scroll_Down/D XK_Scroll_Down/U 
JL: XK_Scroll_Up/D XK_Scroll_Up/U 
K5[D]: XK_Button_1/D 
K5[U]: XK_Button_1/U 
~~~

It goes without saying that these debugging options will be very helpful when you start developing your own bindings.  The `-d` option can be combined with various option characters to choose exactly which kinds of debugging output you want; `r` ("regex") prints the matched translation section (if any) along with the window name and class of the focused window; `s` ("strokes") prints the parsed contents of the configuration file in a human-readable form whenever the file is loaded; `k` ("keys") shows the recognized translations as the program executes them, in the same format as `s`; and `j` adds some debugging output from the Jack driver. You can also just use `-d` to enable all debugging output. (Most of these options are also available as directives in the shuttlerc file; please check the distributed example.shuttlerc for details.)

## MIDI Output

If the shuttlepro program was built with Jack MIDI support, it can also be used to translate input from the Shuttle device to corresponding MIDI messages rather than key presses. This is useful if you want to hook up the device to any kind of MIDI-capable program, such as software synthesizers or a digital audio workstation (DAW) program like [Ardour][].

[Ardour]: https://ardour.org/

You need to run the program as `shuttlepro -o` to enable MIDI output at run time. This will start up Jack (if it is not already running) and create a Jack client named `shuttlepro` with a single MIDI output port which can then be connected to the MIDI inputs of other programs. The Jack client name can also be changed with the `-j` option, which is useful if you're running multiple instances of the program with different Shuttle devices (possibly using different configurations).

We recommend using a Jack front-end and patchbay program like [QjackCtl][] to manage Jack and to set up the MIDI connections. In QjackCtl's setup, make sure that you have selected `seq` as the MIDI driver. This exposes the ALSA sequencer ports of non-Jack ALSA MIDI applications as Jack MIDI ports, so that they can easily be connected to shuttlepro. (We're assuming that you're using Jack1 here. Jack2 works in a very similar way, but may require some more fiddling; in particular, you may have to use [a2jmidid][] as a separate ALSA-Jack MIDI bridge in order to have the ALSA MIDI devices show properly as Jack MIDI devices.)

[QjackCtl]: https://qjackctl.sourceforge.io/
[a2jmidid]: http://repo.or.cz/a2jmidid.git

The shuttlepro program also supports Jack session management, which makes it possible to record the options the program was invoked with along with the MIDI connections. This feature can be used with any Jack session management software. Specifically, QjackCtl has its own built-in Jack session manager which is available in its Session dialog. To use this, launch shuttlepro and any other Jack applications you want to have in the session, use QjackCtl to set up all the connections as needed, and then the "Save" option in the Session dialog to have the session recorded. Now, at any later time you can relaunch the same session with the "Load" option in the same dialog.

The example.shuttlerc file comes with a sample configuration in the `[MIDI]` section for illustration purposes. This special default section is only active if the program is run with the `-o` option. It allows MIDI output to be sent to any connected applications, no matter which window currently has the keyboard focus. This is probably the most common way to use this feature, but of course it is also possible to have application-specific MIDI translations, in the same way as with X11 key bindings. In fact, you can freely mix mouse actions, key presses and MIDI messages in all translations.

The sample `[MIDI]` section implements a simplistic DAW controller which can be used as a (rather rudimentary) Mackie control surface, e.g., with Ardour. It maps some of the keys, as well as the shuttle and jog wheels to playback controls and cursor movement commands. The configuration entry looks as follows:

~~~
[MIDI]

 K6 A7  # Stop
 K7 A#7 # Play
 K8 B7  # Record

 K5 D8  # Left
 K9 D#8 # Right

 IL G7  # Rewind
 IR G#7 # Fast Forward
 S0 A7  # Stop

 # MCU jog wheel
 JL CC60~
 JR CC60~
~~~

Note that the Mackie control protocol consists of various different MIDI messages, mostly note and control change messages. We'll discuss the syntax of these items in the *MIDI Translations* section below.

To try it, run `shuttlepro -o`, fire up Ardour, and configure a Mackie control surface in Ardour which takes input from the MIDI output of the `shuttlepro` client. The playback controls and the jog wheel should then work exactly like a real Mackie-like MIDI controller connected directly to Ardour.

# Translation Syntax

The shuttlerc file consists of sections defining translation classes. Each section generally looks like this, specifying the name of a translation class, optionally a regular expression to be matched against the window class or title, and a list of translations:

~~~
[name] regex
K<1..15> output # key
S<-7..7> output # shuttle value
I<LR>    output # shuttle rotation
J<LR>    output # jog wheel rotation 
~~~

The `#` character at the beginning of a line and after whitespace is special; it indicates that the rest of the line is a comment, which is skipped by the parser. Empty lines and lines containing nothing but whitespace are also ignored.

Each `[name] regex` line introduces the list of translations for the named translation class. The name is only used for debugging output, and needn't be unique. When focus is on a window whose class or title matches the regular expression `regex`, the corresponding translations are in effect. An empty regex for the last class will always match, allowing default translations. Any output sequences not bound in a matched section will be loaded from the default section if they are bound there.

The translations define what output should be produced for the given input. Each translation must be on a line by itself. The first token of each translation denotes the key, shuttle or jog wheel event to be translated:

- `K` followed by the key number denotes one of the buttons on the Shuttle device. The PRO version of the device has 15 such buttons, the Xpress version only five (`K5` .. `K9`). See the example.shuttlerc file for a picture showing how the buttons are laid out.

- `S` followed by any of the values -7..7 denotes a specific position of the shuttle wheel, with 0 denoting the center position.

- `IL` denotes left (counter-clockwise), `IR` right (clockwise) rotation of the shuttle wheel.

- `JL` denotes left (counter-clockwise), `JR` right (clockwise) rotation of the jog wheel.

The input event is followed by the output sequence consisting of one or more key, mouse and MIDI events. We'll describe these below. In each translation section, the translations must be unique, i.e., there may be at most one translation for each kind of input event.

## Key and Mouse Translations

Input events can generate sequences of multiple keystrokes, including the pressing and releasing of modifier keys. The output sequence consists of one or more tokens described by the following EBNF grammar:

~~~
token   ::= "RELEASE" | keycode [ "/" flag ] | string
keycode ::= "XK_Button_1" | "XK_Button_2" | "XK_Button_3" |
            "XK_Scroll_Up" | "XK_Scroll_Down" |
            "XK_..." (X keysyms, see /usr/include/X11/keysymdef.h)
flag    ::= "U" | "D" | "H"
string  ::= '"' { character } '"'
~~~

Besides the key codes from the keysymdef.h file, there are also some special additional key codes to denote mouse button (`XK_Button_1`, `XK_Button_2`, `XK_Button_3`) and scroll wheel (`XK_Scroll_Up`, `XK_Scroll_Down`) events.

Any keycode can be followed by an optional `/D`, `/U`, or `/H` flag, indicating that the key is just going down (without being released), going up, or going down and being held until the "off" event is received. So, in general, modifier key codes will be followed by `/D`, and precede the keycodes they are intended to modify. If a sequence requires different sets of modifiers for different keycodes, `/U` can be used to release a modifier that was previously pressed with `/D`. Sequences may also have separate press and release sequences, separated by the special word `RELEASE`. Examples:

~~~
K5 "qwer"
K6 XK_Right
K7 XK_Alt_L/D XK_Right
K8 "V" XK_Left XK_Page_Up "v"
K9 XK_Alt_L/D "v" XK_Alt_L/U "x" RELEASE "q"
~~~

One pitfall for beginners is that character strings in double quotes are just a shorthand for the corresponding X key codes, ignoring case. Thus, e.g., `"abc"` actually denotes the keysym sequence `XK_a XK_b XK_c`, as does `"ABC"`. So in either case the *lowercase* string `abc` will be output. To output uppercase letters, it is always necessary to add one of the shift modifiers to the output sequence. E.g., `XK_Shift_L/D "abc"` will output `ABC` in uppercase.

Translations are handled in slightly different ways depending on the type of input event. For key inputs (`K`), there are separate separate press and release sequences. At the end of the press sequence, all down keys marked by `/D` will be released, and the last key not marked by `/D`, `/U`, or `/H` will remain pressed. The release sequence will begin by releasing the last held key. If keys are to be pressed as part of the release sequence, then any keys marked with `/D` will be repressed before continuing the sequence. Keycodes marked with `/H` remain held between the press and release sequences. For instance, let's take a look at one of the more conspicuous translations in the example above:

~~~
K9 XK_Alt_L/D "v" XK_Alt_L/U "x" RELEASE "q"
~~~

When the `K9` key is pressed, the key sequence `Alt+v x` is initiated, keeping the `x` key pressed (so it may start auto-repeating after a while). The program then sits there waiting (possibly executing other translations) until you release the `K9` key again, at which point the `x` key is released and the `q` key is pressed (and released).

For the shuttle and jog wheel events there are no such separate press and release sequences. Only a single sequence is output whenever they are moved, and at the end of the sequence, all down keys will be released. For instance, the following translations move the cursor left or right when the jog wheel is rotated left or right, respectively. Also, the number of times one of the cursor keys is output corresponds to the actual change in the value. Thus, if in the example you move the jog wheel clockwise by 4 ticks, say, the program will press (and release) `XK_Right` four times, moving the cursor 4 positions to the right.

~~~
JL XK_Left
JR XK_Right
~~~

For the shuttle wheel with its 15 discrete positions (-7..7), you have two options. You can treat it in the same fashion as the jog wheel, translating incremental movements, by using `IL` and `IR` in lieu of `JL` and `JR`:

~~~
IL XK_Left
IR XK_Right
~~~

Or you can assign different output sequences to each of the 15 shuttle positions, using the `S-7` .. `S7` input events. For instance, the following might be used with the Kdenlive video editor:

~~~
S-2 "KJJ"  # fast rewind
S-1 "KJ"   # rewind
S0  "K"    # stop
S1  "KL"   # forward
S2  "KLL"  # fast forward
~~~

## MIDI Translations

The output sequence can involve as many MIDI messages as you want, and these can be combined freely with keyboard and mouse events in any order. There's no limitation on the type or number of MIDI messages that you can put into a translation rule. However, as already discussed in Section *MIDI Output* above, you need to invoke the shuttlepro program with the `-o` option to make MIDI output work. (Otherwise, MIDI messages in the output translations will just be silently ignored.)

shuttlepro uses the following human-readable notation for the various kinds of MIDI messages (notes, program change, control change and pitch bend; aftertouch and system messages are *not* supported right now, although they might be added in the future). The syntax of these tokens is as follows:

~~~
token ::= ( note | msg ) [ number ] [ "-" number] [ "~" ]
note  ::= ( "A" | ... | "G" ) [ "#" | "b" ]
msg   ::= "CH" | "PB" | "PC" | "CC"
~~~

Case is ignored here, so `CC`, `cc` or even `Cc` are considered to be exactly the same token by the parser, although by convention we usually write them in uppercase. Numbers are always integers in decimal.

MIDI messages are on channel 1 by default, but you can change this with a dash followed by the desired channel number (1..16). E.g., `C3-10` denotes note `C3` on MIDI channel 10. If multiple messages are output on the same MIDI channel, then you can also use the special `CH` token, which doesn't generate any output by itself, but sets the default channel for subsequent MIDI messages in the sequence. For instance, the sequence `C5-2 E5-2 G5-2`, which outputs a C major chord on MIDI channel 2, can also be abbreviated as `CH2 C5 E5 G5`.

Note messages are specified using the customary notation (note name `A..G`, optionally followed by an accidental, `#` or `b`, followed by the MIDI octave number. Note that all MIDI octaves start at the note C, so `B0` comes before `C1`. By default, `C5` denotes middle C (see Section *Octave Numbering* below on how to change this). Enharmonic spellings are equivalent, so, e.g., `D#` and `Eb` denote exactly the same MIDI note.

Here is a quick rundown of the recognized MIDI messages, with an explanation of how they work.

**CCn:** Generates a MIDI control change message for controller number *n*, where *n* must be in the range 0..127. In the case of jog or shuttle, the controller value will correspond to the jog/shuttle position, clamped to the 0..127 (single data byte) range. For key input, the control change message will be sent once with a value of 127 when the key is pressed, and then again with a value of 0 when the key is released.

**Example:** `CC7` generates a MIDI message to change the volume controller (controller #7), while `CC1` changes the modulation wheel (controller #1, usually some kind of vibrato effect). You can bind these, e.g., to the jog wheel or a key as follows:

~~~
JL CC7
JR CC7
K5 CC1
~~~

When used with the jog wheel, you can also generate relative control changes in a special "sign bit" format which is commonly used for endless rotary controllers. In this case, a +1 change is represented by the controller value 1, and a -1 change by 65 (a 1 with the sign in the 7th bit). This special mode of operation is indicated with the `~` suffix. E.g., here's how to bind an MCU-style jog wheel (`CC60`) event to the Shuttle's jog wheel:

~~~
JL CC60~
JR CC60~
~~~

**PB:** Generates a MIDI pitch bend message. This works pretty much like a MIDI control change message, but with an extended range of 0..16383, where 8192 denotes the center value. Obviously, this message is best bound to the shuttle (albeit with a resolution limited to 14 steps), but it also works with the jog wheel (with each tick representing 1/128th of the full pitch bend range) and even key input (in this case, 8192 is used as the "off" value, so the pitch only bends up, never down).

**Example:** Just `PB` generates a pitch bend message. You usually want to bind this to the incremental shuttle events, so the corresponding translations would normally look like this:

~~~
IL PB
IR PB
~~~

**PCn:** This generates a MIDI program change message for the given program number *n*, which must be in the 0..127 range. This type of message is most useful with key input, where it is output when the key is pressed (no output when the key is released, as there's no on/off status for this message; to have another `PC` message generated at key release time, it must be put explicitly into the `RELEASE` part of the key binding). In jog and shuttle assignments, this simply outputs the program change message every time the wheel position changes (which probably isn't very useful, although you could conceivably bind different `PC` messages to different shuttle wheel positions).

**Example:** The following will output a change to program 5 when `K5` is pressed, and another change to program 0 when the key is released (note that if you leave away the `RELEASE PC0` part, then only the `PC5` will be output when pressing the key, nothing happens when the key is released):

~~~
K5 PC5 RELEASE PC0
~~~

**MIDI notes:** Like `PC` messages, these are most useful when bound to key inputs. The note starts (sending a note on MIDI message with maximum velocity) when pressing the key, and finishes (sending the corresponding note off message) when releasing the key. In jog and shuttle assignments, a pair of note on and note off messages is generated (again, this probably isn't very useful unless you bind it to the shuttle wheel).

**Example:** The following binds key K6 to a C-7 chord in the middle octave:

~~~
K6 C5 E5 G5 Bb5
~~~

## Octave Numbering

A note on the octave numbers in MIDI note designations is in order here. There are various different standards for numbering octaves, and different programs use different standards, which can be rather confusing. E.g., there's the ASA (Acoustical Society of America) standard where middle C is C4, also known as "scientific" or "American standard" pitch notation. At least two other standards exist specifically for MIDI octave numbering, one in which middle C is C3 (so the lowest MIDI octave starts at C-2), and zero-based octave numbers, which start at C0 and have middle C at C5. There's not really a single "best" standard here, but the latter tends to appeal to mathematically inclined and computer-savvy people, and is also what is used by default in the shuttlerc file.

However, you may want to change this, e.g., if you're working with documentation or MIDI monitoring software which uses a different numbering scheme. To do this, just specify the desired offset for the lowest MIDI octave with the special `MIDI_OCTAVE` directive in the configuration file. For instance:

~~~
MIDI_OCTAVE -1 # ASA pitches (middle C is C4)
~~~

Note that this transposes *all* existing notes in translations following the directive, so if you add this option to an existing configuration, you probably have to edit the note messages in it accordingly.

# Notes

ShuttlePRO is free and open source software licensed under the GPLv3, please check the accompanying LICENSE file for details.

Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)  
Copyright 2018 Albert Graef (<aggraef@gmail.com>)

ShuttlePRO was originally written in 2013 by Eric Messick, based on earlier code by Trammell Hudson (<hudson@osresearch.net>) and Arendt David (<admin@prnet.org>). The present version, by Albert Graef, offers some improvements, such as additional command line options, automatic detection of Shuttle devices, and, most notably, Jack MIDI support.

Eric's original README along with some accompanying (now largely obsolete) files can still be found in the attic subdirectory in the sources. You might want to consult these in order to get the program to work on older Linux systems.
