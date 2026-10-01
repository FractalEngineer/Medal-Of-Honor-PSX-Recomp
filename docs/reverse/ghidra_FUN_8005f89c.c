// Ghidra 12.1.4 headless decompilation of FUN_8005f89c (display/projection setup)
// from overlay capture ov_8005F000.bin (overlay code, not in the main EXE).
// See docs/reverse/GHIDRA.md.

=== FUNCTION FUN_8005f89c @ 8005f89c body 8005f89c..8005fb4f  
   CALLER 8005fe04 FUN_8005fe04  
--- DECOMP ---  

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4
FUN_8005f89c(uint param_1,char param_2,undefined1 param_3,undefined1 param_4,undefined1 param_5)

{
  bool bVar1;
  undefined4 uVar2;
  uint uVar3;
  uint uVar4;
  
  uVar3 = 0x200;
  uVar4 = 0xf0;
  if (param_1 < 4) {
    if (param_1 == 0) {
      uVar3 = 0x100;
    }
    else if (param_1 == 1) {
      uVar3 = 0x140;
    }
    else if (param_1 == 2) {
      DAT_8007bf7c = 1;
      _DAT_8007bf80 = 5;
      _DAT_8007bf84 = 8;
    }
    else if (param_1 == 3) {
      uVar4 = 0x100;
      DAT_8007bf7c = 1;
      _DAT_8007bf80 = 2;
      _DAT_8007bf84 = 3;
    }
    _DAT_8007bf50 = param_1;
    func_0x8001bb28(0x8007be5c,0,0,uVar3,uVar4);
    func_0x8001bb28(0x8007bed4,0,uVar4,uVar3,uVar4);
    func_0x8001bbe8(0x8007beb8,0,uVar4,uVar3,uVar4);
    func_0x8001bbe8(0x8007bf30,0,0,uVar3,uVar4);
    _DAT_8007bf68 = uVar3 - 1;
    _DAT_8007bf6c = uVar4 - 1;
    _DAT_8007bf70 = _DAT_8007bf68 >> 1;
    _DAT_8007bf74 = _DAT_8007bf6c >> 1;
    _DAT_8007bf60 = 0;
    _DAT_8007bf64 = 0;
    bVar1 = param_2 == '\0';
    if (bVar1) {
      DAT_8007bf55 = 0xff;
      DAT_8007bf56 = 0xff;
      DAT_8007bf57 = 0xff;
    }
    else {
      DAT_8007beef = param_5;
      DAT_8007be77 = param_5;
      DAT_8007be75 = param_3;
      DAT_8007be76 = param_4;
      DAT_8007beed = param_3;
      DAT_8007beee = param_4;
    }
    DAT_8007beec = !bVar1;
    DAT_8007be74 = !bVar1;
    _DAT_8007bf4c = 0x8007be5c;
    DAT_8007bf90 = 1;
    DAT_8007bf54 = param_2;
    _DAT_8007bf58 = uVar3;
    _DAT_8007bf5c = uVar4;
    uVar2 = func_0x8001d590(uVar3);
    uVar2 = func_0x8001d450(uVar2,0x3fddb3d7);
    uVar2 = func_0x8001d450(uVar2,0x3f000000);
    uVar2 = func_0x8001e11c(uVar2);
    _DAT_8007bf78 = uVar2;
    func_0x8001baa0();
    func_0x8001ba78(uVar3 >> 1,uVar4 >> 1);
    func_0x8001ba68(uVar2);
    func_0x8001722c(_DAT_8007bed0,0x600);
    uVar2 = 0;
    if (_DAT_8007becc != 0) {
      func_0x8001722c(_DAT_8007becc,0x600);
      uVar2 = 0;
    }
  }
  else {
    uVar2 = 0xfffffffe;
  }
  return uVar2;
}

  