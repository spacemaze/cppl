// RUN: cppl -root=%S -buildRoot=%T -preamble=%S/preamble.hpp -o %T/a.out \
// RUN:    --verbose \
// RUN:    -FH "-DH" -FP "-DP -DV=\"QUOTED ARG\" -DV2=ESCAPED\\ ARG" -FC "-DC" -FL "-LNON_EXISTING" | FileCheck %s
// RUN: %T/a.out

// CHECK:      Extra args, phase 'Preamble':
// CHECK-NEXT: -DH
// CHECK:      Extra args, phase 'Parse':
// CHECK-NEXT: -DP -DV="QUOTED ARG" -DV2=ESCAPED ARG
// CHECK:      Extra args, phase 'CodeGen':
// CHECK-NEXT: -DC
// CHECK:      Extra args, phase 'Link':
// CHECK-NEXT: -LNON_EXISTING

// CHECK: PREAMBLE{{.*}}
// CHECK-NEXT: {{.*}}-DH{{.*}}

// CHECK: PARSE{{.*}}
// CHECK-NEXT: {{.*}}-DP -DV="QUOTED ARG" -DV2=ESCAPED ARG{{.*}}

// CHECK: INST DECL{{.*}}
// CHECK-NEXT: {{.*}}-DC{{.*}}

// CHECK: MAIN OBJ{{.*}}
// CHECK-NEXT: {{.*}}-DP -DV="QUOTED ARG" -DV2=ESCAPED ARG -DC{{.*}}

int main() {
  with (
    auto TestScope = levitation::Test::context()
        .expect("P1::A::f()")
    .open()
  ) {
    P1::A::f();
  }
  return levitation::Test::result();
}