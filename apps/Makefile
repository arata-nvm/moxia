.PHONY: build
build: rpn

.FORCE:

rpn: .FORCE
	make -C ./rpn

clean:
	find . -name "*.o" -exec rm {} \;
