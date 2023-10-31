package backslash.bs5.asm;
import java.util.*;

public class BS5program {
    private int PC;
    private int linenum;
    private Map<Integer,BS5MemoryCell> bs5memoryMap;
    private Map<String,BS5Label> bs5Labels;
    private int memorySize;
    private String currentGlobalLabel;

    public BS5program() throws BS5Exception {
        this(65536);
    }

    public BS5program(int memorySize) throws BS5Exception {
        PC = 0;
        linenum = 0;
        this.memorySize = memorySize>65536?65536:(memorySize<256?256:memorySize);
        bs5memoryMap = new HashMap<Integer,BS5MemoryCell>();
        bs5Labels = new HashMap<String,BS5Label>();
        currentGlobalLabel=null;
        addLabel("__GLOBAL__");
    }

// does addr is correct (in range) ,if no then exception, if yes: does the memory is already mapped to a value ?
    private boolean isMemoryMapped(int addr) throws BS5Exception {
        if (addr < 0 || addr >= memorySize) throw new BS5Exception("Out of range memory address (program too large ?)"); 
        return bs5memoryMap.containsKey(addr);
    }

    public BS5program incrementLine() {
        linenum++;
        return this;
    }

    // if return -1 then label not found
    public int findLabelAddress(String label, int linenum) {
        BS5Label labelObj;
        if (label.charAt(0) != '.') { // global label, get it directly
            labelObj = bs5Labels.get(label);
            return labelObj==null?-1:labelObj.getAddr(); // found global label address
        } else { // local label
            for (String labelKey: bs5Labels.keySet()) { // search each label entry
                if (labelKey.contains(label)) {  // if the label contains the local label
                    labelObj = bs5Labels.get(labelKey); // check if it is in line range
                    if (    labelObj.getName().equals(label) &&
                            labelObj.getLinenum() <= linenum &&
                            labelObj.getLinenumend() >= linenum ) {
                        return labelObj.getAddr(); // found local label address
                    }
                }
            }
        }
        return -1; // not fiound
    }

//  Add label
    public BS5program addLabel(String label) throws BS5Exception {
        if (label.charAt(0) != '.') {  // global label
            if (bs5Labels.containsKey(label)) throw new BS5Exception("Duplicate label definition '" + label + "'");
            bs5Labels.put(label, new BS5Label(label, PC, linenum));
            if (currentGlobalLabel != null) {
                bs5Labels.get(currentGlobalLabel).setLinenumend(linenum-1);
            }
            currentGlobalLabel = label;
        } else { // local label
            String localGlobal = currentGlobalLabel + label; // local label named globally
            if (bs5Labels.containsKey(localGlobal)) throw new BS5Exception("Duplicate local label definition '" + label + "' for global label '" + currentGlobalLabel + "'");
            bs5Labels.put(localGlobal, new BS5Label( bs5Labels.get(currentGlobalLabel),label, PC, linenum));           
        }
        return this;
    }

// Directive "org"
    public BS5program org(String addr) throws BS5Exception {
        PC = BS5MemoryCell.get16bitsNumeral(this,PC,linenum,addr);
        return this;
    }

// Directive raw word data "dw"
    public BS5program dw(List<String> dw_list) throws BS5Exception {
        for (String dw: dw_list) addWord(dw);
        return this;
    }

