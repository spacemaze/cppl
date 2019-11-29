; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=i386-unknown-openbsd6.2 | FileCheck %s

%struct.DateTime = type { i64, i32, i32, i32, i32, i32, double, i8 }

define void @computeJD(%struct.DateTime*) nounwind {
; CHECK-LABEL: computeJD:
; CHECK:       # %bb.0:
; CHECK-NEXT:    pushl %ebp
; CHECK-NEXT:    movl %esp, %ebp
; CHECK-NEXT:    pushl %ebx
; CHECK-NEXT:    pushl %edi
; CHECK-NEXT:    pushl %esi
; CHECK-NEXT:    andl $-8, %esp
; CHECK-NEXT:    subl $40, %esp
; CHECK-NEXT:    movl 8(%ebp), %ebx
; CHECK-NEXT:    movl 8(%ebx), %esi
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    cmpl $3, 12(%ebx)
; CHECK-NEXT:    setl %al
; CHECK-NEXT:    subl %eax, %esi
; CHECK-NEXT:    movl $-1374389535, %ecx # imm = 0xAE147AE1
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    imull %ecx
; CHECK-NEXT:    movl %edx, %ecx
; CHECK-NEXT:    movl %edx, %eax
; CHECK-NEXT:    shrl $31, %eax
; CHECK-NEXT:    sarl $5, %ecx
; CHECK-NEXT:    addl %eax, %ecx
; CHECK-NEXT:    movl $1374389535, %edx # imm = 0x51EB851F
; CHECK-NEXT:    movl %esi, %eax
; CHECK-NEXT:    imull %edx
; CHECK-NEXT:    movl %edx, %edi
; CHECK-NEXT:    movl %edx, %eax
; CHECK-NEXT:    shrl $31, %eax
; CHECK-NEXT:    sarl $7, %edi
; CHECK-NEXT:    addl %eax, %edi
; CHECK-NEXT:    imull $36525, %esi, %eax # imm = 0x8EAD
; CHECK-NEXT:    addl $172251900, %eax # imm = 0xA445AFC
; CHECK-NEXT:    movl $1374389535, %edx # imm = 0x51EB851F
; CHECK-NEXT:    imull %edx
; CHECK-NEXT:    movl %edx, %eax
; CHECK-NEXT:    shrl $31, %eax
; CHECK-NEXT:    sarl $5, %edx
; CHECK-NEXT:    addl %eax, %edx
; CHECK-NEXT:    addl 16(%ebx), %ecx
; CHECK-NEXT:    addl %edi, %ecx
; CHECK-NEXT:    leal 257(%ecx,%edx), %eax
; CHECK-NEXT:    movl %eax, {{[0-9]+}}(%esp)
; CHECK-NEXT:    fildl {{[0-9]+}}(%esp)
; CHECK-NEXT:    fadds {{\.LCPI.*}}
; CHECK-NEXT:    fmuls {{\.LCPI.*}}
; CHECK-NEXT:    fnstcw {{[0-9]+}}(%esp)
; CHECK-NEXT:    movzwl {{[0-9]+}}(%esp), %eax
; CHECK-NEXT:    orl $3072, %eax # imm = 0xC00
; CHECK-NEXT:    movw %ax, {{[0-9]+}}(%esp)
; CHECK-NEXT:    fldcw {{[0-9]+}}(%esp)
; CHECK-NEXT:    fistpll {{[0-9]+}}(%esp)
; CHECK-NEXT:    fldcw {{[0-9]+}}(%esp)
; CHECK-NEXT:    movb $1, 36(%ebx)
; CHECK-NEXT:    imull $3600000, 20(%ebx), %eax # imm = 0x36EE80
; CHECK-NEXT:    imull $60000, 24(%ebx), %ecx # imm = 0xEA60
; CHECK-NEXT:    addl %eax, %ecx
; CHECK-NEXT:    fldl 28(%ebx)
; CHECK-NEXT:    fmuls {{\.LCPI.*}}
; CHECK-NEXT:    fnstcw {{[0-9]+}}(%esp)
; CHECK-NEXT:    movzwl {{[0-9]+}}(%esp), %eax
; CHECK-NEXT:    orl $3072, %eax # imm = 0xC00
; CHECK-NEXT:    movw %ax, {{[0-9]+}}(%esp)
; CHECK-NEXT:    movl %ecx, %eax
; CHECK-NEXT:    sarl $31, %eax
; CHECK-NEXT:    fldcw {{[0-9]+}}(%esp)
; CHECK-NEXT:    fistpll {{[0-9]+}}(%esp)
; CHECK-NEXT:    fldcw {{[0-9]+}}(%esp)
; CHECK-NEXT:    addl {{[0-9]+}}(%esp), %ecx
; CHECK-NEXT:    adcl {{[0-9]+}}(%esp), %eax
; CHECK-NEXT:    addl {{[0-9]+}}(%esp), %ecx
; CHECK-NEXT:    adcl {{[0-9]+}}(%esp), %eax
; CHECK-NEXT:    movl %ecx, (%ebx)
; CHECK-NEXT:    movl %eax, 4(%ebx)
; CHECK-NEXT:    leal -12(%ebp), %esp
; CHECK-NEXT:    popl %esi
; CHECK-NEXT:    popl %edi
; CHECK-NEXT:    popl %ebx
; CHECK-NEXT:    popl %ebp
; CHECK-NEXT:    retl
  %2 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 7
  %3 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 1
  %4 = load i32, i32* %3, align 4
  %5 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 2
  %6 = load i32, i32* %5, align 4
  %7 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 3
  %8 = load i32, i32* %7, align 4
  %9 = icmp slt i32 %6, 3
  %10 = add i32 %6, 12
  %11 = select i1 %9, i32 %10, i32 %6
  %12 = sext i1 %9 to i32
  %13 = add i32 %4, %12
  %14 = sdiv i32 %13, -100
  %15 = sdiv i32 %13, 400
  %16 = mul i32 %13, 36525
  %17 = add i32 %16, 172251900
  %18 = sdiv i32 %17, 100
  %19 = mul i32 %11, 306001
  %20 = add i32 %19, 306001
  %21 = sdiv i32 %20, 10000
  %22 = add i32 %8, 2
  %23 = add i32 %22, %14
  %24 = add i32 %23, %15
  %25 = add i32 %24, 255
  %26 = add i32 %25, %18
  %27 = sitofp i32 %26 to double
  %28 = fadd double %27, -1.524500e+03
  %29 = fmul double %28, 8.640000e+07
  %30 = fptosi double %29 to i64
  %31 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 0
  store i8 1, i8* %2, align 4
  %32 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 4
  %33 = load i32, i32* %32, align 4
  %34 = mul i32 %33, 3600000
  %35 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 5
  %36 = load i32, i32* %35, align 4
  %37 = mul i32 %36, 60000
  %38 = add i32 %37, %34
  %39 = sext i32 %38 to i64
  %40 = getelementptr inbounds %struct.DateTime, %struct.DateTime* %0, i32 0, i32 6
  %41 = load double, double* %40, align 4
  %42 = fmul double %41, 1.000000e+03
  %43 = fptosi double %42 to i64
  %44 = add i64 %39, %43
  %45 = add i64 %44, %30
  store i64 %45, i64* %31, align 4
  ret void
}

attributes #0 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="i486" "target-features"="+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
