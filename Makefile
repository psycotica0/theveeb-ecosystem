
include build-config.mk

all:    targets
#all:   tests
#all:   objs
#all:   outputs

dir             := .
include         Rules.mk

.PHONY: targets clean distclean

%: %.o
	$(CC) $(LDFLAGS) $^ $(LL_ALL) $(LL_TGT) -o $@
	$(STRIP) $@
 
clean:
	$(RM) $(CLEAN)

distclean: clean