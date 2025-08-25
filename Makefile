src_sotest = src/sotest.c
exe_sotest = test/sotest

src_dlib1 = src/dlib1.c
obj_dlib1 = test/dlib1.so
src_dlib2 = src/dlib2.c
obj_dlib2 = test/dlib2.so

flags = -std=c99 -march=x86-64 -Wall -Wextra -Werror -Wno-parentheses -Wno-unused-value -D_GNU_SOURCE -O2 -ldl

all: $(exe_sotest) $(obj_dlib1) $(obj_dlib2)

$(exe_sotest): $(src_sotest)
	/usr/bin/gcc $(flags) $^ -o $@

$(obj_dlib1): $(src_dlib1)
	/usr/bin/gcc $(flags) -fPIC -shared $^ -o $@

$(obj_dlib2): $(src_dlib2)
	/usr/bin/gcc $(flags) -fPIC -shared $^ -o $@

clean:
	/usr/bin/rm -f $(exe_sotest) $(obj_dlib1) $(obj_dlib2)
