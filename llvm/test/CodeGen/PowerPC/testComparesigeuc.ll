; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -verify-machineinstrs -mtriple=powerpc64-unknown-linux-gnu -O2 \
; RUN:   -ppc-gpr-icmps=all -ppc-asm-full-reg-names -mcpu=pwr8 < %s | FileCheck %s \
; RUN:  --implicit-check-not cmpw --implicit-check-not cmpd --implicit-check-not cmpl \
; RUN:  --check-prefixes=CHECK,BE
; RUN: llc -verify-machineinstrs -mtriple=powerpc64le-unknown-linux-gnu -O2 \
; RUN:   -ppc-gpr-icmps=all -ppc-asm-full-reg-names -mcpu=pwr8 < %s | FileCheck %s \
; RUN:  --implicit-check-not cmpw --implicit-check-not cmpd --implicit-check-not cmpl \
; RUN:  --check-prefixes=CHECK,LE

@glob = common local_unnamed_addr global i8 0, align 1

; Function Attrs: norecurse nounwind readnone
define signext i32 @test_igeuc(i8 zeroext %a, i8 zeroext %b) {
; CHECK-LABEL: test_igeuc:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    sub r3, r3, r4
; CHECK-NEXT:    not r3, r3
; CHECK-NEXT:    rldicl r3, r3, 1, 63
; CHECK-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, %b
  %conv2 = zext i1 %cmp to i32
  ret i32 %conv2
}

; Function Attrs: norecurse nounwind readnone
define signext i32 @test_igeuc_sext(i8 zeroext %a, i8 zeroext %b) {
; CHECK-LABEL: test_igeuc_sext:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    sub r3, r3, r4
; CHECK-NEXT:    rldicl r3, r3, 1, 63
; CHECK-NEXT:    addi r3, r3, -1
; CHECK-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, %b
  %sub = sext i1 %cmp to i32
  ret i32 %sub

}

; Function Attrs: norecurse nounwind readnone
define signext i32 @test_igeuc_z(i8 zeroext %a) {
; CHECK-LABEL: test_igeuc_z:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    li r3, 1
; CHECK-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, 0
  %conv2 = zext i1 %cmp to i32
  ret i32 %conv2
}

; Function Attrs: norecurse nounwind readnone
define signext i32 @test_igeuc_sext_z(i8 zeroext %a) {
; CHECK-LABEL: test_igeuc_sext_z:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    li r3, -1
; CHECK-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, 0
  %conv2 = sext i1 %cmp to i32
  ret i32 %conv2
}

; Function Attrs: norecurse nounwind
define void @test_igeuc_store(i8 zeroext %a, i8 zeroext %b) {
; BE-LABEL: test_igeuc_store:
; BE:       # %bb.0: # %entry
; BE-NEXT:    addis r5, r2, .LC0@toc@ha
; BE-NEXT:    sub r3, r3, r4
; BE-NEXT:    ld r4, .LC0@toc@l(r5)
; BE-NEXT:    not r3, r3
; BE-NEXT:    rldicl r3, r3, 1, 63
; BE-NEXT:    stb r3, 0(r4)
; BE-NEXT:    blr
;
; LE-LABEL: test_igeuc_store:
; LE:       # %bb.0: # %entry
; LE-NEXT:    sub r3, r3, r4
; LE-NEXT:    addis r5, r2, glob@toc@ha
; LE-NEXT:    not r3, r3
; LE-NEXT:    rldicl r3, r3, 1, 63
; LE-NEXT:    stb r3, glob@toc@l(r5)
; LE-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, %b
  %conv3 = zext i1 %cmp to i8
  store i8 %conv3, i8* @glob
  ret void
; CHECK_LABEL: test_igeuc_store:
}

; Function Attrs: norecurse nounwind
define void @test_igeuc_sext_store(i8 zeroext %a, i8 zeroext %b) {
; BE-LABEL: test_igeuc_sext_store:
; BE:       # %bb.0: # %entry
; BE-NEXT:    addis r5, r2, .LC0@toc@ha
; BE-NEXT:    sub r3, r3, r4
; BE-NEXT:    ld r4, .LC0@toc@l(r5)
; BE-NEXT:    rldicl r3, r3, 1, 63
; BE-NEXT:    addi r3, r3, -1
; BE-NEXT:    stb r3, 0(r4)
; BE-NEXT:    blr
;
; LE-LABEL: test_igeuc_sext_store:
; LE:       # %bb.0: # %entry
; LE-NEXT:    sub r3, r3, r4
; LE-NEXT:    addis r5, r2, glob@toc@ha
; LE-NEXT:    rldicl r3, r3, 1, 63
; LE-NEXT:    addi r3, r3, -1
; LE-NEXT:    stb r3, glob@toc@l(r5)
; LE-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, %b
  %conv3 = sext i1 %cmp to i8
  store i8 %conv3, i8* @glob
  ret void
; CHECK-TBD-LABEL: @test_igeuc_sext_store
; CHECK-TBD: subf [[REG1:r[0-9]+]], r3, r4
; CHECK-TBD: rldicl [[REG2:r[0-9]+]], [[REG1]], 1, 63
; CHECK-TBD: addi [[REG3:r[0-9]+]], [[REG2]], -1
; CHECK-TBD: stb  [[REG3]]
; CHECK-TBD: blr
}

; Function Attrs : norecurse nounwind
define void @test_igeuc_z_store(i8 zeroext %a) {
; BE-LABEL: test_igeuc_z_store:
; BE:       # %bb.0: # %entry
; BE-NEXT:    addis r3, r2, .LC0@toc@ha
; BE-NEXT:    li r4, 1
; BE-NEXT:    ld r3, .LC0@toc@l(r3)
; BE-NEXT:    stb r4, 0(r3)
; BE-NEXT:    blr
;
; LE-LABEL: test_igeuc_z_store:
; LE:       # %bb.0: # %entry
; LE-NEXT:    addis r3, r2, glob@toc@ha
; LE-NEXT:    li r4, 1
; LE-NEXT:    stb r4, glob@toc@l(r3)
; LE-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, 0
  %conv3 = zext i1 %cmp to i8
  store i8 %conv3, i8* @glob
  ret void
}

; Function Attrs: norecurse nounwind
define void @test_igeuc_sext_z_store(i8 zeroext %a) {
; BE-LABEL: test_igeuc_sext_z_store:
; BE:       # %bb.0: # %entry
; BE-NEXT:    addis r3, r2, .LC0@toc@ha
; BE-NEXT:    li r4, -1
; BE-NEXT:    ld r3, .LC0@toc@l(r3)
; BE-NEXT:    stb r4, 0(r3)
; BE-NEXT:    blr
;
; LE-LABEL: test_igeuc_sext_z_store:
; LE:       # %bb.0: # %entry
; LE-NEXT:    addis r3, r2, glob@toc@ha
; LE-NEXT:    li r4, -1
; LE-NEXT:    stb r4, glob@toc@l(r3)
; LE-NEXT:    blr
entry:
  %cmp = icmp uge i8 %a, 0
  %conv3 = sext i1 %cmp to i8
  store i8 %conv3, i8* @glob
  ret void
}
