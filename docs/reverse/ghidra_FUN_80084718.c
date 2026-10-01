// Ghidra 12.1.4 headless decompilation of FUN_80084718 (per-entity transform setup)
// from a full 2 MB live-RAM image imported at 0x80000000. See docs/reverse/GHIDRA.md.

=== FUNCTION FUN_80084718 @ 80084718 body 80084718..800847cb  
   CALLER 800814c4 FUN_800814c4  
   CALLER 80046dd4 FUN_80046dd4  
   CALLER 80080dd4 FUN_80080dd4  
--- DECOMP ---  

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_80084718(int param_1,undefined4 param_2,int param_3,undefined4 param_4)

{
  undefined4 local_28;
  undefined4 local_24;
  undefined4 local_20;
  undefined4 local_1c;
  undefined2 local_18;
  int local_14;
  int local_10;
  int local_c;
  
  local_18 = 0x1000;
  local_14 = (int)*(short *)(param_1 + 0x98) << 0x13;
  local_28 = 0x1000;
  local_20 = 0x1000;
  local_10 = (int)*(short *)(param_1 + 0x9a) << 0x13;
  local_24 = 0;
  local_1c = 0;
  DAT_8009eab4 = (uint)*(byte *)(param_3 + 0xf);
  local_c = (int)*(short *)(param_1 + 0x9c) << 0x13;
  PTR_DAT_8009eabc = (undefined *)&DAT_800ab148;
  DAT_8009eab8 = *(undefined4 *)(*(int *)(param_1 + 0x80) + 4);
  _DAT_1f8003fc = 0;
  FUN_80013ae4(param_2,&local_28,param_4,param_1);
  return;
}

  