
all: pneuma
run: all
	./pneuma

pneuma: Makefile *.cc
	clang++ -o pneuma *.cc -std=c++1z \
	-lwayland-client -lwayland-egl \
	-lEGL -lGL \
	-g3
