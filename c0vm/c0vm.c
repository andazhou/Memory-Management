#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "lib/xalloc.h"
#include "lib/stack.h"
#include "lib/contracts.h"
#include "lib/c0v_stack.h"
#include "lib/c0vm.h"
#include "lib/c0vm_c0ffi.h"
#include "lib/c0vm_abort.h"

/* call stack frames */
typedef struct frame_info frame;
struct frame_info {
  c0v_stack_t S; /* Operand stack of C0 values */
  ubyte *P;      /* Function body */
  size_t pc;     /* Program counter */
  c0_value *V;   /* The local variables */
};

int execute(struct bc0_file *bc0) {
  REQUIRES(bc0 != NULL);

  /* Variables */
  c0v_stack_t S = c0v_stack_new(); /* Operand stack of C0 values */
  ubyte *P = bc0->function_pool->code;      /* The array of bytes that make up the current function */
  size_t pc = 0;     /* Your current location within the current byte array P */
  c0_value *V = xcalloc(bc0->function_pool->num_vars, sizeof(c0_value));
  (void) V;

  /* The call stack, a generic stack that should contain pointers to frames */
  /* You won't need this until you implement functions. */
  gstack_t callStack = stack_new();
  (void) callStack;

  while (true) {

#ifdef DEBUG
    /* You can add extra debugging information here */
    fprintf(stderr, "Opcode %x -- Stack size: %zu -- PC: %zu\n",
            P[pc], c0v_stack_size(S), pc);
#endif

    switch (P[pc]) {

    /* Additional stack operation: */

    case POP: {
      pc++;
      c0v_pop(S);
      break;
    }

    case DUP: {
      pc++;
      c0_value v = c0v_pop(S);
      c0v_push(S,v);
      c0v_push(S,v);
      break;
    }

     case SWAP: {
      pc++;

      c0_value a = c0v_pop(S);
      c0_value b = c0v_pop(S);

      c0v_push(S, a);
      c0v_push(S, b);

      break;
      }



    /* Returning from a function.
     * This currently has a memory leak! It will need to be revised
     * when you write INVOKESTATIC. */

    case RETURN: {
      c0_value retval = c0v_pop(S);
      assert(c0v_stack_empty(S));
#ifdef DEBUG
      fprintf(stderr, "Returning %d from execute()\n", val2int(retval));
#endif
      // Free everything before returning from the execute function!

      if(stack_empty(callStack)) {
        c0v_stack_free(S);
        stack_free(callStack, NULL);
        free(V);
        return val2int(retval);

        }
      else {
        free(V);
        frame *retFrame = pop(callStack);
        c0v_stack_free(S);
        V = retFrame->V;
        S = retFrame->S;
        P = retFrame->P;
        pc = retFrame->pc;
        c0v_push(S, retval);
        free(retFrame);
        }
      break;
      }


    /* Arithmetic and Logical operations */

    case IADD: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));

      c0v_push(S, int2val(a+b));
      break;
      }

    case ISUB: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));

      c0v_push(S, int2val(b-a));
      break;
      }


    case IMUL: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));

      c0v_push(S, int2val(a*b));
      break;
      }


    case IDIV: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      if(b == INT_MIN && a == -1) c0_arith_error("Overflow");
      if(a == 0) c0_arith_error("Division by zero");
      c0v_push(S, int2val(b/a));
      break;
      }

    case IREM: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      if(b == INT_MIN && a == -1) c0_arith_error("Overflow");
      if(a == 0) c0_arith_error("Division by zero");
      c0v_push(S, int2val(b%a));
      break;
      }

    case IAND: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      c0v_push(S, int2val(b&a));
      break;
      }

    case IOR: {


      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      c0v_push(S, int2val(b|a));
      break;
      }


    case IXOR: {


      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      c0v_push(S, int2val(b^a));
      break;
      }


    case ISHL: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      if(a < 0 || a >= 32) c0_arith_error("Shift number not valid");
      c0v_push(S, int2val(b<<a));
      break;
      }

    case ISHR: {

      pc++;
      int32_t a = val2int(c0v_pop(S));
      int32_t b = val2int(c0v_pop(S));
      if(a < 0 || a >= 32) c0_arith_error("Shift number not valid");
      c0v_push(S, int2val(b>>a));
      break;
      }


    /* Pushing constants */

    case BIPUSH: {
      int8_t v = (int8_t)P[pc+1];
      c0v_push(S, int2val((int32_t)v));
      pc = pc + 2;
      break;
      }


    case ILDC: {
      uint32_t c1 = P[pc+1];
      uint32_t c2 = P[pc+2];
      c0v_push(S, int2val((int32_t)bc0->int_pool[(uint16_t)c1<<8|c2]));
      pc = pc + 3;
      break;
      }

    case ALDC: {
      uint32_t c1 = P[pc+1];
      uint32_t c2 = P[pc+2];
      c0v_push(S, ptr2val((char*)&(bc0->string_pool[(uint16_t)c1<<8|c2])));
      pc = pc + 3;
      break;
      }

    case ACONST_NULL: {
      c0v_push(S, ptr2val(NULL));
      pc++;
      break;
      }


    /* Operations on local variables */

    case VLOAD: {
      c0v_push(S, V[P[pc+1]]);
      pc = pc + 2;
      break;
      }

    case VSTORE: {
      uint32_t val = P[pc+1];
      V[val] = c0v_pop(S);
      pc = pc + 2;
      break;
      }



    /* Control flow operations */

    case NOP: {
      pc++;
      break;
      }

    case IF_CMPEQ: {
      c0_value v1 = c0v_pop(S);
      c0_value v2 = c0v_pop(S);
      int o1 = P[pc+1];
      int o2 = P[pc+2];
      if(val_equal(v1,v2)) {
        pc = pc + (int16_t)(o1<<8|o2);
        }
      else {
        pc = pc + 3;
        }
      break;
      }


    case IF_CMPNE: {
      c0_value v1 = c0v_pop(S);
      c0_value v2 = c0v_pop(S);
      int o1 = P[pc+1];
      int o2 = P[pc+2];
      if(!val_equal(v1,v2)) {
        pc = pc + (o1<<8|o2);
        }
      else {
        pc = pc + 3;
        }
      break;
      }


    case IF_ICMPLT: {
      int32_t v1 = val2int(c0v_pop(S));
      int32_t v2 = val2int(c0v_pop(S));
      int o1 = P[pc+1];
      int o2 = P[pc+2];
      if(v2 < v1) {
        pc = pc + (o1<<8 | o2);
        }
      else {
        pc = pc + 3;
        }
      break;
      }


    case IF_ICMPGE: {

      int32_t v1 = val2int(c0v_pop(S));
      int32_t v2 = val2int(c0v_pop(S));
      int o1 = P[pc+1];
      int o2 = P[pc+2];
      if(v2 >= v1) {
        pc = pc + (o1<<8 | o2);
        }
      else {
        pc = pc + 3;
        }
      break;
      }

    case IF_ICMPGT: {

      int32_t v1 = val2int(c0v_pop(S));
      int32_t v2 = val2int(c0v_pop(S));
      int o1 = P[pc+1];
      int o2 = P[pc+2];
      if(v2 > v1) {
        pc = pc + (o1<<8 | o2);
        }
      else {
        pc = pc + 3;
        }
      break;
      }


    case IF_ICMPLE: {

      int32_t v1 = val2int(c0v_pop(S));
      int32_t v2 = val2int(c0v_pop(S));
      int32_t o1 = P[pc+1];
      uint32_t o2 = P[pc+2];
      if(v2 <= v1) {
        pc = pc + (int16_t)(o1<<8 | o2);
        }
      else {
        pc = pc + 3;
        }
      break;
      }

    case GOTO: {
      int32_t o1 = P[pc+1];
      uint32_t o2 = P[pc+2];
      pc = pc + (int16_t)(o1<<8 | o2);
      break;
      }

    case ATHROW: {
      char* message = val2ptr(c0v_pop(S));
      c0_user_error(message);
      pc++;
      break;
      }

    case ASSERT: {

      c0v_pop(S);
      int32_t x = val2int(c0v_pop(S));
      if(x == 0) c0_assertion_failure("error");
      pc++;
      break;
      }


    /* Function call operations: */

    case INVOKESTATIC: {
      uint32_t c1 = P[pc+1];
      uint32_t c2 = P[pc+2];

      frame *curFrame = xmalloc(sizeof(frame));
      curFrame->V = V;
      curFrame->S = S;
      curFrame->P = P;
      curFrame->pc = pc + 3;
      push(callStack, curFrame);

      struct function_info *finfo = &bc0->function_pool[(uint32_t)(c1<<8|c2)];
      c0_value *Vn = xmalloc(finfo->num_vars * sizeof(c0_value));

      for(int i = 0; i < finfo->num_args; i++) {
        Vn[finfo->num_args-1-i] = c0v_pop(S);
        }


      V = Vn;
      S = c0v_stack_new();
      P = finfo->code;
      pc = 0;
      break;
      }

    case INVOKENATIVE: {
      uint32_t c1 = P[pc+1];
      uint32_t c2 = P[pc+2];
      struct native_info *ninfo = &bc0->native_pool[(c1<<8|c2)];
      c0_value *Vn = xmalloc(ninfo->num_args * sizeof(c0_value));
      for(int i = 0; i < ninfo->num_args; i++) {
        Vn[ninfo->num_args-1-i] = c0v_pop(S);
        }
      c0v_push(S, (native_function_table[ninfo->function_table_index])(Vn));
      pc = pc + 3;
      free(Vn);
      break;
      }



    /* Memory allocation operations: */

    case NEW: {
      uint32_t s = P[pc+1];
      c0v_push(S, ptr2val(xmalloc(s)));
      pc = pc + 2;
      break;
      }

    case NEWARRAY: {
      int32_t n = val2int(c0v_pop(S));
      if(n < 0) c0_memory_error("Invalid number of elements");
      int32_t s = P[pc+1];
      struct c0_array_header *a = xcalloc(1,(n * s) + sizeof(struct c0_array_header));
      a->count = n;
      a->elt_size = s;
      c0v_push(S, ptr2val(a));
      pc = pc + 2;
      break;
      }

    case ARRAYLENGTH: {
      struct c0_array_header *a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Null access");

      c0v_push(S, int2val(a->count));
      pc++;
    //  free(a);
      break;
      }



    /* Memory access operations: */

    case AADDF: {
      ubyte f = (uint32_t)P[pc+1];
      ubyte* a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Null access");
      c0v_push(S, ptr2val(a + f));
      pc = pc + 2;
      break;
      }

    case AADDS: {
      int32_t i = val2int(c0v_pop(S));
      if(i < 0) c0_memory_error("Not a valid index");
      c0_array* a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Null access");
      if(i >= a->count) c0_memory_error("Not a valid index");
      if(a->elt_size < 0) c0_memory_error("Invalid size");
      c0v_push(S,ptr2val(a->elt_size*i + (uint8_t*)a + 2*sizeof(int)));
      pc++;
      break;
      }

    case IMLOAD: {
      void* a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Unsafe access");
      c0v_push(S, int2val(*(uint8_t*)a));
      pc++;
      break;
      }

    case IMSTORE: {
      int x = val2int(c0v_pop(S));
      int *a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Unsafe access");
      *a = x;
      pc++;
      break;
      }

    case AMLOAD: {
      void**a =val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Unsafe access");
      c0v_push(S,ptr2val(a));
      pc++;
      break;
      }


    case AMSTORE: {
      c0_value x = c0v_pop(S);
      c0_value *a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Unsafe access");
      *a = x;
      pc++;
      break;
      }

    case CMLOAD: {
      char *a =val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Unsafe access");
      uint8_t x = (uint8_t)(*a);
      c0v_push(S, int2val(x));
      pc++;
      break;
      }

    case CMSTORE: {
      int32_t x = val2int(c0v_pop(S));
      char *a = val2ptr(c0v_pop(S));
      if(a == NULL) c0_memory_error("Unsafe access");
      *a = x&0x7f;
      pc++;
      break;
      }


    default:
      fprintf(stderr, "invalid opcode: 0x%02x\n", P[pc]);
      abort();
    }
  }

  /* cannot get here from infinite loop */
  assert(false);
}

