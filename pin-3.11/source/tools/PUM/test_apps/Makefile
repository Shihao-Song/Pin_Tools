CC      := g++
FLAGS   := -Og

all: test_app_base test_app_our

test_app_base: test_app_base.cc
	$(CC) $(FLAGS) test_app_base.cc -o test_app_base

test_app_our: test_app_our.cc
	$(CC) $(FLAGS) test_app_our.cc -o test_app_our

clean:
	rm test_app_base
	rm test_app_our
