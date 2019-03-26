#==============================================
# makefile for libsvr
#==============================================

include ../makefile_common

C_OBJS=${shell ls *.c}
CPP_OBJS=$(shell ls *.cpp)

PROGS=$(LIBF_SVR)

all:$(PROGS) $(C_OBJS:.c=.d) $(CPP_OBJS:.cpp=.d)

$(PROGS):$(C_OBJS:.c=.o) $(CPP_OBJS:.cpp=.o)
	@echo Build...$@
	@$(AR) $(ARFLAGS) $(PROGS) $(C_OBJS:.c=.o) $(CPP_OBJS:.cpp=.o)
	@$(RANLIB) $(PROGS)

%.o: %.c
	@echo Compile...$@
	@$(CC) $(C_LFLAGS) $(INC_LIBSVR) -c $< -o $@ 

%.d: %.c
	@set -e; \
	$(CC) -MM $(C_LFLAGS) $(INC_LIBSVR) $< \
	| sed 's/$(*F)\.o[ :]*/$(subst /,\/,$(basename $@)).o $(subst /,\/,$@) : /g' > $@; \
	[ -s $@ ] || rm -f $@
	
%.o: %.cpp
	@echo Compile...$@
	@$(CPP) $(CXX_LFLAGS) $(INC_LIBSVR) -c $< -o $@ 

%.d: %.cpp
	@set -e; \
	$(CPP) -MM $(CXX_LFLAGS) $(INC_LIBSVR) $< \
	| sed 's/$(*F)\.o[ :]*/$(subst /,\/,$(basename $@)).o $(subst /,\/,$@) : /g' > $@; \
	[ -s $@ ] || rm -f $@

clean:
	rm -f $(PROGS)
	rm -f $(C_OBJS:.c=.o)
	rm -f $(C_OBJS:.c=.d)
	rm -f $(CPP_OBJS:.cpp=.o)
	rm -f $(CPP_OBJS:.cpp=.d)
	@echo Clean libsvr...done!
