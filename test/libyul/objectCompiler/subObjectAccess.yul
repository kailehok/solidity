object "A" {
  code {
    pop(dataoffset("A"))
    pop(dataoffset("B"))
    pop(dataoffset("A.B"))
  }

  object "B" {
    code {
      pop(dataoffset("B.C"))
    }
    object "C" {
      code {
        invalid()
      }
    }
  }
}
// ----
// Assembly:
//     /* "source":26:46   */
//   pop(0x00)
//     /* "source":51:71   */
//   pop(dataOffset(sub_0))
//     /* "source":76:98   */
//   pop(dataOffset(sub_0))
// stop
//
// sub_0: assembly {
//         /* "source":136:158   */
//       pop(dataOffset(sub_0))
//     stop
//
//     sub_0: assembly {
//             /* "source":203:212   */
//           invalid
//     }
// }
// Bytecode: 600050600a50600a50fe600450fefe
// Opcodes: PUSH1 0x0 POP PUSH1 0xA POP PUSH1 0xA POP INVALID PUSH1 0x4 POP INVALID INVALID
// SourceMappings: 26:20:0:-:0;;51;;76:22;
