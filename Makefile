VERBOSE := $(V)
DEVICE=/dev/ttyACM0

upload:
	platformio run --target upload --upload-port $(DEVICE) $(VERBOSE)

debug:
	platformio debug --upload-port $(DEVICE) $(VERBOSE)

monitor:
	platformio device monitor -b 115200 -f send_on_enter -f printable --echo --eol LF -p $(DEVICE)

compiledb:
	platformio run --target compiledb $(VERBOSE)
	[ -L compile_commands.json ] || ln -s .pio/build/megaatmega2560/compile_commands.json
