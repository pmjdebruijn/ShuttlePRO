# ShuttlePRO

Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)  
Copyright 2018 Albert Graef <<aggraef@gmail.com>>

This is a user program for interpreting key, shuttle, and jog events from a Contour Design Shuttle device, like the ShuttlePRO v2 or the Shuttle Xpress. It was originally written in 2013 by Eric Messick. The latest work on it was done by Albert Graef, offering various useful improvements, such as additional command line options, automatic detection of Shuttle devices, the ability to detect applications using their `WM_CLASS` property (in addition to window titles), and support for Jack MIDI output. The latter lets you use the Shuttle as a fully configurable MIDI controller.

**NOTE:** Eric's original README file along with some accompanying (now largely obsolete) files can be found in the attic subdirectory. You might want to consult these in order to get the program to work on older Linux systems. But for newer Linux systems you should be able to find all necessary information to get up and running in this file.

## Description

The `shuttlepro` program translates input events (button presses, jog and shuttle wheel movements) of the Shuttle device into X keystrokes, mouse button presses, scroll wheel events, or, as an option, MIDI output. It enables you to use the device conveniently on Linux to operate various kinds of multimedia programs, typically audio and video editors, digital audio workstation (DAW) programs and the like. It does this by checking the (name and class of the) window which has keyboard focus and picking a suitable set of translations from its configuration (shuttlerc) file. Default translations for programs not explicitly specified in the configuration file can be given as well.

Shuttle events can generate sequences of multiple keystrokes, including the pressing and releasing of modifier keys. In addition, MIDI messages can be generated and output using Jack MIDI.

The shuttlerc file is just an ordinary text file which you can edit to your heart's content, in order to configure the program for use with any kind of application taking keyboard, mouse or MIDI input. A sample shuttlerc file containing configurations for various popular applications is included, see example.shuttlerc in the sources.

## Installation

First, make sure that you have the required dependencies installed. The program needs a few X11 libraries and Jack (the latter is only required if you plan to utilize the MIDI support). And of course you need GNU make and gcc (the GNU C compiler). On Ubuntu and other Debian-based systems you should be able to get everything that's needed by running this command:

    sudo apt install build-essential libx11-dev libxtst-dev libjack-dev

Then just run `make` and `sudo make install`. This installs the example.shuttlerc file as /etc/shuttlerc, and the `shuttlepro` program in the default install location. Usually this will be /usr/local/bin, but the installation prefix can be changed with the `prefix` variable in the Makefile. Also, package maintainers can use the `DESTDIR` variable as usual to install into a staging directory for packaging purposes.

## Configuration File

After installation the system-wide default configuration file will be in /etc/shuttlerc, where the program will be able to find it. We recommend copying this file to your home directory, renaming it to .shuttlerc:

    cp /etc/shuttlerc ~/.shuttlerc

The ~/.shuttlerc file, if it exists, takes priority over /etc/shuttlerc, so it becomes your personal `shuttlepro` configuration. You can edit this file as you see fit, in order to customize existing or adding your own application configurations. (If you create any new entries which might be useful to other users of this program, please consider submitting them so that they can be included in future releases.)

**NOTE:** The program re-reads the shuttlerc file whenever it notices that the file has been changed, but in the current implementation it only checks when a shuttle event is sent with a different window focused from the previous shuttle event. Thus you can edit the file while the program keeps running, but you'll have to switch windows *and* operate the device to have the changes take effect.

## Usage

The `shuttlepro` program is a command line application, so you typically run it from the terminal, but of course it is also possible to invoke it from your desktop environment's startup files once you've set up everything to your liking.

Before you can use the program, you have to make sure that you can access the device. On modern Linux systems, becoming a member of the `input` group should be all that is needed:

    sudo useradd -G input username

Log out and in again, and you should be set. Now make sure that your Shuttle device is connected, and try running `shuttlepro` from the command line (without any arguments). The program should hopefully detect your device and print something like:

    shuttlepro: found shuttle device:
	/dev/input/by-id/usb-Contour_Design_ShuttleXpress-event-if00

(The precise name of the device will differ, depending on the type of device that you have. E.g., the output above indicates that a Shuttle Xpress was found.)

