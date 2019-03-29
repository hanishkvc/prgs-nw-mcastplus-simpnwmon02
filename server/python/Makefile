
PY_FILES=context.py hkvc-nw-recover.py hkvc-nw-send-mcast.py network.py status.py

tag: $(PY_FILES)
	ctags -R *

MOD_FILES = network.py mcast.py context.py status.py

ccompile: $(MOD_FILES) compile.py
	python3 compile.py build_ext --inplace
	mkdir MOVE || /bin/true
	mv $(MOD_FILES) compile.py MOVE/

clean:
	rm -rf __pycache__

cclean:
	rm *.so || /bin/true
	rm *.c || /bin/true
	mv MOVE/*py . || /bin/true
	rmdir MOVE || /bin/true
	rm -rf build || /bin/true
