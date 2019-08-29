


.PHONY: doc clean

all: doc

clean:
	@echo "Cleaning up"
	@svn --force rm ./html > /dev/null 2>&1

doc :
	@echo "Generating doxygen"
	@doxygen ./BUTool.cfg


