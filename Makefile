SUBDIRS=rx tx

all:
	@for i in $(SUBDIRS); do \
		echo "making $$i..."; \
		if [ -f "$$i/build.sh" ]; then \
			(cd $$i; sh ./build.sh); \
		else \
			(cd $$i; $(MAKE)); \
		fi; \
	done

clean:
	@for i in $(SUBDIRS); do \
		echo "cleaning $$i..."; \
		(cd $$i; $(MAKE) clean); \
	done
