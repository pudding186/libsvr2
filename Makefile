#==============================================
# makefile for libsvr2
#==============================================
BLDDIRS= src

all: 
	@for dir in ${BLDDIRS};do \
		($(MAKE) -C $$dir all) || exit 1; \
	done

clean:
	@for dir in ${BLDDIRS};do \
		($(MAKE) -C $$dir clean) || exit 1; \
	done
