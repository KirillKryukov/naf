
.PHONY: default all large clean

export TMPDIR=temp

default: all

all:
	@diff -q Makefile Makefile
	@mkdir -p temp
	@./test-runner.pl interface alphabet charcount small
	@echo "Success!"

large:
	@diff -q Makefile Makefile
	@mkdir -p temp
	@./test-runner.pl large
	@echo "Success!"

clean:
	@rm -f */*.out
	@rm -f */*.err
	@echo "Test directories cleaned."
