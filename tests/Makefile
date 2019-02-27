
.PHONY: default all clean

default: all

all:
	@diff -q Makefile Makefile
	@./test-runner.pl interface small
	@echo "Success!"

clean:
	@rm -f */*.out
	@rm -f */*.err
	@echo "Test directories cleaned."
