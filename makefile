all:
	gcc -O2 capturev4l2.c -o capturev4l2 `pkg-config --cflags --libs opencv`
	gcc -O2 capturev4l2_server.c -o capturev4l2_server `pkg-config --cflags --libs opencv`

face:
	gcc -O2 capturev4l2.c -o capturev4l2 `pkg-config --cflags --libs opencv`
	gcc -O2 capturev4l2_server.c -o capturev4l2_server `pkg-config --cflags --libs opencv` -D FACEDETECT