If the program fails to find your device, you'll have to locate it yourself and specify the absolute pathname to it on the command line. Usually there should be an entry under /dev/input/by-id for it, which is simply a symbolic link to some device node under /dev/input. Naming the device on the command line will also be necessary if you have multiple Shuttle devices. In this case you may want to run a separate instance of `shuttlepro` for each of them (possibly with different configurations, using the `-r` option, see below).

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
JR[]: XK_Scroll_Down/D XK_Scroll_Down/U 
JL[]: XK_Scroll_Up/D XK_Scroll_Up/U 
K5[D]: XK_Button_1/D 
K5[U]: XK_Button_1/U 
~~~

It goes without saying that these debugging options will be very helpful when you start developing your own bindings.

It's also possible to use alternative configuration files, by specifying the shuttlerc file to be used with the `-r` option. Also, try `shuttlepro -h` which prints a help message with the available options and a brief description.

More information about the available configurations and on how to actually create your own configurations can be found in the example.shuttlerc file. You may also want to look at the comments at the top of readconfig.c for further technical details.

## MIDI Support

The `shuttlepro` program can also be used to translate input from the Shuttle device to corresponding MIDI messages rather than key presses, using [Jack][] MIDI for output. This is useful if you want to hook up the device to any kind of MIDI-capable program, such as software synthesizers or a DAW program like [Ardour][].

[Ardour]: https://ardour.org/
[Jack]: http://jackaudio.org/

The program will automatically be built with Jack MIDI support if the Jack development files are available at compile time. (However, if you do have Jack installed, but you still want to build a Jack-less version of the program, you can do that by running `make JACK=` instead of just `make`.)

If the program was built with Jack MIDI support, you still need to run it as `shuttlepro -o` to enable MIDI output at run time. This will start up Jack (if it is not already running) and create a Jack client named `shuttlepro` with a single MIDI output port which can then be connected to the MIDI inputs of other programs.

We recommend using a Jack front-end and patchbay program like [QjackCtl][] to manage Jack and to set up the MIDI connections. Non-Jack ALSA MIDI applications can be accommodated using the [a2jmidid][] program, an ALSA-Jack MIDI bridge which exposes ALSA sequencer ports as Jack MIDI ports, so that they can easily be connected using QjackCtl. (Starting up a2jmidid can be handled automatically by QJackCtl as well; in QJackCtl's Setup dialog, on the Options tab, simply place the command `a2jmidid -e &` in the "Execute script after Startup" field. This will work no matter which Jack version you use, but it should be mentioned that the latest versions of Jack1 have the a2jmidid functionality already built into it.)

[QjackCtl]: https://qjackctl.sourceforge.io/
[a2jmidid]: http://repo.or.cz/a2jmidid.git

The example.shuttlerc file comes with a sample configuration in the `[MIDI]` section for illustration purposes. This special default section is only active if the program is run with the `-o` option. It allows MIDI output to be sent to any connected applications, no matter which window currently has the keyboard focus. This is probably the most common way to use this feature, but of course it is also possible to have application-specific MIDI translations, in the same way as with X11 key bindings. In fact, you can freely mix mouse actions, key presses and MIDI messages in all translations.

The sample `[MIDI]` section implements a simplistic DAW controller which can be used as a (rather rudimentary) Mackie control surface, e.g., with Ardour. The buttons `K5` .. `K9` are mapped to the playback controls (Rewind, Stop, Play, Record, Fast Forward), and the jog wheel emulates the MCU jog wheel (`CC60`). The configuration entry looks as follows:

~~~
[MIDI]

 K5 G7  # Rewind
 K6 A7  # Stop
 K7 A#7 # Play
 K8 B7  # Record
 K9 G#7 # Fast Forward

 JL CC60~
 JR CC60~
~~~

(Note the `CC60~` event in the jog wheel translations. The `~` suffix indicates that `CC60` is to be treated as an endless rotary encoder which represents relative changes of the jog wheel value in a special "sign bit" format, see the readconfig.c file for details.)

To try it, run `shuttlepro -o`, fire up Ardour, and configure a Mackie control surface in Ardour which takes input from the MIDI output of the `shuttlepro` client. The playback controls and the jog wheel should then work exactly like a real Mackie-like MIDI controller connected directly to Ardour.
