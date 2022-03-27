.PHONY: build
build: rpn fault readfile grep cp

.FORCE:

rpn: .FORCE
	make -C ./rpn

fault: .FORCE
	make -C ./fault

readfile: .FORCE	
	make -C ./readfile

grep: .FORCE	
	make -C ./grep

cp: .FORCE	
	make -C ./cp

clean:
	find . -name "*.o" -exec rm {} \;