    private BS5program addMemoryCell(BS5MemoryCell mcell) throws BS5Exception {
        if (isMemoryMapped(PC)) throw new BS5Exception("Memory address " + String.format("0x%04x", PC) + " is already encoded with a value.");
        bs5memoryMap.put(PC, mcell);
        PC++;
        return this;
    }

// Base 16 bits value added into memory map
    private BS5program addWord(String word) throws BS5Exception {
        return addMemoryCell(new BS5MemoryCell(this,PC,linenum,word));
    }

// Two registers encoding
    private BS5program add_oooo_yyyy_xxxx(String ccc, String f, String instruction, String ry, String rx) throws BS5Exception {
        return addMemoryCell(new BS5MemoryCell_oooo_yyyy_xxxx(this, PC, linenum, ccc, f, instruction, ry, rx));
    }

// 8 bits immediate (valid also for signed 8 bits immediate: oooo_siii_iiii )   
    private BS5program add_oooo_iiii_iiii(String ccc, String f, String instruction, String immediat) throws BS5Exception {
        return addMemoryCell(new BS5MemoryCell_oooo_iiii_iiii(this, PC, linenum, ccc, f, instruction, immediat));
    }

// 1 register and 4 bits immediate
    private BS5program add_oooo_iiii_xxxx(String ccc, String f, String instruction, String rx, String immediat) throws BS5Exception {
        return addMemoryCell(new BS5MemoryCell_oooo_iiii_xxxx(this, PC, linenum, ccc, f, instruction, rx, immediat));
    }

// 1 register
    private BS5program add_oooo_oooo_xxxx(String ccc, String f, String instruction, String rx) throws BS5Exception {
        return addMemoryCell(new BS5MemoryCell_oooo_oooo_xxxx(this, PC, linenum, ccc, f, instruction, rx));
    }

// 4 bits immediate
    private BS5program add_oooo_oooo_iiii(String ccc, String f, String instruction, String immediate) throws BS5Exception {
        return addMemoryCell(new BS5MemoryCell_oooo_oooo_iiii(this, PC, linenum, ccc, f, instruction, immediate));
    }


/*
ccc f 0000 yyyy xxxx
	mov [Rx], Ry
*/
    public BS5program asm_mov_atRx_Ry(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"nf":f, "mov [Rx], Ry", ry, rx);
    }
/*
ccc f 0001 yyyy xxxx
	mov Rx, [Ry] 
*/
    public BS5program asm_mov_Rx_atRy(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"nf":f, "mov Rx, [Ry]", ry, rx);
    }

/*
ccc f 0010 yyyy xxxx
	add Rx, [Ry]
*/
    public BS5program asm_add_Rx_atRy(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"fl":f, "add Rx, [Ry]", ry, rx);
    }

/*
ccc f 0011 yyyy xxxx
	sub Rx, [Ry]
*/
    public BS5program asm_sub_Rx_atRy(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"fl":f, "sub Rx, [Ry]", ry, rx);
    }

/*
ccc f 0100 yyyy xxxx
	mov Rx, Ry
*/
    public BS5program asm_mov_Rx_Ry(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"nf":f, "mov Rx, Ry", ry, rx);
    }

/*
ccc f 0101 yyyy xxxx
	add Rx, Ry
*/
    public BS5program asm_add_Rx_Ry(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"fl":f, "add Rx, Ry", ry, rx);
    }

/*
ccc f 0110 yyyy xxxx
	sub Rx, Ry
*/
    public BS5program asm_sub_Rx_Ry(String ccc, String f, String ry, String rx) throws BS5Exception {
        return add_oooo_yyyy_xxxx(ccc, f==null?"fl":f, "sub Rx, Ry", ry, rx);
    }
/*
ccc f 0111 iiii iiii
	mov low R0, imm8
*/
    public BS5program asm_mov_low_R0_imm8(String ccc, String f, String imm8) throws BS5Exception {
        return add_oooo_iiii_iiii(ccc, f==null?"nf":f, "mov low R0, imm8", imm8);
    }

/*
ccc f 1000 iiii iiii
	mov high R0, imm8
*/
    public BS5program asm_mov_high_R0_imm8(String ccc, String f, String imm8) throws BS5Exception {
        return add_oooo_iiii_iiii(ccc, f==null?"nf":f, "mov high R0, imm8", imm8);
    }

/*
ccc f 1001 siii iiii
	add R15, simm8
*/
    public BS5program asm_add_R15_simm8(String ccc, String f, String simm8) throws BS5Exception {
        return add_oooo_iiii_iiii(ccc, f==null?"nf":f, "add R15, simm8", simm8);
    }

/*
ccc f 1010 iiii xxxx
	mov C, Rx:imm4
*/
    public BS5program asm_mov_C_Rx_imm4(String ccc, String f, String rx, String imm4) throws BS5Exception {
        return add_oooo_iiii_xxxx(ccc, f==null?"nf":f, "mov C, Rx:imm4", rx, imm4);
    }

/*ccc f 1011 iiii xxxx
	mov Rx:imm4, C
*/
    public BS5program asm_mov_Rx_imm4_C(String ccc, String f, String rx, String imm4) throws BS5Exception {
        return add_oooo_iiii_xxxx(ccc, f==null?"nf":f, "mov Rx:imm4, C", rx, imm4);
    }

