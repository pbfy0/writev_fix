all: bin/writev_shim.so bin/launcher

bin/launcher: src/launcher.c
	gcc src/launcher.c -ldl -obin/launcher

bin/writev_shim.so: src/shim.c
	gcc src/shim.c -fPIC -shared -obin/writev_shim.so

clean:
	rm bin/writev_shim.so bin/launcher
