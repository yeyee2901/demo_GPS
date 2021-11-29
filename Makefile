BOARD = uno
PLATFORMIO = pio -f -c

all:
	${PLATFORMIO} vim run

clean:
	${PLATFORMIO} vim run -t clean

upload: all
	${PLATFORMIO} vim run -t upload

uploadfs:
	${PLATFORMIO} vim run -t uploadfs

program:
	${PLATFORMIO} vim run -t program

update:
	pio init --ide=vim --board=${BOARD}