/*ccc f 1100 iiii xxxx
	mov Rx:imm4, 0
*/
    public BS5program asm_mov_Rx_imm4_0(String ccc, String f, String rx, String imm4) throws BS5Exception {
        return add_oooo_iiii_xxxx(ccc, f==null?"nf":f, "mov Rx:imm4, 0", rx, imm4);
    }

/*ccc f 1101 iiii xxxx
	mov Rx:imm4, 1
*/
    public BS5program asm_mov_Rx_imm4_1(String ccc, String f, String rx, String imm4) throws BS5Exception {
        return add_oooo_iiii_xxxx(ccc, f==null?"nf":f, "mov Rx:imm4, 1", rx, imm4);
    }

/*ccc f 1110 iiii xxxx
	not Rx:imm4
*/
    public BS5program asm_not_Rx_imm4(String ccc, String f, String rx, String imm4) throws BS5Exception {
        return add_oooo_iiii_xxxx(ccc, f==null?"fl":f, "not Rx:imm4", rx, imm4);
    }

/*
ccc f 1111 0000 iiii
	add R0, imm4
*/
    public BS5program asm_add_R0_imm4(String ccc, String f, String imm4) throws BS5Exception {
        return add_oooo_oooo_iiii(ccc, f==null?"fl":f, "add R0, imm4", imm4);
    }

/*
ccc f 1111 0001 iiii
	sub R0, imm4
*/
    public BS5program asm_sub_R0_imm4(String ccc, String f, String imm4) throws BS5Exception {
        return add_oooo_oooo_iiii(ccc, f==null?"fl":f, "sub R0, imm4", imm4);
    }

/*
ccc f 1111 0010 iiii
	shl R0, imm4
*/
    public BS5program asm_shl_R0_imm4(String ccc, String f, String imm4) throws BS5Exception {
        return add_oooo_oooo_iiii(ccc, f==null?"fl":f, "shl R0, imm4", imm4);
    }

/*
ccc f 1111 0011 iiii
	shr R0, imm4
*/
    public BS5program asm_shr_R0_imm4(String ccc, String f, String imm4) throws BS5Exception {
        return add_oooo_oooo_iiii(ccc, f==null?"fl":f, "shr R0, imm4", imm4);
    }

/*
ccc f 1111 0100 xxxx
    add Rx, 1
*/
    public BS5program asm_add_Rx_1(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"fl":f,"add Rx, 1", rx);
    }

/*
ccc f 1111 0101 xxxx
    sub Rx, 1
*/
    public BS5program asm_sub_Rx_1(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"fl":f,"sub Rx, 1", rx);
    }

/*
ccc f 1111 0110 xxxx
    mov low R0, low Rx
*/
    public BS5program asm_mov_low_R0_low_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov low R0, low Rx", rx);
    }

/*
ccc f 1111 0111 xxxx
    mov low R0, high Rx
*/
    public BS5program asm_mov_low_R0_high_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov low R0, high Rx", rx);
    }

/*
ccc f 1111 1000 xxxx
    mov high R0, low Rx
*/
    public BS5program asm_mov_high_R0_low_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov high R0, low Rx", rx);
    }

/*
ccc f 1111 1001 xxxx
    mov high R0, high Rx
*/
    public BS5program asm_mov_high_R0_high_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov high R0, high Rx", rx);
    }

/*
ccc f 1111 1010 xxxx
    mov low Rx, 0
*/
    public BS5program asm_mov_low_Rx_0(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov low Rx, 0", rx);
    }

/*
ccc f 1111 1011 xxxx
    mov high Rx, 0
*/
    public BS5program asm_mov_high_Rx_0(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov high Rx, 0", rx);
    }

/*
ccc f 1111 1100 xxxx
    mov Rx, 0
*/
    public BS5program asm_mov_Rx_0(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"nf":f,"mov Rx, 0", rx);
    }

/*
ccc f 1111 1101 xxxx
    not Rx
*/
    public BS5program asm_not_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"fl":f,"not Rx", rx);
    }

/*
ccc f 1111 1110 xxxx
    and R0, Rx
*/
    public BS5program asm_and_R0_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"fl":f,"and R0, Rx", rx);
    }

/*
ccc f 1111 1111 xxxx
    or  R0, Rx
*/
    public BS5program asm_or_R0_Rx(String ccc, String f, String rx) throws BS5Exception {
        return add_oooo_oooo_xxxx(ccc, f==null?"fl":f,"or R0, Rx", rx);
    }

}