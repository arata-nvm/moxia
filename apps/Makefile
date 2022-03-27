.PHONY: build
build: rpn fault readfile

.FORCE:

rpn: .FORCE
	make -C ./rpn

fault: .FORCE
	make -C ./fault

readfile: .FORCE	
	make -C ./readfile

clean:
	find . -name "*.o" -exec rm {} \;
