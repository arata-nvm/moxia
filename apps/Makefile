.PHONY: build
build: rpn fault

.FORCE:

fault: .FORCE
	make -C ./fault
	
rpn: .FORCE
	make -C ./rpn

clean:
	find . -name "*.o" -exec rm {} \;
