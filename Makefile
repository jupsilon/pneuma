
all: pneuma
run: all
	./pneuma

pneuma: Makefile *.cc
	clang++ -o pneuma *.cc -std=c++1z \
	-lwayland-client -lwayland-egl \
	-lEGL -lGL \
	-I${HOME}/boost_1_64_0/include \
	-L${HOME}/boost_1_64_0/lib -lboost_coroutine -lboost_context
