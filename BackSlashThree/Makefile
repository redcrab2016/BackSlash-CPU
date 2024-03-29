# Usage:
# make        # compile all binary
# make clean  # remove ALL binaries and objects

.PHONY = all clean bs3rt bs3asm bs3meta distribute

CC = @gcc

LINKERFLAG = -lm -pthread
# maxi run speed
DEBUG = -std=gnu11 -O3
# mini size
#DEBUG = -std=gnu11 -Os
# debug info with default optimization
#DEBUG = -std=gnu11 -g  # for gdb debugging 
BINS := bs3asm bs3rt

all: ${BINS} bs3meta distribute

distribute: ${BINS}
	@echo ">>> Distribute into dist folder."
	@mkdir -p dist/include
	@mkdir -p dist/examples
	@mkdir -p include
	@cp -pv bs3asm dist/.
	@cp -pv bs3rt dist/.
	@cp -prv include/* dist/include/.
	@cp -prv examples/* dist/examples/.
	@cp -pv bs3c.sh dist/.
	@cp -pv bs3meta/tinyc/target/*dependencies.jar dist/bs3tinyc.jar

bs3meta: ${BINS}
	@echo ">>> Build bs3tinyc"
	@mvn -f bs3meta/tinyc/pom.xml clean package install org.apache.maven.plugins:maven-assembly-plugin:single

bs3rt: bs3_mainrt.o dev_bs3gfx.o dev_bs3rambank.o dev_bs3rombank.o dev_bs3timer.o dev_bs3inout.o dev_rtc72421.o bs3_bus.o bs3.o bs3_asm_code_map.o
	@echo ">>> Creating" $@
	${CC} bs3_mainrt.o bs3.o dev_bs3gfx.o dev_bs3rambank.o dev_bs3rombank.o dev_bs3timer.o dev_bs3inout.o dev_rtc72421.o bs3_bus.o bs3_asm_code_map.o -o $@ ${LINKERFLAG}

bs3asm: bs3_asm_code_map.o  bs3_asm_line.o  bs3_asm_message.o  bs3_asm.o  bs3_asm_pass1.o  bs3_asm_pass2.o  bs3_debug.o  bs3_disasm.o  bs3_instr.o  bs3_main.o bs3_bus.o dev_bs3gfx.o dev_bs3rombank.o dev_bs3rambank.o dev_bs3timer.o dev_bs3inout.o dev_rtc72421.o bs3.o
	@echo ">>> Creating" $@
	${CC}  $^ -o $@ ${LINKERFLAG}

bs3_mainrt.o : bs3_main.c
	@echo ">>> Creating object bs3_mainrt.o"
	${CC} ${DEBUG} -D BS3RUNTIME -c bs3_main.c -o bs3_mainrt.o

%.o: %.c
	@echo  ">>> Creating object" $@
	${CC} ${DEBUG} -c $<

clean:
	@echo ">>> Cleaning up..."
	@rm -rvf *.o ${BINS}
	@rm -fv dist/bs3*
	@rm -fv dist/include/*
	@rm -rfv dist/examples/*