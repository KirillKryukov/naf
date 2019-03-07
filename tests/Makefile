
.PHONY: default all clean

export TMPDIR=temp

default: all

all:
	@diff -q Makefile Makefile
	@mkdir -p temp
	@./test-runner.pl interface alphabet small
	@echo "Success!"

clean:
	@rm -f */*.out
	@rm -f */*.err
	@echo "Test directories cleaned."
