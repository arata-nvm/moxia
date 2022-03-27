.PHONY: build
build: rpn fault readfile grep

.FORCE:

rpn: .FORCE
	make -C ./rpn

fault: .FORCE
	make -C ./fault

readfile: .FORCE	
	make -C ./readfile

grep: .FORCE	
	make -C ./grep

clean:
	find . -name "*.o" -exec rm {} \;
