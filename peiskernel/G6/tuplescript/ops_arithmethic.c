/** \file ops_arithmethic.c
    \brief Defines all built-in arithmethic and boolean operations */

 case SUM: case PLUS: case SYM_PLUS: case PROD: case ASTERIX: {
   for(i=0;i<argNum;i++) if(valueStack[valueStart+i]->kind != eInt) break;
   if(i == argNum) {
     l=value2int(valueStack[valueStart]);
     if(operand == SUM || operand == PLUS || operand == SYM_PLUS)
       for(i=1;i<argNum;i++) { l += value2int(valueStack[valueStart+i]); unreference(valueStack[valueStart+i]); }
	  else 
	    for(i=1;i<argNum;i++) { l *= value2int(valueStack[valueStart+i]); unreference(valueStack[valueStart+i]); }
     aritIResult(l);
   } else {
     d=value2double(valueStack[valueStart]);
     if(operand == SUM || operand == PLUS || operand == SYM_PLUS)
       for(i=1;i<argNum;i++) { d += value2double(valueStack[valueStart+i]); unreference(valueStack[valueStart+i]); }
     else 
       for(i=1;i<argNum;i++) { d *= value2double(valueStack[valueStart+i]); unreference(valueStack[valueStart+i]); }
     aritDResult(d);
   }	  
   valueDepth=valueStart+1;
 } break;

 case EQ:
 case SYM_EQ: {
   Value *a = valueStack[valueStart];
   Value *b = valueStack[valueStart+1];
   if(a->kind != b->kind) aritBResult(0);
   else if(a == b) aritBResult(1);
   else if(a->kind == eInt || a->kind == eAtom) aritBResult(a->data.ivalue == b->data.ivalue); 
   else if(a->kind == eConstString || a->kind == eDynamicString) aritBResult(a->data.string == b->data.string);
   else aritBResult(0);
   unreference(b);
   valueDepth=valueStart+1;
 } break;

 case EQUAL: {
   Value *a = valueStack[valueStart];
   Value *b = valueStack[valueStart+1];
   switch(a->kind) {
   case eError: aritBResult(b->kind == a->kind); break;
   case eAtom: 
     if(b->kind == eAtom) aritBResult(b->data.token == a->data.token);
     else if(b->kind == eConstString || b->kind == eDynamicString) aritBResult(strcmp(token2string(a->data.token),b->data.string) == 0);
     else aritBResult(0);
     break;
   case eConstString:
   case eDynamicString:
     if(b->kind == eAtom) aritBResult(strcmp(token2string(a->data.token),b->data.string) == 0);
     else if(b->kind == eConstString || b->kind == eDynamicString) aritBResult(strcmp(a->data.string,b->data.string) == 0);
     else aritBResult(0);
     break;
   case eFloat: 
     if(b->kind == eInt) return aritBResult(a->data.dvalue == (double) b->data.ivalue);
     else if(b->kind == eFloat) return aritBResult(a->data.dvalue == b->data.dvalue);
     else aritBResult(0);
     break;
   case eInt:
     if(b->kind == eInt) return aritBResult(a->data.ivalue == b->data.ivalue);
     else if(b->kind == eFloat) return aritBResult((double)a->data.ivalue == b->data.dvalue);
     else aritBResult(0);
     break;
   default:
     aritBResult(a == b);
   }
   unreference(b);
   valueDepth=valueStart+1;
 } break;

 case MINUS:
 case SYM_MINUS: {
   /* Performs a math operation and stores result on the stack. Uses aritResult to optimistically avoid creating redundant value objects */
   if(argNum == 1) {
     /* If there is only one argument given, just negate it, using the right datatype (int/float) */
     if(valueStack[valueStart]->kind == eInt) aritIResult(-value2int(valueStack[valueStart]));
     else aritDResult(-value2double(valueStack[valueStart]));
   } else {
     /* If multiple arguments given perform subtraction using the right datatypes */
     for(i=0;i<argNum;i++) if(valueStack[valueStart+i]->kind != eInt) break;
     if(i == argNum) {
       l=value2int(valueStack[valueStart]);
       for(i=1;i<argNum;i++) { l -= value2int(valueStack[valueStart+i]); unreference(valueStack[valueStart+i]); }
       aritIResult(l);
     } else {
       d=value2double(valueStack[valueStart]);
       for(i=1;i<argNum;i++) { d -= value2double(valueStack[valueStart+i]); unreference(valueStack[valueStart+i]); }
       aritDResult(d);
     }
     valueDepth=valueStart+1;
   }
 } break;

 case TRUE: valueStack[valueDepth++]=boolean2value(1); break;
 case FALSE: valueStack[valueDepth++]=boolean2value(0); break;

